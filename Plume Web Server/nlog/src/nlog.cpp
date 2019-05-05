#include <atltime.h>
#include "../include/nlog.h"

#include "iocp.hpp"
#include "simple_lock.hpp"

namespace nlog{

/*
*	CLogIO used for overlapping IO
*/
class CLogIO : public OVERLAPPED
{
    std::wstring  __strBuf;
public:
    CLogIO()
    { 
        Init();
    }

    CLogIO(const std::wstring& strBuf, const LARGE_INTEGER& offset)
    {
        Init();

        __strBuf = strBuf;
        this->Offset = offset.LowPart;
        this->OffsetHigh = offset.HighPart;
    }

    size_t Size() const 
    {
        return __strBuf.length() * sizeof(std::wstring::value_type);
    }

    const wchar_t* szBuf() 
    {
        return __strBuf.c_str();
    }
private:
    void Init() 
    {
        memset( this, 0, sizeof(OVERLAPPED) );
    }
};

/*
*	Forward declaration, string format auxiliary function
*/
std::wstring  StrFormat(const wchar_t * format, ...);
std::wstring& StrReplace(std::wstring& target, const std::wstring& before, 
    const std::wstring& after);
std::wstring StrToWStr(const std::string& str);

/*
*	设置静态成员对象的初始化顺序
*/
#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable : 4073 ) 
#pragma init_seg( lib )
#pragma warning ( pop )
#endif

/*
*	CLog static member
*/
std::map<std::string, CLog*>   CLog::__Instances;
std::auto_ptr<CSimpleLock>     CLog::__pLock(new CSimpleLock);

CLog& 
CLog::Instance( std::string guid /*= ""*/ )
{
    CAutoLock lock(*__pLock);

    std::map<std::string, CLog*>::iterator it = __Instances.find(guid);
    if(it == __Instances.end())
    {
        CLog* pLog = new CLog();
        __Instances[guid] = pLog;
        return *pLog;
    }
    else
        return *it->second;
}

bool 
CLog::Release( std::string guid /*= ""*/ )
{
    CLog* _this = 0;
    {
        CAutoLock lock(*__pLock);

        std::map<std::string, CLog*>::iterator i = __Instances.find(guid);
        if(i == __Instances.end()) {
            return false;
        }

        _this = i->second;
        __Instances.erase(i);
    }

    /*
    *   关闭完成端口, 并释放完成的资源	
    */
    if(_this->__hFile != INVALID_HANDLE_VALUE) {
        ::CloseHandle(_this->__hFile);
        _this->__hFile = INVALID_HANDLE_VALUE;
    }

    _this->CompleteHandle(true);
    _this->__pIocp->Close();
    
    return true;
}

bool 
CLog::ReleaseAll()
{
    CAutoLock lock(*__pLock);

    std::map<std::string, CLog*>::iterator i = __Instances.begin();
    while(i != __Instances.end())
    {
        if(i->second->__hFile != INVALID_HANDLE_VALUE) {
            ::CloseHandle(i->second->__hFile);
            i->second->__hFile = INVALID_HANDLE_VALUE;
        }

        i->second->CompleteHandle(true);
        i->second->__pIocp->Close();

        i = __Instances.erase(i);
    }

    return true;
}

/*
*	CLog member function
*/
CLog::CLog()
    : __pIocp(new CIOCP(1))
    , __count(0)
    , __hFile(INVALID_HANDLE_VALUE)
    , __bAlreadyInit(false)
    , __filterLevel(LV_PRO)
{
    __liNextOffset.QuadPart = 0;

    SetConfig(Config());
}

CLog::~CLog()
{
    if(__pIocp) {
        delete __pIocp;
        __pIocp = 0;
    }
}

bool 
CLog::SetConfig( const Config& setting )
{
    __config = setting;
    if(setting.logDir.empty())
        __config.logDir = L"{module_dir}\\log\\";

    if(setting.fileName.empty())
        __config.fileName = L"log-%m%d-%H%M.log";

    if(setting.dateFormat.empty())
        __config.dateFormat = L"%m-%d %H:%M:%S";

    if(setting.prefixion.empty())
        /*[{time}] [{id}] [{level}] [{file}:{line}]*/
        __config.prefixion = L"[{time}][{level}][{id}]: ";

    return true;
}

nlog::Config 
CLog::GetConfig() const
{
    return __config;
}

void
CLog::SetLevel( LogLevel level )
{
    __filterLevel = level;
}

bool 
CLog::InitLog()
{
    CAutoLock lock(*__pLock);

    if(__bAlreadyInit) {
        return false;
    }
    
    std::wstring fileName = Format(__config.logDir) + L"\\" + Format(__config.fileName);
    bool bFileExist = false;
    do {
        bFileExist = (::GetFileAttributesW(fileName.c_str()) != INVALID_FILE_ATTRIBUTES);

        if(!bFileExist)
            ::CreateDirectoryW(Format(__config.logDir).c_str(), 0);

        __hFile = ::CreateFileW( 
            fileName.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,     //共享读取
            NULL,
            OPEN_ALWAYS,         //打开文件若不存在则创建
            FILE_FLAG_OVERLAPPED,//使用重叠IO
            0);

        if(__hFile == INVALID_HANDLE_VALUE) {
            fileName.insert(fileName.rfind(L"."), L"_1");
        }

    }while( __hFile == INVALID_HANDLE_VALUE );

    ::GetFileSizeEx(__hFile, &__liNextOffset);
    __pIocp->AssociateDevice(__hFile, 0 );

    if(!bFileExist) {
#ifdef UNICODE
        /*如果是Unicode则写入BOM, 文件头LE:0xfffe BE:0xfeff*/
        WriteLog(std::wstring(1, 0xfeff));
#endif
    }

    __bAlreadyInit = true;
    return true;
}

bool 
CLog::CompleteHandle( bool bClose /*= false*/ )
{
    DWORD       dwNumBytes = 0;
    ULONG_PTR   compKey    = 0;
    CLogIO*     pIo        = 0;

    while(true) 
    {
        /* get已完成的状态，如果pio不为null那么就将其释放 */
        __pIocp->GetStatus( &dwNumBytes, &compKey, (OVERLAPPED**)&pIo, 0 );

        if( NULL != pIo ) { 
            ::InterlockedDecrement((LONG*)&__count);

            delete pIo; 
            pIo = NULL; 
        }
        else if( !bClose ) {
            break;
        }

        if( bClose && 0 == __count ) { 
            break;
        }
    }
    return true;
}

std::wstring 
CLog::Format( const std::wstring& strBuf, const LogInfomation& info )
{
    /*
    *	这里的查找, 还存在大量的性能浪费, 后面可以专门做优化
    */
    std::wstring result  = (LPCTSTR)CTime::GetCurrentTime().Format(strBuf.c_str());
    std::wstring strTime = (LPCTSTR)CTime::GetCurrentTime().Format(__config.dateFormat.c_str());
    std::wstring strId   = StrFormat(L"%- 8X", ::GetCurrentThreadId());
    std::wstring strLine = StrFormat(L"%- 4d", info.line);
    
    std::wstring strLevel;
    switch(info.level) {
    case LV_ERR: strLevel = L"ERR"; break;
    case LV_WAR: strLevel = L"WAR"; break;
    case LV_APP: strLevel = L"APP"; break;
    case LV_PRO: strLevel = L"PRO"; break;
    }

    std::wstring strModule;
    {
        wchar_t buffer[MAX_PATH] = {0};
        ::GetModuleFileNameW(0, buffer, sizeof buffer);

        if(wchar_t *fname = wcsrchr(buffer, L'\\'))
            *fname = L'\0';

        strModule = buffer;
    }

    result = StrReplace(result, L"{module_dir}",strModule);
    result = StrReplace(result, L"{level}", strLevel);
    result = StrReplace(result, L"{time}", strTime);
    result = StrReplace(result, L"{id}", strId);
    result = StrReplace(result, L"{file}", info.file);
    result = StrReplace(result, L"{line}", strLine);
    
    return result;
}

CLog& 
CLog::FormatWriteLog( const std::wstring& strBuf, const LogInfomation& info /*= Loginfomation()*/ )
{
    if(__filterLevel >= info.level) {
        if(!__bAlreadyInit) {
            InitLog();
        }

        return WriteLog(Format(__config.prefixion, info) + strBuf + L"\r\n");
    }
    else
        return *this;
}

CLog& 
CLog::WriteLog( const std::wstring& strBuf )
{
    CompleteHandle();

    CLogIO* pIo = NULL;
    {
        CAutoLock lock(*__pLock);
        pIo = new CLogIO(strBuf, __liNextOffset);
        ::InterlockedExchangeAdd64(&__liNextOffset.QuadPart, pIo->Size());
    }

    /* 投递重叠IO */
    ::WriteFile( __hFile, pIo->szBuf(), pIo->Size(), NULL, pIo );
    ::InterlockedIncrement((LONG*)&__count);

    if(GetLastError() == ERROR_IO_PENDING) {
        /*
        *   MSDN:The error code WSA_IO_PENDING indicates that the overlapped operation 
        *   has been successfully initiated and that completion will be indicated at a 
        *   later time.
        */
        SetLastError(S_OK);
    }
    return *this;
}

/*
*	CLogHelper firend function
*/
CLogHelper& 
time(CLogHelper& slef)
{
    return slef << (LPCTSTR)CTime::GetCurrentTime().Format(
        CLog::Instance(slef.__sessionId).__config.dateFormat.c_str());
}

CLogHelper& 
id(CLogHelper& slef)
{
    return slef << StrFormat(L"%- 8X", ::GetCurrentThreadId());
}

CLogHelper::CLogHelper(LogLevel level, const char* file, 
    const unsigned int line, const std::string& guid /*= ""*/) 
    : __sessionId(guid)
{
    const char* fname = strrchr(file, '\\');

    __logInfo.file  = StrToWStr(fname ? fname + 1 : file);
    __logInfo.level = level;
    __logInfo.line  = line;
}

CLogHelper::~CLogHelper()
{
    CLog::Instance(__sessionId).FormatWriteLog(__strbuf.str(), __logInfo);
}

CLogHelper& 
CLogHelper::Format()
{
    return *this;
}

CLogHelper& 
CLogHelper::Format(const wchar_t * _Format, ...)
{
    va_list  marker = NULL;  
    va_start(marker, _Format);

    std::wstring text(_vscwprintf(_Format, marker) + 1, 0);
    vswprintf_s(&text[0], text.capacity(), _Format, marker);
    va_end(marker);

    return (*this) << text.data();
}

CLogHelper& 
CLogHelper::Format(const char * _Format, ...)
{
    va_list  marker = NULL;  
    va_start(marker, _Format);

    std::string text(_vscprintf(_Format, marker) + 1, 0);
    vsprintf_s(&text[0], text.capacity(), _Format, marker);
    va_end(marker);

    return (*this) << StrToWStr(text.data());
}

CLogHelper& 
CLogHelper::operator<<(CLogHelper&(__cdecl* pfn)(CLogHelper &))
{
    return ((*pfn)(*this));
}

CLogHelper& 
CLogHelper::operator<<(const std::string& info)
{
    return (*this) << StrToWStr(info);
}

/*
*	string format auxiliary function
*/
std::wstring 
StrFormat(const wchar_t * format, ...) 
{  
    va_list marker = NULL;  
    va_start(marker, format);

    std::wstring text(_vsctprintf(format, marker) + 1, 0);
    _vstprintf_s(&text[0], text.capacity(), format, marker);
    va_end(marker);

    return text.data();
}

std::wstring& 
StrReplace(std::wstring& target, const std::wstring& before, 
    const std::wstring& after)
{  
    std::wstring::size_type beforeLen = before.length();  
    std::wstring::size_type afterLen  = after.length();  
    std::wstring::size_type pos       = target.find(before, 0);

    while( pos != std::wstring::npos ) {  
        target.replace(pos, beforeLen, after);  
        pos = target.find(before, (pos + afterLen));  
    }

    return target;
}

std::wstring 
StrToWStr(const std::string& str)
{
    size_t len = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

    std::wstring buff(len, 0);
    ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, (LPWSTR)buff.data(), len);

    if(buff[buff.size()-1] == '\0')
        buff.erase( buff.begin() + (buff.size()-1) );

    return buff;
}

}// namespace nlog