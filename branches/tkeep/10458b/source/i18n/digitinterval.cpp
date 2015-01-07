/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitinterval.cpp
 */

#include "unicode/utypes.h"

#include "digitinterval.h"

U_NAMESPACE_BEGIN

UBool DigitInterval::DigitInterval(
        int32_t digitsLeft, int32_t digitsRight) {
    setDigitsLeft(digitsLeft);
    setDigitsRight(digitsRight);
}

void DigitInterval::expandToContain(const DigitInterval &rhs) {
    if (fSmallestInclusive > rhs.fSmallestInclusive) {
        fSmallestInclusive = rhs.fSmallestInclusive;
    }
    if (fLargestExclusive < rhs.fLargestExclusive) {
        fLargestExclusive = rhs.fLargestExclusive;
    }
}

void DigitInterval::shrinkToFitWithin(const DigitInterval &rhs) {
    if (fSmallestInclusive < rhs.fSmallestInclusive) {
        fSmallestInclusive = rhs.fSmallestInclusive;
    }
    if (fLargestExclusive > rhs.fLargestExclusive) {
        fLargestExclusive = rhs.fLargestExclusive;
    }
}

void DigitInterval::setDigitsLeft(int32_t count) {
    fLargestExclusive = count < 0 ? INT32_MAX : count;
}

void DigitInterval::setDigitsRight(int32_t count) {
    fSmallestInclusive = count < 0 ? INT32_MIN : -count;
}


U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
