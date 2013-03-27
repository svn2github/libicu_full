/*
******************************************************************************
*
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File umutex.cpp
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97    aliu        Creation.
*   04/07/99    srl         updated
*   05/13/99    stephen     Changed to umutex (from cmutex).
*   11/22/99    aliu        Make non-global mutex autoinitialize [j151]
******************************************************************************
*/

#include "unicode/utypes.h"
#include "uassert.h"
#include "ucln_cmn.h"

/*
 * ICU Mutex wrappers.  Wrap operating system mutexes, giving the rest of ICU a
 * platform independent set of mutex operations.  For internal ICU use only.
 */

#if U_PLATFORM_HAS_WIN32_API
    /* Prefer native Windows APIs even if POSIX is implemented (i.e., on Cygwin). */
#   undef POSIX
#elif U_PLATFORM_IMPLEMENTS_POSIX
#   define POSIX
#else
#   undef POSIX
#endif

#if defined(POSIX)
# include <pthread.h> /* must be first, so that we get the multithread versions of things. */
#endif /* POSIX */

#if U_PLATFORM_HAS_WIN32_API
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# include <windows.h>
#endif

#include "umutex.h"
#include "cmemory.h"


// The ICU global mutex. Used when ICU implementation code passes NULL for the mutex pointer.
static UMutex   globalMutex = U_MUTEX_INITIALIZER;


#if defined(POSIX)

//
// POSIX implementation of UMutex.
//
// Each UMutex has a corresponding pthread_mutex_t.
// All are statically initialized and ready for use.
// There is no runtime mutex initialization code needed.

U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    #if U_DEBUG
        // #if to avoid unused variable warnings in non-debug builds.
        int sysErr = pthread_mutex_lock(&mutex->fMutex);
        U_ASSERT(sysErr == 0);
    #else
        pthread_mutex_lock(&mutex->fMutex);
    #endif
}


U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    #if U_DEBUG
        // #if to avoid unused variable warnings in non-debug builds.
        int sysErr = pthread_mutex_unlock(&mutex->fMutex);
        U_ASSERT(sysErr == 0);
    #else
        pthread_mutex_unlock(&mutex->fMutex);
    #endif
}

static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initCondition = PTHREAD_COND_INITIALIZER;

typedef UBool (*U_PINIT_ONCE_FN) (
  U_INIT_ONCE     *initOnce,
  void            *parameter,
  void            **context
);


UBool u_InitOnceExecuteOnce(
    U_INIT_ONCE     *initOnce,
    U_PINIT_ONCE_FN initFn,
    void            *parameter,
    void            **context) {

    (void)context;  // Unused for now.
    pthread_mutex_lock(&initMutex);
    while (TRUE) {
        int32_t state = initOnce->fState;
        if (state == 2) {
            break;
        }
        if (state == 0) {
            initOnce->fState = 1;
            pthread_mutex_unlock(&initMutex);
            (*initFn)(initOnce, parameter, &initOnce->fContext);
            pthread_mutex_lock(&initMutex);
            initOnce->fState = 2;
            pthread_cond_broadcast(&initCondition);
        } else if (state == 1) {
            // Initialization in progress on another thread.
            pthread_cond_wait(&initCondition, &initMutex);
        }
    }
    pthread_mutex_unlock(&initMutex);
    return TRUE;
}

// This function is called when a test of a UInitOnce::fState reveals that
//   initialization has not completed, that we either need to call the
//   function on this thread, or wait for some other thread to complete.
//
// The actual call to the init function is made inline by template code
//   that knows the C++ types involved. This function returns TRUE if
//   the caller needs to call the Init function.
//
UBool u_initImplPreInit(UInitOnce *uio) {
    pthread_mutex_lock(&initMutex);
    int32_t state = uio->fState;
    if (state == 0) {
        uio->fState = 1;
        pthread_mutex_unlock(&initMutex);
        return true;   // Caller will next call the init function.
    } else if (state == 2) {
        // Another thread alread completed the initialization, in
        //   a race with this thread. We can simply return FALSE, indicating no
        //   further action is needed by the caller.
        pthread_mutex_unlock(&initMutex);
        return FALSE;
    } else {
        // Another thread is currently running the initialization.
        // Wait until it completes.
        U_ASSERT(state == 1);
        while (uio->fState == 1) {
            pthread_cond_wait(&initCondition, &initMutex);
        }
        U_ASSERT(uio->fState == 2);
        pthread_mutex_unlock(&initMutex);
        return FALSE;
    }
}

// This function is called by the thread that ran an initialization function,
// just after completing the function.
//   Some threads may be waiting on the condition, requiring the broadcast wakeup.
//   Some threads may be racing to test the fState variable outside of the mutex, 
//   requiring the use of store/release when changing its value.
//
//   success: True:  the inialization succeeded. No further calls to the init
//                   function will be made.
//            False: the initializtion failed. The next call to u_initOnce()
//                   will retry the initialization.

void u_initImplPostInit(UInitOnce *uio, UBool success) {
    int32_t nextState = success? 2: 0;
    pthread_mutex_lock(&initMutex);
    uio->fState.store(nextState, std::memory_order_release);
    pthread_cond_broadcast(&initCondition);
    pthread_mutex_unlock(&initMutex);
}


void u_initOnceReset(UInitOnce *uio) {
    uio->fState = 0;
}
        
    

#elif U_PLATFORM_HAS_WIN32_API
//
// Windows implementation of UMutex.
//
// Each UMutex has a corresponding Windows CRITICAL_SECTION.
// CRITICAL_SECTIONS must be initialized before use. This is done
//   with a InitOnceExcuteOnce operation.
//
// InitOnceExecuteOnce was introduced with Windows Vista. For now ICU
// must support Windows XP, so we roll our own. ICU will switch to the
// native Windows InitOnceExecuteOnce when possible.

typedef UBool (*U_PINIT_ONCE_FN) (
  U_INIT_ONCE     *initOnce,
  void            *parameter,
  void            **context
);

UBool u_InitOnceExecuteOnce(
  U_INIT_ONCE     *initOnce,
  U_PINIT_ONCE_FN initFn,
  void            *parameter,
  void            **context) {
      for (;;) {
          long previousState = InterlockedCompareExchange( 
              &initOnce->fState,  //  Destination,
              1,                  //  Exchange Value
              0);                 //  Compare value
          if (previousState == 2) {
              // Initialization was already completed.
              if (context != NULL) {
                  *context = initOnce->fContext;
              }
              return TRUE;
          }
          if (previousState == 1) {
              // Initialization is in progress in some other thread.
              // Loop until it completes.
              Sleep(1);
              continue;
          }
           
          // Initialization needed. Execute the callback function to do it.
          U_ASSERT(previousState == 0);
          U_ASSERT(initOnce->fState == 1);
          UBool success = (*initFn)(initOnce, parameter, &initOnce->fContext);
          U_ASSERT(success); // ICU is not supporting the failure case.

          // Assign the state indicating that initialization has completed.
          // Using InterlockedCompareExchange to do it ensures that all
          // threads will have a consistent view of memory.
          previousState = InterlockedCompareExchange(&initOnce->fState, 2, 1);
          U_ASSERT(previousState == 1);
          // Next loop iteration will see the initialization and return.
      }
};

static UBool winMutexInit(U_INIT_ONCE *initOnce, void *param, void **context) {
    UMutex *mutex = static_cast<UMutex *>(param);
    U_ASSERT(sizeof(CRITICAL_SECTION) <= sizeof(mutex->fCS));
    InitializeCriticalSection((CRITICAL_SECTION *)mutex->fCS);
    return TRUE;
}

/*
 *   umtx_lock
 */
U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    u_InitOnceExecuteOnce(&mutex->fInitOnce, winMutexInit, mutex, NULL);
    EnterCriticalSection((CRITICAL_SECTION *)mutex->fCS);
}

U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    LeaveCriticalSection((CRITICAL_SECTION *)mutex->fCS);
}

#endif  // Windows Implementation



/*-----------------------------------------------------------------
 *
 *  Atomic Increment and Decrement
 *     umtx_atomic_inc
 *     umtx_atomic_dec
 *
 *----------------------------------------------------------------*/

#if defined (POSIX) && (U_HAVE_GCC_ATOMICS == 0)
static UMutex   gIncDecMutex = U_MUTEX_INITIALIZER;
#endif

U_CAPI int32_t U_EXPORT2
umtx_atomic_inc(int32_t *p)  {
    int32_t retVal;
    #if U_PLATFORM_HAS_WIN32_API
        retVal = InterlockedIncrement((LONG*)p);
    #elif defined(USE_MAC_OS_ATOMIC_INCREMENT)
        retVal = OSAtomicIncrement32Barrier(p);
    #elif (U_HAVE_GCC_ATOMICS == 1)
        retVal = __sync_add_and_fetch(p, 1);
    #elif defined (POSIX)
        umtx_lock(&gIncDecMutex);
        retVal = ++(*p);
        umtx_unlock(&gIncDecMutex);
    #else
        /* Unknown Platform. */
        retVal = ++(*p);
    #endif
    return retVal;
}

U_CAPI int32_t U_EXPORT2
umtx_atomic_dec(int32_t *p) {
    int32_t retVal;
    #if U_PLATFORM_HAS_WIN32_API
        retVal = InterlockedDecrement((LONG*)p);
    #elif defined(USE_MAC_OS_ATOMIC_INCREMENT)
        retVal = OSAtomicDecrement32Barrier(p);
    #elif (U_HAVE_GCC_ATOMICS == 1)
        retVal = __sync_sub_and_fetch(p, 1);
    #elif defined (POSIX)
        umtx_lock(&gIncDecMutex);
        retVal = --(*p);
        umtx_unlock(&gIncDecMutex);
    #else
        /* Unknown Platform. */
        retVal = --(*p);
    #endif
    return retVal;
}


