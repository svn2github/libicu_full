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

#include "stdio.h"   // TODO: remove, debug only.

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

/* On WIN32 mutexes are reentrant.  On POSIX platforms they are not, and a deadlock
 *  will occur if a thread attempts to acquire a mutex it already has locked.
 *  ICU mutexes (in debug builds) include checking code that will cause an assertion
 *  failure if a mutex is reentered.  If you are having deadlock problems
 *  on a POSIX machine, debugging may be easier on Windows.
 */


#if U_PLATFORM_HAS_WIN32_API
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            InterlockedCompareExchangePointer(dest, newval, oldval)

#elif defined(POSIX)
#if (U_HAVE_GCC_ATOMICS == 1)
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            __sync_val_compare_and_swap(dest, oldval, newval)
#else
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            mutexed_compare_and_swap(dest, newval, oldval)
#endif

#else   
/* Unknown platform.  Note that user can still set mutex functions at run time. */
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            mutexed_compare_and_swap(dest, newval, oldval)

#endif

/*  Forward declarations */
static void *mutexed_compare_and_swap(void **dest, void *newval, void *oldval);



static UMutex   globalMutex = U_MUTEX_INITIALIZER;    // The ICU global mutex. Used when ICU implementation code
                                // passes NULL for the mutex pointer.

static UMutex   implMutex = U_MUTEX_INITIALIZER;      


/*
 *  User mutex implementation functions.  If non-null, call back to these rather than
 *  directly using the system (Posix or Windows) APIs.  See u_setMutexFunctions().
 *    (declarations are in uclean.h)
 */
static UMtxInitFn    *pMutexInitFn    = NULL;
static UMtxFn        *pMutexDestroyFn = NULL;
static UMtxFn        *pMutexLockFn    = NULL;
static UMtxFn        *pMutexUnlockFn  = NULL;
static const void    *gMutexContext   = NULL;

#if defined(POSIX)
/*
 *   umtx_lock
 */
U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    int sysErr = pthread_mutex_lock(&mutex->mutex);
    U_ASSERT(sysErr == 0);
}


/*
 * umtx_unlock
 */
U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    int sysErr = pthread_mutex_unlock(&mutex->mutex);
    U_ASSERT(sysErr == 0);
}

#elif U_PLATFORM_HAS_WIN32_API
// TODO

#endif


U_CAPI void U_EXPORT2 
u_setMutexFunctions(const void *context, UMtxInitFn *i, UMtxFn *d, UMtxFn *l, UMtxFn *u,
                    UErrorCode *status) {
#if 0
    if (U_FAILURE(*status)) {
        return;
    }

    /* Can not set a mutex function to a NULL value  */
    if (i==NULL || d==NULL || l==NULL || u==NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    /* If ICU is not in an initial state, disallow this operation. */
    if (cmemory_inUse()) {
        *status = U_INVALID_STATE_ERROR;
        return;
    }
    
    /* Kill any existing global mutex.  POSIX platforms have a global mutex
     * even before any other part of ICU is initialized.
     */
    umtx_destroy(&globalUMTX);

    /* Swap in the mutex function pointers.  */
    pMutexInitFn    = i;
    pMutexDestroyFn = d;
    pMutexLockFn    = l;
    pMutexUnlockFn  = u;
    gMutexContext   = context;

#if defined (POSIX) 
    /* POSIX platforms must have a pre-initialized global mutex 
     * to allow other mutexes to initialize safely. */
    umtx_init(&globalUMTX);
#endif
#endif // temporary removal
}


/*   synchronized compare and swap function, for use when OS or compiler built-in
 *   equivalents aren't available.
 */
static void *mutexed_compare_and_swap(void **dest, void *newval, void *oldval) {
    umtx_lock(&implMutex);
    void *temp = *dest;
    if (temp == oldval) {
        *dest = newval;
    }
    umtx_unlock(&implMutex);
    
    return temp;
}



/*-----------------------------------------------------------------
 *
 *  Atomic Increment and Decrement
 *     umtx_atomic_inc
 *     umtx_atomic_dec
 *
 *----------------------------------------------------------------*/

/* Pointers to user-supplied inc/dec functions.  Null if no funcs have been set.  */
static UMtxAtomicFn  *pIncFn = NULL;
static UMtxAtomicFn  *pDecFn = NULL;
static const void *gIncDecContext  = NULL;

static UMTX    gIncDecMutex = NULL;

U_CAPI int32_t U_EXPORT2
umtx_atomic_inc(int32_t *p)  {
    int32_t retVal;
    if (pIncFn) {
        retVal = (*pIncFn)(gIncDecContext, p);
    } else {
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
    }
    return retVal;
}

U_CAPI int32_t U_EXPORT2
umtx_atomic_dec(int32_t *p) {
    int32_t retVal;
    if (pDecFn) {
        retVal = (*pDecFn)(gIncDecContext, p);
    } else {
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
    }
    return retVal;
}



U_CAPI void U_EXPORT2
u_setAtomicIncDecFunctions(const void *context, UMtxAtomicFn *ip, UMtxAtomicFn *dp,
                                UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return;
    }
    /* Can not set a mutex function to a NULL value  */
    if (ip==NULL || dp==NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    /* If ICU is not in an initial state, disallow this operation. */
    if (cmemory_inUse()) {
        *status = U_INVALID_STATE_ERROR;
        return;
    }

    pIncFn = ip;
    pDecFn = dp;
    gIncDecContext = context;

#if U_DEBUG
    {
        int32_t   testInt = 0;
        U_ASSERT(umtx_atomic_inc(&testInt) == 1);     /* Sanity Check.    Do the functions work at all? */
        U_ASSERT(testInt == 1);
        U_ASSERT(umtx_atomic_dec(&testInt) == 0);
        U_ASSERT(testInt == 0);
    }
#endif
}



/*
 *  Mutex Cleanup Function
 *
 *      reset the mutex function callback pointers.
 */
U_CFUNC UBool umtx_cleanup(void) {
#if 0
    ICUMutex *thisMutex = NULL;
    ICUMutex *nextMutex = NULL;

    /* Extra, do-nothing function call to suppress compiler warnings on platforms where
     *   mutexed_compare_and_swap is not otherwise used.  */
    mutexed_compare_and_swap(&globalUMTX, NULL, NULL);

    /* Delete all of the ICU mutexes.  Do the global mutex last because it is used during
     * the umtx_destroy operation of other mutexes.
     */
    for (thisMutex=mutexListHead; thisMutex!=NULL; thisMutex=nextMutex) {
        UMTX *umtx = thisMutex->owner;
        nextMutex = thisMutex->next;
        U_ASSERT(*umtx = (void *)thisMutex);
        if (umtx != &globalUMTX) {
            umtx_destroy(umtx);
        }
    }
    umtx_destroy(&globalUMTX);
        
    pMutexInitFn    = NULL;
    pMutexDestroyFn = NULL;
    pMutexLockFn    = NULL;
    pMutexUnlockFn  = NULL;
    gMutexContext   = NULL;
    pIncFn          = NULL;
    pDecFn          = NULL;
    gIncDecContext  = NULL;
    gIncDecMutex    = NULL;

#if defined (POSIX) 
    /* POSIX platforms must come out of u_cleanup() with a functioning global mutex 
     * to permit the safe resumption of use of ICU in multi-threaded environments. 
     */
    umtx_init(&globalUMTX);
#endif
#endif
    return TRUE;

}

