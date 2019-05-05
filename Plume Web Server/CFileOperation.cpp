#include "header.h"
#include "CFileOperation.h"

CFileOperation::CFileOperation(HANDLE hBindFile)
{
	this->hFile = hBindFile;
	::GetFileSizeEx(hBindFile, (PLARGE_INTEGER)&this->FileLen);
}

CFileOperation::CFileOperation(LPCSTR lpFileName, DWORD dwOpenMode, DWORD dwShareMode, bool bIsDirectory)
{
	Open(lpFileName, dwOpenMode, dwShareMode, bIsDirectory);
}

void CFileOperation::Close()
{
	if (this->hFile != INVALID_HANDLE_VALUE) {
		::CloseHandle(this->hFile);
		this->hFile = INVALID_HANDLE_VALUE;
		this->FileLen = 0;
	}
}

HANDLE CFileOperation::Open(LPCSTR lpFileName, DWORD dwOpenMode, DWORD dwShareMode, bool bIsDirectory)
{
	DWORD dwInternalOpenMode;
	DWORD dwInternalShareMode;

	switch (dwOpenMode) {
		case CFO_OPT_READ:
			dwInternalOpenMode = GENERIC_READ;
			break;
		case CFO_OPT_WRITE:
			dwInternalOpenMode = GENERIC_WRITE;
			break;
		case CFO_OPT_RW:
			dwInternalOpenMode = GENERIC_READ | GENERIC_WRITE;
			break;
		case CFO_OPT_OVERRIDE:
			dwInternalOpenMode = GENERIC_READ | GENERIC_WRITE;
			break;
		case CFO_OPT_MODIFY:
			dwInternalOpenMode = GENERIC_WRITE;
			break;
		case CFO_OPT_READ_MODIFY:
			dwInternalOpenMode = GENERIC_READ | GENERIC_WRITE;
			break;
		default:
			dwInternalOpenMode = GENERIC_READ;
			break;
	}

	switch (dwShareMode) {
		case CFO_SHARE_ALL:
			dwInternalShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			break;
		case CFO_SHARE_WRITE:
			dwInternalShareMode = FILE_SHARE_WRITE;
			break;
		case CFO_SHARE_READ:
			dwInternalShareMode = FILE_SHARE_READ;
			break;
		case CFO_SHARE_DENY:
			dwInternalShareMode = NULL;
			break;
		default:
			dwInternalShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			break;
	}

	this->hFile = ::CreateFileA(lpFileName, dwInternalOpenMode, dwInternalShareMode, NULL, dwOpenMode == CFO_OPT_OVERRIDE ? CREATE_ALWAYS : (dwOpenMode < CFO_OPT_OVERRIDE ? OPEN_EXISTING : OPEN_ALWAYS), bIsDirectory ? FILE_FLAG_BACKUP_SEMANTICS /* 文件夹 */ : FILE_FLAG_SEQUENTIAL_SCAN /* 普通文件，顺序扫描 */, NULL);
	if(this->hFile != INVALID_HANDLE_VALUE) ::GetFileSizeEx(this->hFile, (PLARGE_INTEGER)&this->FileLen);
	return this->hFile;
}

HANDLE CFileOperation::GetFileHandle()
{
	return this->hFile;
}

long long CFileOperation::MoveFilePointer(DWORD where, long long distance)
{
	long long NewFilePointer = 0;
	// MSDN: If your compiler has built-in support for 64-bit integers, use the QuadPart member to store the 64-bit integer. 
	LARGE_INTEGER li = { 0 };
	li.QuadPart = distance;

	::SetFilePointerEx(this->hFile, li, (PLARGE_INTEGER)&NewFilePointer, where);
	return NewFilePointer;
}

long long CFileOperation::GetFilePointer()
{
	return MoveFilePointer(CFO_FILE_CURRENT, 0);
}

void CFileOperation::MoveToBegin()
{
	MoveFilePointer(CFO_FILE_BEGIN, 0);
}

void CFileOperation::MoveToEnd()
{
	MoveFilePointer(CFO_FILE_END, 0);
}

// 这个方法提供类中私有成员中存储的文件大小。
// 如果文件是读操作的话，不会改变大小，则可以使用此方法，避免频繁API调用。
long long CFileOperation::GetStockFileLength()
{
	return this->FileLen;
}

long long CFileOperation::GetRealFileLength()
{
	::GetFileSizeEx(this->hFile, (PLARGE_INTEGER)&this->FileLen);
	return this->FileLen;
}

bool CFileOperation::IsFileEnd()
{
	return (GetFilePointer() >= GetRealFileLength());
}

BOOL CFileOperation::Read(DWORD dwLength, PVOID lpvBuffer)
{
	DWORD Read = 0;
	return ::ReadFile(this->hFile, lpvBuffer, dwLength, &Read, NULL);
}

BOOL CFileOperation::Write(DWORD dwLength, PVOID lpvBuffer)
{
	DWORD Written = 0;
	BOOL ret = ::WriteFile(this->hFile, lpvBuffer, dwLength, &Written, NULL);

	return ret && (Written != 0);
}

BOOL CFileOperation::WriteLine(LPCSTR lpszMessage)
{
	const char endline[] = { '\r', '\n' };
	BOOL ret = Write((DWORD)strlen(lpszMessage), (PVOID)lpszMessage);
	return ret && Write(sizeof(endline), (PVOID)endline);
}

BOOL CFileOperation::GetFileDate(DWORD dwType, LPSYSTEMTIME lpTime)
{
	FILETIME create = { 0 };
	FILETIME access = { 0 };
	FILETIME modify = { 0 };
	return (
			::GetFileTime(this->hFile, &create, &access, &modify) &&
			::FileTimeToSystemTime((const LPFILETIME)(dwType == CFO_FILE_TIME_CREATE ? &create : (dwType == CFO_FILE_TIME_ACCESS ? &access : &modify)), lpTime)
		   );
}
