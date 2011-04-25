/*
*******************************************************************************
* Copyright (C) 2011, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File TZNAMES_IMPL.CPP
*
*******************************************************************************
*/

#include "unicode/utypes.h"
#if !UCONFIG_NO_FORMATTING

#include "tznames_impl.h"
#include "uresimp.h"
#include "ureslocs.h"

#include "unicode/urename.h"
#include "unicode/tznames.h"

U_NAMESPACE_BEGIN

static const char gMetaZones[]          = "metaZones";
static const char gZoneStrings[]        = "zoneStrings";
static const char gCuTag[]              = "cu";
static const char gEcTag[]              = "ec";

TimeZoneNamesImpl::TimeZoneNamesImpl(UErrorCode& status) :
    fZoneStrings(NULL),
    fTzNamesMap(NULL),
    fMzNamesMap(NULL),
    fNamesTrie(TRUE),
    fNamesTrieFullyLoaded(FALSE),
    TimeZoneNames(status) {

}

TimeZoneNames*
TimeZoneNamesImpl::createInstance(const Locale& locale, UErrorCode& status) {
    initialize(locale, status);
    return TimeZoneNames::createInstance(locale, status);
}

void
TimeZoneNamesImpl::initialize(const Locale& locale, UErrorCode& status) {
    fZoneStrings = NULL;
    fZoneStrings = ures_open(U_ICUDATA_ZONE, locale.getName(), &status);
    // fZoneStrings = ures_getByKeyWithFallback(fZoneStrings, gZoneStrings, fZoneStrings, &status);
    fZoneStrings = ures_getByKey(fZoneStrings, gZoneStrings, fZoneStrings, &status);
    if (U_FAILURE(status)) {
        status = U_ZERO_ERROR;
        ures_close(fZoneStrings);
        fZoneStrings = NULL;
    }

    fNamesTrieFullyLoaded = FALSE;
}

ZNames::ZNames(UnicodeString* names, UBool shortCommonlyUsed) {
    // TODO(claireho) Replaced by StringEnumeration?

    for (int32_t i = 0; i < KEYS_SIZE; i++) {
        fNames[i].setTo(names[i]);
    }
    fShortCommonlyUsed = shortCommonlyUsed;
}

ZNames*
ZNames::getInstance(UResourceBundle* rb, const char* key) {
    UBool cu[1];
    UErrorCode status = U_ZERO_ERROR;
    loadData(rb, key, cu, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    return new ZNames(fNames, cu[0]);
}

UnicodeString*
ZNames::getName(TimeZoneNames::UNameType type) {
    // TODO(claireho) fName cannot be null. Need to add flag to indicate data is null.
    /*
    if (fNames == NULL) {
        return NULL;
    }
    */
    switch(type) {
    case TimeZoneNames::UTZN_LONG_GENERIC:
    case TimeZoneNames::UTZN_LONG_STANDARD:
    case TimeZoneNames::UTZN_LONG_DAYLIGHT:
    case TimeZoneNames::UTZN_SHORT_STANDARD:
    case TimeZoneNames::UTZN_SHORT_DAYLIGHT:
        return &(fNames[static_cast<uint32_t>(type)]);
    case TimeZoneNames::UTZN_SHORT_GENERIC:
        if (fShortCommonlyUsed) {
            return &(fNames[static_cast<uint32_t>(type)]);
        }
    case TimeZoneNames::UTZN_SHORT_STANDARD_COMMONLY_USED:
        if (fShortCommonlyUsed) {
            return &(fNames[static_cast<uint32_t>(TimeZoneNames::UTZN_SHORT_STANDARD)]);
        }
    case TimeZoneNames::UTZN_SHORT_DAYLIGHT_COMMONLY_USED:
        if (fShortCommonlyUsed) {
            return &(fNames[static_cast<uint32_t>(TimeZoneNames::UTZN_SHORT_DAYLIGHT)]);
        }
        break;
    }

    return NULL;
}

void
ZNames::cleanDataBuffer(void) {
    for (int32_t i = 0; i < KEYS_SIZE; i++) {
      fNames[i].remove();
    }
}

void
ZNames::loadData(UResourceBundle* rb, const char* key, UBool* shortCommonlyUsed,
                 UErrorCode& status) {
    cleanDataBuffer();  // cleanup fNames[].
    if (rb == NULL || key == NULL || key[0] != '\0') {
        // TODO(claireho) Is this error correct?
        status = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    shortCommonlyUsed[0] = FALSE;
    UResourceBundle* rbTable = NULL;
    UErrorCode tmpStatus = U_ZERO_ERROR;
    rbTable = ures_getByKeyWithFallback(rb, key, rbTable, &tmpStatus);
    if (U_FAILURE(tmpStatus)) {
        // TODO(claireho) review this logic
        status = U_MISSING_RESOURCE_ERROR;
        return;
    }

    UBool isEmpty = TRUE;
    int32_t len = 0;
    tmpStatus = U_ZERO_ERROR;
    const UChar* value;
    for (int32_t i = 0; i < KEYS_SIZE; i++) {
        tmpStatus = U_ZERO_ERROR;
        value = ures_getStringByKeyWithFallback(rbTable, KEYS[i], &len, &tmpStatus);
        if (value != NULL) {
            fNames[i].setTo(value, len);
            isEmpty = FALSE;
        }
    }
    if (isEmpty) {
        status = U_MISSING_RESOURCE_ERROR;
        return;
    }

    UResourceBundle* cuRes = ures_getByKeyWithFallback(rbTable, gCuTag, NULL, &tmpStatus);
    if (cuRes != NULL) {
        // cu is optional
        tmpStatus = U_ZERO_ERROR;
        int32_t cu = ures_getInt(cuRes, &tmpStatus);
        shortCommonlyUsed[0] = (cu != 0);
    }
}

TZNames*
TZNames::getInstance(UResourceBundle* rb, const char* key, UErrorCode& status) {
    if (rb == NULL || key == NULL || key[0] == '\0') {
      // TODO(claireho) Is this error correct?
      status = U_INTERNAL_PROGRAM_ERROR;
      return NULL;
    }

    UResourceBundle* rbTable = NULL;
    UErrorCode tmpStatus = U_ZERO_ERROR;
    rbTable = ures_getByKeyWithFallback(rb, key, rbTable, &tmpStatus);
    tmpStatus = U_ZERO_ERROR;
    int32_t len = 0;
    UnicodeString locationName = ures_getStringByKeyWithFallback(rbTable, gEcTag, &len, &tmpStatus);

    UBool cu[1];
    loadData(rb, key, cu, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    return new TZNames(fNames, cu[0], locationName);
}

TZNames::TZNames(UnicodeString names[], UBool shortCommonlyUsed,
                 const UnicodeString& locationName) :
    ZNames(names, shortCommonlyUsed),
    fLocationName(locationName) {
}

UnicodeString&
TZNames::getLocationName() {
    return fLocationName;
}

U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
