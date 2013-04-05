/*
 *******************************************************************************
 * Copyright (C) 2013-2013, Google, International Business Machines Corporation
 * and others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/timeperiod.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

TimePeriod::TimePeriod(const TimePeriod& other) {
}


TimePeriod* TimePeriod::forAmounts(
    const TimeUnitAmount * const *timeUnitAmounts,
    int32_t length,
    UErrorCode& status) {
  return NULL;
}

TimePeriod::~TimePeriod() {
}

UBool TimePeriod::operator==(const UObject& other) const {
}

UBool TimePeriod::operator!=(const UObject& other) const {
}

const TimeUnitAmount *TimePeriod::getAmount(
    TimeUnit::UTimeUnitFields field) const {
  return NULL;
}

U_NAMESPACE_END

#endif
