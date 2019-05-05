#ifndef iocp_h__
#define iocp_h__

/*
*    完成端口的简单封装 
*    2016-6 By qiling
*/

#define  WIN32_LEAN_AND_MEAN 
#include <windows.h>

class CIOCP 
{
    HANDLE _hiocp;

    /* 不可拷贝 */
    CIOCP(const CIOCP&);
    CIOCP& operator=(const CIOCP&);
public:
    /* 不可隐式实例化 */
    explicit CIOCP(int nMaxConcurrency = -1)
        : _hiocp(NULL)
    { 
        if (nMaxConcurrency != -1) {
            Create(nMaxConcurrency);
        }
    }

    ~CIOCP() 
    { 
        Close();
    }

    void Close() 
    {
        if (NULL != _hiocp) {
            ::CloseHandle(_hiocp); 
            _hiocp = NULL;
        }
    }

    bool Create(int nMaxConcurrency = 0) 
    {
        _hiocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, nMaxConcurrency);
        return _hiocp != NULL;
    }

    bool AssociateDevice(HANDLE hDevice, ULONG_PTR CompKey) 
    {
        if (NULL != _hiocp) {
            return ::CreateIoCompletionPort(hDevice, _hiocp, CompKey, 0) == _hiocp;
        }
        return false;
    }
    
    bool AssociateSocket(SOCKET hSocket, ULONG_PTR CompKey) 
    {
        if (NULL != _hiocp) {
            return AssociateDevice((HANDLE) hSocket, CompKey);
        }
        return false;
    }

    bool PostStatus(ULONG_PTR CompKey, DWORD dwNumBytes = 0,  OVERLAPPED* po = NULL) 
    {
        if (NULL != _hiocp) {
            return !!::PostQueuedCompletionStatus(_hiocp, dwNumBytes, CompKey, po);
        }
        return false;
    }

    bool GetStatus(LPDWORD pdwNumBytes, ULONG_PTR* pCompKey, LPOVERLAPPED* ppo, DWORD dwMilliseconds = INFINITE) 
    {
        if (NULL != _hiocp) {
            return !!::GetQueuedCompletionStatus(_hiocp, pdwNumBytes, pCompKey, ppo, dwMilliseconds);
        }
        return false;
    }
};

#endif // iocp_h__