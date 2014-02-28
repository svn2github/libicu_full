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

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

static UChar gNegPrefix[] = {0x2D, 0x0};

IntFormatter::IntFormatter() :
        fGroupingSize(0), fGroupingSize2(0), fGroupingSeparator(),
        fPosPrefix(), fNegPrefix(TRUE, gNegPrefix, -1), fPosSuffix(),
        fNegSuffix() {
}

IntFormatter::~IntFormatter() { }

void IntFormatter::like(const DecimalFormat &df, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fGroupingSize = 0;
    fGroupingSize2 = 0;
    if (df.isGroupingUsed()) {
        fGroupingSize = df.getGroupingSize();
        fGroupingSize2 = df.getSecondaryGroupingSize();
        fGroupingSeparator =
                df.getDecimalFormatSymbols()->getSymbol(
                        DecimalFormatSymbols::kGroupingSeparatorSymbol);
        df.getPositivePrefix(fPosPrefix);
        df.getNegativePrefix(fNegPrefix);
        df.getPositiveSuffix(fPosSuffix);
        df.getNegativeSuffix(fNegSuffix);
    }
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
    

UnicodeString &IntFormatter::format(
        const Formattable &number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    int32_t value = number.getLong(status);
    if (U_FAILURE(status)) {
        return appendTo;
    }
    int32_t groupingSize = fGroupingSize <= 0 ? INT32_MAX : fGroupingSize;
    int32_t groupingSize2 = fGroupingSize2 <= 0 ? groupingSize : fGroupingSize2;
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
    // Have to allow for one separator per digit
    // TODO support grouping
    UChar buffer[20];
    UChar *endBuffer = &buffer[20];
    UChar *ptr = endBuffer;
    int32_t groupCounter = groupingSize;
    while (value) {
        if (!groupCounter) {
            annotator.markStart(
                    NumberFormat::kGroupingSeparatorField, appendTo.length());
            appendTo.append(fGroupingSeparator);
            annotator.markEnd(
                    NumberFormat::kGroupingSeparatorField, appendTo.length());
            groupCounter = groupingSize2;
        }
        *--ptr = (UChar) ((value % 10) + 0x30);
        value /= 10;
        --groupCounter;
    }
    annotator.markStart(
            NumberFormat::kIntegerField, appendTo.length());
    if (ptr == endBuffer) {
         appendTo.append(0x30);
    } else {
        appendTo.append(ptr, 0, endBuffer - ptr);
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

