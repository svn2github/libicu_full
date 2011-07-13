/*
********************************************************************************
*   Copyright (C) 2005-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINNMTST.H
*
********************************************************************************
*/

#ifndef __WINNMTST
#define __WINNMTST

#include "unicode/utypes.h"

#if U_PLATFORM == U_PF_WINDOWS

#if !UCONFIG_NO_FORMATTING

/**
 * \file 
 * \brief C++ API: Format dates using Windows API.
 */

class TestLog;

class Win32NumberTest
{
public:
    static void testLocales(TestLog *log);

private:
    Win32NumberTest();
};

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // U_PLATFORM == U_PF_WINDOWS

#endif // __WINNMTST
