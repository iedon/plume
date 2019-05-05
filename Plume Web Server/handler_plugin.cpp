#include "header.h"
#include "handler_plugin.h"
#include "general.h"
#include "request.h"

std::vector<PLUGIN>				VAR_PLUGINS;							// 全局已启用插件列表
std::vector<PLUGIN_EVENT>		VAR_PLUGIN_EVENTS;						// 全局已注册插件事件

void Load_Plugins()
{
	char *tmp = (char *)malloc(MAX_BUFF);
	::GetPrivateProfileStringA("Plugins", "Enabled", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	if (strlen(tmp) == 0) {

		free(tmp);
		return;
	}

	CStringPlus Config(tmp);
	std::vector<CStringPlus> reg_plugin;
	Config.split(reg_plugin, ",");

	int plugin_index = 0;
	for (auto & plugin : reg_plugin)
	{
		::GetPrivateProfileStringA("Plugins", plugin.c_str(), "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
		if (strlen(tmp) == 0) {

			VAR_SYS_LOG("Error loading plugin: %s, unable to get plugin path. OS Error(%d): %s", plugin.c_str(), ::GetLastError(), GetLastErrorMessage().c_str());
			continue;
		}

		char *drive = (char *)malloc(MAX_BUFF);
		char *dir = (char *)malloc(MAX_BUFF);
		memset(drive, 0, MAX_BUFF);
		memset(dir, 0, MAX_BUFF);
		_splitpath(tmp, drive, dir, nullptr, nullptr);

		::SetCurrentDirectoryA(CStringPlus(drive).append(dir).c_str());
		free(drive);
		free(dir);

		HMODULE hDLL = ::LoadLibraryA(tmp);
		if (hDLL == NULL) {
			
			::SetCurrentDirectoryA(APP_DIRECTORY);
			VAR_SYS_LOG("Error loading plugin: %s, OS Error(%d): %s", plugin.c_str(), ::GetLastError(), GetLastErrorMessage().c_str());
			continue;
		}

		pFnGetPluginInfo pGetPluginInfo = (pFnGetPluginInfo)::GetProcAddress(hDLL, CStringPlus(APP_NAME).append("_").append("GetPluginInfo").c_str());
		pFnInitPlugin pInitPlugin = (pFnInitPlugin)::GetProcAddress(hDLL, CStringPlus(APP_NAME).append("_").append("InitPlugin").c_str());
		pFnPluginProc pPluginProc = (pFnPluginProc)::GetProcAddress(hDLL, CStringPlus(APP_NAME).append("_").append("PluginProc").c_str());
		pFnFreePlugin pFreePlugin = (pFnFreePlugin)::GetProcAddress(hDLL, CStringPlus(APP_NAME).append("_").append("FreePlugin").c_str());

		if (!pGetPluginInfo || !pInitPlugin || !pPluginProc || !pFreePlugin) {
		
			::FreeLibrary(hDLL);
			::SetCurrentDirectoryA(APP_DIRECTORY);
			VAR_SYS_LOG("Error loading plugin: %s, unable to locate export function. OS Error(%d): %s", plugin.c_str(), ::GetLastError(), GetLastErrorMessage().c_str());
			continue;
		}

		PLUGIN p;

		p.plugin_name = plugin;
		p.fnGetPluginInfo = pGetPluginInfo;
		p.fnInitPlugin = pInitPlugin;
		p.fnPluginProc = pPluginProc;
		p.fnFreePlugin = pFreePlugin;

		plume_server s;
		s.pServer = nullptr;
		s.dwConnID = 0;
		s.pFnGetEnvVar = (pFnGetEnvVar)GetEnvVar;
		s.pFnServerFunc = (pFnServerFunc)ServerFunc;
		s.pFnRead = (pFnRead)Read;
		s.pFnWrite = (pFnWrite)Write;
		s.plugin_index = plugin_index;

		VAR_PLUGINS.push_back(p);

		if (!p.fnInitPlugin(APP_VER, &s)) {

			VAR_PLUGINS.pop_back();
			::FreeLibrary(hDLL);
			::SetCurrentDirectoryA(APP_DIRECTORY);
			VAR_SYS_LOG("Error loading plugin: %s, init plugin failed.", plugin.c_str());
			continue;
		}

		plugin_index++;
		::SetCurrentDirectoryA(APP_DIRECTORY);
	}

	free(tmp);
}

bool PluginSendHeader(plume_server *s, CStringPlus & headerBuffer)
{
	std::vector<CStringPlus> header_line;
	headerBuffer.split(header_line, "\r\n");
	if (!header_line.size()) return false;

	CStringPlus				status_text;
	USHORT					status_code = 0;
	bool					ForceCloseConnection = false;
	std::vector<HP_THeader> parsed_headers;

	for (auto & hl : header_line) {

		size_t pos = hl.find(":");
		if (pos == CStringPlus::npos) continue;

		CStringPlus Key(hl.substr(0, pos));
		Key.trim();

		if (pos + 1 == hl.length()) continue;

		CStringPlus Value(hl.substr(pos + 1));
		Value.trim();

		CStringPlus status(Key);
		status.to_lowcase();
		if (status == "status") {

			size_t pos = Value.find(" ");
			if (pos == CStringPlus::npos) continue;

			char *end_ptr = nullptr;
			status_code = (USHORT)strtoul(Value.substr(0, pos).c_str(), &end_ptr, 10);
			status_text = Value.length() == pos + 1 ? "" : Value.substr(pos + 1);
			continue;
		}

		if (status == "connection") {

			Value.to_lowcase();
			if (Value == "close") {

				ForceCloseConnection = true;
				continue;
			}
		}

		if (status == "content-length") Key = "Content-Length";

		char *header_key = (char *)Alloc_Request_Mem(s->pServer, s->dwConnID, Key.length() + 1);
		memset(header_key, 0, Key.length() + 1);
		strcpy(header_key, Key.c_str());
		char *header_val = (char *)Alloc_Request_Mem(s->pServer, s->dwConnID, Value.length() + 1);
		memset(header_val, 0, Value.length() + 1);
		strcpy(header_val, Value.c_str());
		HP_THeader add_header = { header_key, header_val };
		parsed_headers.push_back(add_header);

	}
	if (status_code == 0) status_code = 200;

	return Send_Response(s->pServer, s->dwConnID, status_code, parsed_headers, status_text, ForceCloseConnection);
}

bool PLAPI Write(plume_server *s, const void *lpvBuffer, int iLength)
{
	if (iLength == 0) return true;

	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(s->pServer, s->dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return false;

	BOOL ret = TRUE;

	// 如果插件没有遵从标准，未首先发送响应标头，只有数据，就发送一个默认头。
	if (ci->PLUGIN_CONTEXT.headerBuffer.size() == 0) {
	
		ci->PLUGIN_CONTEXT.headerBuffer.append("\r\nContent-Type: text/html");
		PluginSendHeader(s, ci->PLUGIN_CONTEXT.headerBuffer);
	}

	if (ci->PLUGIN_CONTEXT.headerHasLength) {
	
		return (::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)lpvBuffer, iLength) == TRUE);
	}

	// 缓冲区未满
	if (ci->PLUGIN_CONTEXT.outBufferSize <= VAR_PLUGIN_BUFFER_SIZE) {

		ci->PLUGIN_CONTEXT.outBuffer.resize(ci->PLUGIN_CONTEXT.outBufferSize + (DWORD)iLength);
		memcpy(ci->PLUGIN_CONTEXT.outBuffer.data() + ci->PLUGIN_CONTEXT.outBufferSize, lpvBuffer, (DWORD)iLength);
		ci->PLUGIN_CONTEXT.outBufferSize += (DWORD)iLength;

		if (ci->PLUGIN_CONTEXT.outBufferSize > VAR_PLUGIN_BUFFER_SIZE) { // 如果现在满了，就倒空缓冲区，并转为chunked编码了

			CStringPlus len;
			len.format("%x\r\n", ci->PLUGIN_CONTEXT.outBufferSize);
			ci->PLUGIN_CONTEXT.headerBuffer.append("\r\nTransfer-Encoding: chunked");

			ret = (PluginSendHeader(s, ci->PLUGIN_CONTEXT.headerBuffer) == TRUE);

			if (ret) ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)len.c_str(), (int)len.length());
			if (ret) ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)ci->PLUGIN_CONTEXT.outBuffer.data(), (int)ci->PLUGIN_CONTEXT.outBufferSize);
			if (ret) ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)"\r\n", 2);

			return (ret == TRUE);
		}

		ret = TRUE;
	
	} else { // 已满，直接 chunked 发送

		CStringPlus len;	
		len.format("%x\r\n", iLength);

		::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)len.c_str(), (int)len.length());
		if(ret) ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)lpvBuffer, iLength);
		if(ret) ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)"\r\n", 2);
	}

	return (ret == TRUE);
}

bool PLAPI Read(plume_server *s, void *lpvBuffer, DWORD dwBytesToRead, DWORD *dwBytesRead)
{
	if (!lpvBuffer) return false;

	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(s->pServer, s->dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return false;

	bool readRet = false;

	if (ci->RECV_BLOCK.hUploadFile == INVALID_HANDLE_VALUE) return false;
		
	// 上传文件已经读完
	if (ci->RECV_BLOCK.currentPointer == ci->RECV_BLOCK.contentLength) {
		
		::CloseHandle(ci->RECV_BLOCK.hUploadFile);
		ci->RECV_BLOCK.hUploadFile = INVALID_HANDLE_VALUE;

		CStringPlus fn;
		fn.format("%x.tmp", ci);
		::DeleteFileA(CStringPlus(VAR_UPLOAD_TEMP_DIR_PREFIX).append(fn).c_str());

		ci->RECV_BLOCK.currentPointer = 0;
		*dwBytesRead = 0;

		return false;
	}

	CFileOperation file(ci->RECV_BLOCK.hUploadFile);
	file.MoveFilePointer(CFO_FILE_BEGIN, ci->RECV_BLOCK.currentPointer);

	readRet = (::ReadFile(ci->RECV_BLOCK.hUploadFile, lpvBuffer, dwBytesToRead, dwBytesRead, NULL) == TRUE);

	ci->RECV_BLOCK.currentPointer += *dwBytesRead;

	return readRet;
}

bool PLAPI ServerFunc(plume_server *s, USHORT usAction, USHORT usParam, void **lpvBuffer, DWORD dwLength)
{
	if(usAction == PLUME_ACTION_REGISTER_EVENT) {

		PLUGIN_EVENT pe;
		pe.plugin_index = s->plugin_index;
		pe.event_type = usParam;
		VAR_PLUGIN_EVENTS.push_back(pe);
		return true;
	}

	if (usAction == PLUME_ACTION_LOG) {

		if (!*lpvBuffer) return false;

		CONN_INFO *ci = nullptr;
		if(s->pServer && s->dwConnID) ::HP_Server_GetConnectionExtra(s->pServer, s->dwConnID, (PVOID *)&ci);
		
		if (ci != nullptr) {

			VAR_SYS_LOG("[plugin: %s, vhost: %s] %s", VAR_PLUGINS[s->plugin_index].plugin_name.c_str(), VAR_VIRTUAL_HOSTS[ci->vhost_index].vhost_name.c_str(), *lpvBuffer);
		
		} else {
			
			VAR_SYS_LOG("[plugin: %s] %s", VAR_PLUGINS[s->plugin_index].plugin_name.c_str(), *lpvBuffer);
		}

		return true;
	}

	if (usAction == PLUME_ACTION_GET_CONNECTION_COUNTS) {

		*(DWORD *)*lpvBuffer = ::HP_Server_GetConnectionCount(s->pServer);
		return true;
	}

	if (usAction == PLUME_ACTION_SEND_ERROR_PAGE) {

		Send_HttpErrorPage(s->pServer, s->dwConnID, usParam, false, (const char *)*lpvBuffer);
		return true;
	}

	if (usAction == PLUME_ACTION_ALLOC_REQUEST_MEM) {
	
		PVOID pAddr = Alloc_Request_Mem(s->pServer, s->dwConnID, (size_t)dwLength);
		if (pAddr == nullptr) return false;
		*lpvBuffer = pAddr;
		return true;
	}

	if (usAction == PLUME_ACTION_FREE_REQUEST_MEM) {

		if (*lpvBuffer == nullptr) return false;
		return Free_Request_Mem(s->pServer, s->dwConnID, *lpvBuffer);
	}

	if (usAction == PLUME_ACTION_GET_IMPERSONATE_TOKEN) {
	
		if (VAR_IMPERSONATE_TOKEN == INVALID_HANDLE_VALUE) return false;
		*lpvBuffer = &VAR_IMPERSONATE_TOKEN;
		return true;
	}

	// 第一次注册异步读取(标记连接为异步接收动作)
	if (usAction == PLUME_ACTION_REGISTER_ASYNC_READ && s->pServer == nullptr && s->dwConnID == 0) {

		if (!*lpvBuffer) return false;

		VAR_PLUGINS[s->plugin_index].async_read_callback = (pFnAsyncRead)*lpvBuffer;
		return true;
	}

	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(s->pServer, s->dwConnID, (PVOID *)&ci);
	if (ci != nullptr) {

		// 第二次注册异步读取(绑定插件自己的信息)
		if (usAction == PLUME_ACTION_REGISTER_ASYNC_READ) {

			if (!*lpvBuffer) return false;

			ci->RECV_BLOCK.plugin_async_context = *lpvBuffer;
			return true;
		}

		if (usAction == PLUME_ACTION_SEND_HEADER) {

			if (!*lpvBuffer) return false;

			CStringPlus tmp((const char *)*lpvBuffer);
			tmp.to_lowcase();

			ci->PLUGIN_CONTEXT.headerHasLength = (tmp.find("content-length:") != CStringPlus::npos);
			if(!ci->PLUGIN_CONTEXT.headerHasLength) ci->PLUGIN_CONTEXT.headerHasLength = (tmp.find("transfer-encoding:") != CStringPlus::npos);

			ci->PLUGIN_CONTEXT.headerBuffer.append((const char *)*lpvBuffer);
			if (ci->PLUGIN_CONTEXT.headerHasLength) PluginSendHeader(s, ci->PLUGIN_CONTEXT.headerBuffer);
			return true;
		}

		if (usAction == PLUME_ACTION_DONE_REQUEST) {

			BOOL ret = TRUE;

			if (!ci->PLUGIN_CONTEXT.headerHasLength) {

				// 缓冲区未满，执行发送头部，发送数据体
				if (ci->PLUGIN_CONTEXT.outBufferSize <= VAR_PLUGIN_BUFFER_SIZE) {

					ci->PLUGIN_CONTEXT.headerBuffer.append(CStringPlus().format("\r\nContent-Length: %lu", ci->PLUGIN_CONTEXT.outBufferSize));
					ret = (PluginSendHeader(s, ci->PLUGIN_CONTEXT.headerBuffer) == TRUE);
					if (ret) ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)ci->PLUGIN_CONTEXT.outBuffer.data(), ci->PLUGIN_CONTEXT.outBufferSize);

				} else { // 已满，发送 chunked 结束码，告知浏览器/客户端发送结束

					ret = ::HP_Server_Send(s->pServer, s->dwConnID, (const BYTE *)"0\r\n\r\n", 5);
				}

			}

			Done_Request(s->pServer, s->dwConnID, usParam ? true : false);
			return (ret == TRUE);
		}

		if (usAction == PLUME_ACTION_REWRITE_VARIABLE) {
		
			switch (usParam) {
			
				case PLUME_REWRITE_SCRIPT_NAME: ci->PATH_BLOCK.script_name = (const char *)*lpvBuffer; return true; break;
				case PLUME_REWRITE_PATH_TRANSLATED: ci->PATH_BLOCK.path_translated = (const char *)*lpvBuffer; return true; break;
				case PLUME_REWRITE_PATH: ci->PATH_BLOCK.path = (const char *)*lpvBuffer; return true; break;
				case PLUME_REWRITE_QUERY_STRING: ci->PATH_BLOCK.query_string = (const char *)*lpvBuffer; return true; break;
				case PLUME_REWRITE_DOCUMENT_ROOT: ci->PATH_BLOCK.web_root = (const char *)*lpvBuffer; return true; break;
				default: return false; break;
			
			}

			return false;
		}

		if (usAction == PLUME_ACTION_READ_CONFIG) {

			plume_readconfig_struct *prs = (plume_readconfig_struct *)*lpvBuffer;
			::GetPrivateProfileStringA(prs->lpszSectionName, prs->lpszKeyName, "", prs->lpszValue, dwLength, VAR_VIRTUAL_HOSTS[ci->vhost_index].config_file.c_str());
			return true;
		}

	}

	return false;
}

bool PLAPI GetEnvVar(plume_server *s, const char *lpszVarName, void *lpvBuffer, DWORD *dwLength)
{
	// 保命开关
	*(char *)lpvBuffer = '\0';
	CStringPlus var_name(lpszVarName);

	if (var_name == "REQUEST_METHOD") {

		LPCSTR rm = ::HP_HttpServer_GetMethod(s->pServer, s->dwConnID);
		if (rm == nullptr) return false;

		if ((DWORD)strlen(rm) + 1 > *dwLength) {

			*dwLength = (DWORD)strlen(rm) + 1;
			return false;
		}

		strcpy((char *)lpvBuffer, rm);
		return true;

	}

	if (var_name == "SERVER_PORT" || var_name == "LOCAL_PORT") {

		for (auto & server : VAR_SERVERS) {
		
			if (server.pServer == s->pServer) {
				
				char buff[128] = { 0 };
				sprintf(buff, "%lu", server.bind_port);
				// 插件所给缓冲区不足，将 dwLength 设置为所需长度返回给插件。
				if ((DWORD)strlen(buff) + 1 > *dwLength) {
				
					*dwLength = (DWORD)strlen(buff) + 1;
					return false;
				}
				strcpy((char *)lpvBuffer, buff);
				return true;
			}
		}
		return false;
	}

	if (var_name == "HTTPS") {

		if (::HP_Server_IsSecure(s->pServer)) {

			// 插件所给缓冲区不足，将 dwLength 设置为所需长度返回给插件。
			if ((DWORD)sizeof("on") > *dwLength) {

				*dwLength = (DWORD)sizeof("on");
				return false;
			}
			strcpy((char *)lpvBuffer, "on");
			return true;
		}
		
		// 插件所给缓冲区不足，将 dwLength 设置为所需长度返回给插件。
		if ((DWORD)sizeof("off") > *dwLength) {

			*dwLength = (DWORD)sizeof("off");
			return false;
		}

		strcpy((char *)lpvBuffer, "off");
		return true;
	}

	if (var_name == "GATEWAY_INTERFACE") {
		
		if ((DWORD)sizeof("CGI/1.1") > *dwLength) {
		
			*dwLength = (DWORD)sizeof("CGI/1.1");
			return false;
		}

		strcpy((char *)lpvBuffer, "CGI/1.1");
		return true;
	}

	if (var_name == "SERVER_PROTOCOL") {

		USHORT ver = ::HP_HttpServer_GetVersion(s->pServer, s->dwConnID);

		CStringPlus protocol;
		if (ver == HV_1_1) protocol = "HTTP/1.1";
		if (ver == HV_1_0) protocol = "HTTP/1.0";

		if ((DWORD)protocol.length() + 1 > *dwLength) {

			*dwLength = (DWORD)protocol.length() + 1;
			return false;
		}

		strcpy((char *)lpvBuffer, protocol.c_str());
		return true;
	}

	if (var_name == "SERVER_ADMIN") {

		if ((DWORD)VAR_ADMIN_EMAIL.length() + 1 > *dwLength) {

			*dwLength = (DWORD)VAR_ADMIN_EMAIL.length() + 1;
			return false;
		}

		strcpy((char *)lpvBuffer, VAR_ADMIN_EMAIL.c_str());
		return true;
	}

	if (var_name == "SERVER_SOFTWARE") {

		if ((DWORD)APP_TOKEN.length() + 1 > *dwLength) {

			*dwLength = (DWORD)APP_TOKEN.length() + 1;
			return false;
		}

		strcpy((char *)lpvBuffer, APP_TOKEN.c_str());
		return true;
	}

	if (var_name.substr(0, 5) == "HTTP_" || var_name == "ALL_HTTP" || var_name == "ALL_RAW") {
		
		DWORD cookie_counts = 0;
		HP_TCookie *cookies = nullptr;
		::HP_HttpServer_GetAllCookies(s->pServer, s->dwConnID, cookies, &cookie_counts);
		if (cookie_counts != 0) {

			cookies = (HP_TCookie *)Alloc_Request_Mem(s->pServer, s->dwConnID, sizeof(HP_TCookie) * cookie_counts);
			//  GetAllCookies 要使用2次，第一次获取个数，第二次获取所有头部信息。
			::HP_HttpServer_GetAllCookies(s->pServer, s->dwConnID, cookies, &cookie_counts);

		}

		CStringPlus text_cookie_content;

		for (DWORD i = 0; i < cookie_counts; i++) {
		
			if (i == 0) {
			
				text_cookie_content = cookies[i].name;
				text_cookie_content += "=";
				text_cookie_content += cookies[i].value;

			} else {
			
				text_cookie_content += "; ";
				text_cookie_content += cookies[i].name;
				text_cookie_content += "=";
				text_cookie_content += cookies[i].value;
			}
		
		}

		if (var_name == "HTTP_COOKIE") {

			if ((DWORD)text_cookie_content.length() + 1 > *dwLength) {

				*dwLength = (DWORD)text_cookie_content.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, text_cookie_content.c_str());
			return true;
		}

		DWORD header_counts = 0;
		HP_THeader *headers = nullptr;
		::HP_HttpServer_GetAllHeaders(s->pServer, s->dwConnID, headers, &header_counts);
		if (header_counts != 0) {

			headers = (HP_THeader *)Alloc_Request_Mem(s->pServer, s->dwConnID, sizeof(HP_THeader) * header_counts);
			//  GetAllHeaders 也要使用2次，第一次获取个数，第二次获取所有头部信息。
			::HP_HttpServer_GetAllHeaders(s->pServer, s->dwConnID, headers, &header_counts);

		}

		CStringPlus ALL_RAW, ALL_HTTP, ALL_HTTP_NAME;

		for (DWORD i = 0; i < header_counts; i++) {

			ALL_HTTP_NAME = "HTTP_";
			CStringPlus tmp(headers[i].name);
			tmp.to_upcase();
			tmp.replace_all("-", "_");
			ALL_HTTP_NAME += tmp;

			if (var_name == "ALL_RAW") {
			
				if (i == 0) {

					ALL_RAW = headers[i].name;
					ALL_RAW += ": ";
					ALL_RAW += headers[i].value;

				} else {

					ALL_RAW += "\r\n";
					ALL_RAW += headers[i].name;
					ALL_RAW += ": ";
					ALL_RAW += headers[i].value;
				}
			}

			if (var_name == "ALL_HTTP") {

				if (i == 0) {

					ALL_HTTP = ALL_HTTP_NAME;
					ALL_HTTP += ":";
					ALL_HTTP += headers[i].value;

				} else {

					ALL_HTTP += "\r\n";
					ALL_HTTP += ALL_HTTP_NAME;
					ALL_HTTP += ":";
					ALL_HTTP += headers[i].value;
				}
			}

			if (var_name == ALL_HTTP_NAME) {
			
				if ((DWORD)strlen(headers[i].value) + 1 > *dwLength) {

					*dwLength = (DWORD)strlen(headers[i].value) + 1;
					return false;
				}

				strcpy((char *)lpvBuffer, headers[i].value);
				return true;
			}

		}

		ALL_HTTP += "\r\n";

		if (var_name == "ALL_HTTP") {
		
			if ((DWORD)ALL_HTTP.length() + 1 > *dwLength) {

				*dwLength = (DWORD)ALL_HTTP.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, ALL_HTTP.c_str());
			return true;
		
		}
		
		if (var_name == "ALL_RAW") {

			if ((DWORD)ALL_RAW.length() + 1 > *dwLength) {

				*dwLength = (DWORD)ALL_RAW.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, ALL_RAW.c_str());
			return true;
		}
	}

	if (var_name == "CONTENT_TYPE") {

		LPCSTR ct = ::HP_HttpServer_GetContentType(s->pServer, s->dwConnID);
		if (ct == nullptr) return false;

		if ((DWORD)strlen(ct) + 1 > *dwLength) {

			*dwLength = (DWORD)strlen(ct) + 1;
			return false;
		}

		strcpy((char *)lpvBuffer, ct);
		return true;
	}

	if (var_name == "REMOTE_ADDR" || var_name == "REMOTE_HOST" || var_name == "REMOTE_PORT") {

		int addrlen = 128;
		USHORT port = 0;
		char *lpszAddress = (char *)Alloc_Request_Mem(s->pServer, s->dwConnID, addrlen);
		memset(lpszAddress, 0, addrlen);

		::HP_Server_GetRemoteAddress(s->pServer, s->dwConnID, lpszAddress, &addrlen, &port);

		if (var_name == "REMOTE_PORT") {

			char *buff = (char *)Alloc_Request_Mem(s->pServer, s->dwConnID, 128);
			sprintf(buff, "%lu", port);
		
			if ((DWORD)strlen(buff) + 1 > *dwLength) {

				*dwLength = (DWORD)strlen(buff) + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, buff);
			return true;

		} else {
		
			if ((DWORD)addrlen + 1 > *dwLength) {

				*dwLength = (DWORD)addrlen + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, lpszAddress);
			return true;

		}

		return false;
	}

	if (var_name == "LOCAL_ADDR" || var_name == "SERVER_ADDR") {

		for (auto & server : VAR_SERVERS) {

			if (server.pServer == s->pServer) {

				if ((DWORD)server.bind_ip.length() + 1 > *dwLength) {

					*dwLength = (DWORD)server.bind_ip.length() + 1;
					return false;
				}
				strcpy((char *)lpvBuffer, server.bind_ip.c_str());
				return true;
			}
		}

		return false;
	}

	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(s->pServer, s->dwConnID, (PVOID *)&ci);
	if (ci != nullptr) {
	
		if (var_name == "QUERY_STRING") {

			if (ci->PATH_BLOCK.query_string.empty()) return false;
			LPCSTR qs = ci->PATH_BLOCK.query_string.c_str();
			
			if ((DWORD)strlen(qs) + 1 > *dwLength) {

				*dwLength = (DWORD)strlen(qs) + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, qs);
			return true;
		}

		if (var_name == "CONTENT_LENGTH") {

			if (ci->RECV_BLOCK.contentLength == 0) return false;

			char *tmp = (char *)Alloc_Request_Mem(s->pServer, s->dwConnID, 128);
			sprintf(tmp, "%llu", ci->RECV_BLOCK.contentLength);

			if ((DWORD)strlen(tmp) + 1 > *dwLength) {

				*dwLength = (DWORD)strlen(tmp) + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, tmp);
			Free_Request_Mem(s->pServer, s->dwConnID, tmp);
			return true;
		}

		if (var_name == "SERVER_NAME") {

			if (VAR_VIRTUAL_HOSTS[ci->vhost_index].authority.find("*") != CStringPlus::npos) {
				
				LPCSTR host = ::HP_HttpServer_GetHost(s->pServer, s->dwConnID);
				if (host) {

					DWORD length = (DWORD)strlen(host) + 1;
					if (length > *dwLength) {

						*dwLength = length;
						return false;
					}

					const char *split = strrchr(host, ':');
					if (split) {
						
						if (host[0] == '[') { // IPv6 with port
						
							strncpy((char *)lpvBuffer, host + 1, split - host - 2);
							*((char *)(lpvBuffer) + (split - host - 2)) = '\0';

						} else { // IPv4 with port

							strncpy((char *)lpvBuffer, host, split - host);
							*((char *)(lpvBuffer) + (split - host)) = '\0';
						}

					} else { // IPv4 w/o port

						strcpy((char *)lpvBuffer, host);
					}

					return true;
				
				} else for (auto & server : VAR_SERVERS) {

					if (server.pServer == s->pServer) {

						if ((DWORD)server.bind_ip.length() + 1 > *dwLength) {

							*dwLength = (DWORD)server.bind_ip.length() + 1;
							return false;
						}
						strcpy((char *)lpvBuffer, server.bind_ip.c_str());
						return true;
					}
				}
			}

			if ((DWORD)VAR_VIRTUAL_HOSTS[ci->vhost_index].authority.length() + 1 > *dwLength) {

				*dwLength = (DWORD)VAR_VIRTUAL_HOSTS[ci->vhost_index].authority.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, VAR_VIRTUAL_HOSTS[ci->vhost_index].authority.c_str());
			return true;
		}

		if (var_name == "SCRIPT_NAME") {

			if ((DWORD)ci->PATH_BLOCK.script_name.length() + 1 > *dwLength) {

				*dwLength = (DWORD)ci->PATH_BLOCK.script_name.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, ci->PATH_BLOCK.script_name.c_str());
			return true;

		}

		if (var_name == "PATH_TRANSLATED" || var_name == "SCRIPT_FILENAME" || var_name == "DOCUMENT_URI" || var_name == "REQUEST_FILENAME") {

			if ((DWORD)ci->PATH_BLOCK.path_translated.length() + 1 > *dwLength) {

				*dwLength = (DWORD)ci->PATH_BLOCK.path_translated.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, ci->PATH_BLOCK.path_translated.c_str());
			return true;
		}

		if (var_name == "REQUEST_URI" || var_name == "URL") {

			if ((DWORD)ci->PATH_BLOCK.path.length() + 1 > *dwLength) {

				*dwLength = (DWORD)ci->PATH_BLOCK.path.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, ci->PATH_BLOCK.path.c_str());
			return true;
		}

		if (var_name == "DOCUMENT_ROOT") {

			if ((DWORD)ci->PATH_BLOCK.web_root.length() + 1 > *dwLength) {

				*dwLength = (DWORD)ci->PATH_BLOCK.web_root.length() + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, ci->PATH_BLOCK.web_root.c_str());
			return true;
		}

		if (var_name == "INSTANCE_ID") {

			char *tmp = (char *)Alloc_Request_Mem(s->pServer, s->dwConnID, 128);
			sprintf(tmp, "%d", ci->vhost_index);

			if ((DWORD)strlen(tmp) + 1 > *dwLength) {

				*dwLength = (DWORD)strlen(tmp) + 1;
				return false;
			}

			strcpy((char *)lpvBuffer, tmp);
			Free_Request_Mem(s->pServer, s->dwConnID, tmp);
			return true;
		}
	}

	return false;
}
