/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2001, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "callimts.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/datefmt.h"
#include "unicode/smpdtfmt.h"

void CalendarLimitTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite TestCalendarLimit");
    switch (index) {
        // Re-enable this later
        case 0:
            name = "TestCalendarLimit";
            if (exec) {
                logln("TestCalendarLimit---"); logln("");
                TestCalendarLimit();
            }
            break;
        default: name = ""; break;
    }
}


// *****************************************************************************
// class CalendarLimitTest
// *****************************************************************************

// -------------------------------------

void
CalendarLimitTest::test(UDate millis, Calendar* cal, DateFormat* fmt)
{
    UErrorCode exception = U_ZERO_ERROR;
    UnicodeString theDate;
    UErrorCode status = U_ZERO_ERROR;
    cal->setTime(millis, exception);
    if (U_SUCCESS(exception)) {
        fmt->format(millis, theDate);
        UDate dt = fmt->parse(theDate, status);
        // allow a small amount of error (drift)
        if(! withinErr(dt, millis, 1e-10))
            errln(UnicodeString("FAIL:round trip for large milli, got: ") + dt + " wanted: " + millis);
        else {
            logln(UnicodeString("OK: got ") + dt + ", wanted " + millis);
            logln(UnicodeString("    ") + theDate);
        }
    }        
}
 
// -------------------------------------

// bug 986c: deprecate nextDouble/previousDouble
//|double
//|CalendarLimitTest::nextDouble(double a)
//|{
//|    return uprv_nextDouble(a, TRUE);
//|}
//|
//|double
//|CalendarLimitTest::previousDouble(double a)
//|{
//|    return uprv_nextDouble(a, FALSE);
//|}

UBool
CalendarLimitTest::withinErr(double a, double b, double err)
{
    return ( uprv_fabs(a - b) < uprv_fabs(a * err) ); 
}

void
CalendarLimitTest::TestCalendarLimit()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance")) return;
    cal->adoptTimeZone(TimeZone::createTimeZone("GMT"));
    DateFormat *fmt = DateFormat::createDateTimeInstance();
    fmt->adoptCalendar(cal);
    ((SimpleDateFormat*) fmt)->applyPattern("HH:mm:ss.SSS zzz, EEEE, MMMM d, yyyy G");

    // This test used to test the algorithmic limits of the dates that
    // GregorianCalendar could handle.  However, the algorithm has
    // been rewritten completely since then and the prior limits no
    // longer apply.  Instead, we now do basic round-trip testing of
    // some extreme (but still manageable) dates.
    UDate m;
    for ( m = 1e17; m < 1e18; m *= 1.1) {
        test(m, cal, fmt);
    }
    for ( m = -1e14; m > -1e15; m *= 1.1) {
        test(m, cal, fmt);
    }

    // This is 2^52 - 1, the largest allowable mantissa with a 0
    // exponent in a 64-bit double
    UDate VERY_EARLY_MILLIS = - 4503599627370495.0;
    UDate VERY_LATE_MILLIS  =   4503599627370495.0;

    // I am removing the previousDouble and nextDouble calls below for
    // two reasons: 1. As part of jitterbug 986, I am deprecating
    // these methods and removing calls to them.  2. This test is a
    // non-critical boundary behavior test.
    test(VERY_EARLY_MILLIS, cal, fmt);
    //test(previousDouble(VERY_EARLY_MILLIS), cal, fmt);
    test(VERY_LATE_MILLIS, cal, fmt);
    //test(nextDouble(VERY_LATE_MILLIS), cal, fmt);
    delete fmt;
}

// -------------------------------------

// eof
