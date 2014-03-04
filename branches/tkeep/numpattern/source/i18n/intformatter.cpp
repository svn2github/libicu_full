/*
******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
******************************************************************************
*                                                                             
* File INTFORMATTER.CPP                                                             
******************************************************************************
*/

#include "intformatter.h"
#include "unicode/decimfmt.h"
#include "cmemory.h"
#include "unicode/plurrule.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

static UChar gNegPrefix[] = {0x2D, 0x0};
static UChar gDigits[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};

static UChar asUChar(const UnicodeString &str, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0x0;
    }
    if (str.length() > 1) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0x0;
    }
    return str[0];
}

IntFormatter::IntFormatter() :
        fGroupingSize(0), fGroupingSize2(0), fGroupingSeparator(),
        fPosPrefix(), fNegPrefix(TRUE, gNegPrefix, -1), fPosSuffix(),
        fNegSuffix() {
    uprv_memcpy(fDigits, gDigits, 10 * sizeof(UChar));
}

IntFormatter::~IntFormatter() { }

UBool IntFormatter::like(const DecimalFormat &df) {
    fGroupingSize = INT32_MAX;
    fGroupingSize2 = INT32_MAX;
    if (df.isGroupingUsed()) {
        fGroupingSize = df.getGroupingSize();
        int32_t groupingSize2 = df.getSecondaryGroupingSize();
        fGroupingSize2 = groupingSize2 <= 0 ? fGroupingSize : groupingSize2;
    }
    df.getPositivePrefix(fPosPrefix);
    df.getNegativePrefix(fNegPrefix);
    df.getPositiveSuffix(fPosSuffix);
    df.getNegativeSuffix(fNegSuffix);

    const DecimalFormatSymbols *symbols = df.getDecimalFormatSymbols();
    if (symbols == NULL) {
        return FALSE;
    }
    fGroupingSeparator = symbols->getSymbol(
            DecimalFormatSymbols::kGroupingSeparatorSymbol);
    UErrorCode status = U_ZERO_ERROR;
    fDigits[0] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kZeroDigitSymbol), status);
    fDigits[1] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kOneDigitSymbol), status);
    fDigits[2] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kTwoDigitSymbol), status);
    fDigits[3] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kThreeDigitSymbol), status);
    fDigits[4] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kFourDigitSymbol), status);
    fDigits[5] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kFiveDigitSymbol), status);
    fDigits[6] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kSixDigitSymbol), status);
    fDigits[7] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kSevenDigitSymbol), status);
    fDigits[8] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kEightDigitSymbol), status);
    fDigits[9] = asUChar(symbols->getSymbol(
            DecimalFormatSymbols::kNineDigitSymbol), status);
    return U_SUCCESS(status);
}

class SimpleAnnotator {
public:
    SimpleAnnotator(int32_t f) :
            field(f), start(-1), end(-1) { }
    void markStart(int32_t f, int32_t pos) {
        if (f == field && (start < 0 || end < 0)) {
            start = pos;
        }
    }
    void markEnd(int32_t f, int32_t pos) {
        if (f == field && (start < 0 || end < 0)) {
            end = pos;
        }
    }
    void update(FieldPosition &pos) const {
        if (start >= 0 && end >= 0) {
            pos.setBeginIndex(start);
            pos.setEndIndex(end);
        }
    }
private:
    int32_t field;
    int32_t start;
    int32_t end;
};

UnicodeString &IntFormatter::select(
            const Formattable &quantity,
            const PluralRules &rules,
            UnicodeString &result,
            UErrorCode &status) const {
    int32_t value = quantity.getLong(status);
    if (U_FAILURE(status)) {
        return result;
    }
    if (value == -2147483648) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return result;
    }
    result = rules.select(value);
    return result;
}

UnicodeString &IntFormatter::format(
        const Formattable &number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    int32_t value = number.getLong(status);
    if (U_FAILURE(status)) {
        return appendTo;
    }
    if (value == -2147483648) {
        status = U_INVALID_FORMAT_ERROR;
        return appendTo;
    }
    SimpleAnnotator annotator(pos.getField());
    UBool isNeg = (value < 0);
    annotator.markStart(NumberFormat::kSignField, appendTo.length());
    if (isNeg) {
        value = -value;
        appendTo.append(fNegPrefix);
    } else {
        appendTo.append(fPosPrefix);
    }
    annotator.markEnd(NumberFormat::kSignField, appendTo.length());
    UChar buffer[10];
    int32_t idx = 10;
    while (value) {
        buffer[--idx] = fDigits[value % 10];
        value /= 10;
    }
    int32_t digitsLeft = 10 - idx;
    annotator.markStart(
            NumberFormat::kIntegerField, appendTo.length());
    if (digitsLeft == 0) {
         appendTo.append(fDigits[0]);
    } else {
        if (digitsLeft - fGroupingSize > 0) {
            int32_t leadCount = (digitsLeft - fGroupingSize) % fGroupingSize2;
            if (leadCount > 0) {
                appendTo.append(buffer, idx, leadCount);
                idx += leadCount;
                digitsLeft -= leadCount;
                annotator.markStart(NumberFormat::kGroupingSeparatorField, appendTo.length());
                appendTo.append(fGroupingSeparator);
                annotator.markEnd(NumberFormat::kGroupingSeparatorField, appendTo.length());
            }
            while (digitsLeft - fGroupingSize > 0) {
                appendTo.append(buffer, idx, fGroupingSize2);
                idx += fGroupingSize2;
                digitsLeft -= fGroupingSize2;
                annotator.markStart(NumberFormat::kGroupingSeparatorField, appendTo.length());
                appendTo.append(fGroupingSeparator);
                annotator.markEnd(NumberFormat::kGroupingSeparatorField, appendTo.length());
            }
        }
        appendTo.append(buffer, idx, digitsLeft);
    }
    annotator.markEnd(
            NumberFormat::kIntegerField, appendTo.length());
    annotator.markStart(NumberFormat::kSignField, appendTo.length());
    if (isNeg) {
        appendTo.append(fNegSuffix);
    } else {
        appendTo.append(fPosSuffix);
    }
    annotator.markEnd(NumberFormat::kSignField, appendTo.length());
    annotator.update(pos);
    return appendTo;
}


U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

