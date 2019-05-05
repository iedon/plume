#pragma once

void Begin_Request(HP_HttpServer pServer, HP_CONNID dwConnID);
void Done_Request(HP_HttpServer pServer, HP_CONNID dwConnID, bool CloseConnectionAnyway = false);
bool Send_Response(HP_HttpServer pServer, HP_CONNID dwConnID, USHORT usStatusCode, const std::vector<HP_THeader> & Headers = std::vector<HP_THeader>(), const CStringPlus & StatusText = "", bool CloseConnectionAnyway = false, bool UseHPSendFileFunction = false, const CStringPlus & FileAddr = "");
void Send_HttpErrorPage(HP_HttpServer pServer, HP_CONNID dwConnID, USHORT usStatusCode, bool CloseConnectionAnyway = false, const char *custom_err_mg = nullptr);
PVOID Alloc_Request_Mem(HP_HttpServer pServer, HP_CONNID dwConnID, size_t memsize);
bool Free_Request_Mem(HP_HttpServer pServer, HP_CONNID dwConnID, PVOID pAddress);
