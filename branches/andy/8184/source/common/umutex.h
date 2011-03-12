/*
**********************************************************************
*   Copyright (C) 1997-2011, International Business Machines
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

#if defined(U_WINDOWS)
# include <intrin.h>
#endif

#if defined(U_DARWIN)
# include <libkern/OSAtomic.h>
#endif

/* Assume POSIX, and modify as necessary below */
#define POSIX

#if defined(U_WINDOWS)
#undef POSIX
#endif
#if defined(macintosh)
#undef POSIX
#endif
#if defined(OS2)
#undef POSIX
#endif

#if defined(POSIX) && (ICU_USE_THREADS==1)
# include <pthread.h> /* must be first, so that we get the multithread versions of things. */

#endif /* POSIX && (ICU_USE_THREADS==1) */

#ifdef U_WINDOWS
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# include <windows.h>
#endif

/*
 * If we do not compile with dynamic_annotations.h then define
 * empty annotation macros.
 *  See http://code.google.com/p/data-race-test/wiki/DynamicAnnotations
 */
#ifndef ANNOTATE_HAPPENS_BEFORE
# define ANNOTATE_HAPPENS_BEFORE(obj)
# define ANNOTATE_HAPPENS_AFTER(obj)
# define ANNOTATE_UNPROTECTED_READ(x) (x)
#endif

/* APP_NO_THREADS is an old symbol. We'll honour it if present. */
#ifdef APP_NO_THREADS
# define ICU_USE_THREADS 0
#endif

/* ICU_USE_THREADS
 *
 *   Allows thread support (use of mutexes) to be compiled out of ICU.
 *   Default: use threads.
 *   Even with thread support compiled out, applications may override the
 *   (empty) mutex implementation with the u_setMutexFunctions() functions.
 */
#ifndef ICU_USE_THREADS
# define ICU_USE_THREADS 1
#endif

#ifndef UMTX_FULL_BARRIER
# if !ICU_USE_THREADS
#  define UMTX_FULL_BARRIER
# elif U_HAVE_GCC_ATOMICS
#  define UMTX_FULL_BARRIER __sync_synchronize();
# elif defined(U_WINDOWS)
#  define UMTX_FULL_BARRIER _ReadWriteBarrier();
# elif defined(U_DARWIN)
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

#ifndef UMTX_RELEASE_BARRIER
# define UMTX_RELEASE_BARRIER UMTX_FULL_BARRIER
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
 * Code within ICU that accesses shared static or global data should
 * lock a UMTX object while doing so.  
 *
 * All UMTX objects must be static.  No explicit initialization is required.
 *
 * For example:
 *
 * static UMTX lock;         // Declare a mutex.
 * void Function(int arg1, int arg2)
 * {
 *   static Object* foo;     // Shared read-write object
 *   umtx_lock(&lock);       // Lock a specific mutex
 *   foo->Method();
 *   umtx_unlock(&lock);
 * }
 *
 * The global ICU mutex is used throughout ICU, so keep locking
 * short and sweet.  
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

#if (ICU_USE_THREADS == 0)
typedef void *MUTEX_TYPE;
#elif defined(U_WINDOWS)
typedef CRITICAL_SECTION MUTEX_TYPE;
#elif defined(POSIX)
typedef pthread_mutex_t MUTEX_TYPE;
#else
// Unknown platform.
typedef void *MUTEX_TYPE;
#endif


typedef struct ICUMutex {
    UMTX          userMutex_;
    MUTEX_TYPE    platformMutex_;
    UBool         initialized_;
}

#if (ICU_USE_THREADS == 0)
#define ICU_MUTEX_INITIALIZER  {NULL, NULL, FALSE}
#elif defined(U_WINDOWS)
// Note: there is no initizer available for a Win32 CRITICAL_SECTION
#define ICU_MUTEX_INITIALIZER {NULL}
#elif defined(POSIX)
#define ICU_MUTEX_INITIALIZER = {NULL, PTHREAD_MUTEX_INITIALIZER, FALSE}
#else
#define ICU_MUTEX_INITIALIZER = {NULL, NULL, FALSE}
#endif


/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_CAPI void U_EXPORT2 umtx_lock   ( ICUMutex* mutex ); 

/* Unlock a mutex. Pass in NULL if you want the single global
   mutex. 
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_CAPI void U_EXPORT2 umtx_unlock ( ICUMutex* mutex );

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
