/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/basictz.h"
#include "unicode/tzrule.h"
#include "unicode/tztrans.h"
#include "gregoimp.h"

U_NAMESPACE_BEGIN

#define MILLIS_PER_YEAR (365*24*60*60*1000.0)

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(BasicTimeZone)

BasicTimeZone::BasicTimeZone()
: TimeZone() {
}

BasicTimeZone::BasicTimeZone(const UnicodeString &id)
: TimeZone(id) {
}

BasicTimeZone::BasicTimeZone(const BasicTimeZone& source)
: TimeZone(source) {
}

BasicTimeZone::~BasicTimeZone() {
}

UBool
BasicTimeZone::hasEquivalentTransitions(/*const*/ BasicTimeZone& tz, UDate start, UDate end,
                                        UBool ignoreDstAmount, UErrorCode& status) /*const*/ {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (hasSameRules(tz)) {
        return TRUE;
    }
    // Check the offsets at the start time
    int32_t raw1, raw2, dst1, dst2;
    getOffset(start, FALSE, raw1, dst1, status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    tz.getOffset(start, FALSE, raw2, dst2, status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (ignoreDstAmount) {
        if ((raw1 + dst1 != raw2 + dst2)
            || (dst1 != 0 && dst2 == 0)
            || (dst1 == 0 && dst2 != 0)) {
            return FALSE;
        }
    } else {
        if (raw1 != raw2 || dst1 != dst2) {
            return FALSE;
        }            
    }
    // Check transitions in the range
    UDate time = start;
    TimeZoneTransition tr1, tr2;
    while (TRUE) {
        UBool avail1 = getNextTransition(time, FALSE, tr1);
        UBool avail2 = tz.getNextTransition(time, FALSE, tr2);

        if (ignoreDstAmount) {
            // Skip a transition which only differ the amount of DST savings
            if (avail1
                    && (tr1.getFrom()->getRawOffset() + tr1.getFrom()->getDSTSavings()
                            == tr1.getTo()->getRawOffset() + tr1.getTo()->getDSTSavings())
                    && (tr1.getFrom()->getDSTSavings() != 0 && tr1.getTo()->getDSTSavings() != 0)) {
                getNextTransition(tr1.getTime(), FALSE, tr1);
            }
            if (avail2
                    && (tr2.getFrom()->getRawOffset() + tr2.getFrom()->getDSTSavings()
                            == tr2.getTo()->getRawOffset() + tr2.getTo()->getDSTSavings())
                    && (tr2.getFrom()->getDSTSavings() != 0 && tr2.getTo()->getDSTSavings() != 0)) {
                getNextTransition(tr2.getTime(), FALSE, tr2);
            }
        }

        UBool inRange1 = (avail1 && tr1.getTime() <= end);
        UBool inRange2 = (avail2 && tr2.getTime() <= end);
        if (!inRange1 && !inRange2) {
            // No more transition in the range
            break;
        }
        if (!inRange1 || !inRange2) {
            return FALSE;
        }
        if (tr1.getTime() != tr2.getTime()) {
            return FALSE;
        }
        if (ignoreDstAmount) {
            if (tr1.getTo()->getRawOffset() + tr1.getTo()->getDSTSavings()
                        != tr2.getTo()->getRawOffset() + tr2.getTo()->getDSTSavings()
                    || tr1.getTo()->getDSTSavings() != 0 &&  tr2.getTo()->getDSTSavings() == 0
                    || tr1.getTo()->getDSTSavings() == 0 &&  tr2.getTo()->getDSTSavings() != 0) {
                return FALSE;
            }
        } else {
            if (tr1.getTo()->getRawOffset() != tr2.getTo()->getRawOffset() ||
                tr1.getTo()->getDSTSavings() != tr2.getTo()->getDSTSavings()) {
                return FALSE;
            }
        }
        time = tr1.getTime();
    }
    return TRUE;
}

void
BasicTimeZone::getSimpleRules(UDate date, InitialTimeZoneRule*& initial,
        AnnualTimeZoneRule*& std, AnnualTimeZoneRule*& dst, UErrorCode& status) /*const*/ {
    initial = NULL;
    std = NULL;
    dst = NULL;
    if (U_FAILURE(status)) {
        return;
    }
    int32_t initialRaw, initialDst;
    UnicodeString initialName;

    AnnualTimeZoneRule *ar1 = NULL;
    AnnualTimeZoneRule *ar2 = NULL;
    UnicodeString name;

    UBool avail;
    TimeZoneTransition tr;
    // Get the next transition
    avail = getNextTransition(date, FALSE, tr);
    if (avail) {
        tr.getFrom()->getName(initialName);
        initialRaw = tr.getFrom()->getRawOffset();
        initialDst = tr.getFrom()->getDSTSavings();

        // Check if the next transition is either DST->STD or STD->DST and
        // within roughly 1 year from the specified date
        UDate nextTransitionTime = tr.getTime();
        if (((tr.getFrom()->getDSTSavings() == 0 && tr.getTo()->getDSTSavings() != 0)
              || (tr.getFrom()->getDSTSavings() != 0 && tr.getTo()->getDSTSavings() == 0))
            && (date + MILLIS_PER_YEAR > nextTransitionTime)) {
 
            int32_t year, month, dom, dow, doy, mid;
            UDate d;

            // Get local wall time for the next transition time
            Grego::timeToFields(nextTransitionTime + initialRaw + initialDst,
                year, month, dom, dow, doy, mid);
            int32_t weekInMonth = Grego::dayOfWeekInMonth(year, month, dom);
            // Create DOW rule
            DateTimeRule *dtr = new DateTimeRule(month, weekInMonth, dow, mid, DateTimeRule::WALL_TIME);
            tr.getTo()->getName(name);
            ar1 = new AnnualTimeZoneRule(name, tr.getTo()->getRawOffset(), tr.getTo()->getDSTSavings(),
                dtr, year, AnnualTimeZoneRule::MAX_YEAR);

            // Get the next next transition
            avail = getNextTransition(nextTransitionTime, FALSE, tr);
            if (avail) {
                // Check if the next next transition is either DST->STD or STD->DST
                // and within roughly 1 year from the next transition
                if (((tr.getFrom()->getDSTSavings() == 0 && tr.getTo()->getDSTSavings() != 0)
                      || (tr.getFrom()->getDSTSavings() != 0 && tr.getTo()->getDSTSavings() == 0))
                     && nextTransitionTime + MILLIS_PER_YEAR > tr.getTime()) {

                    // Get local wall time for the next transition time
                    Grego::timeToFields(tr.getTime() + tr.getFrom()->getRawOffset() + tr.getFrom()->getDSTSavings(),
                        year, month, dom, dow, doy, mid);
                    weekInMonth = Grego::dayOfWeekInMonth(year, month, dom);
                    // Generate another DOW rule
                    dtr = new DateTimeRule(month, weekInMonth, dow, mid, DateTimeRule::WALL_TIME);
                    tr.getTo()->getName(name);
                    ar2 = new AnnualTimeZoneRule(name, tr.getTo()->getRawOffset(), tr.getTo()->getDSTSavings(),
                        dtr, year - 1, AnnualTimeZoneRule::MAX_YEAR);

                    // Make sure this rule can be applied to the specified date
                    avail = ar2->getPreviousStart(date, tr.getFrom()->getRawOffset(), tr.getFrom()->getDSTSavings(), TRUE, d);
                    if (!avail || d > date
                            || initialRaw != tr.getTo()->getRawOffset()
                            || initialDst != tr.getTo()->getDSTSavings()) {
                        // We cannot use this rule as the second transition rule
                        delete ar2;
                        ar2 = NULL;
                    }
                }
            }
            if (ar2 == NULL) {
                // Try previous transition
                avail = getPreviousTransition(date, TRUE, tr);
                if (avail) {
                    // Check if the previous transition is either DST->STD or STD->DST.
                    // The actual transition time does not matter here.
                    if ((tr.getFrom()->getDSTSavings() == 0 && tr.getTo()->getDSTSavings() != 0)
                        || (tr.getFrom()->getDSTSavings() != 0 && tr.getTo()->getDSTSavings() == 0)) {

                        // Generate another DOW rule
                        Grego::timeToFields(tr.getTime() + tr.getFrom()->getRawOffset() + tr.getFrom()->getDSTSavings(),
                            year, month, dom, dow, doy, mid);
                        weekInMonth = Grego::dayOfWeekInMonth(year, month, dom);
                        dtr = new DateTimeRule(month, weekInMonth, dow, mid, DateTimeRule::WALL_TIME);
                        tr.getTo()->getName(name);
                        ar2 = new AnnualTimeZoneRule(name, tr.getTo()->getRawOffset(), tr.getTo()->getDSTSavings(),
                            dtr, ar1->getStartYear() - 1, AnnualTimeZoneRule::MAX_YEAR);

                        // Check if this rule start after the first rule after the specified date
                        avail = ar2->getNextStart(date, tr.getFrom()->getRawOffset(), tr.getFrom()->getDSTSavings(), FALSE, d);
                        if (!avail || d <= nextTransitionTime) {
                            // We cannot use this rule as the second transition rule
                            delete ar2;
                            ar2 = NULL;
                        }
                    }
                }
            }
            if (ar2 == NULL) {
                // Cannot find a good pair of AnnualTimeZoneRule
                delete ar1;
                ar1 = NULL;
            } else {
                // The initial rule should represent the rule before the previous transition
                ar1->getName(initialName);
                initialRaw = ar1->getRawOffset();
                initialDst = ar1->getDSTSavings();
            }
        }
    } else {
        // Try the previous one
        avail = getPreviousTransition(date, TRUE, tr);
        if (avail) {
            tr.getTo()->getName(initialName);
            initialRaw = tr.getTo()->getRawOffset();
            initialDst = tr.getTo()->getDSTSavings();
        } else {
            // No transitions in the past.  Just use the current offsets
            getOffset(date, FALSE, initialRaw, initialDst, status);
            if (U_FAILURE(status)) {
                //TODO
            }
        }
    }
    // Set the initial rule
    initial = new InitialTimeZoneRule(initialName, initialRaw, initialDst);

    // Set the standard and daylight saving rules
    if (ar1 != NULL && ar2 != NULL) {
        if (ar1->getDSTSavings() != 0) {
            dst = ar1;
            std = ar2;
        } else {
            std = ar1;
            dst = ar2;
        }
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
