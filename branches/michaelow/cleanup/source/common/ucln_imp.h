/*
******************************************************************************
*                                                                            *
* Copyright (C) 2009, International Business Machines                   *
*                Corporation and others. All Rights Reserved.                *
*                                                                            *
******************************************************************************
*   file name:  ucln_imp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   This file contains the platform specific implementation of per-library cleanup.
*
*/


#ifndef __UCLN_IMP_H__
#define __UCLN_IMP_H__

#include "ucln.h"

#if ENABLE_PER_LIBRARY_CLEANUP

#if defined (UCLN_FINI)
/**
 * If UCLN_FINI is defined, it is the (versioned, etc) name of a cleanup
 * entrypoint. Add a stub to call ucln_common_is_closing.   
 * Used on AIX, Solaris, and HP-UX
 */
U_CAPI void U_EXPORT2 UCLN_FINI (void);

U_CAPI void U_EXPORT2 UCLN_FINI ()
{
#if !UCLN_NO_AUTO_CLEANUP
    /* This function must be defined, if UCLN_FINI is defined, else link error. */
     ucln_cleanupOne(UCLN_TYPE);
#endif
}
#endif

#if !UCLN_NO_AUTO_CLEANUP
/* GCC */
#if defined(__GNUC__)
/* GCC - use __attribute((destructor)) */
static void ucln_destructor()   __attribute__((destructor)) ;

static void ucln_destructor() 
{
    ucln_cleanupOne(UCLN_TYPE);
}
#endif


/* Windows: DllMain */

#if defined (U_WINDOWS)
/* 
 * ICU's own DllMain.
 */

/* these are from putil.c */
/* READ READ READ READ!    Are you getting compilation errors from windows.h?
          Any source file which includes this (ucln_imp.h) header MUST 
          be defined with language extensions ON. */
#   define WIN32_LEAN_AND_MEAN
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#   include <windows.h>
/*
 * This is a stub DllMain function with icu specific process handling code.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL status = TRUE;

    switch(fdwReason) {
        case DLL_PROCESS_ATTACH:
             /* ICU does not trap process attach, but must pass these through properly. */
            /* ICU specific process attach could go here */
            break;

        case DLL_PROCESS_DETACH:
            /* Here is the one we actually care about. */

            ucln_cleanupOne(UCLN_TYPE);

            break;

        case DLL_THREAD_ATTACH:
            /* ICU does not trap thread attach, but must pass these through properly. */
            /* ICU specific thread attach could go here */
            break;

        case DLL_THREAD_DETACH:
            /* ICU does not trap thread detach, but must pass these through properly. */
            /* ICU specific thread detach could go here */
            break;

    }
    return status;
}
#endif

#endif

#endif /* ENABLE_PER_LIBRARY_CLEANUP */

#else
#error This file can only be included once.
#endif
