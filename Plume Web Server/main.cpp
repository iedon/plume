/*
******************************************
**********   Plume Web Server   **********
********** Copyright (C) iEdon. **********
******************************************
*/

#include "header.h"
#include "main.h"
#include "general.h"
#include "NTService.h"
#include "worker.h"
#include "handler_plugin.h"

int main(int argc, const char *argv[])
{
	::SetConsoleTitleA(APP_FULL_NAME);
	initalize();

	if (argc > 1) {

		CStringPlus param(argv[1]);
		CStringPlus msg;

		if (param == "/install") {						// 安装服务
			
			if (NTService_Install()) {

				std::cerr << "Service installed.";
			} else {

				msg.format("Unable to install service. %s", GetLastErrorMessage().c_str());
				VAR_SYS_LOG_DAEMON(msg.c_str());
				std::cerr << msg;
			}
			
		} else if (param == "/remove") {				// 卸载服务
			
			if (NTService_Remove()) {
				std::cerr << "Service removed.";
			} else {

				msg.format("Unable to remove service. %s", GetLastErrorMessage().c_str());
				VAR_SYS_LOG_DAEMON(msg.c_str());
				std::cerr << msg;
			}
			
		} else if (param == "/service") {				// Win32 服务入口
			
			NTService_initialize();

		} else if (param == "/worker") {				// 工作进程入口

			Worker_Start();
			::WaitForSingleObject(INVALID_HANDLE_VALUE, INFINITE);

		} else if (param == "/v") {						// 版本信息输出
			
			std::cerr << APP_NAME << "/" << APP_VER;

		} else if (param == "/?" || param == "-h" || param == "/h" || param == "--help") { // 显示帮助信息
			
			print_help(argv[0]);

		} else {

			std::cerr << "Invalid parameter, using \"/?\" to get help." << std::endl;

		}

	} else {

		print_help(argv[0]);

	}

	cleanup();
    return ERROR_SUCCESS;
}

void initalize()
{
	// 初始化应用 token
	APP_TOKEN.append(APP_NAME).append("/").append(APP_VER);

	// 创建并初始化应用目录字符串，设置当前目录为程序目录
	APP_DIRECTORY = (char *)malloc(MAX_BUFF);
	::GetModuleFileNameA(NULL, APP_DIRECTORY, MAX_BUFF);
	(strrchr(APP_DIRECTORY, '\\'))[0] = '\0';
	::SetCurrentDirectoryA(APP_DIRECTORY);

	if (!::LogonUserA("NetworkService", "NT AUTHORITY", NULL, LOGON32_LOGON_SERVICE, LOGON32_PROVIDER_DEFAULT, &VAR_IMPERSONATE_TOKEN)) {
	
		VAR_IMPERSONATE_TOKEN = INVALID_HANDLE_VALUE;
	}

	SYSTEM_INFO sysinfo;
	memset(&sysinfo, 0, sizeof(sysinfo));
	::GetSystemInfo(&sysinfo);
	VAR_MEM_ALLOCATION = sysinfo.dwAllocationGranularity;
	if (VAR_MEM_ALLOCATION == 0) {

		std::cerr << "Error: Unable to get memory allocation granularity.";
		cleanup();
		exit(APP_ERR_CONFIG);
	}

	VAR_CONFIG_FILE.append(APP_DIRECTORY).append("\\Config\\").append(APP_NAME).append(".ini");

	VAR_MAX_CONNECTIONS = ::GetPrivateProfileIntA("Settings", "MaxConn", 30000, VAR_CONFIG_FILE.c_str());
	VAR_LIMIT_BODY = ::GetPrivateProfileIntA("Settings", "LimitBodySize", 8192, VAR_CONFIG_FILE.c_str());
	VAR_LIMIT_BODY *= 1024; // KB转化为B
	if (VAR_LIMIT_BODY == 0) VAR_LIMIT_BODY = 8192 * 1024; // 规避无效设置
	VAR_PLUGIN_BUFFER_SIZE = ::GetPrivateProfileIntA("Plugins", "BufferSize", 64, VAR_CONFIG_FILE.c_str());
	VAR_PLUGIN_BUFFER_SIZE *= 1024; // KB转化为B
	if (VAR_PLUGIN_BUFFER_SIZE > VAR_BUFFER || VAR_PLUGIN_BUFFER_SIZE == 0) VAR_PLUGIN_BUFFER_SIZE = VAR_BUFFER; // 规避无效设置

	char *tmp = (char *)malloc(MAX_BUFF);
	::GetPrivateProfileStringA("Settings", "WriteAccessLog", "True", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	(_stricmp(tmp, "true") == 0) ? VAR_ENABLE_ACCESS_LOG = true : VAR_ENABLE_ACCESS_LOG = false;

	::GetPrivateProfileStringA("Settings", "EnableGZIP", "True", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	(_stricmp(tmp, "true") == 0) ? VAR_ENABLE_GZIP = true : VAR_ENABLE_GZIP = false;

	::GetPrivateProfileStringA("Settings", "DirectoryListing", "True", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	(_stricmp(tmp, "true") == 0) ? VAR_ENABLE_DIR_LISTING = true : VAR_ENABLE_DIR_LISTING = false;

	::GetPrivateProfileStringA("Settings", "ServerAdmin", "", tmp, MAX_BUFF, VAR_CONFIG_FILE.c_str());
	VAR_ADMIN_EMAIL = tmp;

	if (!PathFileExistsA(".\\WebLogs")) {
		CreateDirectoryA(".\\WebLogs", NULL);
	}
	if (!PathFileExistsA(".\\WebTemp")) {
		CreateDirectoryA(".\\WebTemp", NULL);
	}
	if (!PathFileExistsA(".\\WebTemp\\upload")) {
		CreateDirectoryA(".\\WebTemp\\upload", NULL);
	}
	if (!PathFileExistsA(".\\WebTemp\\gzip")) {
		CreateDirectoryA(".\\WebTemp\\gzip", NULL);
	}

	VAR_UPLOAD_TEMP_DIR_PREFIX.append(APP_DIRECTORY).append("\\WebTemp\\upload\\req_");
	VAR_GZIP_CACHE_TEMP_DIR_PREFIX.append(APP_DIRECTORY).append("\\WebTemp\\gzip\\gz_");

	int iDestLength = strlen(APP_DIRECTORY) * 2 + 2;
	WCHAR *uni_text = (WCHAR *)malloc(iDestLength);
	memset(uni_text, 0, iDestLength);
	if (!::SYS_CodePageToUnicode(CP_ACP, APP_DIRECTORY, uni_text, &iDestLength)) {

		free(uni_text);
		free(tmp);
		std::cerr << "Error: Failed to get directory prefix for logs.";
		cleanup();
		exit(APP_ERR_INTERNAL);
	}
	VAR_LOG_DIRECTORY_PREFIX = uni_text;
	VAR_LOG_DIRECTORY_PREFIX.append(L"\\WebLogs");
	free(uni_text);

	_NLOG_CFG cfg = { VAR_LOG_DIRECTORY_PREFIX.c_str(), L"worker.log", L"%Y-%m-%d %H:%M:%S", L"[{time}] [{file}:{line}]: " };
	_NLOG_SET_CONFIG_WITH_ID("__SYS_WORKER_", cfg);
	_NLOG_CFG cfg2 = { VAR_LOG_DIRECTORY_PREFIX.c_str(), L"daemon.log", L"%Y-%m-%d %H:%M:%S", L"[{time}] [{file}:{line}]: " };
	_NLOG_SET_CONFIG_WITH_ID("__SYS_DAEMON_", cfg2);

	free(tmp);
}

void cleanup()
{
	_NLOG_SHUTDOWN();

	for (auto & server : VAR_SERVERS) {

		if (server.SSL_enabled) {
			::HP_SSLServer_CleanupSSLContext(server.pServer);
			::Destroy_HP_HttpsServer(server.pServer);
		}
		else {

			::Destroy_HP_HttpServer(server.pServer);
		}

		::Destroy_HP_HttpServerListener(server.pListener);
	}

	for (auto & plugin : VAR_PLUGINS) {
	
		plugin.fnFreePlugin();
	}

	if (APP_DIRECTORY) {

		free(APP_DIRECTORY);
		APP_DIRECTORY = nullptr;
	}

	if (VAR_IMPERSONATE_TOKEN != INVALID_HANDLE_VALUE) {
		
		::CloseHandle(VAR_IMPERSONATE_TOKEN);
		VAR_IMPERSONATE_TOKEN = INVALID_HANDLE_VALUE;
	}
}

void print_help(const char *full_path)
{
	char *fn = (char *)malloc(MAX_BUFF);
	memset(fn, 0, MAX_BUFF);
	_splitpath(full_path, nullptr, nullptr, fn, nullptr);

	std::cerr << APP_NAME << "/" << APP_VER << std::endl
		<< "Build: " << __DATE__ << std::endl
		<< APP_COPYRIGHT << std::endl << std::endl
		<< "\tUsage:\t" << fn << " [options]" << std::endl << std::endl
		<< "\t\t/?\t\tShow this screen." << std::endl
		<< "\t\t/install\tInstall " << APP_NAME << " as a service." << std::endl
		<< "\t\t/remove\t\tRemove " << APP_NAME << " service." << std::endl
		<< "\t\t/v\t\tShow " << APP_NAME << " version." << std::endl;

	free(fn);
}
