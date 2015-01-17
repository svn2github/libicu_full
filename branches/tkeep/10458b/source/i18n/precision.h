/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* precision.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __PRECISION_H__
#define __PRECISION_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "digitinterval.h"
#include "significantdigitinterval.h"

U_NAMESPACE_BEGIN

class DigitList;


/**
 * This class formats scientific notation.
 */
class U_I18N_API FixedPrecision : public UMemory {
public:
DigitInterval fMin;
DigitInterval fMax;
SignificantDigitInterval fSignificant;

FixedPrecision();

DigitList &round(DigitList &value, int32_t exponent) const;
DigitInterval &getInterval(
        const DigitList &value, DigitInterval &interval) const;
};

U_NAMESPACE_END

#endif  // __PRECISION_H__
