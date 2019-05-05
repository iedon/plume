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

//3.为了方便使用, 为每个实例定义一个实例ID, 和方便打印的宏
#define LOG_1_ID "log_1"
#define LOG_2_ID "log_2"
#define LOG_3_ID "log_3"
#define LOG_4_ID "log_4"

#define LOG_1_APP _NLOG_APP_WITH_ID( LOG_1_ID )
#define LOG_2_APP _NLOG_APP_WITH_ID( LOG_2_ID )
#define LOG_3_APP _NLOG_APP_WITH_ID( LOG_3_ID )
#define LOG_4_APP _NLOG_APP_WITH_ID( LOG_4_ID )

//4.定义log管理器, 主要是为了为每个实例设置不同的配置, 当然也可以使用默认设置
struct _log_mgr 
{
    _log_mgr() 
    {
        _NLOG_CFG cfg = {};

        cfg.logDir = L"{module_dir}\\实例1";
        _NLOG_SET_CONFIG_WITH_ID(LOG_1_ID, cfg);

        cfg.logDir = L"{module_dir}\\实例2";
        _NLOG_SET_CONFIG_WITH_ID(LOG_2_ID, cfg);

        cfg.logDir   = L"";
        cfg.fileName = L"实例3.log";
        _NLOG_SET_CONFIG_WITH_ID(LOG_3_ID, cfg);
    }

    ~_log_mgr() {
        _NLOG_SHUTDOWN();
    }
};

_log_mgr __log;

int main()
{
    LOG_1_APP("我是一号实例, 在目录 \"实例1\" 中...");
    LOG_2_APP("我是二号实例, 我跟一号混饭吃的...");

    LOG_3_APP("我是三号实例, 在默认目录中, 名字叫 \"实例3.log\" ...");
    LOG_4_APP("我是四号实例, 使用的默认设置...");

    return 0;
}