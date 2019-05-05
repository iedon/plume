#pragma once

/*
	1、读入：读入数据。若文件不存在则失败；
	2、写出：写出数据。若文件不存在则失败；
	3、读写：读写数据。若文件不存在则失败；
	4、重写：写出数据。若文件不存在则先创建一个新文件，如果已经存在就先清除其中的所有数据；
	5、改写：写出数据。若文件不存在则创建一个新文件，如果已经存在就直接打开；
	6、改读：读写数据。若文件不存在则创建一个新文件，如果已经存在就直接打开。
*/

#define CFO_OPT_READ 1		// 读入
#define CFO_OPT_WRITE 2		// 写出
#define CFO_OPT_RW 3		// 读写
#define CFO_OPT_OVERRIDE 4	// 重写（覆盖）
#define CFO_OPT_MODIFY 5	// 改写
#define CFO_OPT_READ_MODIFY 6 // 改读

#define CFO_SHARE_ALL 1
#define CFO_SHARE_WRITE 2
#define CFO_SHARE_READ 3
#define CFO_SHARE_DENY 4

#define CFO_FILE_BEGIN FILE_BEGIN
#define CFO_FILE_END FILE_END
#define CFO_FILE_CURRENT FILE_CURRENT

#define CFO_FILE_TIME_CREATE 1
#define CFO_FILE_TIME_ACCESS 2
#define CFO_FILE_TIME_MODIFY 3

class CFileOperation {

private:
	HANDLE hFile = INVALID_HANDLE_VALUE;
	long long FileLen = 0;

public:
	CFileOperation() {};
	~CFileOperation() {}; // 需要手工用 Close() 释放资源
	CFileOperation(HANDLE hBindFile);
	CFileOperation(LPCSTR lpFileName, DWORD dwOpenMode = CFO_OPT_READ, DWORD dwShareMode = CFO_SHARE_ALL, bool bIsDirectory = false);
	HANDLE Open(LPCSTR lpFileName, DWORD dwOpenMode = CFO_OPT_READ, DWORD dwShareMode = CFO_SHARE_ALL, bool bIsDirectory = false);
	void Close();
	HANDLE GetFileHandle();
	long long MoveFilePointer(DWORD where = FILE_CURRENT, long long distance = NULL);
	long long GetFilePointer();
	void MoveToBegin();
	void MoveToEnd();
	long long GetStockFileLength();
	long long GetRealFileLength();
	bool IsFileEnd();
	BOOL Read(DWORD dwLength, PVOID lpvBuffer);
	BOOL Write(DWORD dwLength, PVOID lpvBuffer);
	BOOL WriteLine(LPCSTR lpszMessage);
	BOOL GetFileDate(DWORD dwType, LPSYSTEMTIME lpTime);
};
