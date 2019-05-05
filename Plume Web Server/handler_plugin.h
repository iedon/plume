#pragma once

//////////////////////////////////////////////////////////////// DEFINES FOR PLAPI //////////////////////////////////////////////////////////////////

#define PLAPI __stdcall

#define PLUME_EVENT_HANDLE_REQUEST			0
#define PLUME_EVENT_BEGIN_CONN				1
#define PLUME_EVENT_DONE_CONN				2
#define PLUME_EVENT_BEGIN_REQUEST			3
#define PLUME_EVENT_DONE_REQUEST			4
#define PLUME_EVENT_ONSEND					5
#define PLUME_EVENT_ONBODY					6

#define PLUME_EVENT_RESULT_STOP				0
#define PLUME_EVENT_RESULT_CONTINUE			1
#define PLUME_EVENT_RESULT_DISCONNECT		2

#define PLUME_ACTION_REGISTER_EVENT			0
#define PLUME_ACTION_LOG					1
#define PLUME_ACTION_SEND_ERROR_PAGE		2
#define PLUME_ACTION_READ_CONFIG			3
#define PLUME_ACTION_SEND_HEADER			4
#define PLUME_ACTION_DONE_REQUEST			5
#define PLUME_ACTION_GET_CONNECTION_COUNTS	6
#define PLUME_ACTION_ALLOC_REQUEST_MEM		7
#define PLUME_ACTION_FREE_REQUEST_MEM		8
#define PLUME_ACTION_REGISTER_ASYNC_READ	9
#define PLUME_ACTION_GET_IMPERSONATE_TOKEN  10
#define PLUME_ACTION_REWRITE_VARIABLE		11

#define PLUME_PROC_RESULT_FAILURE			0
#define PLUME_PROC_RESULT_SUCCESS			1

#define PLUME_ASYNC_READ_STOP				0
#define PLUME_ASYNC_READ_CONTINUE			1

#define PLUME_REWRITE_PATH					0
#define PLUME_REWRITE_PATH_TRANSLATED		1
#define PLUME_REWRITE_QUERY_STRING			2
#define PLUME_REWRITE_DOCUMENT_ROOT			3
#define PLUME_REWRITE_SCRIPT_NAME			4

#define QWORD unsigned long long

#define PLAPI_SERVERFUNC bool(PLAPI *pFnServerFunc)	(plume_server *s, USHORT usAction, USHORT usParam, void **lpvBuffer, DWORD dwLength)
#define PLAPI_GETENVVAR  bool(PLAPI *pFnGetEnvVar)	(plume_server *s, const char *lpszVarName, void *lpvBuffer, DWORD *dwLength)
#define PLAPI_READ		 bool(PLAPI *pFnRead)		(plume_server *s, void *lpvBuffer, DWORD dwBytesToRead, DWORD *dwBytesRead)
#define PLAPI_WRITE		 bool(PLAPI *pFnWrite)		(plume_server *s, const void *lpvBuffer, int iLength)

typedef struct plume_server {

	int	plugin_index	  = -1;
	HP_HttpServer pServer = nullptr;
	DWORD dwConnID		  = 0;
	PLAPI_SERVERFUNC;
	PLAPI_GETENVVAR;
	PLAPI_READ;
	PLAPI_WRITE;

} plume_server;

typedef  PLAPI_SERVERFUNC;
typedef  PLAPI_GETENVVAR;
typedef  PLAPI_READ;
typedef  PLAPI_WRITE;

typedef  void(PLAPI *pFnGetPluginInfo)  (char **lpszPluginName, char **lpszVersion, char **lpszAuthor, char **lpszDescription);
typedef  bool(PLAPI *pFnInitPlugin)		(const char *lpszVersion, plume_server *s);
typedef  int(PLAPI *pFnPluginProc)		(plume_server *s, USHORT usEventType, void *lpvBuffer, DWORD dwLength);
typedef  void(PLAPI *pFnFreePlugin)		(void);
typedef	 int(PLAPI *pFnAsyncRead)		(plume_server *s, void *lpvBuffer, DWORD dwLength, void *plugin_async_data);

typedef struct plume_readconfig_struct {

	char* lpszSectionName;
	char* lpszKeyName;
	char* lpszValue;

} plume_readconfig_struct;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct PLUGIN {

	CStringPlus	plugin_name;
	pFnInitPlugin fnInitPlugin		 = nullptr;
	pFnGetPluginInfo fnGetPluginInfo = nullptr;
	pFnPluginProc fnPluginProc		 = nullptr;
	pFnFreePlugin fnFreePlugin		 = nullptr;
	pFnAsyncRead async_read_callback = nullptr;

} PLUGIN;

typedef struct PLUGIN_EVENT {

	int		plugin_index	= -1;
	USHORT	event_type		= PLUME_EVENT_HANDLE_REQUEST;

} PLUGIN_EVENT;

extern std::vector<PLUGIN>			VAR_PLUGINS;				// 全局已启用插件列表
extern std::vector<PLUGIN_EVENT>	VAR_PLUGIN_EVENTS;			// 全局已注册插件事件

void Load_Plugins();
bool PluginSendHeader(plume_server *s, CStringPlus & headerBuffer);
bool PLAPI Write(plume_server *s, const void *lpvBuffer, int iLength);
bool PLAPI Read(plume_server *s, void *lpvBuffer, DWORD dwBytesToRead, DWORD *dwBytesRead);
bool PLAPI ServerFunc(plume_server *s, USHORT usAction, USHORT usParam, void **lpvBuffer, DWORD dwLength);
bool PLAPI GetEnvVar(plume_server *s, const char *lpszVarName, void *lpvBuffer, DWORD *dwLength);
