#include <iostream>

//1.���Ӿ�̬����Ҫ�ڰ���ͷ�ļ�ǰ���� NLOG_STATIC_LIB, �����������Ӵ���
#define NLOG_STATIC_LIB

//2.����ͷ�ļ�
#include "../include/nlog.h"

//3.���Ӿ�̬���lib�ļ�
#ifdef _DEBUG
#pragma comment(lib, "nloglibd.lib")
#else
#pragma comment(lib, "nloglib.lib")
#endif

//4.����log������
struct _log_mgr 
{
    /*
    *	�����ʱ���Զ�����nlog�Ĵ�ӡ����
    */
    _log_mgr() 
    {
        /*
        *	������յĻ�, ����ʹ��Ĭ��ֵ
        */
        _NLOG_CFG cfg = {
            L"{module_dir}\\��־",                      //��־�洢Ŀ¼    Ĭ����: "{module_dir}\\log\\"
            L"custom_style_%m%d.log",                   //�ļ�����ʽ      Ĭ����: "log-%m%d-%H%M.log"
            L"%Y-%m-%d",                                //���ڸ�ʽ        Ĭ����: "%m-%d %H:%M:%S"
            L"[{time}][{level}][{id}][{file}:{line}]: " //ǰ׺��ʽ        Ĭ����: "[{time}][{level}][{id}]: "
        };

        _NLOG_SET_CONFIG(cfg);
    }

    /*
    *	������ʱ�����йر�nlog
    */
    ~_log_mgr() {
        _NLOG_SHUTDOWN();
    }
};

_log_mgr __logmgr;

int main()
{
    _NLOG_APP("Hello %s!", "World");                                        //��ӡC���    ���ֽ�

    _NLOG_WAR() << nlog::time   << L" �Ӵ˿������ǵ���;�����ǳ���...";   //��ӡC++���  ���ֽ�

    _NLOG_ERR("Oh, %s! ", "No") << L"���˶�����Ĵ��벢���������˶�...";    //���ʽ��ӡ

    return 0;
}