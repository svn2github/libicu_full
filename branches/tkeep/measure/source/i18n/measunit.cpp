/*
**********************************************************************
* Copyright (c) 2004-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "unicode/measunit.h"

U_NAMESPACE_BEGIN

static const int32_t kAcceleration = 0;
static const UChar gAcceleration[] = {
        'a', 'c', 'c', 'e', 'l', 'e', 'r', 'a', 't', 'i', 'o', 'n', 0x0};

static const int32_t kDuration = 1;
static const UChar gDuration[] = {
        'd', 'u', 'r', 'a', 't', 'i', 'o', 'n', 0x0};

static const UChar * const gTypes[] = {
    gAcceleration,
    gDuration
};

static const int32_t kGforce = 0;
static const UChar gGforce[] = {
        'g', '-', 'f', 'o', 'r', 'c', 'e', 0x0};

static const UChar * const gAccelerationSubTypes[] = {
        gGforce,
        NULL
};

static const int32_t kYear = 0;
static const UChar gYear[] = {
        'y', 'e', 'a', 'r', 0x0};
    
static const int32_t kMonth = 1;
static const UChar gMonth[] = {
        'm', 'o', 'n', 't', 'h', 0x0};

static const UChar * const gDurationSubTypes[] = {
        gYear,
        gMonth,
        NULL
};

static const UChar * const * const gSubTypes[] = {
        gAccelerationSubTypes,
        gDurationSubTypes
};

MeasureUnit::MeasureUnit(const MeasureUnit &other) : fType(), fSubType() {
    // Same as assignment unless other.fType is an alias. Then fType becomes
    // an alias too.
    fType.fastCopyFrom(other.fType);
    fSubType.fastCopyFrom(other.fSubType);
}

MeasureUnit &MeasureUnit::operator=(const MeasureUnit &other) {
    if (this == &other) {
        return *this;
    }
    fType.fastCopyFrom(other.fType);
    fSubType.fastCopyFrom(other.fSubType);
    return *this;
}

UObject *MeasureUnit::clone() const {
    return new MeasureUnit(*this);
}

MeasureUnit::~MeasureUnit() {
}

UBool MeasureUnit::operator==(const UObject& other) const {
    const MeasureUnit *rhs = dynamic_cast<const MeasureUnit*>(&other);
    if (rhs == NULL) {
        return FALSE;
    }
    return (fType == rhs->fType && fSubType == rhs->fSubType);
}

int32_t MeasureUnit::getAvailable(
        int32_t destCapacity,
        MeasureUnit *dest,
        UErrorCode &errorCode) {
    return 0;
}

int32_t MeasureUnit::getAvailable(
        const UnicodeString &type,
        int32_t destCapacity,
        MeasureUnit *dest,
        UErrorCode &errorCode) {
    return 0;
}

int32_t MeasureUnit::getAvailableTypes(
        int32_t destCapacity,
        UnicodeString *dest,
        UErrorCode &errorCode) {
    return 0;
}

MeasureUnit *MeasureUnit::create(int32_t type, int32_t subType, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    MeasureUnit *result = new MeasureUnit(type, subType);
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

MeasureUnit *MeasureUnit::createGForce(UErrorCode &status) {
    return create(kAcceleration, kGforce, status);
}

MeasureUnit *MeasureUnit::createYear(UErrorCode &status) {
    return create(kDuration, kYear, status);
}

MeasureUnit::MeasureUnit(const UnicodeString &type, const UnicodeString &subType)
        : fType(type), fSubType(subType) {
}

MeasureUnit::MeasureUnit(int32_t typeId, int32_t subTypeId) 
        : fType(TRUE, gTypes[typeId], -1),
          fSubType(TRUE, gSubTypes[typeId][subTypeId], -1) {
}

MeasureUnit::MeasureUnit(int32_t typeId, const UnicodeString &subType) 
        : fType(TRUE, gTypes[typeId], -1),
          fSubType(subType) {
}

MeasureUnit::MeasureUnit() : fType(), fSubType() {
}

U_NAMESPACE_END


