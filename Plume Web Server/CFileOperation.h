#pragma once

/*
	1�����룺�������ݡ����ļ���������ʧ�ܣ�
	2��д����д�����ݡ����ļ���������ʧ�ܣ�
	3����д����д���ݡ����ļ���������ʧ�ܣ�
	4����д��д�����ݡ����ļ����������ȴ���һ�����ļ�������Ѿ����ھ���������е��������ݣ�
	5����д��д�����ݡ����ļ��������򴴽�һ�����ļ�������Ѿ����ھ�ֱ�Ӵ򿪣�
	6���Ķ�����д���ݡ����ļ��������򴴽�һ�����ļ�������Ѿ����ھ�ֱ�Ӵ򿪡�
*/

#define CFO_OPT_READ 1		// ����
#define CFO_OPT_WRITE 2		// д��
#define CFO_OPT_RW 3		// ��д
#define CFO_OPT_OVERRIDE 4	// ��д�����ǣ�
#define CFO_OPT_MODIFY 5	// ��д
#define CFO_OPT_READ_MODIFY 6 // �Ķ�

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
	~CFileOperation() {}; // ��Ҫ�ֹ��� Close() �ͷ���Դ
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
