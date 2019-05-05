#pragma once

EnHandleResult __HP_CALL OnAccept(HP_Server pServer, HP_CONNID dwConnID, SOCKET soClient);
EnHandleResult __HP_CALL OnHandShake(HP_Server pServer, HP_CONNID dwConnID);
EnHandleResult __HP_CALL OnReceive(HP_Server pServer, HP_CONNID dwConnID, const BYTE* pData, int iLength);
EnHandleResult __HP_CALL OnSend(HP_Server pServer, HP_CONNID dwConnID, const BYTE* pData, int iLength);
EnHandleResult __HP_CALL OnClose(HP_Server pServer, HP_CONNID dwConnID, EnSocketOperation enOperation, int iErrorCode);
EnHandleResult __HP_CALL OnShutdown(HP_Server pServer);
EnHttpParseResult __HP_CALL OnMessageBegin(HP_HttpServer pServer, HP_CONNID dwConnID);
EnHttpParseResult __HP_CALL OnRequestLine(HP_HttpServer pServer, HP_CONNID dwConnID, LPCSTR lpszMethod, LPCSTR lpszUrl);
EnHttpParseResult __HP_CALL OnHeader(HP_HttpServer pServer, HP_CONNID dwConnID, LPCSTR lpszName, LPCSTR lpszValue);
EnHttpParseResult __HP_CALL OnHeadersComplete(HP_HttpServer pServer, HP_CONNID dwConnID);
EnHttpParseResult __HP_CALL OnBody(HP_HttpServer pServer, HP_CONNID dwConnID, const BYTE* pData, int iLength);
EnHttpParseResult __HP_CALL OnChunkHeader(HP_HttpServer pServer, HP_CONNID dwConnID, int iLength);
EnHttpParseResult __HP_CALL OnChunkComplete(HP_HttpServer pServer, HP_CONNID dwConnID);
EnHttpParseResult __HP_CALL OnMessageComplete(HP_HttpServer pServer, HP_CONNID dwConnID);
EnHttpParseResult __HP_CALL OnUpgrade(HP_HttpServer pServer, HP_CONNID dwConnID, EnHttpUpgradeType enUpgradeType);
EnHttpParseResult __HP_CALL OnParseError(HP_HttpServer pServer, HP_CONNID dwConnID, int iErrorCode, LPCSTR lpszErrorDesc);
int __HP_CALL OnSNICallback(LPCTSTR lpszServerName);
