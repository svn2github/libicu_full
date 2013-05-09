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






// Forward Declarations
struct UMutex;
struct UInitOnce;


/*
 *   Low Level Atomic Ops Definitions. 
 *      Compiler dependent. Not operating system dependent.
 */
#if __cplusplus>=201103L || defined(__GXX_EXPERIMENTAL_CXX0X__)
//  C++11
//
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
#ifdef __cplusplus
inline int32_t umtx_loadAcquire(atomic_int32_t &var) {
	return var;};
inline void umtx_storeRelease(atomic_int32_t &var, int32_t val) {
	var = val;};
#endif /* __cplusplus */


#elif U_HAVE_GCC_ATOMICS
typedef int32_t atomic_int32_t;
#define ATOMIC_INT32_T_INITIALIZER(val) val
#ifdef __cpllusplus
inline int32_t umtx_loadAcquire(atomic_int32_t &var) {
        int32_t val = var;
        __sync_synchronize();
	return var;};
inline void umtx_storeRelease(atomic_int32_t &var, int32_t val) {
        __sync_synchronize();
	var = val;};
#endif /* __cplusplus */


#endif  /* Low Level Atomic Ops Platfrom Chain */


/*
 *  UInitOnce Definitions.
 *     These are platform neutral.
 */
struct UInitOnce {
    atomic_int32_t   fState;
    UErrorCode       fErrCode;
#if __cplusplus
    void reset() {fState = 0; fState=0;};
    UBool isReset() {return umtx_loadAcquire(fState) == 0;};
// Note: isReset() is used by service registration code.
//                 Thread safety of this usage needs review.
#endif
};
typedef struct UInitOnce UInitOnce;
#define U_INITONCE_INITIALIZER {ATOMIC_INT32_T_INITIALIZER(0), U_ZERO_ERROR}

#if __cplusplus
// TODO: get all ICU files using umutex converted to C++,
//       then remove the __cpluplus conditionals from this file.

U_CAPI UBool U_EXPORT2 umtx_initImplPreInit(UInitOnce &);
U_CAPI void  U_EXPORT2 umtx_initImplPostInit(UInitOnce &, UBool success);

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

#endif /*  __cplusplus */



/*
 *  Mutex Definitions. Platform Dependent.
 *         TODO:  Add a C++11 version.
 *                Need to convert all mutex using files to C++ first.
 */


#if U_PLATFORM_HAS_WIN32_API

/* For CRITICAL_SECTION */
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

typedef struct UMutex {
    UInitOnce         fInitOnce;
    /* CRITICAL_SECTION  fCS; */  /* See note above. Unresolved problems with including
                                   * Windows.h, which would allow using CRITICAL_SECTION
                                   * directly here. */
    char              fCS[U_WINDOWS_CRIT_SEC_SIZE];
} UMutex;
typedef struct UMutex UMutex;

/* Initializer for a static UMUTEX. Deliberately contains no value for the
 *  CRITICAL_SECTION.
 */
#define U_MUTEX_INITIALIZER {U_INITONCE_INITIALIZER}



#elif U_PLATFORM_IMPLEMENTS_POSIX
#include <pthread.h>

struct UMutex {
    pthread_mutex_t  fMutex;
};
typedef struct UMutex UMutex;
#define U_MUTEX_INITIALIZER  {PTHREAD_MUTEX_INITIALIZER}

#else
/* Unknow platform type. */
struct UMutex {
    void *fMutex;
};
typedef struct UMutex UMutex;
#define U_MUTEX_INITIALIZER {NULL}
#error Unknown Platform.

#endif


    
/*
 *  Mutex Implementation function declaratations.
 *     Declarations are platform neutral.
 *     Implementations, in umutex.cpp, are platform specific.
 */

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

#endif /*_CMUTEX*/
/*eof*/
