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

#include "tznames.h"
#include "tznames_impl.h"
#include "umutex.h"
#include "uresimp.h"
#include "ureslocs.h"

#include "unicode/urename.h"

U_NAMESPACE_BEGIN

static UMTX TimeZoneNamesImplLock = NULL;

static const char gMetaZones[]          = "metaZones";
static const char gMapTimezonesTag[]    = "mapTimezones";
static const char gZoneStrings[]        = "zoneStrings";
static const char gCuTag[]              = "cu";
static const char gEcTag[]              = "ec";

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MetaZoneIdsEnumeration)

TimeZoneNamesImpl::TimeZoneNamesImpl(const Locale& locale, UErrorCode& status) :
    fZoneStrings(NULL),
    fTzNamesMap(NULL),
    fMetaZoneIds(NULL),
    fNamesTrie(TRUE),
    fNamesTrieFullyLoaded(FALSE) {
    initialize(locale, status);
}

void
TimeZoneNamesImpl::initialize(const Locale& locale, UErrorCode& status) {
    if (fZoneStrings != NULL) {
        ures_close(fZoneStrings);
    }
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

TimeZoneNamesImpl::~TimeZoneNamesImpl() {
    ures_close(fZoneStrings);
    // TODO(claireho) Need to delete *NamesMap tables
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs() const {
    umtx_lock(&TimeZoneNamesImplLock);
    if (fMetaZoneIds == NULL) {
        UErrorCode status = U_ZERO_ERROR;
        TimeZoneNamesImpl* self = const_cast<TimeZoneNamesImpl*>(this);
        self->fMetaZoneIds = new MetaZoneIdsEnumeration(status);
        if (U_FAILURE(status)) {
            delete fMetaZoneIds;
            self->fMetaZoneIds = NULL;
        }
    }
    umtx_unlock(&TimeZoneNamesImplLock);
    return fMetaZoneIds;
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(const UnicodeString& tzID) const {
    //TODO
    return NULL;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    //TODO
    return mzID;
}

UnicodeString&
TimeZoneNamesImpl::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    //TODO
    return tzID;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const {
    //TODO
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    //TODO
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    //TODO
    return name;
}

UEnumeration*
TimeZoneNamesImpl::find(const UnicodeString& text, int32_t start, int32_t types) const {
    return NULL;
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
ZNames::getName(UTimeZoneNameType type) {
    // TODO(claireho) fName cannot be null. Need to add flag to indicate data is null.
    /*
    if (fNames == NULL) {
        return NULL;
    }
    */
    switch(type) {
    case UTZNM_LONG_GENERIC:
    case UTZNM_LONG_STANDARD:
    case UTZNM_LONG_DAYLIGHT:
    case UTZNM_SHORT_STANDARD:
    case UTZNM_SHORT_DAYLIGHT:
        return &(fNames[static_cast<uint32_t>(type)]);
    case UTZNM_SHORT_GENERIC:
        if (fShortCommonlyUsed) {
            return &(fNames[static_cast<uint32_t>(type)]);
        }
    case UTZNM_SHORT_STANDARD_COMMONLY_USED:
        if (fShortCommonlyUsed) {
            return &(fNames[static_cast<uint32_t>(UTZNM_SHORT_STANDARD)]);
        }
    case UTZNM_SHORT_DAYLIGHT_COMMONLY_USED:
        if (fShortCommonlyUsed) {
            return &(fNames[static_cast<uint32_t>(UTZNM_SHORT_DAYLIGHT)]);
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

ZNames::~ZNames() {
    // TODO(claireho) Implement the destructor.
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

MetaZoneIdsEnumeration::MetaZoneIdsEnumeration(UErrorCode& status) :
    fMetaZoneIds(status) {
    pos=0;
    if (U_FAILURE(status)) {
        return;
    }
    fMetaZoneIds.removeAllElements();

    UErrorCode tmpStatus = U_ZERO_ERROR;
    UResourceBundle *rb = ures_openDirect(NULL, gMetaZones, &status);
    UResourceBundle *bundle = ures_getByKey(rb, gMapTimezonesTag, NULL, &status);
    while (U_SUCCESS(status) && ures_hasNext(bundle)) {
        const char *key;
        int32_t len = 0;
        ures_getNextString(bundle, &len, &key, &tmpStatus);
        if (U_FAILURE(tmpStatus)) {
            break;
        }
        // TODO(claireho) confirm the key is unique.
        fMetaZoneIds.addElement(new UnicodeString(key), status);
        if (U_FAILURE(status)) {
            break;
        }
    }
}

const UnicodeString*
MetaZoneIdsEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && pos < fMetaZoneIds.size()) {
        return (const UnicodeString*)fMetaZoneIds.elementAt(pos++);
    }
    return NULL;
}

void
MetaZoneIdsEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
MetaZoneIdsEnumeration::count(UErrorCode& /*status*/) const {
       return fMetaZoneIds.size();
}

MetaZoneIdsEnumeration::~MetaZoneIdsEnumeration() {
    UnicodeString *s;
    for (int32_t i=0; i<fMetaZoneIds.size(); ++i) {
        if ((s=(UnicodeString *)fMetaZoneIds.elementAt(i))!=NULL) {
            delete s;
        }
    }
}
U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
