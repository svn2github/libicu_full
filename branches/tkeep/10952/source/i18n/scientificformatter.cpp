/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/scientificformatter.h"
#include "unicode/dcfmtsym.h"
#include "unicode/fpositer.h"
#include "unicode/utf16.h"
#include "unicode/uniset.h"
#include "decfmtst.h"
#include "unicode/decimfmt.h"

U_NAMESPACE_BEGIN

static const UChar kSuperscriptDigits[] = {
        0x2070,
        0xB9,
        0xB2,
        0xB3,
        0x2074,
        0x2075,
        0x2076,
        0x2077,
        0x2078,
        0x2079};

static const UChar kSuperscriptPlusSign = 0x207A;
static const UChar kSuperscriptMinusSign = 0x207B;

static UBool copyAsSuperscript(
        const UnicodeString &s,
        int32_t beginIndex,
        int32_t endIndex,
        UnicodeString &result,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    for (int32_t i = beginIndex; i < endIndex;) {
        UChar32 c = s.char32At(i);
        int32_t digit = u_charDigitValue(c);
        if (digit < 0) {
            status = U_INVALID_CHAR_FOUND;
            return FALSE;
        }
        result.append(kSuperscriptDigits[digit]);
        i += U16_LENGTH(c);
    }
    return TRUE;
}

ScientificFormatter::Style *ScientificFormatter::SuperscriptStyle::clone() const {
    return new ScientificFormatter::SuperscriptStyle(*this);
}

UnicodeString &ScientificFormatter::SuperscriptStyle::format(
        const UnicodeString &original,
        FieldPositionIterator &fpi,
        const UnicodeString &preExponent,
        const DecimalFormatStaticSets &staticSets,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    FieldPosition fp;
    int32_t copyFromOffset = 0;
    UBool exponentSymbolFieldPresent = FALSE;
    UBool exponentFieldPresent = FALSE;
    while (fpi.next(fp)) {
        switch (fp.getField()) {
        case UNUM_EXPONENT_SYMBOL_FIELD:
            exponentSymbolFieldPresent = TRUE;
            appendTo.append(
                    original,
                    copyFromOffset,
                    fp.getBeginIndex() - copyFromOffset);
            copyFromOffset = fp.getEndIndex();
            appendTo.append(preExponent);
            break;
        case UNUM_EXPONENT_SIGN_FIELD:
            {
                int32_t beginIndex = fp.getBeginIndex();
                int32_t endIndex = fp.getEndIndex();
                UChar32 aChar = original.char32At(beginIndex);
                if (staticSets.fMinusSigns->contains(aChar)) {
                    appendTo.append(
                            original,
                            copyFromOffset,
                            beginIndex - copyFromOffset);
                    appendTo.append(kSuperscriptMinusSign);
                } else if (staticSets.fPlusSigns->contains(aChar)) {
                    appendTo.append(
                           original,
                           copyFromOffset,
                           beginIndex - copyFromOffset);
                    appendTo.append(kSuperscriptPlusSign);
                } else {
                    status = U_INVALID_CHAR_FOUND;
                    return appendTo;
                }
                copyFromOffset = endIndex;
            }
            break;
        case UNUM_EXPONENT_FIELD:
            exponentFieldPresent = TRUE;
            appendTo.append(
                    original,
                    copyFromOffset,
                    fp.getBeginIndex() - copyFromOffset);
            if (!copyAsSuperscript(
                    original,
                    fp.getBeginIndex(),
                    fp.getEndIndex(),
                    appendTo,
                    status)) {
              return appendTo;
            }
            copyFromOffset = fp.getEndIndex();
            break;
        default:
            break;
        }
    }
    if (!exponentSymbolFieldPresent || !exponentFieldPresent) {
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return appendTo;
    }
    appendTo.append(
            original, copyFromOffset, original.length() - copyFromOffset);
    return appendTo;
}

ScientificFormatter::Style *ScientificFormatter::MarkupStyle::clone() const {
    return new ScientificFormatter::MarkupStyle(*this);
}

UnicodeString &ScientificFormatter::MarkupStyle::format(
        const UnicodeString &original,
        FieldPositionIterator &fpi,
        const UnicodeString &preExponent,
        const DecimalFormatStaticSets &decimalFormatSets,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    FieldPosition fp;
    int32_t copyFromOffset = 0;
    UBool exponentSymbolFieldPresent = FALSE;
    UBool exponentFieldPresent = FALSE;
    while (fpi.next(fp)) {
        switch (fp.getField()) {
        case UNUM_EXPONENT_SYMBOL_FIELD:
            exponentSymbolFieldPresent = TRUE;
            appendTo.append(
                    original,
                    copyFromOffset,
                    fp.getBeginIndex() - copyFromOffset);
            copyFromOffset = fp.getEndIndex();
            appendTo.append(preExponent);
            appendTo.append(fBeginMarkup);
            break;
        case UNUM_EXPONENT_FIELD:
            exponentFieldPresent = TRUE;
            appendTo.append(
                    original,
                    copyFromOffset,
                    fp.getEndIndex() - copyFromOffset);
            copyFromOffset = fp.getEndIndex();
            appendTo.append(fEndMarkup);
            break;
        default:
            break;
        }
    }
    if (!exponentSymbolFieldPresent || !exponentFieldPresent) {
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return appendTo;
    }
    appendTo.append(
            original, copyFromOffset, original.length() - copyFromOffset);
    return appendTo;

}


ScientificFormatter::ScientificFormatter(
        DecimalFormat *fmtToAdopt, Style *styleToAdopt, UErrorCode &status)
        : fPreExponent(),
          fDecimalFormat(fmtToAdopt),
          fStyle(styleToAdopt),
          fStaticSets(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    if (fDecimalFormat == NULL || fStyle == NULL) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    const DecimalFormatSymbols *sym = fDecimalFormat->getDecimalFormatSymbols();
    if (sym == NULL) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    getPreExponent(*sym, fPreExponent);
    fStaticSets = DecimalFormatStaticSets::getStaticSets(status);
}

ScientificFormatter::ScientificFormatter(
        const ScientificFormatter &other)
        : UObject(other),
          fPreExponent(other.fPreExponent),
          fDecimalFormat(NULL),
          fStyle(NULL),
          fStaticSets(other.fStaticSets) {
    fDecimalFormat = static_cast<DecimalFormat *>(
            other.fDecimalFormat->clone());
    fStyle = other.fStyle->clone();
}

ScientificFormatter &ScientificFormatter::operator=(
        const ScientificFormatter &other) {
    if (this == &other) {
        return *this;
    }
    UObject::operator=(other);
    fPreExponent = other.fPreExponent;
    delete fDecimalFormat;
    fDecimalFormat = static_cast<DecimalFormat *>(
            other.fDecimalFormat->clone());
    delete fStyle;
    fStyle = other.fStyle->clone();
    fStaticSets = other.fStaticSets;
    return *this;
}

ScientificFormatter::~ScientificFormatter() {
    delete fDecimalFormat;
    delete fStyle;
}

UnicodeString &ScientificFormatter::format(
        const Formattable &number,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    UnicodeString original;
    FieldPositionIterator fpi;
    fDecimalFormat->format(number, original, &fpi, status);
    return fStyle->format(
            original,
            fpi,
            fPreExponent,
            *fStaticSets,
            appendTo,
            status);
}

void ScientificFormatter::getPreExponent(
        const DecimalFormatSymbols &dfs, UnicodeString &preExponent) {
    preExponent.append(dfs.getConstSymbol(
            DecimalFormatSymbols::kExponentMultiplicationSymbol));
    preExponent.append(dfs.getSymbol(DecimalFormatSymbols::kOneDigitSymbol));
    preExponent.append(dfs.getSymbol(DecimalFormatSymbols::kZeroDigitSymbol));
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
