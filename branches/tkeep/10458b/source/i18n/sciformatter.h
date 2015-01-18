/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* sciformatter.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __SCIFORMATTER_H__
#define __SCIFORMATTER_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "digitformatter.h"

U_NAMESPACE_BEGIN

class DecimalFormatSymbols;
class DigitList;
class DigitInterval;
class UnicodeString;
class FieldPositionHandler;

class U_I18N_API SciFormatterOptions : public UMemory {
    public:
    DigitFormatterOptions fMantissa;
    DigitFormatterIntOptions fExponent;
};

/**
 * This class formats scientific notation.
 */
class U_I18N_API SciFormatter : public UMemory {
public:
SciFormatter();
SciFormatter(const DecimalFormatSymbols &symbols);

void setDecimalFormatSymbols(const DecimalFormatSymbols &symbols);

/**
 * formats in scientifc notation.
 */
UnicodeString &format(
        const DigitList &positiveMantissa,
        int32_t exponent,
        const DigitFormatter &formatter,
        const DigitInterval &mantissaInterval,
        const SciFormatterOptions &options,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

/**
 * Counts how many code points are needed for the formatting.
 */
int32_t countChar32(
        int32_t exponent,
        const DigitFormatter &formatter,
        const DigitInterval &mantissaInterval,
        const SciFormatterOptions &options) const;
private:
UnicodeString fExponent;
};

U_NAMESPACE_END

#endif  // __SCIFORMATTER_H__
