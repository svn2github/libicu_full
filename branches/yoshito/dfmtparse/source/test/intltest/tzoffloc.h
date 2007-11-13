/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef _TIMEZONEOFFSETLOCALTEST_
#define _TIMEZONEOFFSETLOCALTEST_

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"

#define CASE(id,test) case id: name = #test; if (exec) { logln(#test "---"); logln((UnicodeString)""); test(); } break

class TimeZoneOffsetLocalTest : public IntlTest {
    // IntlTest override
    void runIndexedTest(int32_t index, UBool exec, const char*& name, char* par);

public:
    void TestGetOffsetAroundTransition(void);
};

#endif /* #if !UCONFIG_NO_FORMATTING */
 
#endif // _TIMEZONEOFFSETLOCALTEST_
