#include "header.h"
#include "request.h"
#include "general.h"
#include "handler_staticfile.h"
#include "handler_plugin.h"

void Begin_Request(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (!::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci) || ci == nullptr) return;

	LPCSTR http_host = ::HP_HttpServer_GetHost(pServer, dwConnID);
	CStringPlus authority(http_host != nullptr ? http_host : "*");
	size_t split = authority.rfind(":");
	if (split != CStringPlus::npos) {

		authority = authority.substr(0, split);
	}

	char ip[128] = { 0 };
	int ip_length = sizeof(ip);
	USHORT port;
	::HP_Server_GetListenAddress(pServer, ip, &ip_length, &port);

	size_t qs_pos = ci->PATH_BLOCK.path.find("?");
	ci->PATH_BLOCK.script_name = (qs_pos == CStringPlus::npos) ? ci->PATH_BLOCK.path : ci->PATH_BLOCK.path.substr(0, qs_pos);
	CStringPlus win_path_no_query_string(ci->PATH_BLOCK.script_name);
	win_path_no_query_string.replace_all("/", "\\");
	LPCSTR qs = ::HP_HttpServer_GetUrlField(pServer, dwConnID, HUF_QUERY);
	if(qs) ci->PATH_BLOCK.query_string = qs;

	// 根据请求信息找到对应虚拟主机
	bool foundVhost = false;
	for (DWORD i = 0; i < VAR_VIRTUAL_HOSTS.size(); i++) {

		// 通配符匹配域名，IP，端口
		if (::PathMatchSpecA(authority.c_str(), VAR_VIRTUAL_HOSTS[i].authority.c_str()) && VAR_VIRTUAL_HOSTS[i].bind_ip == ip && VAR_VIRTUAL_HOSTS[i].bind_port == port) {

			foundVhost = true;

			// 填写到连接信息
			ci->vhost_index = i;
			ci->PATH_BLOCK.web_root = VAR_VIRTUAL_HOSTS[i].web_root;
			ci->PATH_BLOCK.path_translated = VAR_VIRTUAL_HOSTS[i].web_root + win_path_no_query_string;

			DWORD fileAttr = ::GetFileAttributesA(ci->PATH_BLOCK.path_translated.c_str());
			if (fileAttr != INVALID_FILE_ATTRIBUTES && ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)) {

				// 资源是目录，但用户请求的URL结尾却不是"/"，而且也不是QueryString，则自动重定向
				if (qs_pos == CStringPlus::npos && ci->PATH_BLOCK.path.substr(ci->PATH_BLOCK.path.length() - 1) != "/") {

					CStringPlus redirect(http_host != nullptr ? http_host : "");
					if (http_host) {
						redirect.insert(0, VAR_VIRTUAL_HOSTS[ci->vhost_index].SSL_enabled ? "https://" : "http://");
					}
					redirect.append(::HP_HttpServer_GetUrlField(pServer, dwConnID, HUF_PATH));
					redirect.append("/");

					char *str_location = (char *)Alloc_Request_Mem(pServer, dwConnID, MAX_BUFF);
					strcpy(str_location, redirect.c_str());
					HP_THeader location_header = { "Location", str_location };
					std::vector<HP_THeader> headers;
					headers.push_back(location_header);

					Send_Response(pServer, dwConnID, HSC_MOVED_PERMANENTLY, headers);
					Done_Request(pServer, dwConnID);
					return;
				}

				// 资源是目录，检索目录下面是否有默认页面，按顺序匹配。然后写入 path_translated
				std::vector<CStringPlus> default_pages;
				VAR_VIRTUAL_HOSTS[i].default_pages.split(default_pages, ",");
				bool isEndWithSlash = (ci->PATH_BLOCK.path_translated.substr(ci->PATH_BLOCK.path_translated.length() - 1) != "\\");
				for (auto & dp : default_pages) {

					CStringPlus temp(ci->PATH_BLOCK.path_translated);
					CStringPlus temp1(ci->PATH_BLOCK.script_name);
					if (isEndWithSlash) {

						temp.append("\\").append(dp);
						temp1.append("/").append(dp);
						if (::PathFileExistsA(temp.c_str())) {
						
							ci->PATH_BLOCK.path_translated = temp;
							ci->PATH_BLOCK.script_name = temp1;
							break;
						}

					} else {

						temp.append(dp);
						temp1.append(dp);
						if (::PathFileExistsA(temp.c_str())) {

							ci->PATH_BLOCK.path_translated = temp;
							ci->PATH_BLOCK.script_name = temp1;
							break;
						}
					}
				}
			}
			break;
		}
	}

	// 没有找到对应的虚拟主机(网站)
	if (!foundVhost) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_BAD_REQUEST);
		return;
	}
	if (ci->RECV_BLOCK.contentLength > VAR_LIMIT_BODY) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_REQUEST_ENTITY_TOO_LARGE);
		return;
	}

	for (auto & pe : VAR_PLUGIN_EVENTS) {

		if (pe.event_type == PLUME_EVENT_BEGIN_REQUEST) {

			int ret = VAR_PLUGINS[pe.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_BEGIN_REQUEST, NULL, NULL);
			if (ret == PLUME_EVENT_RESULT_STOP) {

				Done_Request(pServer, dwConnID);
				return;
			}
			if (ret == PLUME_EVENT_RESULT_DISCONNECT) {

				::HP_HttpServer_Release(pServer, dwConnID);
				return;
			}

		}
	}

	bool need_plugin_to_process = false;
	DWORD hm_idx = 0;
	for (hm_idx; hm_idx < VAR_HANDLER_MAPS.size(); hm_idx++) {

		if (VAR_HANDLER_MAPS[hm_idx].ext == GetExtensionByPath(ci->PATH_BLOCK.path_translated)) {

			need_plugin_to_process = true;
			break;
		}
	}

	if (need_plugin_to_process) {

		bool found_bound_processor = false;
		DWORD plugin_idx = 0;
		for (plugin_idx; plugin_idx < VAR_PLUGINS.size(); plugin_idx++) {
		
			if (VAR_PLUGINS[plugin_idx].plugin_name == VAR_HANDLER_MAPS[hm_idx].processor) {
			
				ci->PLUGIN_CONTEXT.plugin_index = plugin_idx;
				found_bound_processor = true;
				break;
			}
		}

		if (!found_bound_processor) {

			VAR_SYS_LOG("Error: No available handler for \"%s\"", VAR_HANDLER_MAPS[hm_idx].processor.c_str());

			Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR, false, "<p>\xe5\xb7\xb2\xe7\xbb\x8f\xe5\x90\xaf\xe7\x94\xa8\xe7\x9a\x84\xe6\x8f\x92\xe4\xbb\xb6\xe4\xb8\xad\xe6\xb2\xa1\xe6\x9c\x89\xe5\x90\x88\xe9\x80\x82\xe7\x9a\x84\xe5\xa4\x84\xe7\x90\x86\xe5\x99\xa8\xe6\x9d\xa5\xe5\xa4\x84\xe7\x90\x86\xe5\xbd\x93\xe5\x89\x8d\xe7\x9a\x84\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82</p>");
			return;
		}

		CStringPlus run_level(VAR_VIRTUAL_HOSTS[ci->vhost_index].run_level);
		run_level.to_lowcase();
		if (run_level != "exec") {
		
			Send_HttpErrorPage(pServer, dwConnID, HSC_FORBIDDEN, false, "<p>\xe5\xbd\x93\xe5\x89\x8d\xe7\xbd\x91\xe7\xab\x99\xe5\xb7\xb2\xe7\xbb\x8f\xe7\xa6\x81\xe6\xad\xa2\xe6\x8f\x92\xe4\xbb\xb6\xe5\xa4\x84\xe7\x90\x86\xe5\x99\xa8\xe6\x9d\xa5\xe5\xa4\x84\xe7\x90\x86\xe5\xbd\x93\xe5\x89\x8d\xe8\xaf\xb7\xe6\xb1\x82\xe3\x80\x82</p>");
			return;
		}

		// 如果没有注册异步接收事件，那么我们现在必须返回以进行数据接收动作
		if (!VAR_PLUGINS[plugin_idx].async_read_callback) return;

		int ret = VAR_PLUGINS[plugin_idx].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_HANDLE_REQUEST, NULL, NULL);
		if (!ret) {
		
			Done_Request(pServer, dwConnID);
		}
		return;
	}

	// 当没有合适的插件来处理请求时，使用静态资源处理器
	Http_StaticFileHandler(pServer, dwConnID);
}

void Done_Request(HP_HttpServer pServer, HP_CONNID dwConnID, bool CloseConnectionAnyway)
{
	CONN_INFO *ci = nullptr;
	if (!::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci) || ci == nullptr) return;

	for (auto & pe : VAR_PLUGIN_EVENTS) {

		if (pe.event_type == PLUME_EVENT_DONE_REQUEST) {

			int ret = VAR_PLUGINS[pe.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_DONE_REQUEST, NULL, NULL);

			if (ret == PLUME_EVENT_RESULT_DISCONNECT) {

				::HP_HttpServer_Release(pServer, dwConnID);
				return;
			}

		}
	}

	// 关闭文件，删除临时文件
	if (ci->RECV_BLOCK.hUploadFile != INVALID_HANDLE_VALUE) {

		::CloseHandle(ci->RECV_BLOCK.hUploadFile);
		ci->RECV_BLOCK.hUploadFile = INVALID_HANDLE_VALUE;

		CStringPlus fn;
		fn.format("%x.tmp", ci);
		::DeleteFileA(CStringPlus(VAR_UPLOAD_TEMP_DIR_PREFIX).append(fn).c_str());
	}

	// 解除内存映射块，关闭文件映射，关闭文件
	if (ci->SEND_BLOCK.pMapAddr) {

		::UnmapViewOfFile(ci->SEND_BLOCK.pMapAddr);
		ci->SEND_BLOCK.pMapAddr = nullptr;
	}
	if (ci->SEND_BLOCK.hFileMapping) {

		::CloseHandle(ci->SEND_BLOCK.hFileMapping);
		ci->SEND_BLOCK.hFileMapping = NULL;
	}
	if (ci->SEND_BLOCK.hFile != INVALID_HANDLE_VALUE) {

		::CloseHandle(ci->SEND_BLOCK.hFile);
		ci->SEND_BLOCK.hFile = INVALID_HANDLE_VALUE;
	}

	// 重置信息表
	ci->protocol = 0;
	ci->version = 0;
	ci->vhost_index = -1;
	ci->PATH_BLOCK.query_string.clear();
	ci->PATH_BLOCK.path.clear();
	ci->PATH_BLOCK.path_translated.clear();
	ci->PATH_BLOCK.script_name.clear();
	ci->PATH_BLOCK.web_root.clear();
	ci->RECV_BLOCK.contentLength = 0;
	ci->RECV_BLOCK.currentPointer = 0;
	ci->RECV_BLOCK.plugin_async_context = nullptr;
	ci->SEND_BLOCK.use_send_blk = false;
	ci->SEND_BLOCK.has_header_sent = false;
	ci->SEND_BLOCK.BeginPointer = 0;
	ci->SEND_BLOCK.BufferRemain = 0;
	ci->SEND_BLOCK.EndPointer = 0;
	ci->PLUGIN_CONTEXT.plugin_index = -1;
	ci->PLUGIN_CONTEXT.headerBuffer.clear();
	ci->PLUGIN_CONTEXT.headerHasLength = false;
	ci->PLUGIN_CONTEXT.outBufferSize = 0;
	ci->PLUGIN_CONTEXT.outBuffer.clear();
	std::vector<BYTE>().swap(ci->PLUGIN_CONTEXT.outBuffer);

	// 清理请求内存
	for (auto & mem : ci->REQ_MEM) {

		if (mem.pAddress != nullptr) {

			free(mem.pAddress);
			mem.pAddress = nullptr;
		}

	}
	ci->REQ_MEM.clear();
	std::vector<REQ_MEM>().swap(ci->REQ_MEM); // 手动清理vector内存，见《Effective STL》第17条

	if (CloseConnectionAnyway || !::HP_HttpServer_IsKeepAlive(pServer, dwConnID)) {
		
		::HP_HttpServer_Release(pServer, dwConnID);
	}

}

bool Send_Response(HP_HttpServer pServer, HP_CONNID dwConnID, USHORT usStatusCode, const std::vector<HP_THeader> & Headers, const CStringPlus & StatusText, bool CloseConnectionAnyway, bool UseHPSendFileFunction, const CStringPlus & FileAddr)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (!::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci) || ci == nullptr) return false;

	char date[30] = { 0 };

	GetHttpGMTTime(date);

	std::vector<HP_THeader> pre_headers;
	HP_THeader add_server_headers[] = { { "Date", date } , {"Server", APP_TOKEN.c_str()} };
	for (auto & pair : add_server_headers) {

		pre_headers.push_back(pair);
	}
	pre_headers.insert(pre_headers.end(), Headers.begin(), Headers.end());
	HP_THeader connection = { "Connection", (CloseConnectionAnyway || !::HP_HttpServer_IsKeepAlive(pServer, dwConnID)) ? "close" : "Keep-Alive" };
	pre_headers.push_back(connection);

	CStringPlus complex_status_desc_text(StatusText.empty() ? GetHttpDefaultStatusCodeDesc(usStatusCode) : StatusText.c_str());

	if (VAR_ENABLE_ACCESS_LOG && ci->vhost_index != -1)
	{
		CStringPlus original_path(::HP_HttpServer_GetUrlField(pServer, dwConnID, HUF_PATH));
		CStringPlus query_string(ci->PATH_BLOCK.query_string);
		
		if (!query_string.empty()) {
		
			original_path.append("?").append(query_string);
		}

		char ip[128] = { 0 };
		int ip_length = sizeof(ip);
		USHORT port;
		::HP_Server_GetRemoteAddress(pServer, dwConnID, ip, &ip_length, &port);
		
		_NLOG_APP_WITH_ID(VAR_VIRTUAL_HOSTS[ci->vhost_index].vhost_name.c_str())("{%s,%lu} %s %s %lu %s", ip, port, ::HP_HttpServer_GetMethod(pServer, dwConnID), original_path.c_str(), usStatusCode, complex_status_desc_text.c_str());
	}

	if (UseHPSendFileFunction) {
	
		return (::HP_HttpServer_SendLocalFile(pServer, dwConnID, FileAddr.c_str(), usStatusCode, complex_status_desc_text.c_str(), &pre_headers[0], (int)pre_headers.size()) == TRUE);

	} else {
	
		return (::HP_HttpServer_SendResponse(pServer, dwConnID, usStatusCode, complex_status_desc_text.c_str(), &pre_headers[0], (int)pre_headers.size(), nullptr, 0) == TRUE);
	}
}

void Send_HttpErrorPage(HP_HttpServer pServer, HP_CONNID dwConnID, USHORT usStatusCode, bool CloseConnectionAnyway, const char *custom_err_mg)
{
	CStringPlus sc;
	sc.format("%d", usStatusCode);
	if (sc.substr(0, 1) == "3") {

		Send_Response(pServer, dwConnID, usStatusCode);
		Done_Request(pServer, dwConnID);
		return;
	}

	CStringPlus err_page(GenerateHttpErrorPage(usStatusCode, custom_err_mg != nullptr ? custom_err_mg : nullptr));
	CStringPlus length;
	length.format("%lu", (DWORD)err_page.length());

	std::vector<HP_THeader> header;
	HP_THeader nvpair = { "Content-Type", "text/html; charset=utf-8" };
	header.push_back(nvpair);

	char *str_length = (char *)Alloc_Request_Mem(pServer, dwConnID, 128);
	strcpy(str_length, length.c_str());
	nvpair = { "Content-Length", str_length };
	header.push_back(nvpair);

	if (Send_Response(pServer, dwConnID, usStatusCode, header, "", CloseConnectionAnyway)) {

		if (strcmp(::HP_HttpServer_GetMethod(pServer, dwConnID), HTTP_METHOD_HEAD) != 0)
			::HP_Server_Send(pServer, dwConnID, (const BYTE *)err_page.data(), (int)err_page.length());

	}

	Done_Request(pServer, dwConnID, CloseConnectionAnyway);
}

// 申请请求(request)内存，可显式用 Free_Request_Mem() 释放，也可不释放（连接完成后，系统自动回收）
PVOID Alloc_Request_Mem(HP_HttpServer pServer, HP_CONNID dwConnID, size_t memsize)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (!::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci) || ci == nullptr) return nullptr;

	PVOID ptr = malloc(memsize);
	if (ptr == nullptr) return nullptr;

	REQ_MEM mem;
	mem.pAddress = ptr;

	ci->REQ_MEM.push_back(mem);
	return ptr;
}

// 显式手动释放请求内存
bool Free_Request_Mem(HP_HttpServer pServer, HP_CONNID dwConnID, PVOID pAddress)
{
	CONN_INFO *ci = nullptr;
	if (!::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci) || ci == nullptr) return false;
	bool released = false;

	for (auto & mem : ci->REQ_MEM) {

		if (mem.pAddress == pAddress) {

			free(pAddress);
			mem.pAddress = nullptr;

			released = true;
			break;
		}
	}

	return released;
}
