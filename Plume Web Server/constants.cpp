#include <iostream>
#include <string>
#include "Windows.h"
#include "constants.h"

const char   *  APP_NAME						= "Plume";							// 应用名称
const char   *  APP_VER							= "0.5.4";							// 应用版本
const char   *  APP_FULL_NAME					= "Plume Web Server";				// 应用全程
const char	 *	APP_DESCRIPTION					= "iEdon Plume Web Server";			// 应用服务描述
const char	 *  APP_COPYRIGHT					= "Copyright (C) 2012-2018 iEdon. All rights reserved.";
char         *  APP_DIRECTORY					= nullptr;							// 应用目录
std::string     APP_TOKEN						= "iEdon-";							// Plume/x.x.x

HANDLE			VAR_IMPERSONATE_TOKEN			= INVALID_HANDLE_VALUE;				// 为插件派生子进程提供安全TOKEN

DWORD				VAR_MEM_ALLOCATION				= 0;					// 系统内存颗粒大小
std::string			VAR_CONFIG_FILE;										// 配置文件
DWORD				VAR_MAX_CONNECTIONS				= 0;					// 最大连接数
unsigned long long	VAR_LIMIT_BODY					= 0;					// 限制 POST 大小
DWORD				VAR_PLUGIN_BUFFER_SIZE			= 0;					// 插件系统动态页面的输出缓存大小
bool				VAR_ENABLE_ACCESS_LOG			= false;				// 是否记录访问日志
bool				VAR_ENABLE_GZIP					= false;				// 是否启用GZIP
bool				VAR_ENABLE_DIR_LISTING			= false;				// 是否启用目录列表
std::wstring		VAR_LOG_DIRECTORY_PREFIX;								// 日志目录前缀
std::string			VAR_ADMIN_EMAIL;										// 管理员邮箱
std::string			VAR_UPLOAD_TEMP_DIR_PREFIX;								// 上传临时存放目录
std::string			VAR_GZIP_CACHE_TEMP_DIR_PREFIX;							// GZIP缓存文件目录

std::vector<MIME_TYPE>			VAR_MIME_TYPES;							// 全局MIME类型
std::vector<SERVER>				VAR_SERVERS;							// 全局服务端表
std::vector<VIRTUAL_HOST>		VAR_VIRTUAL_HOSTS;						// 全局虚拟主机表
std::vector<HANDLER_MAP>		VAR_HANDLER_MAPS;						// 全局处理映射表
std::vector<SSL_CERT>			VAR_SSL_CERTS;							// 全局SSL证书表
std::vector<SNI_MAP>			VAR_SNI_MAPS;							// 全局SNI映射表
