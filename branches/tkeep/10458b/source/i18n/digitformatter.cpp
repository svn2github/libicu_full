/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitformatter.cpp
 */

#include "unicode/utypes.h"

#include "digitformatter.h"
#include "digitinterval.h"
#include "digitlst.h"
#include "digitgrouping.h"
#include "unicode/dcfmtsym.h"
#include "unicode/unum.h"
#include "fphdlimp.h"


U_NAMESPACE_BEGIN

DigitFormatter::DigitFormatter(const DecimalFormatSymbols &symbols) {
    fLocalizedDigits[0] = symbols.getConstSymbol(DecimalFormatSymbols::kZeroDigitSymbol).char32At(0);
    fLocalizedDigits[1] = symbols.getConstSymbol(DecimalFormatSymbols::kOneDigitSymbol).char32At(0);
    fLocalizedDigits[2] = symbols.getConstSymbol(DecimalFormatSymbols::kTwoDigitSymbol).char32At(0);
    fLocalizedDigits[3] = symbols.getConstSymbol(DecimalFormatSymbols::kThreeDigitSymbol).char32At(0);
    fLocalizedDigits[4] = symbols.getConstSymbol(DecimalFormatSymbols::kFourDigitSymbol).char32At(0);
    fLocalizedDigits[5] = symbols.getConstSymbol(DecimalFormatSymbols::kFiveDigitSymbol).char32At(0);
    fLocalizedDigits[6] = symbols.getConstSymbol(DecimalFormatSymbols::kSixDigitSymbol).char32At(0);
    fLocalizedDigits[7] = symbols.getConstSymbol(DecimalFormatSymbols::kSevenDigitSymbol).char32At(0);
    fLocalizedDigits[8] = symbols.getConstSymbol(DecimalFormatSymbols::kEightDigitSymbol).char32At(0);
    fLocalizedDigits[9] = symbols.getConstSymbol(DecimalFormatSymbols::kNineDigitSymbol).char32At(0);
    fGroupingSeparator = symbols.getConstSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol);
    fDecimal = symbols.getConstSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
}

int32_t DigitFormatter::countChar32(
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        UBool alwaysShowDecimal) const {
    int32_t result = interval.length();
    if (alwaysShowDecimal || interval.getLeastSignificantInclusive() < 0) {
        result += fDecimal.countChar32();
    }
    result += grouping.getSeparatorCount(interval.getIntDigitCount()) * fGroupingSeparator.countChar32();
    return result;
}


UnicodeString &DigitFormatter::format(
        const DigitList &digits,
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        UBool alwaysShowDecimal,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    int32_t digitsLeftOfDecimal = interval.getMostSignificantExclusive();
    int32_t lastDigitPos = interval.getLeastSignificantInclusive();
    int32_t intBegin = appendTo.length();
    int32_t currentLength;
    int32_t fracBegin;
    for (int32_t i = digitsLeftOfDecimal - 1; i >= lastDigitPos; --i) { 
        if (i == -1) {
            if (!alwaysShowDecimal) {
                currentLength = appendTo.length();
                appendTo.append(fDecimal);
                handler.addAttribute(UNUM_DECIMAL_SEPARATOR_FIELD, currentLength, appendTo.length());
            }
            fracBegin = appendTo.length();
        }
        appendTo.append(fLocalizedDigits[digits.getDigitByExponent(i)]);
        if (grouping.isSeparatorAt(digitsLeftOfDecimal, i)) {
            currentLength = appendTo.length();
            appendTo.append(fGroupingSeparator);
            handler.addAttribute(UNUM_GROUPING_SEPARATOR_FIELD, currentLength, appendTo.length());
        }
        if (i == 0) {
            if (digitsLeftOfDecimal > 0) {
                handler.addAttribute(UNUM_INTEGER_FIELD, intBegin, appendTo.length());
            }
            if (alwaysShowDecimal) {
                currentLength = appendTo.length();
                appendTo.append(fDecimal);
                handler.addAttribute(
                        UNUM_DECIMAL_SEPARATOR_FIELD,
                        currentLength,
                        appendTo.length());
                
            }
        }
    }
    // lastDigitPos is never > 0 so we are guaranteed that kIntegerField
    // is already added.
    if (lastDigitPos < 0) {
        handler.addAttribute(UNUM_FRACTION_FIELD, fracBegin, appendTo.length());
    }
    return appendTo;
}


U_NAMESPACE_END

