/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitformatter.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __DIGITFORMATTER_H__
#define __DIGITFORMATTER_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class DecimalFormatSymbols;
class DigitList;
class DigitGrouping;
class DigitInterval;
class UnicodeString;
class FieldPositionHandler;

/**
 * Does fixed point formatting.
 */
class U_I18N_API DigitFormatter : public UMemory {
public:
class U_I18N_API Options : public UMemory {
    public:
    Options() : fAlwaysShowDecimal(FALSE) { }
    UBool fAlwaysShowDecimal;
};
class U_I18N_API IntOptions : public UMemory {
    public:
    IntOptions() : fMinDigits(1), fAlwaysShowSign(FALSE) { }
    int32_t fMinDigits;
    UBool fAlwaysShowSign;
};
DigitFormatter();
DigitFormatter(const DecimalFormatSymbols &symbols);

void setDecimalFormatSymbols(const DecimalFormatSymbols &symbols);

/**
 * Fixed point formatting. grouping controls how digit grouping is done
 * While interval specifies what digits of the value get formatted.
 */
UnicodeString &format(
        const DigitList &positiveDigits,
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        const Options &options,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

/**
 * Fixed point formatting for 32 bit ints. SignField and intField indicate
 * the field type for the sign and value of the int when updating
 * field positions so that this method can be used to format plain ints
 * or exponent values.
 */
UnicodeString &formatInt32(
        int32_t value,
        const IntOptions &options,
        int32_t signField,
        int32_t intField,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

/**
 * Counts the number of code points needed for formatting.
 */
int32_t countChar32(
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        const Options &options) const;

/**
 * Counts the number of code points needed for formatting an int32.
 */
int32_t countChar32ForInt(
        int32_t value,
        const IntOptions &options) const;

private:
UChar32 fLocalizedDigits[10];
UnicodeString fGroupingSeparator;
UnicodeString fDecimal;
UnicodeString fNegativeSign;
UnicodeString fPositiveSign;
};

U_NAMESPACE_END

#endif  // __DIGITFORMATTER_H__
