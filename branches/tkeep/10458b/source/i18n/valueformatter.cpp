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
#include "digitlst.h"
#include "digitformatter.h"
#include "sciformatter.h"
#include "precision.h"
#include "unicode/unistr.h"
#include "unicode/plurrule.h"
#include "plurrule_impl.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

const UChar gOther[] = {0x6f, 0x74, 0x68, 0x65, 0x72, 0x0};

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
    case kScientificNotation:
        return UnicodeString(TRUE, gOther, -1);
    default:
        U_ASSERT(FALSE);
        break;
    }
    return UnicodeString();
}

DigitList &
ValueFormatter::round(DigitList &value, UErrorCode &status) const {
    switch (fType) {
    case kFixedDecimal:
        return fFixedPrecision->round(value, 0, status);
    case kScientificNotation:
        return fScientificPrecision->round(value, status);
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
    case kScientificNotation:
        {
            DigitList mantissa(value);
            int32_t exponent = fScientificPrecision->toScientific(mantissa);
            DigitInterval interval;
            return fSciFormatter->format(
                    mantissa,
                    exponent,
                    *fDigitFormatter,
                    fScientificPrecision->fMantissa.getInterval(mantissa, interval),
                    *fScientificOptions,
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
    case kScientificNotation:
        {
            DigitList mantissa(value);
            int32_t exponent = fScientificPrecision->toScientific(mantissa);
            DigitInterval interval;
            return fSciFormatter->countChar32(
                    exponent,
                    *fDigitFormatter,
                    fScientificPrecision->fMantissa.getInterval(mantissa, interval),
                    *fScientificOptions);
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

void
ValueFormatter::prepareScientificFormatting(
        const SciFormatter &sciformatter,
        const DigitFormatter &formatter,
        const ScientificPrecision &precision,
        const SciFormatterOptions &options) {
    fType = kScientificNotation;
    fSciFormatter = &sciformatter;
    fDigitFormatter = &formatter;
    fScientificPrecision = &precision;
    fScientificOptions = &options;
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
