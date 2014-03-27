/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/utypes.h"
#include "unicode/sciformathelper.h"
#include "unicode/dcfmtsym.h"
#include "unicode/fpositer.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

// TODO: Add U_DRAFT_API directives.
// TODO: Add U_FORMATTING directives

U_NAMESPACE_BEGIN

static UChar kExponentDigits[] = {0x2070, 0xB9, 0xB2, 0xB3, 0x2074, 0x2075, 0x2076, 0x2077, 0x2078, 0x2079};

static UnicodeString getMultiplicationSymbol(const DecimalFormatSymbols &dfs) {
    static UChar multSign = 0xD7;
    return UnicodeString(FALSE, &multSign, 1);
}

SciFormatHelper::SciFormatHelper(
        const DecimalFormatSymbols &dfs, UErrorCode &status) : fPreExponent() {
    if (U_FAILURE(status)) {
        return;
    }
    fPreExponent.append(getMultiplicationSymbol(dfs));
    fPreExponent.append(dfs.getSymbol(DecimalFormatSymbols::kOneDigitSymbol));
    fPreExponent.append(dfs.getSymbol(DecimalFormatSymbols::kZeroDigitSymbol));
}

UnicodeString &SciFormatHelper::insetMarkup(
        UnicodeString &s,
        FieldPositionIterator &fpi,
        const UnicodeString &beginMarkup,
        const UnicodeString &endMarkup,
        UErrorCode &status) const {
    FieldPosition fp;
    int32_t adjustment = 0;
    while (fpi.next(fp)) {
        switch (fp.getField()) {
        case UNUM_EXPONENT_SYMBOL_FIELD:
            s.replace(
                    fp.getBeginIndex(),
                    fp.getEndIndex() - fp.getBeginIndex(),
                    fPreExponent + beginMarkup);
            adjustment += (fPreExponent.length() + beginMarkup.length() - (fp.getEndIndex() - fp.getBeginIndex()));
            break;
        case UNUM_EXPONENT_FIELD:
            s.insert(fp.getEndIndex() + adjustment, endMarkup);
            adjustment += endMarkup.length();
            break;
        default:
            break;
        }
    }
    return s;
}

static void makeSuperscript(UnicodeString &s, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; ++i) {
        if (s[i] >= 0x30 && s[i] <= 0x39) {
            s.setCharAt(i, kExponentDigits[s[i] - 0x30]);
        }
    }
}

static UBool isMinusSign(UChar ch) {
    // TODO: revisit this.
    return (ch == 0x2D);
}

static void makeSignSuperscript(UnicodeString &s, int32_t beginIndex, int32_t endIndex) {
    if (endIndex - beginIndex != 1) {
        return;
    }
    if (isMinusSign(s[beginIndex])) {
        s.setCharAt(beginIndex, 0x207B);
    }
}

UnicodeString &SciFormatHelper::toSuperscriptExponentDigits(
        UnicodeString &s,
        FieldPositionIterator &fpi,
        UErrorCode &status) const {
    FieldPosition fp;
    int32_t adjustment = 0;
    while (fpi.next(fp)) {
        switch (fp.getField()) {
        case UNUM_EXPONENT_SYMBOL_FIELD:
            s.replace(
                    fp.getBeginIndex(),
                    fp.getEndIndex() - fp.getBeginIndex(),
                    fPreExponent);
            adjustment += (fPreExponent.length() - (fp.getEndIndex() - fp.getBeginIndex()));
            break;
        case UNUM_EXPONENT_SIGN_FIELD:
            makeSignSuperscript(s, fp.getBeginIndex() + adjustment, fp.getEndIndex() + adjustment);
            break;
        case UNUM_EXPONENT_FIELD:
            makeSuperscript(s, fp.getBeginIndex() + adjustment, fp.getEndIndex() + adjustment);
            break;
        default:
            break;
        }
    }
    return s;
}



U_NAMESPACE_END
