#include <iostream>
#include <time.h>

//1.包含头文件
#include "../include/nlog.h"

//2.连接动态库的lib文件, 注意在项目设置中已将附加库目录设置为"$(SolutionDir)bin\msvc10\lib\"
#ifdef _DEBUG
#pragma comment(lib, "nlogd.lib")
#else
#pragma comment(lib, "nlog.lib")
#endif

/*
*	线程函数, 用于打印日志消息
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
    // 并发运行线程数
    const unsigned int threadCount = 4;

    // 计时
    time_t start = ::clock();
    std::cout << "多线程并发写40w条日志..." << std::endl;

    HANDLE threadHandle[threadCount] = {0};
    for (int i = 0; i < threadCount; ++i) {
        threadHandle[i] = ::CreateThread(NULL, 0, ThreadProc, (LPVOID)i, 0, NULL);
    }

    // 等待线程终止
    ::WaitForMultipleObjects(threadCount, threadHandle, true, INFINITE);

    _NLOG_SHUTDOWN();

    std::cout << "运行时间：" << (double(clock() - start)/CLOCKS_PER_SEC) << std::endl;
    system("pause");
    return 0;
}