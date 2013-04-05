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
    TimeUnitAmount **timeUnitAmounts,
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

UBool TimePeriod::getAmount(
    TimeUnit::UTimeUnitFields field,
    TimeUnitAmount& result,
    UErrorCode& status) const {
  return true;
}

U_NAMESPACE_END

#endif
