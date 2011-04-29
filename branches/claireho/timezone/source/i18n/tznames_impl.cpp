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

#include "cmemory.h"
#include "cstring.h"
#include "tznames.h"
#include "tznames_impl.h"
#include "umutex.h"
#include "uassert.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "zonemeta.h"
#include "ucln_in.h"

U_NAMESPACE_BEGIN

static UMTX gTZNamesImplLock = NULL;

#define ZID_KEY_MAX  128
#define MZ_PREFIX_LEN 5

static const char gZoneStrings[]        = "zoneStrings";
static const char gMZPrefix[]           = "meta:";

static const char* KEYS[]               = {"lg", "ls", "ld", "sg", "ss", "sd"};
static const int32_t KEYS_SIZE = (sizeof KEYS / sizeof KEYS[0]);

static const char gCuTag[]              = "cu";
static const char gEcTag[]              = "ec";

static const char EMPTY[]               = "<empty>";   // place holder for empty ZNames/TZNames

U_CDECL_BEGIN
/**
 * Deleter for ZNames
 */
static void U_CALLCONV
deleteZNames(void *obj) {
    if (obj != EMPTY) {
        delete (U_NAMESPACE_QUALIFIER ZNames *)obj;
    }
}
/**
 * Deleter for TZNames
 */
static void U_CALLCONV
deleteTZNames(void *obj) {
    if (obj != EMPTY) {
        delete (U_NAMESPACE_QUALIFIER TZNames *)obj;
    }
}

static UBool U_CALLCONV tzNamesImpl_cleanup(void) {
    umtx_destroy(&gTZNamesImplLock);
    return TRUE;
}
U_CDECL_END

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MetaZoneIDsEnumeration)

TimeZoneNamesImpl::TimeZoneNamesImpl(const Locale& locale, UErrorCode& status)
: fZoneStrings(NULL), fMZNamesMap(NULL), fTZNamesMap(NULL) {
    ucln_i18n_registerCleanup(UCLN_I18N_TZNAMESIMPL, tzNamesImpl_cleanup);
    initialize(locale, status);
}

void
TimeZoneNamesImpl::initialize(const Locale& locale, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    // Load zoneStrings bundle
    UErrorCode tmpsts = U_ZERO_ERROR;   // OK with fallback warning..
    fZoneStrings = ures_open(U_ICUDATA_ZONE, locale.getName(), &tmpsts);
    fZoneStrings = ures_getByKeyWithFallback(fZoneStrings, gZoneStrings, fZoneStrings, &tmpsts);
    if (U_FAILURE(tmpsts)) {
        status = tmpsts;
        cleanup();
        return;
    }

    // Initialize hashtables holding time zone/meta zone names
    fMZNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    fTZNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }

    uhash_setValueDeleter(fMZNamesMap, deleteZNames);
    uhash_setValueDeleter(fTZNamesMap, deleteTZNames);
    // no key deleters for name maps

    return;
}

TimeZoneNamesImpl::~TimeZoneNamesImpl() {
    cleanup();
}

void
TimeZoneNamesImpl::cleanup() {
    if (fZoneStrings != NULL) {
        ures_close(fZoneStrings);
        fZoneStrings = NULL;
    }
    if (fMZNamesMap != NULL) {
        uhash_close(fMZNamesMap);
        fMZNamesMap = NULL;
    }
    if (fTZNamesMap != NULL) {
        uhash_close(fTZNamesMap);
        fTZNamesMap = NULL;
    }
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const UVector* mzIDs = ZoneMeta::getAvailableMetazoneIDs();
    if (mzIDs == NULL) {
        return new MetaZoneIDsEnumeration();
    }
    return new MetaZoneIDsEnumeration(*mzIDs);
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const UVector* mappings = ZoneMeta::getMetazoneMappings(tzID);
    if (mappings == NULL) {
        return new MetaZoneIDsEnumeration();
    }

    MetaZoneIDsEnumeration *senum = NULL;
    UVector* mzIDs = new UVector(NULL, uhash_compareUChars, status);
    if (mzIDs == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_SUCCESS(status)) {
        for (int32_t i = 0; U_SUCCESS(status) && i < mappings->size(); i++) {
            OlsonToMetaMappingEntry *map = (OlsonToMetaMappingEntry *)mappings->elementAt(i);
            const UChar *mzID = map->mzid;
            if (!mzIDs->contains((void *)mzID)) {
                mzIDs->addElement((void *)mzID, status);
            }
        }
        if (U_SUCCESS(status)) {
            senum = new MetaZoneIDsEnumeration(mzIDs);
        } else {
            delete mzIDs;
        }
    }
    return senum;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    ZoneMeta::getMetazoneID(tzID, date, mzID);
    return mzID;
}

UnicodeString&
TimeZoneNamesImpl::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    ZoneMeta::getZoneIdByMetazone(mzID, UnicodeString(region), tzID);
    return tzID;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneDisplayName(const UnicodeString& mzID,
                                          UTimeZoneNameType type,
                                          UnicodeString& name) const {
    name.remove();  // cleanup result.
    if (mzID.isEmpty()) {
        return name;
    }

    ZNames *znames = loadMetaZoneNames(mzID);
    if (znames != NULL) {
        const UChar* s = znames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    name.remove();  // cleanup result.
    if (tzID.isEmpty()) {
        return name;
    }

    TZNames *tznames = loadTimeZoneNames(tzID);
    if (tznames != NULL) {
        const UChar *s = tznames->getName(type);
        if (s != NULL) {
            name.setTo(TRUE, s, -1);
        }
    }
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    const UChar* locName = NULL;
    TZNames *tznames = loadTimeZoneNames(tzID);
    if (tznames != NULL) {
        locName = tznames->getLocationName();
    }
    if (locName != NULL) {
        name.setTo(TRUE, locName, -1);
        return name;
    }

    return TimeZoneNames::getExemplarLocationName(tzID, name);
}


// Merge the MZ_PREFIX and mzId
static void mergeTimeZoneKey(const UnicodeString& mzID, char* result) {
    if (mzID.isEmpty()) {
        result[0] = '\0';
        return;
    }

    char mzIdChar[ZID_KEY_MAX + 1];
    int32_t keyLen;
    int32_t prefixLen = uprv_strlen(gMZPrefix);
    keyLen = mzID.extract(0, mzID.length(), mzIdChar, ZID_KEY_MAX + 1, US_INV);
    uprv_memcpy((void *)result, (void *)gMZPrefix, prefixLen);
    uprv_memcpy((void *)(result + prefixLen), (void *)mzIdChar, keyLen);
    result[keyLen + prefixLen] = '\0';
}

ZNames*
TimeZoneNamesImpl::loadMetaZoneNames(const UnicodeString& mzID) const {
    if (mzID.length() > (ZID_KEY_MAX - MZ_PREFIX_LEN)) {
        return NULL;
    }

    ZNames *znames = NULL;

    UErrorCode status = U_ZERO_ERROR;
    UChar mzIDKey[ZID_KEY_MAX + 1];
    mzID.extract(mzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(status == U_ZERO_ERROR);   // already checked length above
    mzIDKey[mzID.length()] = 0;

    umtx_lock(&gTZNamesImplLock);
    {
        void *cacheVal = uhash_get(fMZNamesMap, mzIDKey);
        if (cacheVal == NULL) {
            char key[ZID_KEY_MAX + 1];
            mergeTimeZoneKey(mzID, key);
            znames = ZNames::createInstance(fZoneStrings, key);

            if (znames == NULL) {
                cacheVal = (void *)EMPTY;
            } else {
                cacheVal = znames;
            }
            // Use the persisten ID as the resource key, so we can
            // avoid duplications.
            const UChar* newKey = ZoneMeta::findMetaZoneID(mzID);
            if (newKey != NULL) {
                uhash_put(fMZNamesMap, (void *)newKey, cacheVal, &status);
            } else {
                // Should never happen with a valid input
                if (znames != NULL) {
                    // It's not possible that we get a valid ZNames with unknown ID.
                    // But just in case..
                    delete znames;
                    znames = NULL;
                }
            }
        } else if (cacheVal != EMPTY) {
            znames = (ZNames *)cacheVal;
        }
    }
    umtx_unlock(&gTZNamesImplLock);

    return znames;
}

static void convertTzToCLDRFomat(const UnicodeString& tzID, char* result) {
    int32_t len = tzID.length();
    for (int32_t i = 0; i < len; i++) {
        char c = (char)tzID.charAt(i);
        if (c == '/') c = ':';  // Reaplce '/' with ':'
        result[i] = c;
    }
    result[len] = '\0';
}

TZNames*
TimeZoneNamesImpl::loadTimeZoneNames(const UnicodeString& tzID) const {
    if (tzID.length() > ZID_KEY_MAX) {
        return NULL;
    }

    TZNames *tznames = NULL;

    UErrorCode status = U_ZERO_ERROR;
    UChar tzIDKey[ZID_KEY_MAX + 1];
    int32_t tzIDKeyLen = tzID.extract(tzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(status == U_ZERO_ERROR);   // already checked length above
    tzIDKey[tzIDKeyLen] = 0;

    umtx_lock(&gTZNamesImplLock);
    {
        void *cacheVal = uhash_get(fTZNamesMap, tzIDKey);
        if (cacheVal == NULL) {
            char key[ZID_KEY_MAX + 1];
            UErrorCode status = U_ZERO_ERROR;
            // Replace "/" with ":".
            convertTzToCLDRFomat(tzID, key);
            tznames = TZNames::createInstance(fZoneStrings, key);

            if (tznames == NULL) {
                cacheVal = (void *)EMPTY;
            } else {
                cacheVal = tznames;
            }
            // Use the persistent ID as the resource key, so we can
            // avoid duplications.
            const UChar* newKey = ZoneMeta::findTimeZoneID(tzID);
            if (newKey != NULL) {
                uhash_put(fTZNamesMap, (void *)newKey, cacheVal, &status);
            } else {
                // Should never happen with a valid input
                if (tznames != NULL) {
                    // It's not possible that we get a valid TZNames with unknown ID.
                    // But just in case..
                    delete tznames;
                    tznames = NULL;
                }
            }
        } else if (cacheVal != EMPTY) {
            tznames = (TZNames *)cacheVal;
        }
    }
    umtx_unlock(&gTZNamesImplLock);

    return tznames;

}


ZNames::ZNames(const UChar** names, UBool shortCommonlyUsed)
: fNames(names), fShortCommonlyUsed(shortCommonlyUsed) {
}

ZNames::~ZNames() {
    if (fNames != NULL) {
        uprv_free(fNames);
    }
}

ZNames*
ZNames::createInstance(UResourceBundle* rb, const char* key) {
    UBool shortCommonlyUsed = FALSE;
    const UChar** names = loadData(rb, key, shortCommonlyUsed);
    if (names == NULL) {
        // No names data available
        return NULL; 
    }
    return new ZNames(names, shortCommonlyUsed);
}

const UChar*
ZNames::getName(UTimeZoneNameType type) {
    if (fNames == NULL) {
        return NULL;
    }
    const UChar *name = NULL;
    switch(type) {
    case UTZNM_LONG_GENERIC:
        name = fNames[0];
        break;
    case UTZNM_LONG_STANDARD:
        name = fNames[1];
        break;
    case UTZNM_LONG_DAYLIGHT:
        name = fNames[2];
        break;
    case UTZNM_SHORT_GENERIC:
        if (fShortCommonlyUsed) {
            name = fNames[3];
        }
        break;
    case UTZNM_SHORT_STANDARD:
        name = fNames[4];
        break;
    case UTZNM_SHORT_DAYLIGHT:
        name = fNames[5];
        break;
    case UTZNM_SHORT_STANDARD_COMMONLY_USED:
        if (fShortCommonlyUsed) {
            name = fNames[4];
        }
        break;
    case UTZNM_SHORT_DAYLIGHT_COMMONLY_USED:
        if (fShortCommonlyUsed) {
            name = fNames[5];
        }
        break;
    }
    return name;
}

const UChar**
ZNames::loadData(UResourceBundle* rb, const char* key, UBool& shortCommonlyUsed) {
    if (rb == NULL || key == NULL || *key == 0) {
        return NULL;
    }

    UErrorCode status = U_ZERO_ERROR;
    const UChar **names = NULL;

    UResourceBundle* rbTable = NULL;
    rbTable = ures_getByKeyWithFallback(rb, key, rbTable, &status);
    if (U_SUCCESS(status)) {
        names = (const UChar **)uprv_malloc(sizeof(const UChar*) * KEYS_SIZE);
        if (names != NULL) {
            UBool isEmpty = TRUE;
            for (int32_t i = 0; i < KEYS_SIZE; i++) {
                status = U_ZERO_ERROR;
                int32_t len = 0;
                const UChar *value = ures_getStringByKeyWithFallback(rbTable, KEYS[i], &len, &status);
                if (U_FAILURE(status) || len == 0) {
                    names[i] = NULL;
                } else {
                    names[i] = value;
                    isEmpty = FALSE;
                }
            }
            if (isEmpty) {
                // No need to keep the names array
                uprv_free(names);
                names = NULL;
            }
        }

        if (names != NULL) {
            status = U_ZERO_ERROR;
            UResourceBundle* cuRes = ures_getByKeyWithFallback(rbTable, gCuTag, NULL, &status);
            int32_t cu = ures_getInt(cuRes, &status);
            if (U_SUCCESS(status)) {
                shortCommonlyUsed = (cu != 0);
            }
            ures_close(cuRes);
        }
    }
    ures_close(rbTable);
    return names;
}

TZNames::TZNames(const UChar** names, UBool shortCommonlyUsed, const UChar* locationName)
: ZNames(names, shortCommonlyUsed), fLocationName(locationName) {
}

TZNames::~TZNames() {
}

const UChar*
TZNames::getLocationName() {
    return fLocationName;
}

TZNames*
TZNames::createInstance(UResourceBundle* rb, const char* key) {
    if (rb == NULL || key == NULL || *key == 0) {
        return NULL;
    }
    TZNames* tznames = NULL;
    UErrorCode status = U_ZERO_ERROR;
    // printf("\n key:%s", key);  // check for duplicate keys ...
    UResourceBundle* rbTable = ures_getByKeyWithFallback(rb, key, NULL, &status);
    if (U_SUCCESS(status)) {
        int32_t len = 0;
        const UChar* locationName = ures_getStringByKeyWithFallback(rbTable, gEcTag, &len, &status);
        if (U_FAILURE(status) || len == 0) {
            locationName = NULL;
        }

        UBool shortCommonlyUsed = FALSE;
        const UChar** names = loadData(rb, key, shortCommonlyUsed);

        if (locationName != NULL || names != NULL) {
            tznames = new TZNames(names, shortCommonlyUsed, locationName);
        }
    }
    ures_close(rbTable);
    return tznames;
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration() 
: fLen(0), fPos(0), fMetaZoneIDs(NULL), fLocalVector(NULL) {
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration(const UVector& mzIDs) 
: fPos(0), fMetaZoneIDs(&mzIDs), fLocalVector(NULL) {
    fLen = fMetaZoneIDs->size();
}

MetaZoneIDsEnumeration::MetaZoneIDsEnumeration(UVector *mzIDs)
: fLen(0), fPos(0), fMetaZoneIDs(mzIDs), fLocalVector(mzIDs) {
    if (fMetaZoneIDs) {
        fLen = fMetaZoneIDs->size();
    }
}

const UnicodeString*
MetaZoneIDsEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && fMetaZoneIDs != NULL && fPos < fLen) {
        unistr.setTo((const UChar*)fMetaZoneIDs->elementAt(fPos++));
    }
    return &unistr;
}

void
MetaZoneIDsEnumeration::reset(UErrorCode& /*status*/) {
    fPos = 0;
}

int32_t
MetaZoneIDsEnumeration::count(UErrorCode& /*status*/) const {
    return fLen;
}

MetaZoneIDsEnumeration::~MetaZoneIDsEnumeration() {
    if (fLocalVector) {
        delete fLocalVector;
    }
}

U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
