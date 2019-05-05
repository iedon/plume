#include <iostream>
#include <time.h>

//1.����ͷ�ļ�
#include "../include/nlog.h"

//2.���Ӷ�̬���lib�ļ�, ע������Ŀ�������ѽ����ӿ�Ŀ¼����Ϊ"$(SolutionDir)bin\msvc10\lib\"
#ifdef _DEBUG
#pragma comment(lib, "nlogd.lib")
#else
#pragma comment(lib, "nlog.lib")
#endif

/*
*	�̺߳���, ���ڴ�ӡ��־��Ϣ
*/
DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
    for (int i = 1; i <= 100000; ++ i) {
        _NLOG_APP(L"log infomation number: %-d ", i);
    }

    return 0L;
}

int main()
{
    // ���������߳���
    const unsigned int threadCount = 4;

    // ��ʱ
    time_t start = ::clock();
    std::cout << "���̲߳���д40w����־..." << std::endl;

    HANDLE threadHandle[threadCount] = {0};
    for (int i = 0; i < threadCount; ++i) {
        threadHandle[i] = ::CreateThread(NULL, 0, ThreadProc, (LPVOID)i, 0, NULL);
    }

    // �ȴ��߳���ֹ
    ::WaitForMultipleObjects(threadCount, threadHandle, true, INFINITE);

    _NLOG_SHUTDOWN();

    std::cout << "����ʱ�䣺" << (double(clock() - start)/CLOCKS_PER_SEC) << std::endl;
    system("pause");
    return 0;
}