/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitinterval.h
*
* created on: 2015jan6
* created by: Travis Keep
*/

#ifndef __DIGIT_INTERVAL_H__
#define __DIGIT_INTERVAL_H__

#include "unicode/utypes.h"

U_NAMESPACE_BEGIN

/**
 * An interval of digits.
 */
class U_I18N_API DigitInterval : public UMemory {
public:

    /**
     * Spans all digits
     */
    DigitInterval()
            : fLargestExclusive(INT32_MAX), fSmallestInclusive(INT32_MIN) { }

    /**
     * Spans digitsLeft of decimal point to digitsRight of decimal point
     * DigitInterval(0, 0) is empty set. If digitsLeft or digitsRight is
     * negative, they are treated as spanning all digits to the left or
     * right of decimal place respectively.
     */
    DigitInterval(int32_t digitsLeft, int32_t digitsRight);

    /**
     * Expand this interval so that it contains all of rhs.
     */
    void expandToContain(const DigitInterval &rhs);

    /**
     * Shrink this interval so that it contains no more than rhs.
     */
    void shrinkToFitWithin(const DigitInterval &rhs);

    /**
     * Changes the number of digits to the left of the decimal point that
     * this interval spans. If count is negative, it means span all digits
     * to the left of the decimal point.
     */
    void setDigitsLeft(int32_t count);

    /**
     * Changes the number of digits to the right of the decimal point that
     * this interval spans. If count is negative, it means span all digits
     * to the right of the decimal point.
     */
    void setDigitsRight(int32_t count);

    /**
     * If returns 8. The largest digit in interval is the 10^7 digit.
     * Returns INT32_MAX if this interval spans all digits to left of
     * decimal point.
     */
    int32_t getLargestExclusive() const {
        return fLargestExclusive;
    }

    /**
     * If returns -3. The smallest digit in interval is the 10^-3 digit.
     * Returns INT32_MIN if this interval spans all digits to right of
     * decimal point.
     */
    int32_t getSmallestInclusive() const {
        return fSmallestInclusive;
    }
private:
    int32_t fLargestExclusive;
    int32_t fSmallestInclusive;
};

U_NAMESPACE_END

#endif  // __DIGITINTERVAL_H__
