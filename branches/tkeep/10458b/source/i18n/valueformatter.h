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
class DigitFormatter;
class DigitInterval;

/**
 * A closure around formatting a value. As these instances are designed to
 * be short lived (they only exist while formatting a value), they
 * do not make defensive copies nor do they adopt their attributes. Rather
 * the caller maintains ownership of all attributes.
 */
class ValueFormatter : public UObject {
public:
    ValueFormatter() : fType(kFormatTypeCount) {
    }

    /**
     * formats value and appends to appendTo. Returns appendTo.
     * value must be a real, positive number.
     */
    UnicodeString &format(
        const DigitList &value,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

    /**
     * Returns the number of code points needed.
     */
    int32_t countChar32(const DigitList &value) const;
  
    void prepareFixedDecimalFormatting(
        const DigitFormatter &formatter,
        const DigitGrouping &grouping,
        const DigitInterval &minimumInterval,
        const DigitInterval &maximumInterval,
        UBool alwaysShowDecimal);
private:
    ValueFormatter(const ValueFormatter &);
    ValueFormatter &operator=(const ValueFormatter &);
    enum FormatType {
        kFixedDecimal,
        kScientificNotation,
        kFormatTypeCount
    };

    FormatType fType;

    // for fixed decimal formatting
    const DigitFormatter *fDigitFormatter;
    const DigitGrouping *fGrouping;
    const DigitInterval *fMinimumInterval;
    const DigitInterval *fMaximumInterval;
    UBool fAlwaysShowDecimal;
    DigitInterval &fixedDecimalInterval(
            const DigitList &value, DigitInterval &interval) const;

};

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif /* VALUEFORMATTER_H */
