#include "header.h"
#include "handler_staticfile.h"
#include "request.h"
#include "general.h"
#include "xxhash/xxhash.h"

void Http_StaticFileHandler(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return;

	// 过滤末尾是"."的非法请求
	if (ci->PATH_BLOCK.path_translated.back() == '.') {
	
		Send_HttpErrorPage(pServer, dwConnID, HSC_NOT_FOUND);
		return;
	}

	// 如果请求的资源是目录
	CStringPlus real_path_translated(ci->PATH_BLOCK.web_root);
	size_t qs_pos = ci->PATH_BLOCK.path.find("?");
	CStringPlus win_path_no_query_string(qs_pos == CStringPlus::npos ? ci->PATH_BLOCK.path : ci->PATH_BLOCK.path.substr(0, qs_pos));
	win_path_no_query_string.replace_all("/", "\\");
	real_path_translated.append(win_path_no_query_string);
	DWORD fileAttr = ::GetFileAttributesA(real_path_translated.c_str());

	if (fileAttr != INVALID_FILE_ATTRIBUTES && ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)) {

		// 如果没有匹配到默认页面，则输出目录列表页面
		// 也就是 ci->PATH_BLOCK.path_translated 没有追加 index 页面，即 ci->PATH_BLOCK.path_translated == real_path_translated
		if (ci->PATH_BLOCK.path_translated == real_path_translated) {
			
			// 检查配置是否允许目录列表
			if (VAR_ENABLE_DIR_LISTING) {

				directory_listing(pServer, dwConnID);
				return;

			} else {

				// 但是不允许目录列表，则返回403
				Send_HttpErrorPage(pServer, dwConnID, HSC_FORBIDDEN);
				return;
			}
		}
	}

	// 检查文件是否存在，不存在返回404
	if (!::PathFileExistsA(ci->PATH_BLOCK.path_translated.c_str())) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_NOT_FOUND);
		return;
	}

	CFileOperation file(ci->PATH_BLOCK.path_translated.c_str(), CFO_OPT_READ, CFO_SHARE_ALL);

	// 文件存在，但文件打开操作失败，可能没有权限等，返回403
	if (file.GetFileHandle() == INVALID_HANDLE_VALUE) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_FORBIDDEN);
		return;
	}

	// GZIP 缓存，判断缓存是否已经被生成，如果有，直接发送，如果没有，则添加记录并生成，为以后使用做好准备。
	bool hasGzipped = false;
	CStringPlus gzip_file_path;
	if (VAR_ENABLE_GZIP) {

		LPCSTR ae = nullptr;
		bool isClientSupportGzip = (::HP_HttpServer_GetHeader(pServer, dwConnID, "Accept-Encoding", &ae) && ae != nullptr && strstr(ae, "gzip") != NULL);

		if (isClientSupportGzip) {

			SYSTEMTIME st = { 0 };
			file.GetFileDate(CFO_FILE_TIME_MODIFY, &st);
			char tmp[128] = { 0 };
			GetHttpGMTTime(tmp, &st);

			XXH32_hash_t hash_pathtranslated = XXH32(ci->PATH_BLOCK.path_translated.data(), ci->PATH_BLOCK.path_translated.size(), ci->vhost_index);
			XXH32_hash_t hash_modifytime = XXH32(tmp, strlen(tmp), ci->vhost_index);

			gzip_file_path = VAR_GZIP_CACHE_TEMP_DIR_PREFIX;
			gzip_file_path.append(CStringPlus().format("%x_%x.gz", hash_pathtranslated, hash_modifytime));

			if (::PathFileExistsA(gzip_file_path.c_str())) {

				// 已经存在缓存，则不需要再次进行GZIP压缩，关闭原始文件，稍后将重新打开GZIP缓存。
				file.Close();
				hasGzipped = true;

			} else {

				if (file.GetStockFileLength() <= VAR_LIMIT_LEN_OF_FILE_TO_GZIP) {

					bool operationSuccess = true;
					CFileOperation gzip_file(gzip_file_path.c_str(), CFO_OPT_OVERRIDE, CFO_SHARE_ALL);
					if (gzip_file.GetFileHandle() == INVALID_HANDLE_VALUE) operationSuccess = false;

					BYTE *original_data = (BYTE *)Alloc_Request_Mem(pServer, dwConnID, (size_t)file.GetStockFileLength());
					if (!file.Read((DWORD)file.GetStockFileLength(), original_data)) operationSuccess = false;

					// HP-Socket: Gzip 压缩（返回值：0 -> 成功，-3 -> 输入数据不正确，-5 -> 输出缓冲区不足）
					DWORD gzip_data_length = ::SYS_GuessCompressBound((DWORD)file.GetStockFileLength(), TRUE);
					BYTE *gzip_data = (BYTE *)Alloc_Request_Mem(pServer, dwConnID, gzip_data_length);

					if (!::SYS_GZipCompress(original_data, (DWORD)file.GetStockFileLength(), gzip_data, &gzip_data_length) == 0) operationSuccess = false;

					if (!gzip_file.Write(gzip_data_length, gzip_data)) operationSuccess = false;

					gzip_file.Close();
					file.Close();

					hasGzipped = operationSuccess;

				} else {

					// 否则原始文件长度太长，不予压缩，关闭文件
					file.Close();
				
				}
				
			}

			// 因为上述过程结束后已经关闭file，现在重新用file打开已经缓存后的gzip文件(缓存成功)或回滚原始文件(缓存失败)
			file.Open(hasGzipped ? gzip_file_path.c_str() : ci->PATH_BLOCK.path_translated.c_str(), CFO_OPT_READ, CFO_SHARE_ALL);
			if (file.GetFileHandle() == INVALID_HANDLE_VALUE) {

				// 然后不幸地又发生了奇怪的错误
				VAR_SYS_LOG("Error opening file.");
				Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
				return;
			}

		} // 否则客户端不支持GZIP，不予压缩
	}

	std::vector<HP_THeader> headers;

	CStringPlus content_type(GetMIMETypeByExtension(GetExtensionByPath(ci->PATH_BLOCK.path_translated)).c_str());
	char *str_content_type = (char *)Alloc_Request_Mem(pServer, dwConnID, 1024);
	strcpy(str_content_type, content_type.c_str());
	HP_THeader header = { "Content-Type", str_content_type };
	headers.push_back(header);

	// 获取浏览器端的资源缓存状况以及多线程下载请求头
	CStringPlus If_Modified_Since;
	LPCSTR ims = nullptr;
	if (::HP_HttpServer_GetHeader(pServer, dwConnID, "If-Modified-Since", &ims) && ims != nullptr && strlen(ims) != 0) {
	
		If_Modified_Since = ims;
	}

	CStringPlus If_None_Match;
	LPCSTR inm = nullptr;
	if (::HP_HttpServer_GetHeader(pServer, dwConnID, "If-None-Match", &inm) && inm != nullptr && strlen(inm) != 0) {

		If_None_Match = inm;
	}

	CStringPlus Range;
	LPCSTR rg = nullptr;
	bool enable_partial_content = false;
	if (::HP_HttpServer_GetHeader(pServer, dwConnID, "Range", &rg) && rg != nullptr && strlen(rg) != 0) {

		Range = rg;
		enable_partial_content = true;
	}

	// 如果是提供压缩后的缓存
	if (hasGzipped) {
	
		// 再次打开原始文件以获得其最后修改日期
		CFileOperation original_file(ci->PATH_BLOCK.path_translated.c_str(), CFO_OPT_READ, CFO_SHARE_ALL);
		if (original_file.GetFileHandle() == INVALID_HANDLE_VALUE) {
		
			// 如果不幸发生错误
			VAR_SYS_LOG("Error opening file.");
			Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
			file.Close();
			return;
		}

		SYSTEMTIME st = { 0 };
		original_file.GetFileDate(CFO_FILE_TIME_MODIFY, &st);
		char tmp[128] = { 0 };
		GetHttpGMTTime(tmp, &st);

		original_file.Close();

		// 生成上次修改头和标签头，以便与浏览器确认缓存
		HP_THeader last_modified = { "Last-Modified", tmp };
		headers.push_back(last_modified);

		XXH32_hash_t hash_pathtranslated = XXH32(ci->PATH_BLOCK.path_translated.data(), ci->PATH_BLOCK.path_translated.size(), ci->vhost_index);
		XXH32_hash_t hash_modifytime = XXH32(tmp, strlen(tmp), ci->vhost_index);
		CStringPlus ETag;
		ETag.format("\"gz-%x-%x\"", hash_pathtranslated, hash_modifytime);

		char *str_ETag = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
		strcpy(str_ETag, ETag.c_str());
		HP_THeader etag_header = { "ETag", str_ETag };
		headers.push_back(etag_header);

		HP_THeader content_encoding_header = { "Content-Encoding", "gzip" };
		headers.push_back(content_encoding_header);

		HP_THeader vary_header = { "Vary", "Accept-Encoding" };
		headers.push_back(vary_header);

		if (!enable_partial_content && If_Modified_Since == tmp && If_None_Match == ETag) {

			file.Close();
			Send_HttpErrorPage(pServer, dwConnID, HSC_NOT_MODIFIED);
			return;

		}

		if (enable_partial_content && ((!If_Modified_Since.empty() && If_Modified_Since != tmp) || (!If_None_Match.empty() && If_None_Match != ETag))) {

			// 如果是断点续传请求，然而资源标签和这里不一致，则通告客户端请求不满足
			file.Close();
			Send_HttpErrorPage(pServer, dwConnID, HSC_REQUESTED_RANGE_NOT_SATISFIABLE);
			return;

		}

	} else {

		SYSTEMTIME st = { 0 };
		file.GetFileDate(CFO_FILE_TIME_MODIFY, &st);
		char tmp[128] = { 0 };
		GetHttpGMTTime(tmp, &st);

		// 生成上次修改头和标签头，以便与浏览器确认缓存
		HP_THeader last_modified = { "Last-Modified", tmp };
		headers.push_back(last_modified);

		XXH32_hash_t hash_pathtranslated = XXH32(ci->PATH_BLOCK.path_translated.data(), ci->PATH_BLOCK.path_translated.size(), ci->vhost_index);
		XXH32_hash_t hash_modifytime = XXH32(tmp, strlen(tmp), ci->vhost_index);
		CStringPlus ETag;
		ETag.format("\"%x-%x\"", hash_pathtranslated, hash_modifytime);
		char *str_ETag = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
		strcpy(str_ETag, ETag.c_str());
		HP_THeader etag_header = { "ETag", str_ETag };
		headers.push_back(etag_header);

		if (!enable_partial_content && If_Modified_Since == tmp && If_None_Match == ETag) {

			file.Close();
			Send_HttpErrorPage(pServer, dwConnID, HSC_NOT_MODIFIED);
			return;

		}
		
		if (enable_partial_content && ((!If_Modified_Since.empty() && If_Modified_Since != tmp) || (!If_None_Match.empty() && If_None_Match != ETag))) {
		
			// 如果是断点续传请求，然而资源标签和这里不一致，则通告客户端请求不满足
			file.Close();
			Send_HttpErrorPage(pServer, dwConnID, HSC_REQUESTED_RANGE_NOT_SATISFIABLE);
			return;

		}
	
	}

	// 通告浏览器，本服务器支持断点续传
	HP_THeader accept_ranges_header = { "Accept-Ranges", "bytes" };
	headers.push_back(accept_ranges_header);
	
	// 判断是否是HEAD请求
	bool isHEAD_Method = (strcmp(::HP_HttpServer_GetMethod(pServer, dwConnID), HTTP_METHOD_HEAD) == 0);

	// 对于小于 4M 长度定义的小文件，直接用 HP-Socket 自带的文件传输传送(因为库限制4M以下)。
	if (!enable_partial_content && file.GetStockFileLength() < 4 * 1024 * 1024) {

		CStringPlus content_length(file.GetStockFileLength());
		char *str_content_length = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
		strcpy(str_content_length, content_length.c_str());
		HP_THeader content_length_header = { "Content-Length", str_content_length };
		headers.push_back(content_length_header);

		// 响应请求
		Send_Response(pServer, dwConnID, HSC_OK, headers, "", false, (file.GetStockFileLength() != 0 /* 如果文件为空，则不用库自带发送，会出错 */ && !isHEAD_Method /* HEAD 请求不用发数据，所以这里取反 */), isHEAD_Method ?  "" : (hasGzipped ? gzip_file_path : ci->PATH_BLOCK.path_translated));
		
		file.Close();

		Done_Request(pServer, dwConnID);
		return;
	}

	// 继续处理非小文件的请求

	bool use_partial_content          = false;
	unsigned long long start_pos      = 0;
	unsigned long long end_pos        = 0;
	unsigned long long content_length = 0;

	// 启用断点续传，则开始解析，只有解析通过，才继续使用断点续传
	if (enable_partial_content) {

		/*
			byte-range-spec 里的 last-byte-pos 可以省略，代表从 first-byte-pos 一直请求到结束位置。其实 first-byte-pos 也是可以省略的，只不过就不叫 byte-range-spec 了，而是叫 suffix-byte-range-spec，规范如下：
			suffix-byte-range-spec = “-” suffix-length
			suffix-length = 1*DIGIT
			比如“Range: bytes=-200”，它不是表示请求文件开始位置的 201 个字节，而是表示要请求文件结尾处的 200 个字节。

			如果 byte-range-spec 的 last-byte-pos 小于 first-byte-pos，那么这个 Range 请求就是无效请求，server 需要忽略这个 Range 请求，然后回应一个 200 OK，把整个文件发给 client。
			如果 byte-range-spec 里的 first-byte-pos 大于文件长度，或者 suffix-byte-range-spec 里的 suffix-length 等于 0，那么这个 Range 请求被认为是不能满足的，server 需要回应一个 416。
		*/

		CStringPlus start, end;
		if (ParseRangeByte(Range, start, end)) {

			if (start == "-") {
			
				if (end != "-") {
				
					char *ptr = NULL;
					end_pos = (unsigned long long)strtoll(end.c_str(), &ptr, 10);
					if (end_pos != 0 && end_pos <= (unsigned long long)file.GetStockFileLength()) {

						start_pos = (unsigned long long)file.GetStockFileLength() - end_pos;
						end_pos = (unsigned long long)file.GetStockFileLength() - 1;
						content_length = end_pos - start_pos + 1;
						use_partial_content = true;

					} else {
					
						file.Close();
						Send_HttpErrorPage(pServer, dwConnID, HSC_REQUESTED_RANGE_NOT_SATISFIABLE);
						return;
					}
				}

			} else {
			
				char *ptr = NULL;
				start_pos = (unsigned long long)strtoll(start.c_str(), &ptr, 10);

				if (start_pos >= (unsigned long long)file.GetStockFileLength()) {
					
					file.Close();
					Send_HttpErrorPage(pServer, dwConnID, HSC_REQUESTED_RANGE_NOT_SATISFIABLE);
					return;
				}

				if (end == "-") {
					
					end_pos = (unsigned long long)file.GetStockFileLength() - 1;
					content_length = end_pos - start_pos + 1;
					use_partial_content = true;

				} else {
					
					char *tmp_ptr = NULL;
					end_pos = (unsigned long long)strtoll(end.c_str(), &tmp_ptr, 10);

					if (end_pos >= (unsigned long long)file.GetStockFileLength()) {

						file.Close();
						Send_HttpErrorPage(pServer, dwConnID, HSC_REQUESTED_RANGE_NOT_SATISFIABLE);
						return;
					}

					if (end_pos != 0 && end_pos >= start_pos) {
					
						content_length = end_pos - start_pos + 1;
						use_partial_content = true;
					}
				}
			}
		}
	}

	// 如果不用断点续传，则视为普通请求，填充始末位置与数据长度
	if (!use_partial_content) {
		
		content_length = (unsigned long long)file.GetStockFileLength();
		start_pos = 0;
		end_pos = (unsigned long long)file.GetStockFileLength() - 1;

	} else {
	
		// 如果用断点续传，则写入头部信息

		CStringPlus cr_header;
		cr_header.format("bytes %llu-%llu/%llu", start_pos, end_pos, (unsigned long long)file.GetStockFileLength());
		char *str_content_range = (char *)Alloc_Request_Mem(pServer, dwConnID, 1024);
		strcpy(str_content_range, cr_header.c_str());
		HP_THeader content_range_header = { "Content-Range", str_content_range };
		headers.push_back(content_range_header);
	
	}


	CStringPlus cl_header(content_length);
	char *str_content_length = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
	strcpy(str_content_length, cl_header.c_str());
	HP_THeader content_length_header = { "Content-Length", str_content_length };
	headers.push_back(content_length_header);
	
	Send_Response(pServer, dwConnID, use_partial_content ? HSC_PARTIAL_CONTENT : HSC_OK, headers);

	if (isHEAD_Method) {
	
		file.Close();
		Done_Request(pServer, dwConnID);
		return;
	}

	// 填写传输信息快
	ci->SEND_BLOCK.hFile        = file.GetFileHandle();
	ci->SEND_BLOCK.BeginPointer = start_pos;
	ci->SEND_BLOCK.EndPointer   = end_pos;
	ci->SEND_BLOCK.use_send_blk = true;

	// 最后，投递传输任务，此过程的任务就到此结束了
	PostFileTransfering(pServer, dwConnID);
}

void directory_listing(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	const char * DIRECTORY_LISTING_HEAD = "<!DOCTYPE HTML><html><head><meta charset=\"utf-8\"><meta name=\"generator\"content=\"%s\"/><title>Index of %s</title><script>function cka(o){o.getElementsByTagName(\"a\")[0].click()}</script></head><body><style>tr{background-color:#ffffff;line-height:1.3}body{background-color:#e5e5e5;}a{text-decoration:none}tbody tr:hover a{color:white}tbody tr:hover td{background-color:#444444;color:white} tr a:hover{color:orange}.thd td{background-color:#aaaaaa;height:24px;border-bottom:1px solid #777777;line-height:1.4}tr{background:#fff}tr:nth-child(2n){background:#fafafa}#tright{text-align:right;padding-right:5pt}#tcenter{text-align:center}.bkrtn{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAGWSURBVDjLpZG9S5thFMXPG2NUxE8oxFAtKFnyLwiCHaxOnToodmoVh0qJFBVcRXQLuOhWLLQoWtsIfkCzmNLioA52EYz64mBKFAJKCXnuuU8HWykaS3i92z3Dj/O717HW4j7juxm8+TZQMvS1f9QzgNRZUnuKBTj/KkSTfbGG8tBrVYWbdUEqKMzQcFuEGzRc+tD76aQgILrZNx6sCI01V7XAcQBahaoiJzlkf2WRzv5E6jT1mUamlvvXv99SIDVAEgqFKEESYgU+x4fyQBnCwTAiDyNPjZGRzlh7Y0GFgbXn08HKhlck4Z65ECFC1SE0PXiEUn8AqsRe6gcO3IPol+Fk7NYRZ7reDbrn7tvjjLs392zRed+97Bymj2KJncTJZe4SF/kL1FbXwhh5cucXxMhLMTL/d//4YjVq8rK0f7wPv68UdTX1kLx0FlT43zyebLUdbR2gKuKrcWxN7DoA4C/23yYvMBSoVYjhdV40QIxAlLCWECNeAAT1TwPx2ICWoCroTYFXCqqglwYUIr6wAlKh1Ov8N9v2/gMRLRuAAAAAAElFTkSuQmCC\")no-repeat 10px center}.bkdir{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAGrSURBVDjLxZO7ihRBFIa/6u0ZW7GHBUV0UQQTZzd3QdhMQxOfwMRXEANBMNQX0MzAzFAwEzHwARbNFDdwEd31Mj3X7a6uOr9BtzNjYjKBJ6nicP7v3KqcJFaxhBVtZUAK8OHlld2st7Xl3DJPVONP+zEUV4HqL5UDYHr5xvuQAjgl/Qs7TzvOOVAjxjlC+ePSwe6DfbVegLVuT4r14eTr6zvA8xSAoBLzx6pvj4l+DZIezuVkG9fY2H7YRQIMZIBwycmzH1/s3F8AapfIPNF3kQk7+kw9PWBy+IZOdg5Ug3mkAATy/t0usovzGeCUWTjCz0B+Sj0ekfdvkZ3abBv+U4GaCtJ1iEm6ANQJ6fEzrG/engcKw/wXQvEKxSEKQxRGKE7Izt+DSiwBJMUSm71rguMYhQKrBygOIRStf4TiFFRBvbRGKiQLWP29yRSHKBTtfdBmHs0BUpgvtgF4yRFR+NUKi0XZcYjCeCG2smkzLAHkbRBmP0/Uk26O5YnUActBp1GsAI+S5nRJJJal5K1aAMrq0d6Tm9uI6zjyf75dAe6tx/SsWeD//o2/Ab6IH3/h25pOAAAAAElFTkSuQmCC\")no-repeat 10px center}.bkfile{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAADoSURBVBgZBcExblNBGAbA2ceegTRBuIKOgiihSZNTcC5LUHAihNJR0kGKCDcYJY6D3/77MdOinTvzAgCw8ysThIvn/VojIyMjIyPP+bS1sUQIV2s95pBDDvmbP/mdkft83tpYguZq5Jh/OeaYh+yzy8hTHvNlaxNNczm+la9OTlar1UdA/+C2A4trRCnD3jS8BB1obq2Gk6GU6QbQAS4BUaYSQAf4bhhKKTFdAzrAOwAxEUAH+KEM01SY3gM6wBsEAQB0gJ+maZoC3gI6iPYaAIBJsiRmHU0AALOeFC3aK2cWAACUXe7+AwO0lc9eTHYTAAAAAElFTkSuQmCC\")no-repeat 10px center}.bkmovie{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAIfSURBVDjLpZNPaBNBGMXfbrubzBqbg4kL0lJLgiVKE/AP6Kl6UUFQNAeDIAjVS08aELx59GQPAREV/4BeiqcqROpRD4pUNCJSS21OgloISWMEZ/aPb6ARdNeTCz92mO+9N9/w7RphGOJ/nsH+olqtvg+CYJR8q9VquThxuVz+oJTKeZ63Uq/XC38E0Jj3ff8+OVupVGLbolkzQw5HOqAxQU4wXWWnZrykmYD0QsgAOJe9hpEUcPr8i0GaJ8n2vs/sL2h8R66TpVfWTdETHWE6GRGKjGiiKNLii5BSLpN7pBHpgMYhMkm8tPUWz3sL2D1wFaY/jvnWcTTaE5DyjMfTT5J0XIAiTRYn3ASwZ1MKbTmN7z+KaHUOYqmb1fcPiNa4kQBuyvWAHYfcHGzDgYcx9NKrwJYHCAyF21JiPWBnXMAQOea6bmn+4ueYGZi8gtymNVobF7BG5prNpjd+eW6X4BSUD0gOdCpzA8MpA/v2v15kl4+pK0emwHSbjJGBlz+vYM1fQeDrYOBTdzOGvDf6EFNr+LYjHbBgsaCLxr+moNQjU2vYhRXpgIUOmSWWnsJRfjlOZhrexgtYDZ/gWbetNRbNs6QT10GJglNk64HMaGgbAkoMo5fiFNy7CKDQUGqE5r38YktxAfSqW7Zt33l66WtkAkACjuNsaLVaDxlw5HdJ/86aYrG4WCgUZD6fX+jv/U0ymfxoWVZomuZyf+8XqfGP49CCrBUAAAAASUVORK5CYII=\")no-repeat 10px center}.bkhtml{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHUSURBVDjLxZM7a1RhEIafc3J2z6qJkIuCKChItBNSBQ0iIlZiK4gWItj6HwRbC7FRf4CVnSCIkH9gJVjYiCDximCyZ7/zfXOz2A0I2qVwmmFg3rm870wVEezFavZoey7Q3Hv+/Z87qDsiTlZFBJIGKStZlFSCTpyUlAZgfXXfH9BAPTCberVANBB3RAJRR8wp6jzd/DotALA9UcyZgZxis2QNijpZjSJBVqeIszTfkMY65cAjuHxmgSzGlbUFrp1d5ObGErcuLLNxep5hU3H93AqjYcXti4cZZ2OSDU9CnVURddqmIovTDmoev/5GVcGDF585tjzg1JGWo0tDDgxrThxq6XojieOd0nRZ6dVpBxU3zi/T1BVdViKCcTbcYX11ngB6cca9MSlGlprojHqcglycVJyHL79Q1Jn0TgBdb1gEbz9OeL81IYsRAakYvQSeC/WvVOiLE8GsM4xnvsuGe/Do1RY/dpRenIP753hyZxURJ3JQXbr/Lq6uLfLpZ6aIk9XJssv8VK5dNcQcmcl7fKVl89kHmu0dJRVjYTRHGVSMpELaQLVCtEY8EAvMHHUwn067+0LVybtvok9KSODZiaKEOJENihPm01gD3P+62Oq/f+Nv2d9y2D8jLUEAAAAASUVORK5CYII=\")no-repeat 10px center}.bkmusic{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAETSURBVBgZfcExS0JRGIDh996OFIQEgSRhTS1Bg0trw937B9UPCAT3hnJ1kYbGhrv0BxoaXSsMhBCsyUEcoiTKUM/3HU8Fce4Q+DyRZz5DcOkdiqIIiiAo7xiCMXs4HI4ZisPhOMcQOJQbOoxxKHm22UUxBBbHM1cRfw58GUtMIAieTIwgxAQWRclMEZSYwCIIGYsixASCYsl4pgiGwDFF+HWUaDopbfCGHRp+nCWSTktFXvFDOKyuNNYp4LhFriPPaXW5UWAV5Y6HNH+/dbHJIjN6NHlJzMnxWqNIDqFHh8/U7hiEJbp0+ar0m2a4MGFEjie6jCrtJs1y57FuI21R6w8g8uwnH/VJJK1ZrT3gn8gz3zcVUYEwGmDcvQAAAABJRU5ErkJggg==\")no-repeat 10px center}.bkphp{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAGsSURBVDjLjZNLSwJRFICtFv2AgggS2vQLDFvVpn0Pi4iItm1KItvWJqW1pYsRemyyNILARbZpm0WtrJ0kbmbUlHmr4+t0z60Z7oSSAx935txzvrlPBwA4EPKMEVwE9z+ME/qtOkbgqtVqUqPRaDWbTegE6YdQKBRkJazAjcWapoGu6xayLIMoilAoFKhEEAQIh8OWxCzuQwEmVKtVMAyDtoiqqiBJEhSLRSqoVCqAP+E47keCAvfU5sDQ8MRs/OYNtr1x2PXdwuJShLLljcFlNAW5HA9khLYp0TUhSYMLHm7PLEDS7zyw3ybRqyfg+TyBtwl2sDP1nKWFiUSazFex3tk45sXjL1Aul20CGTs+syVY37igBbwg03eMsfH9gwSsrZ+Doig2QZsdNiZmMkVrKmwc18azHKELyQrOMEHTDJp8HXu1hostG8dY8PiRngdWMEq467ZwbDxwlIR8XrQLcBvn5k9Gpmd8fn/gHlZWT20C/D4k8eTDB3yVFKjX6xSbgD1If8G970Q3QbvbPehAyxL8SibJEdaxo5dikqvS28sInCjp4Tqb4NV3fgPirZ4pD4KS4wAAAABJRU5ErkJggg==\")no-repeat 10px center}.bkscript{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAJ+SURBVBgZBcExbFRlAADg7//fu7teC3elQEoMgeDkYDQ6oMQQTYyGxMHZuDA6Ypw0cWI20cHJUdl0cJLIiomR6OACGhUCpqGWtlzbu/b97/3v9/tCKQVc/e7RRXz+7OrSpUXbW7S9tu8ddv0M+3iCjF1s42v8WAP0XffKi2eOXfro9dMAYJ766SL1092jfDa17DfZgycHfvh7/hau1QB9161PhgE8epoNQlAHqprRIDo3iqoYDSpeOjv2zHRl7atfNj6LALltJys1Xc9+CmYtTxtmR8yO2D7kv4MMPr7x0KULK54/NThdA+S2XTs+jOYN86MsxqBGVRErKkEV6BHynp//2fXbw9lGDZBTWp+OK7PDzqIpYiyqSMxBFakUVYVS2dxrfHHrrz1crQG6lM6vTwZmR0UHhSoHsSBTKeoS9YU8yLrUXfj+w9d2IkBOzfkz05F5KkKkCkFERACEQil0TSOnJkMNV67fHNdVHI4GUcpZVFAUZAEExEibs4P5osMeROiadHoUiIEeCgFREAoRBOMB2weNrkmbNz+9UiBCTs1yrVdHqhgIkRL0EOj7QGG5jrZ2D+XUbADEy9dunOpSun7xuXMe7xUPNrOd/WyeyKUIoRgOGS8xWWZ7b6FLaROgzim9iXd+vXvf7mHtoCnaXDRtkLpel3t9KdamUx+8fcbj7YWc0hZAndv25XffeGH8yfuvAoBcaHOROhS+vLlhecD+wUJu222AOrft/cdPZr65ddfqsbHVyZLVlZHpysjx5aHRMBrV0XuX141qtnb25bb9F6Duu+7b23funb195955nMRJnMAJTJeGg8HS0sBkZWx1suz3Px79iZ8A/gd7ijssEaZF9QAAAABJRU5ErkJggg==\")no-repeat 10px center}.bkimage{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAMOSURBVDjLVZNNaBxlAIafb+ab2Z3N7Oxv/nYTEyv2LzQJpKBgrQqNUKmY4kUIXqUHT70p9iB48CKIiN5E0It6KFiwiv9FpAVpKUggNc3mZ7vpJpv9n93ZnZ35PNRI+8B7e9/n9gqlFAeIVUfPeN3zh0R0eVpYM1OanhvTCEY0f3tU79+ctnpfHM73fuQhxIHAWHnmkOGXPjgZyS09l5hnNv4YOdMhoQmigzqGt4nhfeub1fpnVsl/e+hMv/q/QKy+Me0EO5dfso/OvzB8grgV4HGXJC7jwAQ2oxxDuC36xZ+Rhe+v6iutZf2iqklReNe0tPSHZ2Nz84ujR7ht3iJKjcexiOIQI8SiixxcR7QtRORFlK7O9t0rlyy4KBEj5+YisVeez85wy9zGIUeGDDYhDhYOITYuoh2BvTJ68y7B0GnCym8XGq+KL2U0MrE8Z2SRVhqdPmlCsvgk8RlCkgAivRbUNKj1YPMeeu4wcnjRql7/+jVpyvxsPjbK3whi5LEAB0WWgBRgqwAaFah04X4V7puwdwddz+FXjJMSbXI8aSTYCgU2oKMwEdgCEoDhug/G5SjsmFDUoV+DXJ7BnpiUVCNBaJqEXfDVfwG6CjoKnF4crZGCVvNBug0IPXzPZOCnAunfk8W6ro7H2gK3A02gGoDeA1MDGx2nkYG6C24bvDaMSzq7ZfxBsiC7O+aNDaWOn0oLfl0HMwDlQRCAHYUkEGvFkLsp2G9Bo0n41AiNG6sMBvY1yZr6/JsV//XZZ3WZaEp2t6DvgWFA1QRHQbwjSDeTUGvCiSPU1ovU/typQPIrTV0yrrl3vE+/+8XlaCIgq8H+BtSLUN2C2ibsl8ArR+HYGE0rwvbvRTr96HsL6od1CUDDf+enK92JwT+982cWEswvRmiug6qAr0E4AV4uoFXosnV1g8bN5kcp7E8eOZOYKtmUqm/ZiDdfPhV3Zp6IM5k0SIUBstwmXKvCX5UdM6y9n2b34wV1IXxEcEBU3J4dprU0zODpjFBTIyoIxgjXxlB/PIl1eUmdLjzc/xceOVXddrB6BQAAAABJRU5ErkJggg==\")no-repeat 10px center}.bkbinary{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAALnSURBVDjLfZNLaFx1HIW/e2fuzJ00w0ymkpQpiUKfMT7SblzU4kayELEptRChUEFEqKALUaRUV2YhlCLYjYq4FBeuiqZgC6FIQzBpEGpDkzHNs5PMTJtmHnfu6//7uSh2IYNnffg23zmWqtIpd395YwiRL1Q0qyIfD56cmOvUs/4LWJg40auiH6jI+7v3ncybdo2Hy9ebKvqNGrn03Nj1+x0Bi1dHHVV9W0U+ye4d2d83+Ca2GJrlGZx0gkppkkfrsysqclFFvh8++3v7CWDh6ugIohfSPcPH+w6fwu05ABoSby9yb3Kc/mePYXc9TdCqslWapVGdn1Zjxo++O33Fujtx4gdEzj61f8xyC8/jN2rsVOcxYZOoVSZtBewZOAT+NonuAWw3S728wFZpFm975cekGjlz8NXLVtSo0SxPImGdtFfFq5epr21wdOxrnMwuaC2jrRJWfYHdxRfIFeDWr0unkyrSUqxcyk2TLQzQrt6hqydPvidDBg/8VTAp8DegvYa3OU1z+SbuM6dQI62kioAAVgondwAnncWvzCDNCk4CLO9vsJVw8xqN+iPiTB5SaTSKURGSaoTHHgxoAMlduL1HiFMZXP8BsvkbO1GD2O3GpLOIF0KsSBijxmCrMY+FqgGJQDzQgGT3XrJ7DuI5EKZd4iDG+CHG84m8AIki1Ai2imRsx4FEBtQHCUB8MG1wi8QKGhjEC4mbAVHTx8kNYSuoiGurkRtLN76ivb0K6SIkusCEoBEgaCQYPyT2QhKpAXKHTiMmQ2lmChWZTrw32v9TsLOyVlu8Nhi2G4Vs32HsTC9IA2KPRuU2Erp097+O5RRYvz3H1r3JldivfY7IR0+mfOu7l3pV5EM1cq744mi+OPwaRD71tSk0Vsp3/uLB6s2minyrIpeOf7a00fFMf1w+MqRGzqvIW/teecdqV5a5P/8ncXv9ZxUdf/lCae5/3/hvpi4OjajIp4ikVOTLY+cXr3Tq/QPcssKNXib9yAAAAABJRU5ErkJggg==\")no-repeat 10px center}.bkoffice{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAKdSURBVDjLjZP9S1NRGMdH9GckFFT6Q5ZhWVGZEGhFSFmaoiCB72VJxYwowSAhKUqY0gZplOR+MQqCwtQSC19wXcXFfjF1zXa3u817d+927t2Lfjv3xF6Eoi58fjrn+dzn+XIeAwCDDv22UHIpef9gK2VTsi5NkKtpmhSLxdbi8Tj+BD2HyWTqSpcYbGdLnbMFeTFX+aF1ofkIxKYs+K8fhRLwIRwOIxgMQhRFeL1eJuF5Ht3d3UmJYTJzO5bqjk+bKvONisMGiRuH0n4YwR8LUFUViqJAkiQIgsAEhBCEQiGYzebfkm/HsrBy/1ydPp9+IRKJgAych+xeRscbB1oH7ajumUSblaMjRDeMxDLxlGdj4VZ+WUIQi6iIDJZC8brQ/HwO3KIfjmUvLlmmsLjsZp243e6UQLqYhaU7Jw8mBDqipREabbP91TyUsMrCu/bsKwT/KssjEAikBL7GvevEeCBOHhbFyJNiyL0tUEY6ockSjNZ5GF/acLWfQ1PvHERJZpnIspwSKN8tm93jhZmXq3eUaSEX4lGCqOpjF0JklYYmg6gifNISwqrCQtRJCjThcY06tQ2e8WLinSiBMFUFYaISGi3sG6uBebQKlk9V6BktxQ3rCRayPlJK4DTXyJ+zYe6ob0tksMo1IEo7eTJ6AW+5e6CPCxP292i2FLLiDQKy2Fcf+JiNm42nGxKCsL0YROFhGi6BdeY2gqEARmYHUPuggIqjjKRgZaar2vthN770V9idti74HI9AJneByDz6xzrR8qIIrU/P4IrpFGrvFrFudJIC7nX7Ts/QfoF/lwNhKAf+4T0QpytoBgr7k8fvBu/7CRfvREDypz+kNSZIW6Z9NKCwfvC3ZUovpncVtr1pggxd8h/rnEBf/Yxfiwq9OE8N8XAAAAAASUVORK5CYII=\")no-repeat 10px center}.bkjava{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAILSURBVDjLrVM7ixNhFB2LFKJV+v0L24nabIogtmItin9ALBS3tXNt3CWgVlpMsAgrWRexkCSTd0KimYS8Q94vsnlrikAec7z34hSibON+cJjXPeee79xvFADK/0C5UIFyubxLUEulklooFNR8Pn+Sy+VOstmsmslk1HQ6raZSqd2/BCqVyh4RW/V6HePxGJPJRDCdTuU6Go0EZ2dnIFEkk8lWIpHYEwEi24lszGYzjHptfPvsgvbuEJ9ePMPH548Epwf70N4f4fuXY6rpYDgcIh6PG7FYzM62dSav12spfHXn2rk4fbmPxWIhIpFIRFfIzk+v1wvDMLAhka9vD+B88gCv79lxdPeG4M39W/jw9KF8q+oJzOdz2VIoFPqhOJ3O7mAwwHK5xGazketqtRKws3+Bto1arYZgMFhTHA6HC78XW6P0wYJmcAy2y+9arRYoPCHTpOD3+w8Vm8122xTgQhobqtUqms0mGo0GeDLckdOnESIcDqPdbnN3aJp2VbFarTfN7kxmUqfTkSLuyM8syB3pLMj7fr8Pn883kTFaLJbr1EHfbrdilwm9Xg/dblfABNMF3/NWisUiKPjHIkDrMou43e4CF+m6LkUMU4idcFc+WJwRkbU/TiKtS4QrgUDgmGZrcEcelXkKORsWJ9sGkV3n/kzRaHSHgtrQjEGCHJSAyBuPx7Nz4X/jL/ZqKRurPTy6AAAAAElFTkSuQmCC\")no-repeat 10px center}.bkcss{padding-left:35px;background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAGeSURBVDjLxVNNS0JRED3qA0MRoqCFouBGN9Yia9HGRa3b9x/6M62jf9CuTS0EV0arXASG0gcUYWIgmvi6792P7sz1WUI7Fw0Mc70z59wz88aYMQbLWBxL2tIEXrN5+mcPWkvrBsZQVNYDSKmglLTZ0J4lwjCER8XZ7OYcSDMxRs/cEdCZSKKoNeUU7u/rjoBMiE8GuKQrcCA1A0XuFK2sZKwC3xE4Zo1UahX5/Dam0yH6/Q4KhV17H+Lu7gKVyiESCQ/dbgPD4QvfSykQlzKcMxP4+fnGJr4seAdPT01MJh8oFve4uNOp20fWQBilQqvAEtBQqE+6IBuPe3h8bML3hyiX95FOr6HXayOT2UCpdIDR6I1r6VF6KK61z5N1ROAkvdBuX+H6+oznksttodE4wevrLbdC8h1GwCMZJF+pgIdSrR6xtCCYWLnrnBuP31GrHfN5MHhgcDRUj3pzbAFarfOFSUf++4tEA3dRwhNCsKRkMv2r+Oe7R7+jvbArNotu/6wC3/Z7yX3TdhkjbDS8eUTi5EoGuLhosX//N34Dm6aVPfzbYjIAAAAASUVORK5CYII=\")no-repeat 10px center}</style><h3>Index of %s</h3><table style=\"width:100%%;border-collapse: collapse;border-bottom:1px solid #888888;border-right:1px solid #cccccc\"id=\"mtb\"><thead Class=\"thd\"><tr><td>&nbsp;&nbsp;\xe5\x90\x8d\xe7\xa7\xb0/Name</td><td>\xe5\xa4\xa7\xe5\xb0\x8f/Size</td><td>\xe6\x97\xa5\xe6\x9c\x9f/Date (UTC)</td></tr></thead><tfoot Class=\"thd\"><tr><td colspan=\"3\"><div style=\"margin-left:10px;float:left\">\xe6\x80\xbb\xe5\x85\xb1: %d \xe4\xb8\xaa\xe5\xaf\xb9\xe8\xb1\xa1</div><div style=\"float:right;margin-right:10px\">%s</div></td></tr></tfoot><tbody><tr ondblclick=cka(this)><td class=\"bkrtn\"><a href=\"../\">..\xe8\xbf\x94\xe5\x9b\x9e\xe4\xb8\x8a\xe4\xb8\x80\xe7\xba\xa7</a><td></td><td></td></tr>";
	const char * DIRECTORY_LISTING_ITEM = "<tr ondblclick=cka(this)><td class=\"%s\"><a href=\"%s\">%s</a><td>%s</td><td>%s</td></tr>";
	const char * DIRECTORY_LISTING_FOOT = "</tbody></table></body></html>";

	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return;

	CStringPlus dir(ci->PATH_BLOCK.path_translated);

	CStringPlus		 items_file;
	CStringPlus		 items_dir;
	DWORD			 file_counters = 0;

	WIN32_FIND_DATAA wfd = { 0 };
	HANDLE hFind = ::FindFirstFileA(CStringPlus(dir).append("*.*").c_str(), &wfd);

	while (hFind != INVALID_HANDLE_VALUE && ::GetLastError() != ERROR_NO_MORE_FILES) {
		
		bool isSubDirectory = ((wfd.dwFileAttributes != INVALID_FILE_ATTRIBUTES) && ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));

		if (isSubDirectory && (strcmp(wfd.cFileName, ".") == 0 || strcmp(wfd.cFileName, "..") == 0)) {
			
			::FindNextFileA(hFind, &wfd);
			continue;
		}

		CStringPlus file(ci->PATH_BLOCK.path);
		file.append(wfd.cFileName);

		SYSTEMTIME st = { 0 };
		if (!::FileTimeToSystemTime(&wfd.ftLastWriteTime, &st)) {

			Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
			return;
		}
		CStringPlus date;
		date.format("%d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		
		char *tmp = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
		::StrFormatByteSize64A((long long)(wfd.nFileSizeLow | (long long)wfd.nFileSizeHigh << 32), tmp, 128);
		
		CStringPlus item;
		item.format(DIRECTORY_LISTING_ITEM, isSubDirectory ? "bkdir" : dirlist_GetFileTypeImgClass(file), file.c_str(), wfd.cFileName, isSubDirectory ? "&lt;DIR&gt;" : tmp, date.c_str());

		Free_Request_Mem(pServer, dwConnID, tmp);

		// 将单项文件名称转为UTF-8编码
		int iDestLength = (int)item.length() * 2 + 2;
		WCHAR *uni_text = (WCHAR *)Alloc_Request_Mem(pServer, dwConnID, iDestLength);
		memset(uni_text, 0, iDestLength);
		if (!::SYS_CodePageToUnicode(CP_ACP, item.c_str(), uni_text, &iDestLength)) {

			Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
			return;
		}

		int iDestLength_u8 = iDestLength * 3 + 1;
		char *utf8_text = (char *)Alloc_Request_Mem(pServer, dwConnID, iDestLength_u8);
		memset(utf8_text, 0, iDestLength_u8);
		if (!::SYS_UnicodeToUtf8(uni_text, utf8_text, &iDestLength_u8)) {

			Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
			return;
		}

		item = utf8_text;

		Free_Request_Mem(pServer, dwConnID, utf8_text);
		Free_Request_Mem(pServer, dwConnID, uni_text);

		if (isSubDirectory) {
		
			items_dir += item;
		}
		else {

			items_file += item;
		}
		
		file_counters++;
		::FindNextFileA(hFind, &wfd);

	}

	::FindClose(hFind);

	// 将标题和索引转化为UTF-8编码
	int iDestLength = (int)ci->PATH_BLOCK.path.length() * 2 + 2;
	WCHAR *uni_text = (WCHAR *)Alloc_Request_Mem(pServer, dwConnID, iDestLength);
	memset(uni_text, 0, iDestLength);
	if (!::SYS_CodePageToUnicode(CP_ACP, ci->PATH_BLOCK.path.c_str(), uni_text, &iDestLength)) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
		return;
	}

	int iDestLength_u8 = iDestLength * 3 + 1;
	char *utf8_text = (char *)Alloc_Request_Mem(pServer, dwConnID, iDestLength_u8);
	memset(utf8_text, 0, iDestLength_u8);
	if (!::SYS_UnicodeToUtf8(uni_text, utf8_text, &iDestLength_u8)) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR);
		return;
	}


	CStringPlus	output;
	output.format(DIRECTORY_LISTING_HEAD, APP_FULL_NAME, utf8_text, utf8_text, file_counters, APP_FULL_NAME);
	output.append(items_dir).append(items_file).append(DIRECTORY_LISTING_FOOT);

	Free_Request_Mem(pServer, dwConnID, utf8_text);
	Free_Request_Mem(pServer, dwConnID, uni_text);

	std::vector<HP_THeader> header;
	HP_THeader nvpair = { "Content-Type", "text/html; charset=utf-8" };
	header.push_back(nvpair);

	// 如果启用GZIP，且客户端支持GZIP，页面不是过大，则发送GZIP档
	if (VAR_ENABLE_GZIP && output.length() < VAR_LIMIT_LEN_OF_FILE_TO_GZIP) {

		LPCSTR ae = nullptr;
		if (::HP_HttpServer_GetHeader(pServer, dwConnID, "Accept-Encoding", &ae)) {
			
			if (ae != nullptr && strstr(ae, "gzip") != NULL) {
			
				nvpair = { "Content-Encoding", "gzip" };
				header.push_back(nvpair);
				nvpair = { "Vary", "Accept-Encoding" };
				header.push_back(nvpair);

				// HP-Socket: Gzip 压缩（返回值：0 -> 成功，-3 -> 输入数据不正确，-5 -> 输出缓冲区不足）
				DWORD gzip_data_length = ::SYS_GuessCompressBound((DWORD)output.length(), TRUE);
				BYTE *gzip_data = (BYTE *)Alloc_Request_Mem(pServer, dwConnID, gzip_data_length);

				if (::SYS_GZipCompress((const BYTE *)output.data(), (DWORD)output.length(), gzip_data, &gzip_data_length) == 0) {
					
					char gzip_data_length_str[128] = { 0 };
					sprintf(gzip_data_length_str, "%lu", gzip_data_length);
					nvpair = { "Content-Length",  gzip_data_length_str };
					header.push_back(nvpair);

					if (Send_Response(pServer, dwConnID, HSC_OK, header)) {

						if (strcmp(::HP_HttpServer_GetMethod(pServer, dwConnID), HTTP_METHOD_HEAD) != 0)
							::HP_Server_Send(pServer, dwConnID, gzip_data, gzip_data_length);
					}

					Done_Request(pServer, dwConnID);
					return;
				}
				
			}
		}
	}

	// 不启用或客户端不支持GZIP的情况，直接发送
	char *content_length_str = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
	memset(content_length_str, 0, 128);
	sprintf(content_length_str, "%lu", (DWORD)output.length());
	nvpair = { "Content-Length",  content_length_str };
	header.push_back(nvpair);

	if (Send_Response(pServer, dwConnID, HSC_OK, header)) {

		if (strcmp(::HP_HttpServer_GetMethod(pServer, dwConnID), HTTP_METHOD_HEAD) != 0)
			::HP_Server_Send(pServer, dwConnID, (const BYTE *)output.data(), (int)output.length());
	}

	Done_Request(pServer, dwConnID);
}

LPCSTR dirlist_GetFileTypeImgClass(const CStringPlus & path)
{
	CStringPlus ext(GetExtensionByPath(path));

	if (ext == "css") return "bkcss";
	if (ext == "html") return "bkhtml";
	if (ext == "java") return "bkjava";
	if (ext == "php") return "bkphp";
	if (ext == "doc" || ext == "docx" || ext == "xls" || ext == "xlsx" || ext == "ppt" || ext == "pptx" || ext == "mdb" || ext == "accdb") return "bkoffice";
	if (ext == "exe" || ext == "zip" || ext == "7z" || ext == "rar" || ext == "tar" || ext == "gz" || ext == "bin" || ext == "tgz" || ext == "bz2" || ext == "iso" || ext == "xz") return "bkbinary";
	if (ext == "js" || ext == "vbs" || ext == "py" || ext == "lua" || ext == "pl") return "bkscript";
	if (ext == "mp4" || ext == "mov" || ext == "rm" || ext == "rmvb" || ext == "wmv" || ext == "avi" || ext == "3gp" || ext == "flv" || ext == "mkv") return "bkmovie";
	if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "wma" || ext == "aac" || ext == "flac") return "bkmusic";
	if (ext == "ico" || ext == "png" || ext == "bmp" || ext == "jpg" || ext == "jpeg" || ext == "tiff" || ext == "tif" || ext == "gif" || ext == "jng" || ext == "svg") return "bkimage";

	return "bkfile";
}

void PostFileTransfering(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return;

	if (ci->SEND_BLOCK.use_send_blk) {

		if (ci->SEND_BLOCK.hFile == INVALID_HANDLE_VALUE) return;

		// 如果文件映射句柄为空/不存在，则创建。
		if (!ci->SEND_BLOCK.hFileMapping) {

			ci->SEND_BLOCK.hFileMapping = ::CreateFileMappingA(ci->SEND_BLOCK.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		}

		// 如果失败/还是不行，则记录错误，强制断开连接
		if (!ci->SEND_BLOCK.hFileMapping) {

			::CloseHandle(ci->SEND_BLOCK.hFile);
			ci->SEND_BLOCK.hFile = INVALID_HANDLE_VALUE;

			VAR_SYS_LOG("Unable to create file mapping. OS Error(%d): %s", ::GetLastError(), GetLastErrorMessage().c_str());
			return;
		}

		unsigned long long FileMapStart = (ci->SEND_BLOCK.BeginPointer / VAR_MEM_ALLOCATION) * VAR_MEM_ALLOCATION;
		unsigned long long RemainLength = ci->SEND_BLOCK.EndPointer - ci->SEND_BLOCK.BeginPointer + 1;
		unsigned long long BufferLength = RemainLength < VAR_BUFFER ? RemainLength : VAR_BUFFER;
		unsigned long long MapViewSize = (ci->SEND_BLOCK.BeginPointer % VAR_MEM_ALLOCATION) + BufferLength;
		unsigned long long ViewDelta = ci->SEND_BLOCK.BeginPointer - FileMapStart;

		ci->SEND_BLOCK.pMapAddr = MapViewOfFile(ci->SEND_BLOCK.hFileMapping, FILE_MAP_READ, FileMapStart >> 32, FileMapStart & 0xffffffff, (size_t)MapViewSize);
		if (!ci->SEND_BLOCK.pMapAddr) {

			::CloseHandle(ci->SEND_BLOCK.hFileMapping);
			ci->SEND_BLOCK.hFileMapping = NULL;
			::CloseHandle(ci->SEND_BLOCK.hFile);
			ci->SEND_BLOCK.hFile = INVALID_HANDLE_VALUE;

			VAR_SYS_LOG("Unable to map a view of file. OS Error(%d): %s", ::GetLastError(), GetLastErrorMessage().c_str());
			return;
		}

		// 通过偏移计算得到了数据指针
		BYTE* pData = (BYTE *)ci->SEND_BLOCK.pMapAddr + (size_t)ViewDelta;

		// 重置传输数据块中的 Buffer 信息为 BufferLength
		ci->SEND_BLOCK.BufferRemain += (DWORD)BufferLength;

		// 开始传输，稍后将触发 OnSend() 回调，继续传输任务
		if (!::HP_Server_Send(pServer, dwConnID, pData, (int)BufferLength)) {

			::UnmapViewOfFile(ci->SEND_BLOCK.pMapAddr);
			ci->SEND_BLOCK.pMapAddr = nullptr;
			::CloseHandle(ci->SEND_BLOCK.hFileMapping);
			ci->SEND_BLOCK.hFileMapping = NULL;
			::CloseHandle(ci->SEND_BLOCK.hFile);
			ci->SEND_BLOCK.hFile = INVALID_HANDLE_VALUE;

			return;
		}
	}
}
