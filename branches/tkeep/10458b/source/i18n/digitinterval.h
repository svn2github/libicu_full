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

#ifndef __DIGITINTERVAL_H__
#define __DIGITINTERVAL_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

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
    void setIntDigitCount(int32_t count);

    /**
     * Changes the number of digits to the right of the decimal point that
     * this interval spans. If count is negative, it means span all digits
     * to the right of the decimal point.
     */
    void setFracDigitCount(int32_t count);

    /**
     * If returns 8, the most significant digit in interval is the 10^7 digit.
     * Returns INT32_MAX if this interval spans all digits to left of
     * decimal point.
     */
    int32_t getMostSignificantExclusive() const {
        return fLargestExclusive;
    }

    /**
     * Returns number of digits to the left of the decimal that this
     * interval includes. This is a synonym for getMostSignificantExclusive().
     */
    int32_t getIntDigitCount() const { 
        return fLargestExclusive;
    }

    /**
     * Returns the total number of digits that this interval spans.
     * Caution: If this interval spans all digits to the left or right of
     * decimal point instead of some fixed number, then what length()
     * returns is undefined.
     */
    int32_t length() const {
        return fLargestExclusive - fSmallestInclusive;
     }

    /**
     * If returns -3, the least significant digit in interval is the 10^-3
     * digit. Returns INT32_MIN if this interval spans all digits to right of
     * decimal point.
     */
    int32_t getLeastSignificantInclusive() const {
        return fSmallestInclusive;
    }
private:
    int32_t fLargestExclusive;
    int32_t fSmallestInclusive;
};

U_NAMESPACE_END

#endif  // __DIGITINTERVAL_H__
