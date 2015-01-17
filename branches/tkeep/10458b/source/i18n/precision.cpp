/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: precisison.cpp
 */

#include "unicode/utypes.h"

#include "precision.h"
#include "digitlst.h"

U_NAMESPACE_BEGIN

FixedPrecision::FixedPrecision() {
    fMin.setIntDigitCount(1);
    fMin.setFracDigitCount(0);
}

DigitList &
FixedPrecision::round(DigitList &value, int32_t exponent) const {
    value.roundAtExponent(
            exponent + fMax.getLeastSignificantInclusive(),
            fSignificant.getMax());
    return value;
}

DigitInterval &
FixedPrecision::getInterval(
        const DigitList &value, DigitInterval &interval) const {
    value.getSmallestInterval(interval, fSignificant.getMin());
    interval.expandToContain(fMin);
    interval.shrinkToFitWithin(fMax);
    return interval;
}

U_NAMESPACE_END

