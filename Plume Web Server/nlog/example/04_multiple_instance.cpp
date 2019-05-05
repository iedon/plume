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

//3.Ϊ�˷���ʹ��, Ϊÿ��ʵ������һ��ʵ��ID, �ͷ����ӡ�ĺ�
#define LOG_1_ID "log_1"
#define LOG_2_ID "log_2"
#define LOG_3_ID "log_3"
#define LOG_4_ID "log_4"

#define LOG_1_APP _NLOG_APP_WITH_ID( LOG_1_ID )
#define LOG_2_APP _NLOG_APP_WITH_ID( LOG_2_ID )
#define LOG_3_APP _NLOG_APP_WITH_ID( LOG_3_ID )
#define LOG_4_APP _NLOG_APP_WITH_ID( LOG_4_ID )

//4.����log������, ��Ҫ��Ϊ��Ϊÿ��ʵ�����ò�ͬ������, ��ȻҲ����ʹ��Ĭ������
struct _log_mgr 
{
    _log_mgr() 
    {
        _NLOG_CFG cfg = {};

        cfg.logDir = L"{module_dir}\\ʵ��1";
        _NLOG_SET_CONFIG_WITH_ID(LOG_1_ID, cfg);

        cfg.logDir = L"{module_dir}\\ʵ��2";
        _NLOG_SET_CONFIG_WITH_ID(LOG_2_ID, cfg);

        cfg.logDir   = L"";
        cfg.fileName = L"ʵ��3.log";
        _NLOG_SET_CONFIG_WITH_ID(LOG_3_ID, cfg);
    }

    ~_log_mgr() {
        _NLOG_SHUTDOWN();
    }
};

_log_mgr __log;

int main()
{
    LOG_1_APP("����һ��ʵ��, ��Ŀ¼ \"ʵ��1\" ��...");
    LOG_2_APP("���Ƕ���ʵ��, �Ҹ�һ�Ż췹�Ե�...");

    LOG_3_APP("��������ʵ��, ��Ĭ��Ŀ¼��, ���ֽ� \"ʵ��3.log\" ...");
    LOG_4_APP("�����ĺ�ʵ��, ʹ�õ�Ĭ������...");

    return 0;
}