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
#include "digitlst.h"

U_NAMESPACE_BEGIN


/**
 * A precision manager for values to be formatted as fixed point.
 * Handles rounding of number to prepare it for formatting.
 */
class U_I18N_API FixedPrecision : public UMemory {
public:

/**
 * The smallest format interval allowed. Default is 1 integer digit and no
 * fraction digits.
 */
DigitInterval fMin;

/**
 * The largest format interval allowed. Must contain fMin.
 *  Default is all digits.
 */
DigitInterval fMax;

/**
 * Min and max significant digits allowed. The default is no constraints.
 */
SignificantDigitInterval fSignificant;

/**
 * The rounding increment or zero if there is no rounding increment.
 * Default is zero.
 */
DigitList fRoundingIncrement;

FixedPrecision();

/**
 * Rounds value in place to prepare it for formatting.
 * @param value The value to be rounded. It is rounded in place.
 * @param exponent Always pass 0 for fixed decimal formatting. scientific
 *  precision passes the exponent value.  Essentially, it divides value by
 *  10^exponent, rounds and then multiplies by 10^exponent.
 * @param status error returned here.
 * @return reference to value.
 */
DigitList &round(DigitList &value, int32_t exponent, UErrorCode &status) const;

/**
 * Returns the interval to use to format the rounded value.
 * @param roundedValue the already rounded value to format.
 * @param interval modified in place to be the interval to use to format
 *   the rounded value.
 * @return a reference to interval.
 */
DigitInterval &getInterval(
        const DigitList &roundedValue, DigitInterval &interval) const;
};

/**
 * A precision manager for values to be expressed as scientific notation.
 */
class U_I18N_API ScientificPrecision : public UMemory {
public:
    FixedPrecision fMantissa;

    /**
     * rounds value in place to prepare it for formatting.
     * @param value The value to be rounded. It is rounded in place.
     * @param status error returned here.
     * @return reference to value.
     */
    DigitList &round(DigitList &value, UErrorCode &status) const;

    /**
     * Converts value to a mantissa and exponent.
     *
     * @param value modified in place to be the mantissa. Depending on
     *   the precision settings, the resulting mantissa may not fall
     *   between 1.0 and 10.0.
     * @return the exponent of value.
     */
    int32_t toScientific(DigitList &value) const;
private:
    int32_t getMultiplier() const;

};



U_NAMESPACE_END

#endif  // __PRECISION_H__
