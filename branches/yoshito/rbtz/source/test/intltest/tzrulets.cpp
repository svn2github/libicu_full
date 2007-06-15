/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dtrule.h"
#include "unicode/tzrule.h"
#include "unicode/rbtz.h"
#include "unicode/simpletz.h"
#include "unicode/tzrule.h"
#include "unicode/calendar.h"
#include "unicode/ucal.h"
#include "unicode/unistr.h"
#include "tzrulets.h"

#define CASE(id,test) case id: name = #test; if (exec) { logln(#test "---"); logln((UnicodeString)""); test(); } break
#define HOUR (60*60*1000)

void TimeZoneRuleTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) {
        logln("TestSuite TestTimeZoneRule");
    }
    switch (index) {
        CASE(0, TestSimpleRuleBasedTimeZone);
        CASE(1, TestHistoricalRuleBasedTimeZone);
        CASE(2, TestOlsonTransition);
        CASE(3, TestRBTZTransition);
        CASE(4, TestHasEquivalentTransitions);
        CASE(5, TestVTimeZoneRoundTrip);
        CASE(6, TestVTimeZoneRoundTripPartial);
        CASE(7, TestVTimeZoneSimpleWrite);
        CASE(8, TestVTimeZoneHeaderProps);
        CASE(9, TestGetSimpleRules);
        default: name = ""; break;
    }
}

/*
 * Compare SimpleTimeZone with equivalent RBTZ
 */
void
TimeZoneRuleTest::TestSimpleRuleBasedTimeZone(void) {
    UErrorCode status = U_ZERO_ERROR;
    SimpleTimeZone stz(-1*HOUR, "TestSTZ",
        UCAL_SEPTEMBER, -30, -UCAL_SATURDAY, 1*HOUR, SimpleTimeZone::WALL_TIME,
        UCAL_FEBRUARY, 2, UCAL_SUNDAY, 1*HOUR, SimpleTimeZone::WALL_TIME,
        1*HOUR, status);
    if (U_FAILURE(status)) {
        errln("FAIL: Couldn't create SimpleTimezone.");
    }

    DateTimeRule *dtr;
    AnnualTimeZoneRule *atzr;
    int32_t STARTYEAR = 2000;

    InitialTimeZoneRule *ir = new InitialTimeZoneRule("RBTZ_Initial", -1*HOUR, 1*HOUR); // starts with DST

    // Original rules
    RuleBasedTimeZone *rbtz1 = new RuleBasedTimeZone("RBTZ1", ir->clone());
    dtr = new DateTimeRule(UCAL_SEPTEMBER, 30, UCAL_SATURDAY, FALSE,
        1*HOUR, DateTimeRule::WALL_TIME);
    atzr = new AnnualTimeZoneRule("RBTZ_DST1", -1*HOUR, 1*HOUR, dtr, STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbtz1->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 1-1.");
    }
    dtr = new DateTimeRule(UCAL_FEBRUARY, 2, UCAL_SUNDAY,
        1*HOUR, DateTimeRule::WALL_TIME);
    atzr = new AnnualTimeZoneRule("RBTZ_STD1", -1*HOUR, 0, dtr, STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbtz1->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 1-2.");
    }
    rbtz1->complete(status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't complete RBTZ 1.");
    }

    // Equivalent, but different date rule type
    RuleBasedTimeZone *rbtz2 = new RuleBasedTimeZone("RBTZ2", ir->clone());
    dtr = new DateTimeRule(UCAL_SEPTEMBER, -1, UCAL_SATURDAY,
        1*HOUR, DateTimeRule::WALL_TIME);
    atzr = new AnnualTimeZoneRule("RBTZ_DST2", -1*HOUR, 1*HOUR, dtr, STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbtz2->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 2-1.");
    }
    dtr = new DateTimeRule(UCAL_FEBRUARY, 8, UCAL_SUNDAY, true,
        1*HOUR, DateTimeRule::WALL_TIME);
    atzr = new AnnualTimeZoneRule("RBTZ_STD2", -1*HOUR, 0, dtr, STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbtz2->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 2-2.");
    }
    rbtz2->complete(status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't complete RBTZ 2");
    }

    // Equivalent, but different time rule type
    RuleBasedTimeZone *rbtz3 = new RuleBasedTimeZone("RBTZ3", ir->clone());
    dtr = new DateTimeRule(UCAL_SEPTEMBER, 30, UCAL_SATURDAY, false,
        2*HOUR, DateTimeRule::UNIVERSAL_TIME);
    atzr = new AnnualTimeZoneRule("RBTZ_DST3", -1*HOUR, 1*HOUR, dtr, STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbtz3->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 3-1.");
    }
    dtr = new DateTimeRule(UCAL_FEBRUARY, 2, UCAL_SUNDAY,
        0*HOUR, DateTimeRule::STANDARD_TIME);
    atzr = new AnnualTimeZoneRule("RBTZ_STD3", -1*HOUR, 0, dtr, STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbtz3->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 3-2.");
    }
    rbtz3->complete(status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't complete RBTZ 3");
    }

    // Check equivalency for 10 years
    UDate start = getUTCMillis(STARTYEAR, UCAL_JANUARY, 1);
    UDate until = getUTCMillis(STARTYEAR + 10, UCAL_JANUARY, 1);

    if (!(stz.hasEquivalentTransitions(*rbtz1, start, until, TRUE, status))) {
        errln("FAIL: rbtz1 must be equivalent to the SimpleTimeZone in the time range.");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }
    if (!(stz.hasEquivalentTransitions(*rbtz2, start, until, TRUE, status))) {
        errln("FAIL: rbtz2 must be equivalent to the SimpleTimeZone in the time range.");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }
    if (!(stz.hasEquivalentTransitions(*rbtz3, start, until, TRUE, status))) {
        errln("FAIL: rbtz3 must be equivalent to the SimpleTimeZone in the time range.");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }

    delete ir;
    delete rbtz1;
    delete rbtz2;
    delete rbtz3;
}

/*
 * Test equivalency between OlsonTimeZone and custom RBTZ representing the
 * equivalent rules in a certain time range
 */
void
TimeZoneRuleTest::TestHistoricalRuleBasedTimeZone(void) {
    UErrorCode status = U_ZERO_ERROR;

    // Compare to America/New_York with equivalent RBTZ
    BasicTimeZone *ny = (BasicTimeZone*)TimeZone::createTimeZone("America/New_York");

    //RBTZ
    InitialTimeZoneRule *ir = new InitialTimeZoneRule("EST", -5*HOUR, 0);
    RuleBasedTimeZone *rbtz = new RuleBasedTimeZone("EST5EDT", ir);

    DateTimeRule *dtr;
    AnnualTimeZoneRule *tzr;

    // Standard time
    dtr = new DateTimeRule(UCAL_OCTOBER, -1, UCAL_SUNDAY, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EST", -5*HOUR, 0, dtr, 1967, 2006);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 1.");
    }

    dtr = new DateTimeRule(UCAL_NOVEMBER, 1, UCAL_SUNDAY, true, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EST", -5*HOUR, 0, dtr, 2007, AnnualTimeZoneRule::MAX_YEAR);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 2.");
    }

    // Daylight saving time
    dtr = new DateTimeRule(UCAL_APRIL, -1, UCAL_SUNDAY, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EDT", -5*HOUR, 1*HOUR, dtr, 1967, 1973);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 3.");
    }

    dtr = new DateTimeRule(UCAL_JANUARY, 6, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EDT", -5*HOUR, 1*HOUR, dtr, 1974, 1974);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 4.");
    }
    
    dtr = new DateTimeRule(UCAL_FEBRUARY, 23, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EDT", -5*HOUR, 1*HOUR, dtr, 1975, 1975);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 5.");
    }

    dtr = new DateTimeRule(UCAL_APRIL, -1, UCAL_SUNDAY, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EDT", -5*HOUR, 1*HOUR, dtr, 1976, 1986);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 6.");
    }

    dtr = new DateTimeRule(UCAL_APRIL, 1, UCAL_SUNDAY, true, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EDT", -5*HOUR, 1*HOUR, dtr, 1987, 2006);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 7.");
    }

    dtr = new DateTimeRule(UCAL_MARCH, 8, UCAL_SUNDAY, true, 2*HOUR, DateTimeRule::WALL_TIME);
    tzr = new AnnualTimeZoneRule("EDT", -5*HOUR, 1*HOUR, dtr, 2007, AnnualTimeZoneRule::MAX_YEAR);
    rbtz->addTransitionRule(tzr, status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't add AnnualTimeZoneRule 7.");
    }

    rbtz->complete(status);
    if (U_FAILURE(status)) {
        errln("FAIL: couldn't complete RBTZ.");
    }

    // hasEquivalentTransitions
    UDate jan1_1950 = getUTCMillis(1950, UCAL_JANUARY, 1);
    UDate jan1_1967 = getUTCMillis(1971, UCAL_JANUARY, 1);
    UDate jan1_2010 = getUTCMillis(2010, UCAL_JANUARY, 1);        

    if (!ny->hasEquivalentTransitions(*rbtz, jan1_1967, jan1_2010, TRUE, status)) {
        errln("FAIL: The RBTZ must be equivalent to America/New_York between 1967 and 2010");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }
    if (ny->hasEquivalentTransitions(*rbtz, jan1_1950, jan1_2010, TRUE, status)) {
        errln("FAIL: The RBTZ must not be equivalent to America/New_York between 1950 and 2010");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }

    // Same with above, but calling RBTZ#hasEquivalentTransitions against OlsonTimeZone
    if (!rbtz->hasEquivalentTransitions(*ny, jan1_1967, jan1_2010, TRUE, status)) {
        errln("FAIL: The RBTZ must be equivalent to America/New_York between 1967 and 2010");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }
    if (rbtz->hasEquivalentTransitions(*ny, jan1_1950, jan1_2010, TRUE, status)) {
        errln("FAIL: The RBTZ must not be equivalent to America/New_York between 1950 and 2010");
    }
    if (U_FAILURE(status)) {
        errln("FAIL: error returned from hasEquivalentTransitions");
    }

    delete ny;
    delete rbtz;
}

void
TimeZoneRuleTest::TestOlsonTransition(void) {
    //TODO
}

void
TimeZoneRuleTest::TestRBTZTransition(void) {
    //TODO
}

void
TimeZoneRuleTest::TestHasEquivalentTransitions(void) {
    //TODO
}

void
TimeZoneRuleTest::TestVTimeZoneRoundTrip(void) {
    //TODO
}

void
TimeZoneRuleTest::TestVTimeZoneRoundTripPartial(void) {
    //TODO
}

void
TimeZoneRuleTest::TestVTimeZoneSimpleWrite(void) {
    //TODO
}

void
TimeZoneRuleTest::TestVTimeZoneHeaderProps(void) {
    //TODO
}

void
TimeZoneRuleTest::TestGetSimpleRules(void) {
    UDate testTimes[] = {
        getUTCMillis(1970, UCAL_JANUARY, 1),
        getUTCMillis(2000, UCAL_MARCH, 31),
        getUTCMillis(2005, UCAL_JULY, 1),
        getUTCMillis(2010, UCAL_NOVEMBER, 1),        
    };
    int32_t numTimes = sizeof(testTimes)/sizeof(UDate);
    UErrorCode status = U_ZERO_ERROR;
    StringEnumeration *tzenum = TimeZone::createEnumeration();
    InitialTimeZoneRule *initial;
    AnnualTimeZoneRule *std, *dst;
    for (int32_t i = 0; i < numTimes ; i++) {
        while (TRUE) {
            const UnicodeString *tzid = tzenum->snext(status);
            if (tzid == NULL) {
                break;
            }
            if (U_FAILURE(status)) {
                errln("FAIL: error returned while enumerating timezone IDs.");
                break;
            }
            BasicTimeZone *tz = (BasicTimeZone*)TimeZone::createTimeZone(*tzid);
            initial = NULL;
            std = dst = NULL;
            tz->getSimpleRules(testTimes[i], initial, std, dst, status);
            if (U_FAILURE(status)) {
                errln("FAIL: getSimpleRules failed.");
                break;
            }
            if (initial == NULL) {
                errln("FAIL: initial rule must not be NULL");
                break;
            } else if (!(std == NULL && dst == NULL || std != NULL && dst != NULL)) {
                errln("FAIL: invalid std/dst pair.");
                break;
            }
            if (std != NULL) {
                const DateTimeRule *dtr = std->getRule();
                if (dtr->getDateRuleType() != DateTimeRule::DOW) {
                    errln("FAIL: simple std rull must use DateTimeRule::DOW as date rule.");
                    break;
                }
                if (dtr->getTimeRuleType() != DateTimeRule::WALL_TIME) {
                    errln("FAIL: simple std rull must use DateTimeRule::WALL_TIME as time rule.");
                    break;
                }
                dtr = dst->getRule();
                if (dtr->getDateRuleType() != DateTimeRule::DOW) {
                    errln("FAIL: simple dst rull must use DateTimeRule::DOW as date rule.");
                    break;
                }
                if (dtr->getTimeRuleType() != DateTimeRule::WALL_TIME) {
                    errln("FAIL: simple dst rull must use DateTimeRule::WALL_TIME as time rule.");
                    break;
                }                
            }
            // Create an RBTZ from the rules and compare the offsets at the date
            RuleBasedTimeZone *rbtz = new RuleBasedTimeZone(*tzid, initial);
            if (std != NULL) {
                rbtz->addTransitionRule(std, status);
                if (U_FAILURE(status)) {
                    errln("FAIL: couldn't add std rule.");
                }
                rbtz->addTransitionRule(dst, status);
                if (U_FAILURE(status)) {
                    errln("FAIL: couldn't add dst rule.");
                }
            }
            rbtz->complete(status);
            if (U_FAILURE(status)) {
                errln("FAIL: couldn't complete rbtz for " + *tzid);
            }

            int32_t raw0, dst0, raw1, dst1;
            tz->getOffset(testTimes[i], FALSE, raw0, dst0, status);
            if (U_FAILURE(status)) {
                errln("FAIL: couldn't get offsets from tz for " + *tzid);
            }
            rbtz->getOffset(testTimes[i], FALSE, raw1, dst1, status);
            if (U_FAILURE(status)) {
                errln("FAIL: couldn't get offsets from rbtz for " + *tzid);
            }
            if (raw0 != raw1 || dst0 != dst1) {
                errln("FAIL: rbtz created by simple rule does not match the original tz for tzid " + *tzid);
            }
            delete rbtz;
            delete tz;
        }
    }
    delete tzenum;
}


UDate
TimeZoneRuleTest::getUTCMillis(int32_t y, int32_t m, int32_t d,
                               int32_t hr, int32_t min, int32_t sec, int32_t msec) {
    UErrorCode status = U_ZERO_ERROR;
    const TimeZone *tz = TimeZone::getGMT();
    Calendar *cal = Calendar::createInstance(*tz, status);
    if (U_FAILURE(status)) {
        delete cal;
        errln("FAIL: Calendar::createInstance failed");
        return 0.0;
    }
    cal->set(y, m, d, hr, min, sec);
    cal->set(UCAL_MILLISECOND, msec);
    UDate utc = cal->getTime(status);
    if (U_FAILURE(status)) {
        delete cal;
        errln("FAIL: Calendar::getTime failed");
        return 0.0;
    }
    delete cal;
    return utc;
}
#endif /* #if !UCONFIG_NO_FORMATTING */
