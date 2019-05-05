#include "header.h"
#include "netcallback.h"
#include "general.h"
#include "request.h"
#include "handler_staticfile.h"
#include "handler_plugin.h"

EnHandleResult __HP_CALL OnAccept(HP_Server pServer, HP_CONNID dwConnID, SOCKET soClient)
{
	CONN_INFO *ci = new CONN_INFO;

	if (!::HP_Server_SetConnectionExtra(pServer, dwConnID, ci)) {

		delete ci;
		return HR_ERROR;
	}

	ci->PLUGIN_CONTEXT.pServer  = pServer;
	ci->PLUGIN_CONTEXT.dwConnID = (DWORD)dwConnID;
	ci->PLUGIN_CONTEXT.pFnGetEnvVar = (PVOID)GetEnvVar;
	ci->PLUGIN_CONTEXT.pFnServerFunc = (PVOID)ServerFunc;
	ci->PLUGIN_CONTEXT.pFnRead = (PVOID)Read;
	ci->PLUGIN_CONTEXT.pFnWrite = (PVOID)Write;

	for (auto & pe : VAR_PLUGIN_EVENTS) {
	
		if (pe.event_type == PLUME_EVENT_BEGIN_CONN) {

			int ret = VAR_PLUGINS[pe.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_BEGIN_CONN, NULL, NULL);
			if (ret == PLUME_EVENT_RESULT_STOP) {
			
				::HP_Server_Disconnect(pServer, dwConnID, TRUE);
				return HR_OK;
			}
			if (ret == PLUME_EVENT_RESULT_DISCONNECT) {

				::HP_Server_Disconnect(pServer, dwConnID, TRUE);
				return HR_OK;
			}

		}
	}

	return HR_OK;
}

EnHandleResult __HP_CALL OnHandShake(HP_Server pServer, HP_CONNID dwConnID)
{
	return HR_OK;
}

EnHandleResult __HP_CALL OnReceive(HP_Server pServer, HP_CONNID dwConnID, const BYTE* pData, int iLength)
{
	return HR_OK;
}

EnHandleResult __HP_CALL OnSend(HP_Server pServer, HP_CONNID dwConnID, const BYTE* pData, int iLength)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return HR_OK;

	for (auto & pe : VAR_PLUGIN_EVENTS) {

		if (pe.event_type == PLUME_EVENT_ONSEND) {

			int ret = VAR_PLUGINS[pe.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_ONSEND, (void *)pData, iLength);
			if (ret == PLUME_EVENT_RESULT_STOP) {

				if (!ci->SEND_BLOCK.has_header_sent) ci->SEND_BLOCK.has_header_sent = true;
				return HR_OK;
			}
			if (ret == PLUME_EVENT_RESULT_DISCONNECT) {

				if (!ci->SEND_BLOCK.has_header_sent) ci->SEND_BLOCK.has_header_sent = true;
				::HP_HttpServer_Release(pServer, dwConnID);
				return HR_OK;
			}

		}
	}

	if (!ci->SEND_BLOCK.has_header_sent) {

		ci->SEND_BLOCK.has_header_sent = true;
		return HR_OK;
	}

	// ���ʹ�ô��Ϳ췢�ļ������²���
	if (ci->SEND_BLOCK.use_send_blk) {

		// ���ʹ���ˣ��������������⣬ֱ�ӷ��ء�
		if (ci->SEND_BLOCK.hFile == INVALID_HANDLE_VALUE || !ci->SEND_BLOCK.hFileMapping || !ci->SEND_BLOCK.pMapAddr) {

			return HR_OK;
		}
		
		// �Ѿ��ɹ����� iLength ��С�����ݣ�����ʣ�໺����С
		ci->SEND_BLOCK.BufferRemain -= (DWORD)iLength;

		// ����˻�����Ѿ�С�ڵ���0
		if ((long)ci->SEND_BLOCK.BufferRemain <= 0) {

			ci->SEND_BLOCK.BeginPointer += VAR_BUFFER;

			if (ci->SEND_BLOCK.pMapAddr) {

				// �ͷŴ˿��ڴ�ӳ��
				::UnmapViewOfFile(ci->SEND_BLOCK.pMapAddr);
				ci->SEND_BLOCK.pMapAddr = nullptr;
			}

			// ������������ļ������Ѵ���
			if (ci->SEND_BLOCK.BeginPointer >= ci->SEND_BLOCK.EndPointer) {

				// ����������ͷ�ӳ��(���������)���رվ��
				::CloseHandle(ci->SEND_BLOCK.hFileMapping);
				ci->SEND_BLOCK.hFileMapping = NULL;
				::CloseHandle(ci->SEND_BLOCK.hFile);
				ci->SEND_BLOCK.hFile = INVALID_HANDLE_VALUE;

				Done_Request(pServer, dwConnID);
				return HR_OK;
			}

			// Ͷ����һ�δ���
			PostFileTransfering(pServer, dwConnID);
			return HR_OK;
		}
		return HR_OK;
	}
	return HR_OK;
}

EnHandleResult __HP_CALL OnClose(HP_Server pServer, HP_CONNID dwConnID, EnSocketOperation enOperation, int iErrorCode)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return HR_OK;
	::HP_Server_SetConnectionExtra(pServer, dwConnID, nullptr);

	for (auto & pe : VAR_PLUGIN_EVENTS) {

		if (pe.event_type == PLUME_EVENT_DONE_CONN) {

			VAR_PLUGINS[pe.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_DONE_CONN, NULL, NULL);

		}
	}

	// �ر��ļ���ɾ����ʱ�ļ�
	if (ci->RECV_BLOCK.hUploadFile != INVALID_HANDLE_VALUE) {

		::CloseHandle(ci->RECV_BLOCK.hUploadFile);

		CStringPlus fn;
		fn.format("%x.tmp", ci);
		::DeleteFileA(CStringPlus(VAR_UPLOAD_TEMP_DIR_PREFIX).append(fn).c_str());
	}

	// ����ڴ�ӳ��飬�ر��ļ�ӳ�䣬�ر��ļ�
	if (ci->SEND_BLOCK.pMapAddr) {

		::UnmapViewOfFile(ci->SEND_BLOCK.pMapAddr);
	}
	if (ci->SEND_BLOCK.hFileMapping) {

		::CloseHandle(ci->SEND_BLOCK.hFileMapping);
	}
	if (ci->SEND_BLOCK.hFile != INVALID_HANDLE_VALUE) {

		::CloseHandle(ci->SEND_BLOCK.hFile);
	}

	// ���������ڴ�
	for (auto & mem : ci->REQ_MEM) {

		if (mem.pAddress != nullptr) {

			free(mem.pAddress);
			mem.pAddress = nullptr;
		}

	}

	delete ci;
	return HR_OK;
}

EnHandleResult __HP_CALL OnShutdown(HP_Server pServer)
{
	return HR_OK;
}

EnHttpParseResult __HP_CALL OnMessageBegin(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnRequestLine(HP_HttpServer pServer, HP_CONNID dwConnID, LPCSTR lpszMethod, LPCSTR lpszUrl)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return HPR_ERROR;

	ci->PATH_BLOCK.path = FilterPath(lpszUrl);

	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnHeader(HP_HttpServer pServer, HP_CONNID dwConnID, LPCSTR lpszName, LPCSTR lpszValue)
{
	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnHeadersComplete(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return HPR_ERROR;
	
	if (ci->version != NULL) return HPR_OK; // ˵����TCP�����Ѿ���һ���������ڽ��У����ӵڶ�����������

	USHORT peerHttpVer = ::HP_HttpServer_GetVersion(pServer, dwConnID);
	if (peerHttpVer != HV_1_1 && peerHttpVer != HV_1_0) return HPR_ERROR;
	::HP_HttpServer_SetLocalVersion(pServer, (EnHttpVersion)peerHttpVer);
	ci->version = peerHttpVer;

	ci->RECV_BLOCK.contentLength = ::HP_HttpServer_GetContentLength(pServer, dwConnID);
	if (ci->RECV_BLOCK.contentLength == (unsigned long long)-1) ci->RECV_BLOCK.contentLength = 0;

	// ��ʼ��������
	Begin_Request(pServer, dwConnID);
	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnBody(HP_HttpServer pServer, HP_CONNID dwConnID, const BYTE* pData, int iLength)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return HPR_ERROR;

	for (auto & pe : VAR_PLUGIN_EVENTS) {

		if (pe.event_type == PLUME_EVENT_ONBODY) {

			int ret = VAR_PLUGINS[pe.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_ONBODY, (void *)pData, iLength);
			if (ret == PLUME_EVENT_RESULT_STOP) {

				return HPR_OK;
			}

			if (ret == PLUME_EVENT_RESULT_DISCONNECT) {

				return HPR_ERROR;
			}

		}
	}

	if (ci->RECV_BLOCK.plugin_async_context && ci->PLUGIN_CONTEXT.plugin_index != -1) {

		VAR_PLUGINS[ci->PLUGIN_CONTEXT.plugin_index].async_read_callback((plume_server *)&ci->PLUGIN_CONTEXT, (void *)pData, (DWORD)iLength, (PVOID)ci->RECV_BLOCK.plugin_async_context);
		return HPR_OK;
	}

	if (ci->RECV_BLOCK.hUploadFile == INVALID_HANDLE_VALUE) {
		
		CStringPlus fn;
		fn.format("%x.tmp", ci);
		CFileOperation file(CStringPlus(VAR_UPLOAD_TEMP_DIR_PREFIX).append(fn).c_str(), CFO_OPT_READ_MODIFY, CFO_SHARE_ALL);
		if (file.GetFileHandle() == INVALID_HANDLE_VALUE) {

			Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR, false, "\xe8\xaf\xbb\xe5\x8f\x96\xe4\xb8\x8a\xe4\xbc\xa0\xe4\xbf\xa1\xe6\x81\xaf\xe6\x97\xb6\xe5\x8f\x91\xe7\x94\x9f\xe4\xba\x86\xe9\x94\x99\xe8\xaf\xaf\xe3\x80\x82");
			return HPR_OK;
		}

		ci->RECV_BLOCK.hUploadFile = file.GetFileHandle();
	}

	CFileOperation file(ci->RECV_BLOCK.hUploadFile);
	file.MoveToEnd();
	if (!file.Write(iLength, (PVOID)pData)) {

		Send_HttpErrorPage(pServer, dwConnID, HSC_INTERNAL_SERVER_ERROR, false, "\xe8\xaf\xbb\xe5\x8f\x96\xe4\xb8\x8a\xe4\xbc\xa0\xe4\xbf\xa1\xe6\x81\xaf\xe6\x97\xb6\xe5\x8f\x91\xe7\x94\x9f\xe4\xba\x86\xe9\x94\x99\xe8\xaf\xaf\xe3\x80\x82");
		return HPR_OK;
	}

	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnChunkHeader(HP_HttpServer pServer, HP_CONNID dwConnID, int iLength)
{
	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnChunkComplete(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnMessageComplete(HP_HttpServer pServer, HP_CONNID dwConnID)
{
	CONN_INFO *ci = nullptr;
	::HP_Server_GetConnectionExtra(pServer, dwConnID, (PVOID *)&ci);
	if (ci == nullptr) return HPR_ERROR;

	// ��������첽���գ����������Ѿ�������ϣ�����֪ͨ�����ʼ�����ˡ�
	if (!ci->RECV_BLOCK.plugin_async_context && ci->PLUGIN_CONTEXT.plugin_index != -1) {

		int ret = VAR_PLUGINS[ci->PLUGIN_CONTEXT.plugin_index].fnPluginProc((plume_server *)&ci->PLUGIN_CONTEXT, PLUME_EVENT_HANDLE_REQUEST, NULL, NULL);
		if (!ret) {

			Done_Request(pServer, dwConnID);
		}
	}

	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnUpgrade(HP_HttpServer pServer, HP_CONNID dwConnID, EnHttpUpgradeType enUpgradeType)
{
	return HPR_OK;
}

EnHttpParseResult __HP_CALL OnParseError(HP_HttpServer pServer, HP_CONNID dwConnID, int iErrorCode, LPCSTR lpszErrorDesc)
{
	return HPR_ERROR;
}

int __HP_CALL OnSNICallback(LPCTSTR lpszServerName)
{
	/*
		����ֵ��
		0		--		�ɹ���ʹ��Ĭ�� SSL ֤��
		����	--		�ɹ���ʹ�÷���ֵ��Ӧ�� SNI ����֤��
		����	--		ʧ�ܣ��ж� SSL ����

	*/

	for (auto & vsm : VAR_SNI_MAPS)
	{
		if (lpszServerName == vsm.authority) {
		
			return vsm.SNI_index;
		}
	}

	return 0;
}
