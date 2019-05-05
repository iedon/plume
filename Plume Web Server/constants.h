#pragma once

#include "CFileOperation.h"
#include "CStringPlus.h"

#define APP_ERR_CONFIG 10000
#define APP_ERR_SPAWN_WORKER 10001
#define APP_ERR_KILL 10002
#define APP_ERR_CREATE_SERVER 10003
#define APP_ERR_INTERNAL 10004

#define VAR_BUFFER							4 * 1024 * 1024		// �����ļ�ʱ�����Ĵ�С(MB)
#define	VAR_LIMIT_LEN_OF_FILE_TO_GZIP		1 * 1024 * 1024		// �������GZIPѹ����ԭʼ�ļ���С(MB)

#define HTTP_METHOD_POST					"POST"
#define HTTP_METHOD_PUT						"PUT"
#define HTTP_METHOD_PATCH					"PATCH"
#define HTTP_METHOD_GET						"GET"
#define HTTP_METHOD_DELETE					"DELETE"
#define HTTP_METHOD_HEAD					"HEAD"
#define HTTP_METHOD_TRACE					"TRACE"
#define HTTP_METHOD_OPTIONS					"OPTIONS"
#define HTTP_METHOD_CONNECT					"CONNECT"

typedef struct MIME_TYPE {

	CStringPlus		ext;
	CStringPlus		res_type;

} MIME_TYPE;

typedef struct HANDLER_MAP {

	CStringPlus		ext;		// without "."
	CStringPlus		processor;

} HANDLER_MAP;

typedef struct VIRTUAL_HOST {

	CStringPlus		config_file;
	CStringPlus		bind_ip;
	USHORT			bind_port    = 0;
	CStringPlus		authority;
	CStringPlus		web_root;
	CStringPlus		default_pages;
	CStringPlus		run_level;
	DWORD			server_index = 0;
	bool			SSL_enabled  = false;
	CStringPlus		SSL_section_name;
	CStringPlus		vhost_name;

} VIRTUAL_HOST;

typedef struct SNI_MAP {

	CStringPlus		authority;
	DWORD			SNI_index = 0;

} SNI_MAP;

typedef struct SSL_CERT {

	CStringPlus		SSL_section_name;
	DWORD			SNI_index = 0;
	CStringPlus		cert_file;
	CStringPlus		cert_CA_file;
	CStringPlus		cert_key_file;
	CStringPlus		cert_key_password;

} SSL_CERT;

typedef struct SERVER {

	PVOID			pListener        = nullptr;
	PVOID			pServer          = nullptr;
	CStringPlus		bind_ip;
	USHORT			bind_port        = 0;
	bool			SSL_enabled      = false;
	bool			IsFirstSSLLoaded = false;

} SERVER;

typedef struct RECV_BLOCK {

	HANDLE					hUploadFile					= INVALID_HANDLE_VALUE;
	unsigned long long		currentPointer				= 0;
	unsigned long long		contentLength				= 0;
	PVOID					plugin_async_context		= nullptr;

} RECV_BLOCK;

typedef struct SEND_BLOCK {

	bool					use_send_blk	= false;				// �Ƿ�ʹ�ñ������
	bool					has_header_sent = false;				// �Ƿ��ѷ���header(��һ��)
	HANDLE					hFile			= INVALID_HANDLE_VALUE;
	HANDLE					hFileMapping	= NULL;					// Ϊʲô�� NULL ������ INVALID_HANDLE_VALUE���μ� MSDN: CreateFileMapping()
	PVOID					pMapAddr		= nullptr;
	unsigned long long		BeginPointer	= 0;
	unsigned long long		EndPointer		= 0;
	DWORD					BufferRemain	= 0;

} SEND_BLOCK;

typedef struct PATH_BLOCK {

	CStringPlus		web_root;
	CStringPlus		script_name;		// script_name �� path ��������ǣ�script_name �� path ���� default page��
	CStringPlus		path;
	CStringPlus		path_translated;
	CStringPlus		query_string;

} PATH_BLOCK;

typedef struct REQ_MEM {

	PVOID			pAddress = nullptr;

} REQ_MEM;

typedef struct PLUGIN_CONTEXT {

	int			 plugin_index		 = -1;
	PVOID		 pServer;
	DWORD		 dwConnID;
	PVOID		 pFnServerFunc		 = nullptr;
	PVOID		 pFnGetEnvVar		 = nullptr;
	PVOID		 pFnRead			 = nullptr;
	PVOID		 pFnWrite			 = nullptr;

	// ����Ϊ plume_server �����ṹ������Ϊ˽�нṹ

	CStringPlus			headerBuffer;
	bool				headerHasLength	 = false;
	std::vector<BYTE>	outBuffer;
	DWORD				outBufferSize	 = 0;

} PLUGIN_CONTEXT;

typedef struct CONN_INFO {

	DWORD				  protocol = 0;					// Reserved now
	USHORT				  version = 0;
	int					  vhost_index = -1;
	RECV_BLOCK			  RECV_BLOCK;
	SEND_BLOCK			  SEND_BLOCK;
	PATH_BLOCK			  PATH_BLOCK;
	std::vector<REQ_MEM>  REQ_MEM;
	PLUGIN_CONTEXT		  PLUGIN_CONTEXT;

} CONN_INFO;

extern const char * APP_NAME;
extern const char * APP_VER;
extern const char * APP_FULL_NAME;
extern const char * APP_DESCRIPTION;
extern const char * APP_COPYRIGHT;
extern std::string APP_TOKEN;
extern char * APP_DIRECTORY;
extern HANDLE  VAR_IMPERSONATE_TOKEN;

extern DWORD VAR_MEM_ALLOCATION;								// ϵͳ�ڴ������С
extern std::string VAR_CONFIG_FILE;								// �����ļ�
extern DWORD VAR_MAX_CONNECTIONS;								// ���������
extern unsigned long long VAR_LIMIT_BODY;						// ���� POST ��С
extern DWORD VAR_PLUGIN_BUFFER_SIZE;							// ���ϵͳ��̬ҳ�����������С
extern bool VAR_ENABLE_ACCESS_LOG;								// �Ƿ��¼������־
extern bool VAR_ENABLE_GZIP;									// �Ƿ�����GZIP
extern bool VAR_ENABLE_DIR_LISTING;								// �Ƿ�����Ŀ¼�б�
extern std::wstring VAR_LOG_DIRECTORY_PREFIX;					// ��־Ŀ¼ǰ׺
extern std::string VAR_ADMIN_EMAIL;								// ����Ա����
extern std::string VAR_UPLOAD_TEMP_DIR_PREFIX;					// �ϴ���ʱ���Ŀ¼
extern std::string VAR_GZIP_CACHE_TEMP_DIR_PREFIX;				// GZIP�����ļ�Ŀ¼

extern std::vector<MIME_TYPE>		VAR_MIME_TYPES;				// ȫ��MIME����
extern std::vector<SERVER>			VAR_SERVERS;				// ȫ�ַ���˱�
extern std::vector<VIRTUAL_HOST>	VAR_VIRTUAL_HOSTS;			// ȫ������������
extern std::vector<HANDLER_MAP>		VAR_HANDLER_MAPS;			// ȫ�ִ���ӳ���
extern std::vector<SSL_CERT>		VAR_SSL_CERTS;				// ȫ��SSL֤���
extern std::vector<SNI_MAP>			VAR_SNI_MAPS;				// ȫ��SNIӳ���


#define		VAR_SYS_LOG			_NLOG_APP_WITH_ID("__SYS_WORKER_")		// ȫ��LOG����
#define		VAR_SYS_LOG_DAEMON	_NLOG_APP_WITH_ID("__SYS_DAEMON_")		// ȫ��SERVICE LOG����