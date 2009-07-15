/*
******************************************************************************
*
*   Copyright (C) 1997-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File umutex.c
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

#if defined(U_DARWIN)
#include <AvailabilityMacros.h>
#if (ICU_USE_THREADS == 1) && defined(MAC_OS_X_VERSION_10_4) && defined(MAC_OS_X_VERSION_MIN_REQUIRED) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
#if defined(__STRICT_ANSI__)
#define UPRV_REMAP_INLINE
#define inline
#endif
#include <libkern/OSAtomic.h>
#define USE_MAC_OS_ATOMIC_INCREMENT 1
#if defined(UPRV_REMAP_INLINE)
#undef inline
#undef UPRV_REMAP_INLINE
#endif
#endif
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

#include "umutex.h"
#include "cmemory.h"

/*
 * A note on ICU Mutex Initialization and ICU startup:
 *
 *   ICU mutexes, as used through the rest of the ICU code, are self-initializing.
 *   To make this work, ICU uses the _ICU GLobal Mutex_ to synchronize the lazy init
 *   of other ICU mutexes.  For the global mutex itself, we need some other mechanism
 *   to safely initialize it on first use.  This becomes important if two or more
 *   threads were more or less simultaenously the first to use ICU in a process, and
 *   were racing into the mutex initialization code.
 *
 *   Double checked lazy init of OS level mutexes works where it wouldn't with
 *   normal user level data because the OS level lock functions have to
 *   include whatever memory voodoo may be required to ensure that they see current 
 *   mutex state.
 *
 *   The solution for the global mutex init is platform dependent.
 *   On POSIX systems, C-style init can be used on a mutex, with the 
 *   macro PTHREAD_MUTEX_INITIALIZER.  The mutex is then ready for use, without
 *   first calling pthread_mutex_init().
 *
 *   Windows has no equivalent statically initialized mutex or CRITICAL SECION.
 *   InitializeCriticalSection() must be called.  If the global mutex does not
 *   appear to be initialized, a thread will create and initialize a new
 *   CRITICAL_SECTION, then use a Windows InterlockedCompareAndExchange to
 *   avoid problems with race conditions.
 *
 *   If an application has overridden the ICU mutex implementation
 *   by calling u_setMutexFunctions(), the user supplied init function must
 *   be safe in the event that multiple threads concurrently attempt to init
 *   the same mutex.  The first thread should do the init, and the others should
 *   have no effect.
 *
 */ 

/* On WIN32 mutexes are reentrant.  On POSIX platforms they are not, and a deadlock
 * will occur if a thread attempts to acquire a mutex it already has locked.
 * ICU mutexes (in debug builds) include checking code that will cause an assertion
 * failure if a mutex is reentered.  If you are having deadlock problems
 * on a POSIX machine, debugging may be easier on Windows.
 */



#if (ICU_USE_THREADS == 0)
#define MUTEX_TYPE void *
#define PLATFORM_MUTEX_INIT(m) 
#define PLATFORM_MUTEX_LOCK(m) 
#define PLATFORM_MUTEX_UNLOCK(m) 
#define PLATFORM_MUTEX_DESTROY(m) 

#elif defined(U_WINDOWS)
#define MUTEX_TYPE CRITICAL_SECTION
#define PLATFORM_MUTEX_INIT(m) InitializeCriticalSection(m)
#define PLATFORM_MUTEX_LOCK(m) EnterCriticalSection(m)
#define PLATFORM_MUTEX_UNLOCK(m) LeaveCriticalSection(m)
#define PLATFORM_MUTEX_DESTROY(m) DeleteCriticalSection(m)

#elif defined(POSIX)
#define MUTEX_TYPE pthread_mutex_t   
#define PLATFORM_MUTEX_INIT(m) pthread_mutex_init(m, NULL)
#define PLATFORM_MUTEX_LOCK(m) pthread_mutex_lock(m)
#define PLATFORM_MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
#define PLATFORM_MUTEX_DESTROY(m) pthread_mutex_destroy(m)

#else   
/* Unknown platform.  Note that user can still set mutex functions at run time. */
#define MUTEX_TYPE void *
#define PLATFORM_MUTEX_INIT(m) 
#define PLATFORM_MUTEX_LOCK(m)
#define PLATFORM_MUTEX_UNLOCK(m) 
#define PLATFORM_MUTEX_DESTROY(m) 
#endif


typedef struct ICUMutex ICUMutex;

struct ICUMutex {
    UBool        heapAllocated;
    ICUMutex    *next;
    int32_t      recursionCount;
    MUTEX_TYPE   platformMutex;
    UMTX         userMutex;   /* TODO:  make a union?  */
};


#if defined(POSIX)
static ICUMutex globalMutex = {FALSE, NULL, 0, PTHREAD_MUTEX_INITIALIZER};
UMTX  gGlobalMutex = &globalMutex;
#else
UMTX  gGlobalMutex = NULL;
#endif

/* Head of the list of all ICU mutexes.
 * Linked list is through ICUMutex::next
 * Modifications to the list are synchronized with the global mutex.
 * Needed for u_cleanup(), which needs to dispose of all of the ICU mutexes.
 */
static ICUMutex *mutexListHead;


/*
 *  User mutex implementation functions.  If non-null, call back to these rather than
 *  directly using the system (Posix or Windows) APIs.
 *    (declarations are in uclean.h)
 */
static UMtxInitFn    *pMutexInitFn    = NULL;
static UMtxFn        *pMutexDestroyFn = NULL;
static UMtxFn        *pMutexLockFn    = NULL;
static UMtxFn        *pMutexUnlockFn  = NULL;
static const void    *gMutexContext   = NULL;



/*
 *   umtx_lock
 */
U_CAPI void  U_EXPORT2
umtx_lock(UMTX *mutex)
{
    ICUMutex *m;

    if (mutex == NULL) {
        mutex = &gGlobalMutex;
    }
    m = (ICUMutex *)*mutex;
    if (m == NULL) {
        /* See note on double-checked lazy init, above.  We can get away with it here, with mutexes,
         * where we couldn't with normal user level data.
         */
        umtx_init(mutex);    
        m = (ICUMutex *)*mutex;
    }

    if (pMutexLockFn != NULL) {
        (*pMutexLockFn)(gMutexContext, &m->userMutex);
    } else {
        PLATFORM_MUTEX_LOCK(&m->platformMutex);
    }

#if defined(U_DEBUG)
        m->recursionCount++;              /* Recursion causes deadlock on Unixes.               */
        U_ASSERT(m->recursionCount == 1); /* Recursion detection works on Windows.              */
                                          /* Assertion failure on non-Windows indicates a       */
                                          /*   problem with the mutex implementation itself.    */
#endif
}



/*
 * umtx_unlock
 */
U_CAPI void  U_EXPORT2
umtx_unlock(UMTX* mutex)
{
    ICUMutex *m;
    if(mutex == NULL) {
        mutex = &gGlobalMutex;
    }
    m = (ICUMutex *)*mutex;
    if (m == NULL) {
        U_ASSERT(FALSE);  /* This mutex is not initialized.     */
        return; 
    }

#if defined (U_DEBUG)
    m->recursionCount--;
    U_ASSERT(m->recursionCount == 0);  /* Detect unlock of an already unlocked mutex */
#endif

    if (pMutexUnlockFn) {
        (*pMutexUnlockFn)(gMutexContext, &m->userMutex);
    } else {
        PLATFORM_MUTEX_UNLOCK(&m->platformMutex);
    }
}


/* umtx_ct   Allocate and initialize a new ICUMutex.
 *           If a non-null pointer is supplied, initialize an existing ICU Mutex.
 */
static ICUMutex *umtx_ct(ICUMutex *m) {
    if (m == NULL) {
        m = (ICUMutex *)uprv_malloc(sizeof(ICUMutex));
        m ->heapAllocated = TRUE;
    }
    m->next = NULL;
    m->recursionCount = 0;
    if (pMutexInitFn != NULL) {
        UErrorCode status = U_ZERO_ERROR;
        (*pMutexInitFn)(gMutexContext, &m->userMutex, &status);
        U_ASSERT(U_SUCCESS(status));
    } else {
        PLATFORM_MUTEX_INIT(&m->platformMutex);
    }
    return m;
}


/* umtx_dt   Delete a ICUMutex.  Destroy the underlying OS Platform mutex.
 *           Does not touch the linked list of ICU Mutexes.
 *           Statically allocated mutexes are returned to their initial state.
 */
static void umtx_dt(ICUMutex *m) {
    if (pMutexDestroyFn != NULL) {
        (*pMutexDestroyFn)(gMutexContext, &m->userMutex);
    } else {
        PLATFORM_MUTEX_DESTROY(&m->platformMutex);
    }

    if (m->heapAllocated) {
        uprv_free(m);
    } else {
        /* The only non-heap allocated mutex is the global mutex on POSIX platforms,
         * and it must be initialized and ready to use in the event that ICU is restarted.
         */
        umtx_ct(m);
    }
        
}
    

U_CAPI void  U_EXPORT2
umtx_init(UMTX *mutex) {
    ICUMutex *m;
    void *originalValue;

    if (*mutex != NULL) {
        return;
    }
    m = umtx_ct(NULL);

#if defined(U_WINDOWS)
    originalValue = InterlockedCompareExchangePointer(mutex, m, NULL);
#else
    /* For POSIX, the global mutex is always available.
     * For unknown platforms, the global mutex will work if there are user defined mutex functions.
     */
    umtx_lock(NULL);
    originalValue = *mutex;
    if (originalValue == NULL) {
        *mutex = m;
    }
    umtx_unlock(NULL);
#endif

    if (originalValue != NULL) {
        umtx_dt(m);
        return;
    }

    /* Hook the new mutex into the list of all ICU mutexes, so that we can find and
     * delete it for u_cleanup().
     * This is a little tricky when we are initializing the global mutex on Windows.  We will do
     * a recursive call to umtx_lock(), but by this point the mutex is initialized and we will not
     * reenter umtx_init().
     */

    umtx_lock(NULL);
    m->next = mutexListHead;
    mutexListHead = m;
    umtx_unlock(NULL);
    return;
}


/*
 *  umtx_destroy.    Un-initialize a mutex, releasing any underlying resources
 *                   that it may be holding.  Destroying an already destroyed
 *                   mutex has no effect.  Unlike umtx_init(), this function
 *                   is not thread safe;  two threads must not concurrently try to
 *                   destroy the same mutex.
 */                  
U_CAPI void  U_EXPORT2
umtx_destroy(UMTX *mutex) {
    ICUMutex *m;
    if (mutex == NULL) {  /* destroy the global mutex */
        mutex = &gGlobalMutex;
    }
    
    m = (ICUMutex *)*mutex;
    if (m == NULL) {  /* Mutex not initialized  */
        return;
    }

    /* Remove this mutex from the linked list of mutexes.  Do before destroying the
     * underlying OS mutex, because this might be the ICU global mutex
     */
    umtx_lock(NULL);
    if (mutexListHead == m) {
        mutexListHead = m->next;
    } else {
        ICUMutex *prev;
        for (prev = mutexListHead; prev!=NULL && prev->next!=m; prev = prev->next);
            /*  Empty for body */
        if (prev != NULL) {
            prev->next = m->next;
        }
    }
    umtx_unlock(NULL);

    if (pMutexDestroyFn != NULL) {
        (*pMutexDestroyFn)(gMutexContext, &m->userMutex);
    }
    else {
        PLATFORM_MUTEX_DESTROY(&m->platformMutex);
    }

    *mutex = NULL;
}



U_CAPI void U_EXPORT2 
u_setMutexFunctions(const void *context, UMtxInitFn *i, UMtxFn *d, UMtxFn *l, UMtxFn *u,
                    UErrorCode *status) {
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

    /* Swap in the mutex function pointers.  */
    pMutexInitFn    = i;
    pMutexDestroyFn = d;
    pMutexLockFn    = l;
    pMutexUnlockFn  = u;
    gMutexContext   = context;

    /* If there is a static global ICU mutex, initialize it.
     * This will be the case for non-Windows platforms.
     */
    if (gGlobalMutex != NULL) {
        ICUMutex *m = (ICUMutex *)gGlobalMutex;
        umtx_ct(m);
    }
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
        #if defined (U_WINDOWS) && ICU_USE_THREADS == 1
            retVal = InterlockedIncrement((LONG*)p);
        #elif defined(USE_MAC_OS_ATOMIC_INCREMENT)
            retVal = OSAtomicIncrement32Barrier(p);
        #elif defined (POSIX) && ICU_USE_THREADS == 1
            umtx_lock(&gIncDecMutex);
            retVal = ++(*p);
            umtx_unlock(&gIncDecMutex);
        #else
            /* Unknown Platform, or ICU thread support compiled out. */
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
        #if defined (U_WINDOWS) && ICU_USE_THREADS == 1
            retVal = InterlockedDecrement((LONG*)p);
        #elif defined(USE_MAC_OS_ATOMIC_INCREMENT)
            retVal = OSAtomicDecrement32Barrier(p);
        #elif defined (POSIX) && ICU_USE_THREADS == 1
            umtx_lock(&gIncDecMutex);
            retVal = --(*p);
            umtx_unlock(&gIncDecMutex);
        #else
            /* Unknown Platform, or ICU thread support compiled out. */
            retVal = --(*p);
        #endif
    }
    return retVal;
}

/* TODO:  Some POSIXy platforms have atomic inc/dec functions available.  Use them. */





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

#if !U_RELEASE
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
 *      Destroy the global mutex(es), and reset the mutex function callback pointers.
 */
U_CFUNC UBool umtx_cleanup(void) {

    /*  TODO:  destroy all mutexes.  */
    
    pMutexInitFn    = NULL;
    pMutexDestroyFn = NULL;
    pMutexLockFn    = NULL;
    pMutexUnlockFn  = NULL;
    gMutexContext   = NULL;
    gGlobalMutex    = NULL;
    pIncFn          = NULL;
    pDecFn          = NULL;
    gIncDecContext  = NULL;
    gIncDecMutex    = NULL;
    return TRUE;
}


