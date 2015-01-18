/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "valueformatter.h"

#include "digitinterval.h"
#include "digitformatter.h"
#include "precision.h"
#include "unicode/unistr.h"
#include "unicode/plurrule.h"
#include "plurrule_impl.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

UnicodeString
ValueFormatter::select(
        const PluralRules &rules,
        const DigitList &value) const {
    switch (fType) {
    case kFixedDecimal:
        {
            DigitInterval interval;
            return rules.select(
                    FixedDecimal(
                            value,
                            fFixedPrecision->getInterval(value, interval)));
        }
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return UnicodeString();
}

DigitList &
ValueFormatter::round(DigitList &value) const {
    switch (fType) {
    case kFixedDecimal:
        return fFixedPrecision->round(value, 0);
    default:
        U_ASSERT(FALSE);
        break;
    }
    return value;
}


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
                    fFixedPrecision->getInterval(value, interval),
                    *fFixedOptions,
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
                    fFixedPrecision->getInterval(value, interval),
                    *fFixedOptions);
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
        const FixedPrecision &precision,
        const DigitFormatterOptions &options) {
    fType = kFixedDecimal;
    fDigitFormatter = &formatter;
    fGrouping = &grouping;
    fFixedPrecision = &precision;
    fFixedOptions = &options;
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
