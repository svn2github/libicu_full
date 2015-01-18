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
 * A precision manager for values to be expressed as fixed point .
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

/**
 * A precision manager for values to be expressed as scientific notation.
 */
class U_I18N_API ScientificPrecision : public UMemory {
public:
    FixedPrecision fMantissa;
    ScientificPrecision() : fMultiplier(1) { }
    void setExponentMultiplier(int32_t value) {
        fMultiplier = value < 1 ? 1 : value;
    }
    int32_t getExponentMultiplier() const { return fMultiplier; }
    DigitList &round(DigitList &value) const;
    int32_t toScientific(DigitList &value) const;
private:
    int32_t fMultiplier;
};



U_NAMESPACE_END

#endif  // __PRECISION_H__
