/*
*******************************************************************************
* Copyright (C) 1997-2014, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef _NUMBER_FORMAT_TEST_TUPLE
#define _NUMBER_FORMAT_TEST_TUPLE

#include "unicode/utypes.h"
#include "decimalformatpattern.h"

#define NFTT_GET_FIELD(tuple, fieldName, defaultValue) ((tuple).fieldName##Flag ? (tuple).fieldName : (defaultValue))

U_NAMESPACE_BEGIN

enum ENumberFormatTestTupleField {
    kLocale,
    kCurrency,
    kPattern,
    kFormat,
    kOutput,
    kComment,
    kMinIntegerDigits,
    kMaxIntegerDigits,
    kMinFractionDigits,
    kMaxFractionDigits,
    kMinGroupingDigits,
    kBreaks,
    kNumberFormatTestTupleFieldCount,
};

class NumberFormatTestTuple : UMemory {
public:
    Locale locale;
    UnicodeString currency;
    UnicodeString pattern;
    UnicodeString format;
    UnicodeString output;
    UnicodeString comment;
    int32_t minIntegerDigits;
    int32_t maxIntegerDigits;
    int32_t minFractionDigits;
    int32_t maxFractionDigits;
    int32_t minGroupingDigits;
    UnicodeString breaks;

    UBool localeFlag;
    UBool currencyFlag;
    UBool patternFlag;
    UBool formatFlag;
    UBool outputFlag;
    UBool commentFlag;
    UBool minIntegerDigitsFlag;
    UBool maxIntegerDigitsFlag;
    UBool minFractionDigitsFlag;
    UBool maxFractionDigitsFlag;
    UBool minGroupingDigitsFlag;
    UBool breaksFlag;

    NumberFormatTestTuple() {
        clear();
    }
    UBool setField(
            ENumberFormatTestTupleField field,
            const UnicodeString &fieldValue,
            UErrorCode &status);
    void clear();
    UnicodeString &toString(UnicodeString &appendTo) const;
    static ENumberFormatTestTupleField getFieldByName(const UnicodeString &name);
private:
    const void *getFieldAddress(int32_t fieldId) const;
    void *getMutableFieldAddress(int32_t fieldId);
    void setFlag(int32_t fieldId, UBool value);
    UBool isFlag(int32_t fieldId) const;
};

U_NAMESPACE_END

#endif
