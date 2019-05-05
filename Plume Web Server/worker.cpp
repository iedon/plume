#include "header.h"
#include "general.h"
#include "worker.h"
#include "netcallback.h"
#include "handler_plugin.h"
#include <algorithm>

void Worker_Start()
{
	Worker_Init_MIME();
	Worker_Init_Vhost();
	Worker_Init_HandlerMap();
	Worker_Init_SSLCert();
	Load_Plugins();

	if (!VAR_SERVERS.size() || !VAR_VIRTUAL_HOSTS.size()) {
		VAR_SYS_LOG("No configured virtual hosts, shutting down...");
		exit(APP_ERR_CONFIG);
	}

	for (DWORD i = 0; i < VAR_SERVERS.size(); i++) {

		CStringPlus server_addr;
		server_addr.format("{%s,%d}", VAR_SERVERS[i].bind_ip.c_str(), VAR_SERVERS[i].bind_port);

		VAR_SERVERS[i].pListener = ::Create_HP_HttpServerListener();
		if (VAR_SERVERS[i].pListener == nullptr) {
			
			VAR_SYS_LOG("Error(%d): Unable to create listener for server #%d%s, OS Error(%d): %s", APP_ERR_CREATE_SERVER, i, server_addr.c_str(), SYS_GetLastError(), GetLastErrorMessage(SYS_GetLastError()).c_str());
			exit(APP_ERR_CREATE_SERVER);
		}

		VAR_SERVERS[i].pServer = VAR_SERVERS[i].SSL_enabled ? ::Create_HP_HttpsServer(VAR_SERVERS[i].pListener) : ::Create_HP_HttpServer(VAR_SERVERS[i].pListener);
		if (VAR_SERVERS[i].pServer == nullptr) {
			
			VAR_SYS_LOG("Error(%d): Unable to create server #%d%s, OS Error(%d): %s", APP_ERR_CREATE_SERVER, i, server_addr.c_str(), SYS_GetLastError(), GetLastErrorMessage(SYS_GetLastError()).c_str());
			exit(APP_ERR_CREATE_SERVER);
		}

		if (VAR_SERVERS[i].SSL_enabled) {

			for (DWORD j = 0; j < VAR_VIRTUAL_HOSTS.size(); j++) {
				
				if (VAR_VIRTUAL_HOSTS[j].SSL_enabled) {

					if (VAR_VIRTUAL_HOSTS[j].bind_ip == VAR_SERVERS[i].bind_ip && VAR_VIRTUAL_HOSTS[j].bind_port == VAR_SERVERS[i].bind_port) {

						SNI_MAP sm;
						sm.authority = VAR_VIRTUAL_HOSTS[j].authority;

						for (DWORD k = 0; k < VAR_SSL_CERTS.size(); k++) {

							if (VAR_VIRTUAL_HOSTS[j].SSL_section_name == VAR_SSL_CERTS[k].SSL_section_name) {

								if (VAR_SERVERS[i].IsFirstSSLLoaded) {

									VAR_SSL_CERTS[k].SNI_index = ::HP_SSLServer_AddSSLContext(VAR_SERVERS[i].pServer, SSL_VM_NONE, VAR_SSL_CERTS[k].cert_file.c_str(), VAR_SSL_CERTS[k].cert_key_file.c_str(), VAR_SSL_CERTS[k].cert_key_password.c_str(), VAR_SSL_CERTS[k].cert_CA_file.c_str());

									if (VAR_SSL_CERTS[k].SNI_index < 0) {

										VAR_SYS_LOG("Error(%d): Unable to add certificate(%s) for SSL server #%d%s", APP_ERR_CREATE_SERVER, VAR_SSL_CERTS[k].SSL_section_name.c_str(), i, server_addr.c_str());
										exit(APP_ERR_CREATE_SERVER);
									}

								} else {

									if (!::HP_SSLServer_SetupSSLContext(VAR_SERVERS[i].pServer, SSL_VM_NONE, VAR_SSL_CERTS[k].cert_file.c_str(), VAR_SSL_CERTS[k].cert_key_file.c_str(), VAR_SSL_CERTS[k].cert_key_password.c_str(), VAR_SSL_CERTS[k].cert_CA_file.c_str(), OnSNICallback)) {

										VAR_SYS_LOG("Error(%d): Unable to set certificate(%s) for SSL server #%d%s", APP_ERR_CREATE_SERVER, VAR_SSL_CERTS[k].SSL_section_name.c_str(), i, server_addr.c_str());
										exit(APP_ERR_CREATE_SERVER);
									}
									VAR_SSL_CERTS[k].SNI_index = 0; // 作为默认证书
								}
								VAR_SERVERS[i].IsFirstSSLLoaded = true;	// 首个SSL已加载，通告之后的处理只要用 ::HP_SSLServer_AddSSLContext() 添加证书即可
								sm.SNI_index = VAR_SSL_CERTS[k].SNI_index;
							}
						}
						VAR_SNI_MAPS.push_back(sm);
					}
				}
			}
		}

		::HP_Set_FN_HttpServer_OnAccept(VAR_SERVERS[i].pListener, OnAccept);
		::HP_Set_FN_HttpServer_OnSend(VAR_SERVERS[i].pListener, OnSend);
		::HP_Set_FN_HttpServer_OnClose(VAR_SERVERS[i].pListener, OnClose);
		::HP_Set_FN_HttpServer_OnReceive(VAR_SERVERS[i].pListener, OnReceive);
		::HP_Set_FN_HttpServer_OnShutdown(VAR_SERVERS[i].pListener, OnShutdown);
		::HP_Set_FN_HttpServer_OnUpgrade(VAR_SERVERS[i].pListener, OnUpgrade);
		::HP_Set_FN_HttpServer_OnParseError(VAR_SERVERS[i].pListener, OnParseError);
		::HP_Set_FN_HttpServer_OnChunkHeader(VAR_SERVERS[i].pListener, OnChunkHeader);
		::HP_Set_FN_HttpServer_OnChunkComplete(VAR_SERVERS[i].pListener, OnChunkComplete);
		::HP_Set_FN_HttpServer_OnHeadersComplete(VAR_SERVERS[i].pListener, OnHeadersComplete);
		::HP_Set_FN_HttpServer_OnRequestLine(VAR_SERVERS[i].pListener, OnRequestLine);
		::HP_Set_FN_HttpServer_OnBody(VAR_SERVERS[i].pListener, OnBody);
		::HP_Set_FN_HttpServer_OnMessageBegin(VAR_SERVERS[i].pListener, OnMessageBegin);
		::HP_Set_FN_HttpServer_OnHeader(VAR_SERVERS[i].pListener, OnHeader);
		::HP_Set_FN_HttpServer_OnMessageComplete(VAR_SERVERS[i].pListener, OnMessageComplete);
		::HP_Set_FN_HttpServer_OnHandShake(VAR_SERVERS[i].pListener, OnHandShake);
		::HP_Server_SetMaxConnectionCount(VAR_SERVERS[i].pServer, VAR_MAX_CONNECTIONS);
		::HP_Server_SetOnSendSyncPolicy(VAR_SERVERS[i].pServer, OSSP_CLOSE);

		if (!::HP_Server_Start(VAR_SERVERS[i].pServer, VAR_SERVERS[i].bind_ip.c_str(), VAR_SERVERS[i].bind_port)) {

			VAR_SYS_LOG("Error(%d): Unable to start server #%d%s, OS Error(%d): %s", APP_ERR_CREATE_SERVER, i, server_addr.c_str(), SYS_GetLastError(), GetLastErrorMessage(SYS_GetLastError()).c_str());
			exit(APP_ERR_CREATE_SERVER);
		}
	}

	VAR_SYS_LOG("Initialization Sequence Completed");
}

void Worker_Init_MIME()
{
	CFileOperation File(CStringPlus(APP_DIRECTORY).append("\\Config\\MimeTypes.ini").c_str(), CFO_OPT_READ, CFO_SHARE_ALL);
	if (File.GetFileHandle() == INVALID_HANDLE_VALUE)
	{
		VAR_SYS_LOG("Error opening MIME configuration.");
		return;
	}

	char *tmp = (char *)malloc((DWORD)File.GetStockFileLength() + 1);
	memset(tmp, 0, (DWORD)File.GetStockFileLength() + 1);
	File.Read((DWORD)File.GetStockFileLength(), tmp);
	CStringPlus config(tmp);
	free(tmp);
	File.Close();

	std::vector<CStringPlus> config_split;
	config.split(config_split, "\r\n");
	DWORD config_num = (DWORD)config_split.size();

	if (config_num >= 1) {

		bool foundHeader = false;
		DWORD headerIndex;
		for (DWORD idx = 0; idx < config_num; idx++)
		{
			if (config_split[idx] == "[MimeTypes]") {

				foundHeader = true;
				headerIndex = idx;
				break;
			}
		}
		if (!foundHeader) {

			VAR_SYS_LOG("Error loading MIME types.");
			return;
		}

		for (DWORD i = headerIndex + 1; i < config_num; i++)
		{
			std::vector<CStringPlus> mime_text;
			config_split[i].split(mime_text, "=");
			if (mime_text.size() != 2) continue;

			MIME_TYPE mt;
			mt.ext = mime_text[0];
			mt.res_type = mime_text[1];
			VAR_MIME_TYPES.push_back(mt);
		}

	} else {

		VAR_SYS_LOG("No MIME types to load.");
	}
}

/*
 * 因为虚拟主机域(Authority)支持通配符，所以要将其进行降序排列，
 * 让精确度更高的排在上，精确度差(更模糊)的排在最下(例如: *)。
 * 此函数提供给 Worker_Init_Vhost() 中的 std::sort() 使用。
*/
bool DescSortAuthority(VIRTUAL_HOST vh1, VIRTUAL_HOST vh2)
{
	return (vh1.authority.length() > vh2.authority.length());
}

void Worker_Init_Vhost()
{
	CStringPlus root(APP_DIRECTORY);
	root.append("\\Hosts\\");

	WIN32_FIND_DATAA wfd = { 0 };
	HANDLE hFind = ::FindFirstFileA(CStringPlus(root).append("*.ini").c_str(), &wfd);

	while (hFind != INVALID_HANDLE_VALUE && ::GetLastError() != ERROR_NO_MORE_FILES) {

		CStringPlus vhost_fn(wfd.cFileName);
		CStringPlus errmsg("[Vhosts] Error loading vhost: ");
		errmsg.append(vhost_fn).append(", ");

		VIRTUAL_HOST vh;
		vh.config_file = CStringPlus(root).append(vhost_fn);

		char *tmp = (char *)malloc(MAX_BUFF);

		::GetPrivateProfileStringA("VirtualHost", "enabled", "true", tmp, MAX_BUFF, vh.config_file.c_str());
		if (_stricmp("false", tmp) == 0) {

			free(tmp);
			::FindNextFileA(hFind, &wfd);
			continue;
		}

		::GetPrivateProfileStringA("VirtualHost", "bind_ip", "", tmp, MAX_BUFF, vh.config_file.c_str());
		if (strlen(tmp) == 0) {
			
			free(tmp);
			VAR_SYS_LOG(errmsg.append("invalid bind_ip").c_str());
			::FindNextFileA(hFind, &wfd);
			continue;
		}
		CStringPlus bind_ip(tmp);

		USHORT bind_port = ::GetPrivateProfileIntA("VirtualHost", "bind_port", 80, vh.config_file.c_str());

		::GetPrivateProfileStringA("VirtualHost", "authority", "", tmp, MAX_BUFF, vh.config_file.c_str());
		if (strlen(tmp) == 0) {

			free(tmp);
			VAR_SYS_LOG(errmsg.append("invalid authority").c_str());
			::FindNextFileA(hFind, &wfd);
			continue;
		}
		CStringPlus authority(tmp);

		::GetPrivateProfileStringA("VirtualHost", "doc_root", "", tmp, MAX_BUFF, vh.config_file.c_str());
		if (strlen(tmp) == 0) {

			free(tmp);
			VAR_SYS_LOG(errmsg.append("invalid doc_root").c_str());
			::FindNextFileA(hFind, &wfd);
			continue;
		}
		CStringPlus web_root(tmp);

		::GetPrivateProfileStringA("VirtualHost", "default_pages", "", tmp, MAX_BUFF, vh.config_file.c_str());
		if (strlen(tmp) == 0) {

			free(tmp);
			VAR_SYS_LOG(errmsg.append("invalid default_pages").c_str());
			::FindNextFileA(hFind, &wfd);
			continue;
		}
		CStringPlus default_pages(tmp);

		::GetPrivateProfileStringA("VirtualHost", "run_level", "", tmp, MAX_BUFF, vh.config_file.c_str());
		if (strlen(tmp) == 0) {

			free(tmp);
			VAR_SYS_LOG(errmsg.append("invalid run_level").c_str());
			::FindNextFileA(hFind, &wfd);
			continue;
		}
		CStringPlus run_level(tmp);

		bool use_ssl = ::GetPrivateProfileIntA("VirtualHost", "use_ssl", 0, vh.config_file.c_str()) == 0 ? false : true;

		::GetPrivateProfileStringA("VirtualHost", "ssl_section", "", tmp, MAX_BUFF, vh.config_file.c_str());
		CStringPlus ssl_section(tmp);
		if (use_ssl && ssl_section.empty()) {

			free(tmp);
			VAR_SYS_LOG(errmsg.append("SSL enabled but no ssl section specified").c_str());
			::FindNextFileA(hFind, &wfd);
			continue;
		}
		free(tmp);

		vh.authority = authority;
		vh.bind_ip = bind_ip;
		vh.bind_port = bind_port;
		vh.web_root = web_root;
		vh.default_pages = default_pages;
		vh.run_level = run_level;
		vh.SSL_enabled = use_ssl;
		vh.SSL_section_name = ssl_section;

		if (VAR_ENABLE_ACCESS_LOG) {

			vh.vhost_name = vhost_fn.substr(0, vhost_fn.length() - 4 /* strlen(".ini") */);
			int iDestLength = vh.vhost_name.length() * 2 + 2;
			WCHAR *uni_text = (WCHAR *)malloc(iDestLength);
			memset(uni_text, 0, iDestLength);
			if (!::SYS_CodePageToUnicode(CP_ACP, vh.vhost_name.c_str(), uni_text, &iDestLength)) {

				free(uni_text);
				::FindNextFileA(hFind, &wfd);
				continue;
			}
			std::wstring name(VAR_LOG_DIRECTORY_PREFIX);
			name.append(L"\\").append(uni_text);
			free(uni_text);

			_NLOG_CFG cfg = { name.c_str(), L"access-%Y%m%d.log", L"%Y-%m-%d %H:%M:%S", L"{time} -- " };
			_NLOG_SET_CONFIG_WITH_ID(vh.vhost_name.c_str(), cfg);
		}
		
		VAR_VIRTUAL_HOSTS.push_back(vh); // 向全局虚拟主机中添加本虚拟主机

		bool Reged_IP_Port = false;
		bool isSSLServer = false;
		for (DWORD i = 0; i < VAR_SERVERS.size(); i++) { // 检查全局服务端中是否有服务端已绑定所需的IP与端口
			if (VAR_SERVERS[i].bind_port == bind_port && VAR_SERVERS[i].bind_ip == bind_ip) {

				VAR_VIRTUAL_HOSTS.back().server_index = i; // 注意下标从0开始
				Reged_IP_Port = true;

				if (VAR_SERVERS[i].SSL_enabled) isSSLServer = true;
				break;
			}
		}
		
		if (!Reged_IP_Port) { // 如果IP与端口没注册，则向全局服务端中注册服务端并将本虚拟主机与之绑定
			
			SERVER server;
			server.SSL_enabled = use_ssl;
			server.bind_ip = bind_ip;
			server.bind_port = bind_port;
			server.IsFirstSSLLoaded = false; // 后续的加载过程会设置此值
			VAR_SERVERS.push_back(server);
			VAR_VIRTUAL_HOSTS.back().server_index = (DWORD)VAR_VIRTUAL_HOSTS.size() - 1; // 因为下标从0开始，所以索引=元素数-1
		}

		::FindNextFileA(hFind, &wfd);
	}

	::FindClose(hFind);

	// 降序排序虚拟主机
	if (VAR_VIRTUAL_HOSTS.size() > 1)
		std::sort(VAR_VIRTUAL_HOSTS.begin(), VAR_VIRTUAL_HOSTS.end(), DescSortAuthority);
}

void Worker_Init_HandlerMap()
{
	char *tmp = (char *)malloc(MAX_BUFF);
	::GetPrivateProfileStringA("HandlerMap", "Enabled", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	if (strlen(tmp) == 0) {
		
		free(tmp);
		return;
	}

	CStringPlus Config(tmp);
	std::vector<CStringPlus> reg_ext;
	Config.split(reg_ext, ",");

	for (auto & reg : reg_ext)
	{
		HANDLER_MAP hm;
		hm.ext = reg;
		::GetPrivateProfileStringA("HandlerMap", reg.c_str(), "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
		if (strlen(tmp) == 0) {

			VAR_SYS_LOG("Error loading handler: %s", reg.c_str());
			continue;
		}

		hm.processor = tmp;
		VAR_HANDLER_MAPS.push_back(hm);
	}

	free(tmp);
}

void Worker_Init_SSLCert()
{
	char *tmp = (char *)malloc(MAX_BUFF);
	::GetPrivateProfileStringA("SSL", "Enabled", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	if (strlen(tmp) == 0) {

		free(tmp);
		return;
	}

	CStringPlus Config(tmp);
	free(tmp);

	std::vector<CStringPlus> Certs;
	Config.split(Certs, ",");

	for (auto & cert : Certs)
	{
		SSL_CERT sc;
		sc.SSL_section_name = cert;
		
		char *tmp = (char *)malloc(MAX_BUFF);
		::GetPrivateProfileStringA(cert.c_str(), "File", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
		sc.cert_file = tmp;

		::GetPrivateProfileStringA(cert.c_str(), "CA", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
		sc.cert_CA_file = tmp;

		::GetPrivateProfileStringA(cert.c_str(), "KeyFile", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
		sc.cert_key_file = tmp;

		::GetPrivateProfileStringA(cert.c_str(), "KeyPassword", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
		sc.cert_key_password = tmp;

		free(tmp);
		
		if (sc.cert_file == "" || sc.cert_key_file == "") {
		
			VAR_SYS_LOG("Error loading SSL certificate: %s", cert.c_str());
			continue;
		}
		sc.SNI_index = 0; // 负数代表::HP_SSLServer_AddSSLContext()失败，或者使用默认证书（默认0），稍后将在后续加载过程中设置这个值
		VAR_SSL_CERTS.push_back(sc);
	}
}
