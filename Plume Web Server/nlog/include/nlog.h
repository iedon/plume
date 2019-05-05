#ifndef nlog_h__
#define nlog_h__

/*
*    nlog
*    创建:  2016-6-16
*    修改:  2018-8-5
*    Email：<kwok-jh@qq.com>
*    Git:   https://gitee.com/skygarth/nlog

*    异步
*    多线程安全

*    Example:
*    #include "nlog.h"                                             //包含头文件, 并连接对应的lib
*    ...
*    _NLOG_ERR("Hello, %s", "nlog") << " Now Time:" << nlog::time; //c,c++风格混搭格式化输出
*    ...
*    _NLOG_SHUTDOWN();                                             //最后执行清理
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
*	日志等级
*/
enum LogLevel
{
    LV_ERR = 0,
    LV_WAR = 1,    
    LV_APP = 2,    
    LV_PRO = 3
};

/*
*   日志配置数据结构
*/
struct Config 
{
    /*
    *	日志存储目录      default: "{module_dir}\\log\\"
    *   可选变量:
    *   {module_dir}      当前可执行模块目录, 默认是程序的当前目录
    *    %Y,%m,%d,%H ...  时间日期格式化
    */
    std::wstring logDir;

    /*
    *	文件名格式        default: "log-%m%d-%H%M.log"    如(log-0805-2348.log)
    *   可选变量:
    *   %Y,%m,%d,%H ...   时间日期格式化
    */
    std::wstring fileName;

    /*
    *	日期格式          default: "%m-%d %H:%M:%S"       如(08-05 23:48:06)
    *   可选变量:
    *   %Y(%y),%m,%d,%H,%M,%S 时间日期格式化 分别是 年,月,日,时,分,秒
    */
    std::wstring dateFormat;

    /*
    *	前缀格式          default: "[{time}][{level}][{id}]: " 
    *                            如([08-05 23:48:06][ERR][2F84    ]: )
    *   可选变量:
    *   {module_dir}      当前可执行模块目录
    *   {level}           当前打印日志的等级
    *   {time}            当前打印日志的时间 格式由dateFormat指定
    *   {id}              当前打印日志的线程id
    *   {file}            当前打印日志的源文件名
    *   {line}            当前打印日志的源文件行
    */
    std::wstring prefixion;
};

/*
*   日志类
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
    *   获得一个Log的实例, 允许存在多个Log实例, guid代表实例的唯一Id
    *   每一个实例独占一个日志文件, 若它们之间具有相同的文件名称格式
    *   那么后一个被实例化的Log将指向一个具有"_1"的名称
    */
    static CLog& Instance(std::string guid = "");
    static bool  Release (std::string guid = "");
    static bool  ReleaseAll();

    /* 
    *   Log配置, 输出文件名称格式, 打印格式等...
    *   要注意的是, 设置必须在打印第一条日志之前完成否则可能不起任何作用 
    */
    bool    SetConfig(const Config& setting);
    Config  GetConfig() const;

    /* 在任何时候都可以指定日志打印的等级 */
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
*   日志格式化辅助类
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
*	使用默认Log实例, 格式化输出一条信息
*   example:
*   _NLOG_ERR("hello") << "nlog";
*/
#define _NLOG_ERR  nlog::CLogHelper(nlog::LV_ERR, __FILE__, __LINE__).Format
#define _NLOG_WAR  nlog::CLogHelper(nlog::LV_WAR, __FILE__, __LINE__).Format
#define _NLOG_APP  nlog::CLogHelper(nlog::LV_APP, __FILE__, __LINE__).Format
#define _NLOG_PRO  nlog::CLogHelper(nlog::LV_PRO, __FILE__, __LINE__).Format

/*
*	使用指定的Log实例, 格式化输出一条信息
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
*   设置初始配置
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
*	设置实时打印等级
*   example: - 设置只打印警告及以上的日志
*   
*   _NLOG_SET_LEVE(LV_WAR); 
*/
#define _NLOG_SET_LEVE(lev)                  nlog::CLog::Instance().SetLevel(lev)
#define _NLOG_SET_LEVE_WITH_ID(id, lev)      nlog::CLog::Instance(id).SetLevel(lev)
/*
*	执行清理工作, 销毁所有存在的nlog实例
*   example: - 初始配置与自动销毁
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