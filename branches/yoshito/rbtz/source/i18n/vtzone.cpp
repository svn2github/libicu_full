/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/vtzone.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(VTimeZone)

VTimeZone::VTimeZone(const VTimeZone& source) {
    //TODO
}

VTimeZone::~VTimeZone() {
    //TODO
}

VTimeZone&
VTimeZone::operator=(const VTimeZone& right) {
    if (*this != right) {
        BasicTimeZone::operator=(right);
        //TODO
    }
    return *this;
}

UBool
VTimeZone::operator==(const VTimeZone& that) const {
    if (this == &that) {
        return TRUE;
    }
    if (getDynamicClassID() != that.getDynamicClassID()
        || BasicTimeZone::operator==(that) == FALSE) {
        return FALSE;
    }
    //TODO
    return FALSE;
}

UBool
VTimeZone::operator!=(const VTimeZone& that) const {
    return !operator==(that);
}

VTimeZone*
VTimeZone::createVTimeZone(const UnicodeString& ID) {
    //TODO
    return NULL;
}

VTimeZone*
VTimeZone::createVTimeZone(const UnicodeString& vtzdata, UErrorCode& status) {
    //TODO
    return NULL;
}

UBool
VTimeZone::getTZURL(UnicodeString& url) const {
    //TODO
    return FALSE;
}

void
VTimeZone::setTZURL(const UnicodeString& url) {
    //TODO
}

UBool
VTimeZone::getLastModified(UnicodeString& lastModified) const {
    //TODO
    return FALSE;
}

void
VTimeZone::setLastModified(const UnicodeString& lastModified) {
    //TODO
}

void
VTimeZone::write(UnicodeString& result, UErrorCode& status) const {
    //TODO
}

void
VTimeZone::write(UDate cutover, UnicodeString& result, UErrorCode& status) const {
    //TODO
}

void
VTimeZone::writeSimple(UDate time, UnicodeString& result, UErrorCode& status) const {
    //TODO
}

TimeZone*
VTimeZone::clone(void) const {
    return new VTimeZone(*this);
}

int32_t
VTimeZone::getOffset(uint8_t era, int32_t year, int32_t month, int32_t day,
                     uint8_t dayOfWeek, int32_t millis, UErrorCode& status) const {
    return tz->getOffset(era, year, month, day, dayOfWeek, millis, status);
}

int32_t
VTimeZone::getOffset(uint8_t era, int32_t year, int32_t month, int32_t day,
                     uint8_t dayOfWeek, int32_t millis,
                     int32_t monthLength, UErrorCode& status) const {
    return tz->getOffset(era, year, month, day, dayOfWeek, millis, monthLength, status);
}

void
VTimeZone::getOffset(UDate date, UBool local, int32_t& rawOffset,
                     int32_t& dstOffset, UErrorCode& status) const {
    return tz->getOffset(date, local, rawOffset, dstOffset, status);
}

void
VTimeZone::setRawOffset(int32_t offsetMillis) {
    tz->setRawOffset(offsetMillis);
    //TODO
}

int32_t
VTimeZone::getRawOffset(void) const {
    return tz->getRawOffset();
}

UBool
VTimeZone::useDaylightTime(void) const {
    return tz->useDaylightTime();
}

UBool
VTimeZone::inDaylightTime(UDate date, UErrorCode& status) const {
    return tz->inDaylightTime(date, status);
}

UBool
VTimeZone::hasSameRules(const TimeZone& other) const {
    return tz->hasSameRules(other);
}

UBool
VTimeZone::getNextTransition(UDate base, UBool inclusive, TimeZoneTransition& result) /*const*/ {
    return tz->getNextTransition(base, inclusive, result);
}

UBool
VTimeZone::getPreviousTransition(UDate base, UBool inclusive, TimeZoneTransition& result) /*const*/ {
    return tz->getPreviousTransition(base, inclusive, result);
}

InitialTimeZoneRule*
VTimeZone::getInitialRule(UErrorCode& status) /*const*/ {
    return tz->getInitialRule(status);
}

int16_t
VTimeZone::countTransitionRules(UErrorCode& status) /*const*/ {
    return tz->countTransitionRules(status);
}

TimeZoneRule*
VTimeZone::getTransitionRule(int16_t index, UErrorCode& status) /*const*/ {
    return tz->getTransitionRule(index, status);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof

