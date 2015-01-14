/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "valueformatter.h"

#include "digitlst.h"
#include "fphdlimp.h"
#include "digitinterval.h"
#include "digitformatter.h"
#include "unicode/unistr.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

UnicodeString &
ValueFormatter::format(
        const DigitList &value,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    switch (fType) {
    case kFixedDecimal:
        {
            DigitInterval interval;
            return fDigitFormatter->format(
                    value,
                    *fGrouping,
                    fixedDecimalInterval(value, interval),
                    fAlwaysShowDecimal,
                    handler,
                    appendTo);
        }
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return appendTo;
}

int32_t
ValueFormatter::countChar32(const DigitList &value) const {
    switch (fType) {
    case kFixedDecimal:
        {
            DigitInterval interval;
            return fDigitFormatter->countChar32(
                    *fGrouping,
                    fixedDecimalInterval(value, interval),
                    fAlwaysShowDecimal);
        }
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return 0;
}

void
ValueFormatter::prepareFixedDecimalFormatting(
        const DigitFormatter &formatter,
        const DigitGrouping &grouping,
        const DigitInterval &minimumInterval,
        const DigitInterval &maximumInterval,
        UBool alwaysShowDecimal) {
    fType = kFixedDecimal;
    fDigitFormatter = &formatter;
    fGrouping = &grouping;
    fMinimumInterval = &minimumInterval;
    fMaximumInterval = &maximumInterval;
    fAlwaysShowDecimal = alwaysShowDecimal;
}

DigitInterval &
ValueFormatter::fixedDecimalInterval(
        const DigitList &value, DigitInterval &interval) const {
    value.getSmallestInterval(interval);
    interval.expandToContain(*fMinimumInterval);
    interval.shrinkToFitWithin(*fMaximumInterval);
    return interval;
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
