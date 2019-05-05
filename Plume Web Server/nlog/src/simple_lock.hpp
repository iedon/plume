#ifndef simple_lock_h__
#define simple_lock_h__

/*
*   应用于Windows平台的简单互斥锁
*   2018-6 By qiling
*/

#define  WIN32_LEAN_AND_MEAN 
#include <windows.h>

class CSimpleLock  
{  
    CRITICAL_SECTION __cs;
public:  
    CSimpleLock()  
    {  
        // The second parameter is the spin count, for short-held locks it avoid the
        // contending thread from going to sleep which helps performance greatly.
        ::InitializeCriticalSectionAndSpinCount(&__cs, 2000);
    } 

    ~CSimpleLock()  
    {  
        ::DeleteCriticalSection(&__cs);  
    }  

    // If the lock is not held, take it and return true.  If the lock is already
    // held by something else, immediately return false.
    bool Try()
    {
        if (::TryEnterCriticalSection(&__cs) != FALSE) {
            return true;
        }
        return false;
    }

    // Take the lock, blocking until it is available if necessary.
    void Lock()  
    {  
        ::EnterCriticalSection(&__cs);  
    }  

    // Release the lock.  This must only be called by the lock's holder: after
    // a successful call to Try, or a call to Lock.
    void Unlock()  
    {  
        ::LeaveCriticalSection(&__cs);  
    }  
};

// A helper class that acquires the given Lock while the CAutoLock is in scope.
class CAutoLock  
{  
    CAutoLock(const CAutoLock&);
    CAutoLock& operator=(const CAutoLock&);

    CSimpleLock& __pLock;
public:  
    explicit CAutoLock(CSimpleLock& pLock)
        : __pLock(pLock)
    {   
        __pLock.Lock();
    }  

    ~CAutoLock()  
    {  
        __pLock.Unlock();  
    }  
};

#endif // simple_lock_h__