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

UnicodeString &DigitFormatter::format(
        const DigitList &digits,
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        UBool alwaysShowDecimal,
        UnicodeString &appendTo) const {
    int32_t digitsLeftOfDecimal = interval.getLargestExclusive();
    for (int32_t i = digitsLeftOfDecimal - 1; i >= interval.getSmallestInclusive(); --i) { 
        if (!alwaysShowDecimal && i == -1) {
            appendTo.append(fDecimal);
        }
        appendTo.append(fLocalizedDigits[digits.getDigitByExponent(i)]);
        if (grouping.isSeparatorAt(digitsLeftOfDecimal, i)) {
            appendTo.append(fGroupingSeparator);
        }
        if (alwaysShowDecimal && i == 0) {
            appendTo.append(fDecimal);
        }
    }
    return appendTo;
}


U_NAMESPACE_END

