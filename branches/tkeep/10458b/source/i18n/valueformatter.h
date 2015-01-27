/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef VALUEFORMATTER_H
#define VALUEFORMATTER_H

#if !UCONFIG_NO_FORMATTING

#include "unicode/utypes.h"

#include "unicode/uobject.h"


U_NAMESPACE_BEGIN

class UnicodeString;
class DigitList;
class FieldPositionHandler;
class DigitGrouping;
class PluralRules;
class FixedPrecision;
class DigitFormatter;
class DigitFormatterOptions;
class ScientificPrecision;
class SciFormatter;
class SciFormatterOptions;


/**
 * A closure around rounding and formatting a value. As these instances are
 * designed to be short lived (they only exist while formatting a value), they
 * do not make defensive copies nor do they adopt their attributes. Rather
 * the caller maintains ownership of all attributes.
 */
class ValueFormatter : public UObject {
public:
    ValueFormatter() : fType(kFormatTypeCount) {
    }

    /**
     * Rounds the value according to how it will be formatted.
     * Round must be called to adjust value before calling select.
     * value is expected to be real e.g not Infinity or NaN.
     *
     * @param value this value is rounded in place.
     * @param status any error returned here.
     */
    DigitList &round(DigitList &value, UErrorCode &status) const;

    /**
     * Return the plural form to use for a given value.
     * Value should have already been adjusted with round.
     * @return 'zero', 'one', 'two', 'few', 'many', or 'other'
     */
    UnicodeString select(
        const PluralRules &rules,
        const DigitList &value) const;

    /**
     * formats value and appends to appendTo. Returns appendTo.
     * value must be positive.
     */
    UnicodeString &format(
        const DigitList &positiveValue,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

    /**
     * Returns the number of code points needed.
     * value must be positive.
     */
    int32_t countChar32(const DigitList &positiveValue) const;
  
    /**
     * Prepares this instance for fixed decimal formatting.
     */
    void prepareFixedDecimalFormatting(
        const DigitFormatter &formatter,
        const DigitGrouping &grouping,
        const FixedPrecision &precision,
        const DigitFormatterOptions &options);
    /**
     * Prepares this instance for scientific formatting.
     */
    void prepareScientificFormatting(
        const SciFormatter &sciformatter,
        const DigitFormatter &formatter,
        const ScientificPrecision &precision,
        const SciFormatterOptions &options);
private:
    ValueFormatter(const ValueFormatter &);
    ValueFormatter &operator=(const ValueFormatter &);
    enum FormatType {
        kFixedDecimal,
        kScientificNotation,
        kFormatTypeCount
    };

    FormatType fType;

    // for fixed decimal and scientific formatting
    const DigitFormatter *fDigitFormatter;

    // for fixed decimal formatting
    const FixedPrecision *fFixedPrecision;
    const DigitFormatterOptions *fFixedOptions;
    const DigitGrouping *fGrouping;

    // for scientific formatting
    const SciFormatter *fSciFormatter;
    const ScientificPrecision *fScientificPrecision;
    const SciFormatterOptions *fScientificOptions;
};

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif /* VALUEFORMATTER_H */
