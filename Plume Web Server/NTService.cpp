#include "header.h"
#include "NTService.h"
#include "general.h"

SERVICE_STATUS				NTSERVICE_STATUS			=	{ 0 };
SERVICE_STATUS_HANDLE		NTSERVICE_STATUS_HANDLE		=	{ 0 };
extern void cleanup();	// main.cpp

bool NTService_Install()
{
	char *tmp = (char *)malloc(MAX_BUFF);
	::GetModuleFileNameA(NULL, tmp, MAX_BUFF);
	CStringPlus svc_cmd(tmp);
	svc_cmd += " /service";
	free(tmp);

	SC_HANDLE hSC = ::OpenSCManagerA("", "ServicesActive", SC_MANAGER_CREATE_SERVICE);

	if (hSC)
	{
		SC_HANDLE hService = ::CreateServiceA(hSC, APP_NAME, APP_FULL_NAME, SERVICE_ALL_ACCESS,
					SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, 
					SERVICE_ERROR_NORMAL, svc_cmd.c_str(), NULL, NULL, NULL, NULL, NULL);
		if (hService)
		{
			::CloseServiceHandle(hService);
			::CloseServiceHandle(hSC);
			return NTService_ChangeDescription();
		}
		::CloseServiceHandle(hSC);
		return false;
	}
	return false;
}

bool NTService_Remove()
{
	SC_HANDLE hSC = ::OpenSCManagerA("", "ServicesActive", SC_MANAGER_ALL_ACCESS);

	if (hSC)
	{
		SC_HANDLE hService = ::OpenServiceA(hSC, APP_NAME, SERVICE_ALL_ACCESS);
		if (hService)
		{
			SERVICE_STATUS svc_status;
			memset(&svc_status, 0, sizeof(SERVICE_STATUS));
			::ControlService(hService, SERVICE_CONTROL_STOP, &svc_status);
			BOOL ret = ::DeleteService(hService);
			::CloseServiceHandle(hService);
			::CloseServiceHandle(hSC);
			return (ret == TRUE);
		}
		::CloseServiceHandle(hSC);
		return false;
	}
	return false;
}

bool NTService_ChangeDescription()
{
	SC_HANDLE hSC = ::OpenSCManagerA("", "ServicesActive", SC_MANAGER_ALL_ACCESS);

	if (hSC)
	{
		SC_HANDLE hService = ::OpenServiceA(hSC, APP_NAME, SERVICE_ALL_ACCESS);
		if (hService)
		{
			SERVICE_DESCRIPTIONA svc_desc;

			memset(&svc_desc, 0, sizeof(SERVICE_DESCRIPTIONA));
			char *tmp = (char *)malloc(strlen(APP_DESCRIPTION) + 1);
			strcpy(tmp, APP_DESCRIPTION);
			svc_desc.lpDescription = tmp;

			::LockServiceDatabase(hService);
			BOOL ret = ::ChangeServiceConfig2A(hService, SERVICE_CONFIG_DESCRIPTION, &svc_desc);
			::CloseServiceHandle(hService);
			::CloseServiceHandle(hSC);

			free(tmp);
			return (ret == TRUE);
		}
		::CloseServiceHandle(hSC);
		return false;
	}
	return false;
}

DWORD NTService_GetServiceStatus()
{
	SC_HANDLE hSC = ::OpenSCManagerA("", "ServicesActive", SC_MANAGER_CREATE_SERVICE);

	SERVICE_STATUS svc_status;
	memset(&svc_status, 0, sizeof(SERVICE_STATUS));

	if (hSC)
	{
		SC_HANDLE hService = ::OpenServiceA(hSC, APP_NAME, SERVICE_ALL_ACCESS);
		if (hService)
		{
			::QueryServiceStatus(hService, &svc_status);
			::CloseServiceHandle(hService);
		}
		::CloseServiceHandle(hSC);
	}

	return svc_status.dwCurrentState;
}

void NTService_initialize()
{
	NTSERVICE_STATUS.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
	NTSERVICE_STATUS.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;
	NTSERVICE_STATUS.dwCheckPoint = 0;
	NTSERVICE_STATUS.dwWaitHint = 0;
	char *tmp = (char *)malloc(strlen(APP_NAME) + 1);
	strcpy(tmp, APP_NAME);
	SERVICE_TABLE_ENTRYA dispatchTable[] = { {tmp, (LPSERVICE_MAIN_FUNCTIONA)ServiceMain}, {NULL, NULL} };
	::StartServiceCtrlDispatcherA(dispatchTable);
	free(tmp);
}

void WINAPI ServiceMain(DWORD dwArgc, LPCSTR lpszArgv[])
{
	NTSERVICE_STATUS_HANDLE = ::RegisterServiceCtrlHandlerA(APP_NAME, ServiceCtrl);
	VAR_SYS_LOG_DAEMON("Initializing service...");
	HANDLE hJob = ::CreateJobObjectA(NULL, NULL);
	if (!hJob)
	{
		VAR_SYS_LOG_DAEMON("Error creating job object, OS Error(%d): %s", ::GetLastError(), GetLastErrorMessage());
		NTSERVICE_STATUS.dwServiceSpecificExitCode = APP_ERR_SPAWN_WORKER;
		NTSERVICE_STATUS.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		NTSERVICE_STATUS.dwCurrentState = SERVICE_STOPPED;
		::SetServiceStatus(NTSERVICE_STATUS_HANDLE, &NTSERVICE_STATUS);
		return;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
	memset(&jeli, 0, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

	::QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION), 0);
	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	::SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

	bool							flag	=		false;
	SECURITY_ATTRIBUTES				sa;
	STARTUPINFOA					si;
	PROCESS_INFORMATION				pi;
	memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
	memset(&si, 0, sizeof(STARTUPINFO));
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));

	char *tmp = (char *)malloc(MAX_BUFF);
	::GetModuleFileNameA(NULL, tmp, MAX_BUFF);
	CStringPlus svc_cmd(tmp);
	svc_cmd += " /worker";
	free(tmp);

	if (NTSERVICE_STATUS_HANDLE)
	{
		DWORD exitCode = 0;
		while (true) {

			NTSERVICE_STATUS.dwWin32ExitCode = 0;
			NTSERVICE_STATUS.dwServiceSpecificExitCode = 0;
			NTSERVICE_STATUS.dwCurrentState = SERVICE_RUNNING;

			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = NULL;
			sa.bInheritHandle = TRUE;

			si.cb = sizeof(STARTUPINFO);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.wShowWindow = SW_HIDE;

			if (!::CreateProcessA(NULL, (LPSTR)svc_cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, NULL, APP_DIRECTORY, &si, &pi))
			{
				VAR_SYS_LOG_DAEMON("Error creating worker, OS Error(%d): %s", ::GetLastError(), GetLastErrorMessage());

				NTSERVICE_STATUS.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
				NTSERVICE_STATUS.dwServiceSpecificExitCode = APP_ERR_SPAWN_WORKER;
				NTSERVICE_STATUS.dwCurrentState = SERVICE_STOPPED;
				
				break;
			}
			if (!::AssignProcessToJobObject(hJob, pi.hProcess))
			{
				VAR_SYS_LOG_DAEMON("Error assigning job, OS Error(%d): %s", ::GetLastError(), GetLastErrorMessage());

				::TerminateProcess(pi.hProcess, APP_ERR_KILL);
				::CloseHandle(pi.hThread);
				::CloseHandle(pi.hProcess);

				NTSERVICE_STATUS.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
				NTSERVICE_STATUS.dwServiceSpecificExitCode = APP_ERR_KILL;
				NTSERVICE_STATUS.dwCurrentState = SERVICE_STOPPED;
				
				break;
			}
			if (::ResumeThread(pi.hThread) == -1)
			{
				VAR_SYS_LOG_DAEMON("Error resuming thread, OS Error(%d): %s", ::GetLastError(), GetLastErrorMessage());

				::TerminateProcess(pi.hProcess, APP_ERR_KILL);
				::CloseHandle(pi.hThread);
				::CloseHandle(pi.hProcess);

				NTSERVICE_STATUS.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
				NTSERVICE_STATUS.dwServiceSpecificExitCode = APP_ERR_KILL;
				NTSERVICE_STATUS.dwCurrentState = SERVICE_STOPPED;
				
				break;
			}

			::SetServiceStatus(NTSERVICE_STATUS_HANDLE, &NTSERVICE_STATUS);

			if (flag) {

				VAR_SYS_LOG_DAEMON("Worker crashed(0x%02x). Restarting worker process... PID: %d", exitCode, pi.dwProcessId);

			} else {

				VAR_SYS_LOG_DAEMON("Initializing worker process... PID: %d", pi.dwProcessId);
			}

			flag = true;

			::WaitForSingleObject(pi.hProcess, INFINITE);
			::GetExitCodeProcess(pi.hProcess, &exitCode);
			::CloseHandle(pi.hThread);
			::CloseHandle(pi.hProcess);

			if (exitCode == APP_ERR_KILL || exitCode == APP_ERR_CONFIG || exitCode == APP_ERR_SPAWN_WORKER || exitCode == APP_ERR_CREATE_SERVER)
			{
				VAR_SYS_LOG_DAEMON("Unable to restart worker. Terminated. Exit code: 0x%02x", exitCode);

				NTSERVICE_STATUS.dwServiceSpecificExitCode = exitCode;
				NTSERVICE_STATUS.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
				NTSERVICE_STATUS.dwCurrentState = SERVICE_STOPPED;
				
				break;
			}
		}

		::CloseHandle(hJob);
		::SetServiceStatus(NTSERVICE_STATUS_HANDLE, &NTSERVICE_STATUS);

	} else {

		NTSERVICE_STATUS.dwCurrentState = SERVICE_START_PENDING;
		::SetServiceStatus(NTSERVICE_STATUS_HANDLE, &NTSERVICE_STATUS);
	}
}

void WINAPI ServiceCtrl(DWORD dwCtrlCode)
{
	if (dwCtrlCode == SERVICE_CONTROL_SHUTDOWN || dwCtrlCode == SERVICE_CONTROL_STOP)
	{
		if (dwCtrlCode == SERVICE_CONTROL_SHUTDOWN) {
			VAR_SYS_LOG_DAEMON("Received SERVICE_CONTROL_SHUTDOWN, shutting down...");
		} else {
			VAR_SYS_LOG_DAEMON("Received SERVICE_CONTROL_STOP, shutting down...");
		}

		NTSERVICE_STATUS.dwCurrentState = SERVICE_PAUSE_PENDING;
		::SetServiceStatus(NTSERVICE_STATUS_HANDLE, &NTSERVICE_STATUS);

		cleanup();	// main.cpp 中的全局清理方法

		NTSERVICE_STATUS.dwCurrentState = SERVICE_STOPPED;
	}

	::SetServiceStatus(NTSERVICE_STATUS_HANDLE, &NTSERVICE_STATUS);
}
