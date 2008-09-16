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


/*srl special*/
#define UCLN_DEBUG_CLEANUP 1


#if defined(UCLN_DEBUG_CLEANUP)
#include <stdio.h>
#endif

#if 0
/**
 * Give the library an opportunity to register an automatic cleanup. 
 * This may be called more than once.
 */
static void ucln_registerAutomaticCleanup();

/**
 * Unregister an automatic cleanup, if possible. Called from cleanup.
 */
static void ucln_unRegisterAutomaticCleanup();
#endif

static void ucln_cleanup_internal(ECleanupLibraryType libType) 
{
    if (gLibCleanupFunctions[libType])
    {
        gLibCleanupFunctions[libType]();
        gLibCleanupFunctions[libType] = NULL;
    }
}

U_CAPI void U_EXPORT2 ucln_cleanupOne(ECleanupLibraryType libType)
{
    if(libType==UCLN_COMMON) {
#if defined(UCLN_DEBUG_CLEANUP)
        fprintf(stderr, "Cleaning up: UCLN_COMMON with u_cleanup, type %d\n", (int)libType);
#endif
        u_cleanup();
    } else {
#if defined(UCLN_DEBUG_CLEANUP)
        fprintf(stderr, "Cleaning up: ? with u_cleanup, type %d\n", (int)libType);
#endif
        ucln_cleanup_internal(libType);
    }
}


U_CFUNC void
ucln_common_registerCleanup(ECleanupCommonType type,
                            cleanupFunc *func)
{
    /* ucln_registerAutomaticCleanup(); */
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
    /*ucln_unRegisterAutomaticCleanup(); */
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
        ucln_cleanup_internal(libType);
    }

    for (commonFunc++; commonFunc<UCLN_COMMON_COUNT; commonFunc++) {
        if (gCommonCleanupFunctions[commonFunc])
        {
            gCommonCleanupFunctions[commonFunc]();
            gCommonCleanupFunctions[commonFunc] = NULL;
        }
    }
    /* ucln_unRegisterAutomaticCleanup(); */
    return TRUE;
}



#if 0
/* Automatic registration code. Disabled, as it does not handle per-library cleanup. */


/* ------------ automatic cleanup: registration. Choose ONE ------- */

#if defined(UCLN_AUTO_LOCAL)
/* To use:  
 *  1. define UCLN_AUTO_LOCAL, 
 *  2. create ucln_local_hook.c containing implementations of 
 *           static void ucln_registerAutomaticCleanup()
 *           static void ucln_unRegisterAutomaticCleanup()
 */
#include "ucln_local_hook.c"

#elif defined(UCLN_AUTO_ATEXIT)
/*
 * Use the ANSI C 'atexit' function. Note that this mechanism does not
 * guarantee the order of cleanup relative to other users of ICU!
 */
static UBool gAutoCleanRegistered = FALSE;

static void ucln_atexit_handler() 
{
    ucln_common_is_closing();
}

static void ucln_registerAutomaticCleanup() 
{
    if(!gAutoCleanRegistered) {
        gAutoCleanRegistered = TRUE;
        atexit(&ucln_atexit_handler);
    }
}

static void ucln_unRegisterAutomaticCleanup () {
}

#else 

/*
 * "None of the above" - Default (null) implementation of registration.
 */
static void ucln_registerAutomaticCleanup() 
{
}
static void ucln_unRegisterAutomaticCleanup () {
}

#endif
#endif

/**  Auto-client for UCLN_COMMON **/
#define UCLN_TYPE UCLN_COMMON
#include "ucln_imp.h"
