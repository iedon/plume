#include <iostream>

//1.����ͷ�ļ�
#include "../include/nlog.h"

//2.���Ӷ�̬���lib�ļ�, ע������Ŀ�������ѽ����ӿ�Ŀ¼����Ϊ"$(SolutionDir)bin\msvc10\lib\"
#ifdef _DEBUG
#pragma comment(lib, "nlogd.lib")
#else
#pragma comment(lib, "nlog.lib")
#endif

int main()
{
    _NLOG_APP("Hello %s!", "World");                                       //��ӡC���    ���ֽ�

    _NLOG_WAR() << nlog::time  << L" �˺������ǵ����̱����ǳ���...";     //��ӡC++���  ���ֽ�

    _NLOG_ERR("Oh, %s!", "No") << L" ���˶�����Ĵ��벢���������˶�...";   //���ʽ��ӡ

    _NLOG_SET_LEVE(nlog::LV_WAR);                                          //����ֻ��ӡ���漰���ϵ�
    _NLOG_APP(L"���~~~�н�����Ƽ�...");                                      
    _NLOG_WAR(L"����~~~�ɹ��������...");

    _NLOG_SHUTDOWN();                                                      //�ر�nlog, ������Դ
    return 0;
}