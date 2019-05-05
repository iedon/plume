#ifndef nlog_h__
#define nlog_h__

/*
*    nlog
*    ����:  2016-6-16
*    �޸�:  2018-8-5
*    Email��<kwok-jh@qq.com>
*    Git:   https://gitee.com/skygarth/nlog

*    �첽
*    ���̰߳�ȫ

*    Example:
*    #include "nlog.h"                                             //����ͷ�ļ�, �����Ӷ�Ӧ��lib
*    ...
*    _NLOG_ERR("Hello, %s", "nlog") << " Now Time:" << nlog::time; //c,c++������ʽ�����
*    ...
*    _NLOG_SHUTDOWN();                                             //���ִ������
*/

#include <map>
#include <sstream>

#define  WIN32_LEAN_AND_MEAN 
#include <windows.h>

/* export */
#ifdef  NLOG_STATIC_LIB
# define NLOG_LIB 
#else

#pragma warning( push )
#pragma warning( disable : 4251 ) 

#ifdef  NLOG_SHARE_LIB
# define NLOG_LIB __declspec(dllexport)
#else
# define NLOG_LIB __declspec(dllimport)
#endif // NLOG_SHARE_LIB
#endif // NLOG_STATIC_LIB

class CIOCP;
class CSimpleLock;

namespace nlog{

/*
*	��־�ȼ�
*/
enum LogLevel
{
    LV_ERR = 0,
    LV_WAR = 1,    
    LV_APP = 2,    
    LV_PRO = 3
};

/*
*   ��־�������ݽṹ
*/
struct Config 
{
    /*
    *	��־�洢Ŀ¼      default: "{module_dir}\\log\\"
    *   ��ѡ����:
    *   {module_dir}      ��ǰ��ִ��ģ��Ŀ¼, Ĭ���ǳ���ĵ�ǰĿ¼
    *    %Y,%m,%d,%H ...  ʱ�����ڸ�ʽ��
    */
    std::wstring logDir;

    /*
    *	�ļ�����ʽ        default: "log-%m%d-%H%M.log"    ��(log-0805-2348.log)
    *   ��ѡ����:
    *   %Y,%m,%d,%H ...   ʱ�����ڸ�ʽ��
    */
    std::wstring fileName;

    /*
    *	���ڸ�ʽ          default: "%m-%d %H:%M:%S"       ��(08-05 23:48:06)
    *   ��ѡ����:
    *   %Y(%y),%m,%d,%H,%M,%S ʱ�����ڸ�ʽ�� �ֱ��� ��,��,��,ʱ,��,��
    */
    std::wstring dateFormat;

    /*
    *	ǰ׺��ʽ          default: "[{time}][{level}][{id}]: " 
    *                            ��([08-05 23:48:06][ERR][2F84    ]: )
    *   ��ѡ����:
    *   {module_dir}      ��ǰ��ִ��ģ��Ŀ¼
    *   {level}           ��ǰ��ӡ��־�ĵȼ�
    *   {time}            ��ǰ��ӡ��־��ʱ�� ��ʽ��dateFormatָ��
    *   {id}              ��ǰ��ӡ��־���߳�id
    *   {file}            ��ǰ��ӡ��־��Դ�ļ���
    *   {line}            ��ǰ��ӡ��־��Դ�ļ���
    */
    std::wstring prefixion;
};

/*
*   ��־��
*/
class NLOG_LIB CLog
{
    CLog();
    ~CLog();

    CLog(const CLog&);
    CLog operator=(const CLog&);

    friend class CLogHelper;
    friend NLOG_LIB CLogHelper& time(CLogHelper& slef);

    static std::map<std::string, CLog*> __Instances;
    static std::auto_ptr<CSimpleLock>   __pLock;
public:
    /*
    *   ���һ��Log��ʵ��, ������ڶ��Logʵ��, guid����ʵ����ΨһId
    *   ÿһ��ʵ����ռһ����־�ļ�, ������֮�������ͬ���ļ����Ƹ�ʽ
    *   ��ô��һ����ʵ������Log��ָ��һ������"_1"������
    */
    static CLog& Instance(std::string guid = "");
    static bool  Release (std::string guid = "");
    static bool  ReleaseAll();

    /* 
    *   Log����, ����ļ����Ƹ�ʽ, ��ӡ��ʽ��...
    *   Ҫע�����, ���ñ����ڴ�ӡ��һ����־֮ǰ��ɷ�����ܲ����κ����� 
    */
    bool    SetConfig(const Config& setting);
    Config  GetConfig() const;

    /* ���κ�ʱ�򶼿���ָ����־��ӡ�ĵȼ� */
    void    SetLevel(LogLevel level); 
protected:
    bool    InitLog();
    bool    CompleteHandle(bool bClose = false);

    struct  LogInfomation
    {
        LogLevel level;
        unsigned int line;
        std::wstring  file;
    };
    std::wstring Format (const std::wstring& strBuf, const LogInfomation& info = LogInfomation());
    CLog& FormatWriteLog(const std::wstring& strBuf, const LogInfomation& info = LogInfomation());
    CLog& WriteLog      (const std::wstring& strBuf);
private:
    CIOCP*   __pIocp; 
    HANDLE   __hFile;
    Config   __config;
    bool     __bAlreadyInit;
    LogLevel __filterLevel;

    unsigned int __count;
    LARGE_INTEGER __liNextOffset;
};

/*
*   ��־��ʽ��������
*/
class NLOG_LIB CLogHelper
{
public:
    CLogHelper(LogLevel level, const char* file, const unsigned int line, const std::string& guid = "");
    ~CLogHelper();

    CLogHelper& Format();
    CLogHelper& Format(const wchar_t * _Format, ...);
    CLogHelper& Format(const char    * _Format, ...);

    template<class T> 
    CLogHelper& operator<<(T info);
    CLogHelper& operator<<(const std::string& info);
    CLogHelper& operator<<(CLogHelper&(__cdecl* pfn)(CLogHelper &));

    friend NLOG_LIB CLogHelper& time(CLogHelper& slef);
    friend NLOG_LIB CLogHelper& id  (CLogHelper& slef);

private:
    std::string __sessionId;
    std::wstringstream  __strbuf;
    CLog::LogInfomation __logInfo;
};

template<class T> 
CLogHelper& CLogHelper::operator<<(T info){
    __strbuf << info;
    return *this;
}

NLOG_LIB CLogHelper& time(CLogHelper& slef);
NLOG_LIB CLogHelper& id  (CLogHelper& slef);

}// namespace nlog

#ifndef  NLOG_STATIC_LIB
#pragma warning( pop )
#endif

/*
*	ʹ��Ĭ��Logʵ��, ��ʽ�����һ����Ϣ
*   example:
*   _NLOG_ERR("hello") << "nlog";
*/
#define _NLOG_ERR  nlog::CLogHelper(nlog::LV_ERR, __FILE__, __LINE__).Format
#define _NLOG_WAR  nlog::CLogHelper(nlog::LV_WAR, __FILE__, __LINE__).Format
#define _NLOG_APP  nlog::CLogHelper(nlog::LV_APP, __FILE__, __LINE__).Format
#define _NLOG_PRO  nlog::CLogHelper(nlog::LV_PRO, __FILE__, __LINE__).Format

/*
*	ʹ��ָ����Logʵ��, ��ʽ�����һ����Ϣ
*   example:
*   #define LOG_UID    "custom log id"
*   #define LOG_ERR    _NLOG_ERR_WITH_ID(LOG_UID)   
*   ...
*   LOG_ERR("hello") << "nlog";     
*/
#define _NLOG_ERR_WITH_ID(id) nlog::CLogHelper(nlog::LV_ERR, __FILE__, __LINE__, id).Format
#define _NLOG_WAR_WITH_ID(id) nlog::CLogHelper(nlog::LV_WAR, __FILE__, __LINE__, id).Format
#define _NLOG_APP_WITH_ID(id) nlog::CLogHelper(nlog::LV_APP, __FILE__, __LINE__, id).Format
#define _NLOG_PRO_WITH_ID(id) nlog::CLogHelper(nlog::LV_PRO, __FILE__, __LINE__, id).Format

/*
*   ���ó�ʼ����
*   example:
*      
*   _NLOG_CFG cfg = {
*       L"",
*       L"nlog-%m%d%H%M.log",
*       L"",
*       L"[{time}][{level}][{id}][{file}:{line}]: "
*   };
*
*   _NLOG_SET_CONFIG(cfg);
*/

#define _NLOG_CFG                            nlog::Config
#define _NLOG_SET_CONFIG(cfg)                nlog::CLog::Instance().SetConfig(cfg)
#define _NLOG_SET_CONFIG_WITH_ID(id, cfg)    nlog::CLog::Instance(id).SetConfig(cfg)

/*
*	����ʵʱ��ӡ�ȼ�
*   example: - ����ֻ��ӡ���漰���ϵ���־
*   
*   _NLOG_SET_LEVE(LV_WAR); 
*/
#define _NLOG_SET_LEVE(lev)                  nlog::CLog::Instance().SetLevel(lev)
#define _NLOG_SET_LEVE_WITH_ID(id, lev)      nlog::CLog::Instance(id).SetLevel(lev)
/*
*	ִ��������, �������д��ڵ�nlogʵ��
*   example: - ��ʼ�������Զ�����
*
*   struct _NLogMgr 
*   {
*        _NLogMgr() 
*        {
*            _NLOG_CFG cfg = {
*                L"",
*                L"nlog-%m%d%H%M.log",
*                L"",
*                L"[{time}][{level}][{id}][{file}:{line}]: "
*            };
*    
*            _NLOG_SET_CONFIG(cfg);
*        }
*    
*        ~_NLogMgr() {
*            _NLOG_SHUTDOWN();
*        }
*    };
*
*    static _NLogMgr _NLog;
*/
#define _NLOG_SHUTDOWN  nlog::CLog::ReleaseAll

#endif // nlog_h__