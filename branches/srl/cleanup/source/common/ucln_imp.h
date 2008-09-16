/*
******************************************************************************
*                                                                            *
* Copyright (C) 2001-2008, International Business Machines                   *
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

#if defined (UCLN_FINI)
/**
 * If UCLN_FINI is defined, it is the (versioned, etc) name of a cleanup
 * entrypoint. Add a stub to call ucln_common_is_closing.   
 * Used on AIX.
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
#if defined(__GNUC__)
/* GCC - use __attribute((destructor)) */
static void ucln_destructor()   __attribute__((destructor)) ;

static void ucln_destructor() 
{
    ucln_cleanupOne(UCLN_TYPE);
}
#endif
#endif


#else
#error This file can only be included once.
#endif