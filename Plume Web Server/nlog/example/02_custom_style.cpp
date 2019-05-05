#include <iostream>

//1.连接静态库需要在包含头文件前定义 NLOG_STATIC_LIB, 否则会出现连接错误
#define NLOG_STATIC_LIB

//2.包含头文件
#include "../include/nlog.h"

//3.连接静态库的lib文件
#ifdef _DEBUG
#pragma comment(lib, "nloglibd.lib")
#else
#pragma comment(lib, "nloglib.lib")
#endif

//4.定义log管理器
struct _log_mgr 
{
    /*
    *	构造的时候自动设置nlog的打印配置
    */
    _log_mgr() 
    {
        /*
        *	这里填空的话, 代表使用默认值
        */
        _NLOG_CFG cfg = {
            L"{module_dir}\\日志",                      //日志存储目录    默认是: "{module_dir}\\log\\"
            L"custom_style_%m%d.log",                   //文件名格式      默认是: "log-%m%d-%H%M.log"
            L"%Y-%m-%d",                                //日期格式        默认是: "%m-%d %H:%M:%S"
            L"[{time}][{level}][{id}][{file}:{line}]: " //前缀格式        默认是: "[{time}][{level}][{id}]: "
        };

        _NLOG_SET_CONFIG(cfg);
    }

    /*
    *	析构的时候自行关闭nlog
    */
    ~_log_mgr() {
        _NLOG_SHUTDOWN();
    }
};

_log_mgr __logmgr;

int main()
{
    _NLOG_APP("Hello %s!", "World");                                        //打印C风格    多字节

    _NLOG_WAR() << nlog::time   << L" 从此刻起我们的征途便是星辰大海...";   //打印C++风格  宽字节

    _NLOG_ERR("Oh, %s! ", "No") << L"有人动了你的代码并在里面下了毒...";    //混搭式打印

    return 0;
}