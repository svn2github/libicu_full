/*
**********************************************************************
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File UMUTEX.H
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97  aliu        Creation.
*   04/07/99  srl         rewrite - C interface, multiple mutices
*   05/13/99  stephen     Changed to umutex (from cmutex)
******************************************************************************
*/

#ifndef UMUTEX_H
#define UMUTEX_H

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "putilimp.h"

/* For _ReadWriteBarrier(). */
#if defined(_MSC_VER) && _MSC_VER >= 1500
# include <intrin.h>
#endif

/* For CRITICAL_SECTION */
#if U_PLATFORM_HAS_WIN32_API
#if 0  
/* TODO(andy): Why doesn't windows.h compile in all files? It does in some.
 *             The intent was to include windows.h here, and have struct UMutex
 *             have an embedded CRITICAL_SECTION when building on Windows.
 *             The workaround is to put some char[] storage in UMutex instead,
 *             avoiding the need to include windows.h everwhere this header is included.
 */
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# include <windows.h>
#endif  /* 0 */
#define U_WINDOWS_CRIT_SEC_SIZE 64
#endif  /* win32 */


/*
 * Code within ICU that accesses shared static or global data should
 * instantiate a Mutex object while doing so.  The unnamed global mutex
 * is used throughout ICU, so keep locking short and sweet.
 *
 * For example:
 *
 * void Function(int arg1, int arg2)
 * {
 *   static Object* foo;     // Shared read-write object
 *   umtx_lock(NULL);        // Lock the ICU global mutex
 *   foo->Method();
 *   umtx_unlock(NULL);
 * }
 *
 * an alternative C++ mutex API is defined in the file common/mutex.h
 */

/*
 * UMutex - Mutexes for use by ICU implementation code.
 *          Must be declared as static or globals. They cannot appear as members
 *          of other objects.
 *          UMutex structs must be initialized.
 *          Example:
 *            static UMutex = U_MUTEX_INITIALIZER;
 *          The declaration of struct UMutex is platform dependent.
 */



/*  U_INIT_ONCE mimics the windows API INIT_ONCE, which exists on Windows Vista and newer.
 *  When ICU no longer needs to support older Windows platforms (XP) that do not have
 * a native INIT_ONCE, switch this implementation over to wrap the native Windows APIs.
 */
typedef struct U_INIT_ONCE {
    long               fState;
    void              *fContext;
} U_INIT_ONCE;
#define U_INIT_ONCE_STATIC_INIT {0, NULL}

#if U_SHOW_CPLUSPLUS_API


#if __cplusplus>=201103L || defined(__GXX_EXPERIMENTAL_CXX0X__)
// C++11 definitions. 
#include <atomic>
typedef std::atomic<int32_t> atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) ATOMIC_VAR_INIT(val)
inline int32_t umtx_loadAcquire(atomic_int32_t &var) {
	return std::atomic_load_explicit(&var, std::memory_order_acquire);};
inline void umtx_storeRelease(atomic_int32_t &var, int32_t val) {
	var.store(val, std::memory_order_release);};



#elif defined(_MSC_VER)
// MSVC compiler. Reads and writes of volatile variables have
//                acquire and release memory semantics, respectively.
//                This is a Microsoft extension, not standard behavior.
typedef volatile long atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) val
inline int32_t umtx_loadAcquire(atomic_int32_t &var) {
	return var;};
inline void umtx_storeRelease(atomic_int32_t &var, int32_t val) {
	var = val;};
#endif

struct UInitOnce {
    atomic_int32_t   fState;
    UErrorCode       fErrCode;
    void reset() {fState = 0; fState=0;};
    UBool isReset() {return umtx_loadAcquire(fState) == 0;};
};

// Note: isReset() is used by service registration code.
//                 Thread safety of this usage needs review.


#define U_INITONCE_INITIALIZER {ATOMIC_INT32_T_INITIALIZER(0), U_ZERO_ERROR}




UBool umtx_initImplPreInit(UInitOnce &);
void  umtx_initImplPostInit(UInitOnce &, UBool success);

template<class T> void umtx_initOnce(UInitOnce &uio, T *obj, void (T::*fp)()) {
    if (umtx_loadAcquire(uio.fState) == 2) {
        return;
    }
    if (umtx_initImplPreInit(uio)) {
        (obj->*fp)();
        umtx_initImplPostInit(uio, TRUE);
    }
}


// umtx_initOnce variant for plain functions, or static class functions.
//               No context parameter.
inline void umtx_initOnce(UInitOnce &uio, void (*fp)()) {
    if (umtx_loadAcquire(uio.fState) == 2) {
        return;
    }
    if (umtx_initImplPreInit(uio)) {
        (*fp)();
        umtx_initImplPostInit(uio, TRUE);
    }
}

// umtx_initOnce variant for plain functions, or static class functions.
//               With ErrorCode, No context parameter.
inline void umtx_initOnce(UInitOnce &uio, void (*fp)(UErrorCode &), UErrorCode &errCode) {
    if (U_FAILURE(errCode)) {
        return;
    }    
    if (umtx_loadAcquire(uio.fState) != 2 && umtx_initImplPreInit(uio)) {
        // We run the initialization.
        (*fp)(errCode);
        uio.fErrCode = errCode;
        umtx_initImplPostInit(uio, TRUE);
    } else {
        // Someone else already ran the initialization.
        if (U_FAILURE(uio.fErrCode)) {
            errCode = uio.fErrCode;
        }
    }
}


// umtx_initOnce variant for plain functions, or static class functions,
//               with a context parameter.
template<class T> void umtx_initOnce(UInitOnce &uio, void (*fp)(T), T context) {
    if (umtx_loadAcquire(uio.fState) == 2) {
        return;
    }
    if (umtx_initImplPreInit(uio)) {
        (*fp)(context);
        umtx_initImplPostInit(uio, TRUE);
    }
}

// umtx_initOnce variant for plain functions, or static class functions,
//               with a context parameter and an error code.
template<class T> void umtx_initOnce(UInitOnce &uio, void (*fp)(T, UErrorCode &), T context, UErrorCode &errCode) {
    if (U_FAILURE(errCode)) {
        return;
    }    
    if (umtx_loadAcquire(uio.fState) != 2 && umtx_initImplPreInit(uio)) {
        // We run the initialization.
        (*fp)(context, errCode);
        uio.fErrCode = errCode;
        umtx_initImplPostInit(uio, TRUE);
    } else {
        // Someone else already ran the initialization.
        if (U_FAILURE(uio.fErrCode)) {
            errCode = uio.fErrCode;
        }
    }
}

#endif /*  U_SHOW_CPLUSPLUS_API */

#if U_PLATFORM_HAS_WIN32_API
typedef struct UMutex {
    UInitOnce         fInitOnce;
    /* CRITICAL_SECTION  fCS; */  /* See note above. Unresolved problems with including
                                   * Windows.h, which would allow using CRITICAL_SECTION
                                   * directly here. */
    char              fCS[U_WINDOWS_CRIT_SEC_SIZE];
} UMutex;


/* Initializer for a static UMUTEX. Deliberately contains no value for the
 *  CRITICAL_SECTION.
 */
#define U_MUTEX_INITIALIZER {U_INITONCE_INITIALIZER}

#elif U_PLATFORM_IMPLEMENTS_POSIX
#include <pthread.h>

struct UMutex {
    pthread_mutex_t  fMutex;
};
#define U_MUTEX_INITIALIZER  {PTHREAD_MUTEX_INITIALIZER}

#else
/* Unknow platform type. */
struct UMutex {
    void *fMutex;
};
#define U_MUTEX_INITIALIZER {NULL}
#error Unknown Platform.

#endif

#if (U_PLATFORM != U_PF_CYGWIN && U_PLATFORM != U_PF_MINGW) || defined(CYGWINMSVC)
typedef struct UMutex UMutex;
#endif
    
/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_CAPI void U_EXPORT2 umtx_lock(UMutex* mutex); 

/* Unlock a mutex.
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_CAPI void U_EXPORT2 umtx_unlock (UMutex* mutex);

/*
 * Atomic Increment and Decrement of an int32_t value.
 *
 * Return Values:
 *   If the result of the operation is zero, the return zero.
 *   If the result of the operation is not zero, the sign of returned value
 *      is the same as the sign of the result, but the returned value itself may
 *      be different from the result of the operation.
 */
U_CAPI int32_t U_EXPORT2 umtx_atomic_inc(int32_t *);
U_CAPI int32_t U_EXPORT2 umtx_atomic_dec(int32_t *);



/*
 *  UMTX_CHECK .   OBSOLETE.
 *                 Delete this section once all uses have been replaced.
 */

#ifndef UMTX_FULL_BARRIER
# if U_HAVE_GCC_ATOMICS
#  define UMTX_FULL_BARRIER __sync_synchronize();
# elif defined(_MSC_VER) && _MSC_VER >= 1500
    /* From MSVC intrin.h. Use _ReadWriteBarrier() only on MSVC 9 and higher. */
#  define UMTX_FULL_BARRIER _ReadWriteBarrier();
# elif U_PLATFORM_IS_DARWIN_BASED
#  define UMTX_FULL_BARRIER OSMemoryBarrier();
# else
#  define UMTX_FULL_BARRIER \
    { \
        umtx_lock(NULL); \
        umtx_unlock(NULL); \
    }
# endif
#endif

#ifndef UMTX_ACQUIRE_BARRIER
# define UMTX_ACQUIRE_BARRIER UMTX_FULL_BARRIER
#endif

/**
 * \def UMTX_CHECK
 * Encapsulates a safe check of an expression
 * for use with double-checked lazy inititialization.
 * Either memory barriers or mutexes are required, to prevent both the hardware
 * and the compiler from reordering operations across the check.
 * The expression must involve only a  _single_ variable, typically
 *    a possibly null pointer or a boolean that indicates whether some service
 *    is initialized or not.
 * The setting of the variable involved in the test must be the last step of
 *    the initialization process.
 *
 * @internal
 */
#define UMTX_CHECK(pMutex, expression, result) \
    { \
        (result)=(expression); \
        UMTX_ACQUIRE_BARRIER; \
    }
/*
 * TODO: Replace all uses of UMTX_CHECK and surrounding code
 * with SimpleSingleton or TriStateSingleton, and remove UMTX_CHECK.
 */

/*
 *   END OF OBSOLETE UMTX_CHECK
 */

#endif /*_CMUTEX*/
/*eof*/
