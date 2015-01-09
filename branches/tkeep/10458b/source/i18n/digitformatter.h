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

class U_I18N_API DigitFormatter : public UMemory {
public:
DigitFormatter(const DecimalFormatSymbols &symbols);

UnicodeString &format(
        const DigitList &digits,
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        UBool alwaysShowDecimal,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;
int32_t countChar32(
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        UBool alwaysShowDecimal) const;
private:
UChar32 fLocalizedDigits[10];
UnicodeString fGroupingSeparator;
UnicodeString fDecimal;
};

U_NAMESPACE_END

#endif  // __DIGITFORMATTER_H__
