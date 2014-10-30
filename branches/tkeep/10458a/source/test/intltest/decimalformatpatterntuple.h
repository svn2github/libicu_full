/*
*******************************************************************************
* Copyright (C) 1997-2014, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef _DECIMAL_FORMAT_PATTERN_TUPLE
#define _DECIMAL_FORMAT_PATTERN_TUPLE

#include "unicode/utypes.h"
#include "decimalformatpattern.h"

U_NAMESPACE_BEGIN

enum EDecimalFormatPatternField {
    kMinimumIntegerDigits,
    kMaximumIntegerDigits,
    kMinimumFractionDigits,
    kMaximumFractionDigits,
    kUseSignificantDigits,
    kMinimumSignificantDigits,
    kMaximumSignificantDigits,
    kUseExponentialNotation,
    kMinExponentDigits,
    kExponentSignAlwaysShown,
    kCurrencySignCount,
    kGroupingUsed,
    kGroupingSize,
    kGroupingSize2,
    kMultiplier,
    kDecimalSeparatorAlwaysShown,
    kFormatWidth,
    kRoundingIncrementUsed,
    kRoundingIncrement,
    kPad,
    kNegPatternsBogus,
    kPosPatternsBogus,
    kNegPrefixPattern,
    kNegSuffixPattern,
    kPosPrefixPattern,
    kPosSuffixPattern,
    kPadPosition,
    kDecimalFormatPatternFieldCount
};

class DecimalFormatPatternTuple : UMemory {
public:
    DecimalFormatPatternTuple() : fFieldsUsed(0) { }
    UBool setField(
            EDecimalFormatPatternField field,
            const UnicodeString &fieldValue,
            UErrorCode &status);
    UBool clearField(EDecimalFormatPatternField field, UErrorCode &status);
    void merge(const DecimalFormatPatternTuple &other, UBool override=TRUE);
    void clear() { fFieldsUsed = 0; }
    int32_t diffCount(const DecimalFormatPattern &pattern) const;
    int32_t verify(
            const DecimalFormatPattern &pattern,
            UnicodeString &appendTo,
            UnicodeString *reproduceLines,
            int32_t reproduceLinesCount) const;
    UnicodeString &toString(UnicodeString &appendTo) const;
    static EDecimalFormatPatternField getFieldByName(const UnicodeString &name);
private:
    DecimalFormatPattern fFieldValues;
    uint64_t fFieldsUsed;
};

class DecimalFormatPatternTupleVars : UMemory {
public:
    DecimalFormatPatternTupleVars() : count(0) { }
    UBool store(
            const UnicodeString &name,
            const DecimalFormatPatternTuple &tuple,
            UErrorCode &status);
    UBool fetch(
            const UnicodeString &name,
            DecimalFormatPatternTuple &tuple) const;
private:
    UnicodeString names[100];
    DecimalFormatPatternTuple tuples[100];
    int32_t count;
    int32_t find(const UnicodeString &) const;
    DecimalFormatPatternTupleVars(const DecimalFormatPatternTupleVars &);
    DecimalFormatPatternTupleVars &operator=(
            const DecimalFormatPatternTupleVars &);
};

U_NAMESPACE_END

#endif
