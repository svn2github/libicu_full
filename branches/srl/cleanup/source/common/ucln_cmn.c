/*
******************************************************************************
*                                                                            *
* Copyright (C) 2001-2008, International Business Machines                   *
*                Corporation and others. All Rights Reserved.                *
*                                                                            *
******************************************************************************
*   file name:  ucln_cmn.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "utracimp.h"
#include "ustr_imp.h"
#include "unormimp.h"
#include "ucln_cmn.h"
#include "umutex.h"
#include "ucln.h"
#include "cmemory.h"
#include "uassert.h"

static cleanupFunc *gCommonCleanupFunctions[UCLN_COMMON_COUNT];
static cleanupFunc *gLibCleanupFunctions[UCLN_COMMON];

/**
 * Define this to completely disable any automatic calls to u_cleanup
 */
#ifndef UCLN_NO_AUTO_CLEANUP
#define UCLN_NO_AUTO_CLEANUP 0
#endif

/*srl special*/
#define UCLN_DEBUG_CLEANUP 1


#if defined(UCLN_DEBUG_CLEANUP)
#include <stdio.h>
#endif



#if !UCLN_NO_AUTO_CLEANUP
/**
 * Give the library an opportunity to register an automatic cleanup. 
 * This may be called more than once.
 */
static void ucln_registerAutomaticCleanup();

/**
 * Unregister an automatic cleanup, if possible.
 */
static void ucln_unRegisterAutomaticCleanup();
#endif

U_CFUNC void
ucln_common_registerCleanup(ECleanupCommonType type,
                            cleanupFunc *func)
{
    ucln_registerAutomaticCleanup();
    U_ASSERT(UCLN_COMMON_START < type && type < UCLN_COMMON_COUNT);
    if (UCLN_COMMON_START < type && type < UCLN_COMMON_COUNT)
    {
        gCommonCleanupFunctions[type] = func;
    }
}

U_CAPI void U_EXPORT2
ucln_registerCleanup(ECleanupLibraryType type,
                     cleanupFunc *func)
{
#if !UCLN_NO_AUTO_CLEANUP
    ucln_unRegisterAutomaticCleanup();
#endif
    U_ASSERT(UCLN_START < type && type < UCLN_COMMON);
    if (UCLN_START < type && type < UCLN_COMMON)
    {
        gLibCleanupFunctions[type] = func;
    }
}

U_CFUNC UBool ucln_lib_cleanup(void) {
    ECleanupLibraryType libType = UCLN_START;
    ECleanupCommonType commonFunc = UCLN_COMMON_START;

    for (libType++; libType<UCLN_COMMON; libType++) {
        if (gLibCleanupFunctions[libType])
        {
            gLibCleanupFunctions[libType]();
            gLibCleanupFunctions[libType] = NULL;
        }
    }

    for (commonFunc++; commonFunc<UCLN_COMMON_COUNT; commonFunc++) {
        if (gCommonCleanupFunctions[commonFunc])
        {
            gCommonCleanupFunctions[commonFunc]();
            gCommonCleanupFunctions[commonFunc] = NULL;
        }
    }
#if !UCLN_NO_AUTO_CLEANUP
    ucln_unRegisterAutomaticCleanup();
#endif
    return TRUE;
}


/** 
 Automatic cleanups
 **/
#if defined (UCLN_FINI)
U_CAPI void U_EXPORT2 UCLN_FINI (void);
#endif

#if UCLN_NO_AUTO_CLEANUP
/* No automatic cleanups - do nothing.*/

#if defined (UCLN_FINI)
U_CAPI void U_EXPORT2 UCLN_FINI ()
{
}
#endif

#elif defined(UCLN_AUTO_LOCAL)
/* To use:  define UCLN_AUTO_LOCAL, and create ucln_local_hook.c containing implementations of 
 * static void functions ucln_registerAutomaticCleanup() and ucln_unRegisterAutomaticCleanup().
 */
# include "ucln_local_hook.c"

#elif defined(UCLN_AUTO_ATEXIT)
/*
 * Use the 'atexit' function
 */
static UBool gAutoCleanRegistered = FALSE;

static void ucln_atexit_handler() 
{
    u_cleanup();
}

static void ucln_registerAutomaticCleanup() 
{
    if(!gAutoCleanRegistered) {
        gAutoCleanRegistered = TRUE;
        atexit(&ucln_atexit_handler);
    }
}

static void ucln_unRegisterAutomaticCleanup () {
    /* Can't unregister. */
}
#elif defined(__GNUC__)
/* GCC - use __attribute((destructor)) */
static void ucln_destructor()   __attribute__((destructor)) ;

static void ucln_destructor() 
{
    u_cleanup();
#if defined(UCLN_DEBUG_CLEANUP)
    puts("ucln_cmn.c: ucln_destructor() was called, ICU unloaded.");
#endif
}

static void ucln_registerAutomaticCleanup() 
{
    /* nothing to do. */
}
static void ucln_unRegisterAutomaticCleanup () {
    /* Can't unregister. */
}

#elif defined(UCLN_FINI) 

U_CAPI void U_EXPORT2 UCLN_FINI ()
{
    u_cleanup();
#if defined(UCLN_DEBUG_CLEANUP)
    puts("ucln_cmn.c: ucln_fini() was called, ICU unloaded.");
#endif
}


static void ucln_registerAutomaticCleanup() 
{
    /* nothing to do. */
}
static void ucln_unRegisterAutomaticCleanup () {
    /* Can't unregister. */
}

#elif defined (U_WINDOWS) && !defined(__GNUC__)

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE /*hinstDLL*/, DWORD fdwReason, LPVOID /*lpReserved*/ )
{
    switch( fdwReason ) 
    { 
//        case DLL_PROCESS_ATTACH:
//            break;
//
//        case DLL_THREAD_ATTACH:
//            break;
//
//        case DLL_THREAD_DETACH:
//            break;

        case DLL_PROCESS_DETACH:
                u_cleanup();
#if defined(UCLN_DEBUG_CLEANUP)
                puts("ucln_cmn.c: ucln_fini() was called, ICU unloaded.");
#endif         
            break;
    }
    return TRUE;
}


static void ucln_registerAutomaticCleanup() 
{
    /* nothing to do. */
}
static void ucln_unRegisterAutomaticCleanup () {
    /* Can't unregister. */
}

#else

/* Null implementation. */
static void ucln_registerAutomaticCleanup() 
{
#if defined(UCLN_DEBUG_CLEANUP)
    puts("ucln_cmn.c: ucln_registerAutomaticCleanup() was called - no implementation.");
#endif
    /* disabled. */
}
static void ucln_unRegisterAutomaticCleanup() 
{
#if defined(UCLN_DEBUG_CLEANUP)
    puts("ucln_cmn.c: ucln_unRegisterAutomaticCleanup() was called - no implementation.");
#endif
    /* disabled. */
}
#endif
