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

U_NAMESPACE_BEGIN

class U_I18N_API DigitFormatter : public UMemory {
public:
    DigitFormatter() : fStart(2147483647), fEnd(-2147483648) { }
    :


UnicodeString &formatDigits(
    const DigitList &digits,
    const DecimalFormatSymbols &symbols,
    const DigitGrouping &grouping,
    const FixedDigits &fixedDigits;
    UnicodeString &appendTo);

U_NAMESPACE_END

#endif  // __DIGITFORMATTER_H__
