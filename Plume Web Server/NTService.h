#pragma once

extern		SERVICE_STATUS					NTSERVICE_STATUS;
extern		SERVICE_STATUS_HANDLE			NTSERVICE_STATUS_HANDLE;

bool NTService_Install();
bool NTService_Remove();
bool NTService_ChangeDescription();
DWORD NTService_GetServiceStatus();
void NTService_initialize();
void WINAPI ServiceMain(DWORD dwArgc, LPCSTR lpszArgv[]);
void WINAPI ServiceCtrl(DWORD dwCtrlCode);
