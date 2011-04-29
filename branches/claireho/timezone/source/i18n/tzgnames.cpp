/*
*******************************************************************************
* Copyright (C) 2011, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#if !UCONFIG_NO_FORMATTING

#include "unicode/basictz.h"
#include "unicode/locdspnm.h"
#include "unicode/msgfmt.h"
#include "unicode/rbtz.h"
#include "unicode/simpletz.h"
#include "unicode/vtzone.h"

#include "tzgnames.h"
#include "cmemory.h"
#include "cstring.h"
#include "hash.h"
#include "olsontz.h"
#include "uassert.h"
#include "umutex.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "zonemeta.h"

U_NAMESPACE_BEGIN

#define ZID_KEY_MAX  128


static const char gZoneStrings[]                = "zoneStrings";

static const char gRegionFormatTag[]            = "regionFormat";
static const char gFallbackRegionFormatTag[]    = "fallbackRegionFormat";
static const char gFallbackFormatTag[]          = "fallbackFormat";

static const UChar gEmpty[]                     = {0x00};

static const UChar gDefRegionPattern[]          = {0x7B, 0x30, 0x7D, 0x00}; // "{0}"
static const UChar gDefFallbackRegionPattern[]  = {0x7B, 0x31, 0x7D, 0x20, 0x28, 0x7B, 0x30, 0x7D, 0x29, 0x00}; // "{1} ({0})"
static const UChar gDefFallbackPattern[]        = {0x7B, 0x31, 0x7D, 0x20, 0x28, 0x7B, 0x30, 0x7D, 0x29, 0x00}; // "{1} ({0})"

static const double kDstCheckRange      = (double)184*U_MILLIS_PER_DAY;

U_CDECL_BEGIN

typedef struct PartialLocationKey {
    const UChar* tzID;
    const UChar* mzID;
    UBool isLong;
} PartialLocationKey;

static int32_t U_CALLCONV
hashPartialLocationKey(const UHashTok key) {
    // <tzID>&<mzID>#[L|S]
    PartialLocationKey *p = (PartialLocationKey *)key.pointer;
    UnicodeString str(p->tzID);
    str.append((UChar)0x26)
        .append(p->mzID)
        .append((UChar)0x23)
        .append((UChar)(p->isLong ? 0x4C : 0x53));
    return uhash_hashUCharsN(str.getBuffer(), str.length());
}

static UBool U_CALLCONV
comparePartialLocationKey(const UHashTok key1, const UHashTok key2) {
    PartialLocationKey *p1 = (PartialLocationKey *)key1.pointer;
    PartialLocationKey *p2 = (PartialLocationKey *)key2.pointer;

    if (p1 == p2) {
        return TRUE;
    }
    if (p1 == NULL || p2 == NULL) {
        return FALSE;
    }
    // We just check identity of tzID/mzID
    return (p1->tzID == p2->tzID && p1->mzID == p2->mzID && p1->isLong == p2->isLong);
}

/**
 * Deleter for location name
 */
static void U_CALLCONV
deleteLocationName(void *obj) {
    if (obj != gEmpty) {
        uprv_free(obj);
    }
}

/**
 * Cleanup callback
 */
static UBool U_CALLCONV tzGenericNames_cleanup(void) {
//    umtx_destroy(&gTZGNamesImplLock);
    return TRUE;
}
U_CDECL_END

TimeZoneGenericNames::TimeZoneGenericNames(const Locale& locale, UErrorCode& status)
: fLocale(locale),
  fTimeZoneNames(NULL),
  fLocationNamesMap(NULL),
  fPartialLocationNamesMap(NULL),
  fRegionFormat(NULL),
  fFallbackRegionFormat(NULL),
  fFallbackFormat(NULL),
  fLocaleDisplayNames(NULL) {
    initialize(locale, status);
}

TimeZoneGenericNames::~TimeZoneGenericNames() {
    cleanup();
}

void
TimeZoneGenericNames::initialize(const Locale& locale, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    // TimeZoneNames
    fTimeZoneNames = TimeZoneNames::createInstance(locale, status);
    if (U_FAILURE(status)) {
        return;
    }

    // Initialize format patterns
    UnicodeString rpat(TRUE, gDefRegionPattern, -1);
    UnicodeString frpat(TRUE, gDefFallbackRegionPattern, -1);
    UnicodeString fpat(TRUE, gDefFallbackPattern, -1);

    UErrorCode tmpsts = U_ZERO_ERROR;   // OK with fallback warning..
    UResourceBundle *zoneStrings = ures_open(U_ICUDATA_ZONE, locale.getName(), &tmpsts);
    zoneStrings = ures_getByKeyWithFallback(zoneStrings, gZoneStrings, zoneStrings, &tmpsts);

    if (U_SUCCESS(tmpsts)) {
        const UChar *regionPattern = ures_getStringByKeyWithFallback(zoneStrings, gRegionFormatTag, NULL, &tmpsts);
        if (U_SUCCESS(tmpsts) && u_strlen(regionPattern) > 0) {
            rpat.setTo(regionPattern);
        }
        tmpsts = U_ZERO_ERROR;
        const UChar *fallbackRegionPattern = ures_getStringByKeyWithFallback(zoneStrings, gFallbackRegionFormatTag, NULL, &tmpsts);
        if (U_SUCCESS(tmpsts) && u_strlen(fallbackRegionPattern) > 0) {
            frpat.setTo(fallbackRegionPattern);
        }
        tmpsts = U_ZERO_ERROR;
        const UChar *fallbackPattern = ures_getStringByKeyWithFallback(zoneStrings, gFallbackFormatTag, NULL, &tmpsts);
        if (U_SUCCESS(tmpsts) && u_strlen(fallbackPattern) > 0) {
            fpat.setTo(fallbackPattern);
        }
    }

    fRegionFormat = new MessageFormat(rpat, status);
    if (fRegionFormat == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    fFallbackRegionFormat = new MessageFormat(frpat, status);
    if (fFallbackRegionFormat == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    fFallbackFormat = new MessageFormat(fpat, status);
    if (fFallbackFormat == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }

    // locale display names
    fLocaleDisplayNames = LocaleDisplayNames::createInstance(locale);

    // hash table for names
    fLocationNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }
    uhash_setValueDeleter(fLocationNamesMap, deleteLocationName);

    fPartialLocationNamesMap = uhash_open(hashPartialLocationKey, comparePartialLocationKey, NULL, &status);
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }
    uhash_setKeyDeleter(fPartialLocationNamesMap, uhash_freeBlock);
    uhash_setValueDeleter(fPartialLocationNamesMap, uhash_freeBlock);

    // target region
    const char* region = fLocale.getCountry();
    int32_t regionLen = uprv_strlen(region);
    if (regionLen == 0) {
        char loc[ULOC_FULLNAME_CAPACITY];
        uloc_addLikelySubtags(fLocale.getName(), loc, sizeof(loc), &status);

        regionLen = uloc_getCountry(loc, fTargetRegion, sizeof(fTargetRegion), &status);
        if (U_SUCCESS(status)) {
            fTargetRegion[regionLen] = 0;
        } else {
            cleanup();
            return;
        }
    } else if (regionLen < sizeof(fTargetRegion)) {
        uprv_strcpy(fTargetRegion, region);
    } else {
        fTargetRegion[0] = 0;
    }

    fLock = NULL;
}

void
TimeZoneGenericNames::cleanup() {
    if (fRegionFormat != NULL) {
        delete fRegionFormat;
    }
    if (fFallbackRegionFormat != NULL) {
        delete fFallbackRegionFormat;
    }
    if (fFallbackFormat != NULL) {
        delete fFallbackFormat;
    }
    if (fLocaleDisplayNames != NULL) {
        delete fLocaleDisplayNames;
    }

    uhash_close(fLocationNamesMap);
    uhash_close(fPartialLocationNamesMap);

    umtx_destroy(&fLock);
}

UnicodeString&
TimeZoneGenericNames::getDisplayName(const TimeZone& tz, UTimeZoneGenericNameType type, UDate date, UnicodeString& name) const {
    name.remove();
    switch (type) {
    case UTZGNM_LOCATION:
        {
            UnicodeString tzCanonicalID;
            tz.getCanonicalID(tzCanonicalID);
            getGenericLocationName(tzCanonicalID, name);
        }
        break;
    case UTZGNM_LONG:
    case UTZGNM_SHORT:
        formatGenericNonLocationName(tz, type, date, name);
        if (name.isEmpty()) {
            UnicodeString tzCanonicalID;
            tz.getCanonicalID(tzCanonicalID);
            getGenericLocationName(tzCanonicalID, name);
        }
        break;
    }
    return name;
}

UnicodeString&
TimeZoneGenericNames::getGenericLocationName(const UnicodeString& tzCanonicalID, UnicodeString& name) const {
    name.remove();
    if (tzCanonicalID.length() > ZID_KEY_MAX) {
        return name;
    }

    UErrorCode status = U_ZERO_ERROR;
    UChar tzIDKey[ZID_KEY_MAX + 1];
    int32_t tzIDKeyLen = tzCanonicalID.extract(tzIDKey, ZID_KEY_MAX + 1, status);
    U_ASSERT(status == U_ZERO_ERROR);   // already checked length above
    tzIDKey[tzIDKeyLen] = 0;

    TimeZoneGenericNames *nonConstThis = const_cast<TimeZoneGenericNames *>(this);
    const UChar *uname = NULL;
    umtx_lock(&nonConstThis->fLock);
    {
        uname = (const UChar *)uhash_get(fLocationNamesMap, tzIDKey);
    }
    umtx_unlock(&nonConstThis->fLock);

    if (uname != NULL) {
        // gEmpty indicate the name is not available
        if (uname != gEmpty) {
            name.setTo(TRUE, uname, -1);
        }
        return name;
    }

    // Construct location name
    UBool isSingleCountry = FALSE;
    UnicodeString usCountryCode;
    ZoneMeta::getSingleCountry(tzCanonicalID, usCountryCode);
    if (!usCountryCode.isEmpty()) {
        isSingleCountry = TRUE;
    } else {
        ZoneMeta::getCanonicalCountry(tzCanonicalID, usCountryCode);
    }

    if (!usCountryCode.isEmpty()) {
        char countryCode[ULOC_COUNTRY_CAPACITY];
        U_ASSERT(usCountryCode.length() < ULOC_COUNTRY_CAPACITY);
        int32_t ccLen = usCountryCode.extract(0, usCountryCode.length(), countryCode, sizeof(countryCode), US_INV);
        countryCode[ccLen] = 0;

        UnicodeString country;
        fLocaleDisplayNames->regionDisplayName(countryCode, country);

        // Format
        FieldPosition fpos;
        if (isSingleCountry) {
            // If the zone is only one zone in the country, do not add city
            Formattable param[] = {
                Formattable(country)
            };
            fRegionFormat->format(param, 1, name, fpos, status);
        } else {
            // getExemplarLocationName should retur non-empty string
            // if the time zone is associated with a region
            UnicodeString city;
            fTimeZoneNames->getExemplarLocationName(tzCanonicalID, city);

            Formattable params[] = {
                Formattable(city),
                Formattable(country)
            };
            fFallbackRegionFormat->format(params, 2, name, fpos, status);
        }
        if (U_FAILURE(status)) {
            name.remove();
        }
    }

    // Cache the result
    if (U_SUCCESS(status)) {
        umtx_lock(&nonConstThis->fLock);
        {
            uname = (const UChar *)uhash_get(fLocationNamesMap, tzIDKey);
            if (uname == NULL) {
                const UChar* newKey = ZoneMeta::findTimeZoneID(tzCanonicalID);
                U_ASSERT(newKey != NULL);
                if (name.isEmpty()) {
                    // gEmpty to indicate - no location name available
                    uhash_put(fLocationNamesMap, (void *)newKey, (void *)gEmpty, &status);
                } else {
                    UChar* newVal = NULL;
                    int32_t newValLen = name.length();
                    newVal = (UChar *)uprv_malloc(sizeof(UChar) * (newValLen + 1));
                    if (newVal == NULL) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                    } else {
                        name.extract(0, newValLen, newVal);
                        newVal[newValLen] = 0;
                        uhash_put(fLocationNamesMap, (void *)newKey, (void *)newVal, &status);
                        if (U_FAILURE(status)) {
                            uprv_free((void *)newVal);
                        }
                    }
                }
            }
        }
        umtx_unlock(&nonConstThis->fLock);
    }
    if (U_FAILURE(status)) {
        name.remove();
    }
    return name;
}

UnicodeString&
TimeZoneGenericNames::formatGenericNonLocationName(const TimeZone& tz, UTimeZoneGenericNameType type, UDate date, UnicodeString& name) const {
    U_ASSERT(type == UTZGNM_LONG || type == UTZGNM_SHORT);
    name.remove();
    UnicodeString tzID;
    tz.getCanonicalID(tzID);

    // Try to get a name from time zone first
    UTimeZoneNameType nameType = (type == UTZGNM_LONG) ? UTZNM_LONG_GENERIC : UTZNM_SHORT_GENERIC;
    fTimeZoneNames->getTimeZoneDisplayName(tzID, nameType, name);

    if (!name.isEmpty()) {
        return name;
    }

    // Try meta zone
    UnicodeString mzID;
    fTimeZoneNames->getMetaZoneID(tzID, date, mzID);
    if (!mzID.isEmpty()) {
        UErrorCode status = U_ZERO_ERROR;
        UBool useStandard = FALSE;
        int32_t raw, sav;

        tz.getOffset(date, FALSE, raw, sav, status);
        if (U_FAILURE(status)) {
            return name;
        }

        if (sav == 0) {
            TimeZone *tmptz = tz.clone();
            // Check if the zone actually uses daylight saving time around the time
            BasicTimeZone *btz = NULL;
            if (dynamic_cast<OlsonTimeZone *>(tmptz) != NULL
                || dynamic_cast<SimpleTimeZone *>(tmptz) != NULL
                || dynamic_cast<RuleBasedTimeZone *>(tmptz) != NULL
                || dynamic_cast<VTimeZone *>(tmptz) != NULL) {
                btz = (BasicTimeZone*)tmptz;
            }

            if (btz != NULL) {
                TimeZoneTransition before;
                UBool beforTrs = btz->getPreviousTransition(date, TRUE, before);
                if (beforTrs
                        && (date - before.getTime() < kDstCheckRange)
                        && before.getFrom()->getDSTSavings() != 0) {
                    useStandard = FALSE;
                } else {
                    TimeZoneTransition after;
                    UBool afterTrs = btz->getNextTransition(date, FALSE, after);
                    if (afterTrs
                            && (after.getTime() - date < kDstCheckRange)
                            && after.getTo()->getDSTSavings() != 0) {
                        useStandard = FALSE;
                    }
                }
            } else {
                // If not BasicTimeZone... only if the instance is not an ICU's implementation.
                // We may get a wrong answer in edge case, but it should practically work OK.
                tmptz->getOffset(date - kDstCheckRange, FALSE, raw, sav, status);
                if (sav != 0) {
                    useStandard = FALSE;
                } else {
                    tmptz->getOffset(date + kDstCheckRange, FALSE, raw, sav, status);
                    if (sav != 0){
                        useStandard = FALSE;
                    }
                }
                if (U_FAILURE(status)) {
                    delete tmptz;
                    return name;
                }
            }
            delete tmptz;
        }
        if (useStandard) {
            UTimeZoneNameType stdNameType = (nameType == UTZNM_LONG_GENERIC)
                ? UTZNM_LONG_STANDARD : UTZNM_SHORT_STANDARD_COMMONLY_USED;
            UnicodeString stdName;
            fTimeZoneNames->getDisplayName(tzID, stdNameType, date, stdName);
            if (!stdName.isEmpty()) {
                name.setTo(stdName);

                // TODO: revisit this issue later
                // In CLDR, a same display name is used for both generic and standard
                // for some meta zones in some locales.  This looks like a data bugs.
                // For now, we check if the standard name is different from its generic
                // name below.
                UnicodeString mzGenericName;
                fTimeZoneNames->getMetaZoneDisplayName(mzID, nameType, mzGenericName);
                if (stdName.caseCompare(mzGenericName, 0) == 0) {
                    name.remove();
                }
            }
        }
        if (name.isEmpty()) {
            // Get a name from meta zone
            UnicodeString mzName;
            fTimeZoneNames->getMetaZoneDisplayName(mzID, nameType, mzName);
            if (!mzName.isEmpty()) {
                // Check if we need to use a partial location format.
                // This check is done by comparing offset with the meta zone's
                // golden zone at the given date.
                UnicodeString goldenID;
                fTimeZoneNames->getReferenceZoneID(mzID, fTargetRegion, goldenID);
                if (!goldenID.isEmpty() && goldenID != tzID) {
                    TimeZone *goldenZone = TimeZone::createTimeZone(goldenID);
                    int32_t raw1, sav1;

                    // Check offset in the golden zone with wall time.
                    // With getOffset(date, false, offsets1),
                    // you may get incorrect results because of time overlap at DST->STD
                    // transition.
                    goldenZone->getOffset(date + raw + sav, TRUE, raw1, sav1, status);
                    delete goldenZone;
                    if (U_SUCCESS(status)) {
                        if (raw != raw1 || sav != sav1) {
                            // Now we need to use a partial location format
                            getPartialLocationName(tzID, mzID, (nameType == UTZNM_LONG_GENERIC), mzName, name);
                        } else {
                            name.setTo(mzName);
                        }
                    }
                } else {
                    name.setTo(mzName);
                }
            }
        }
    }
    return name;
}

UnicodeString&
TimeZoneGenericNames::getPartialLocationName(const UnicodeString& tzCanonicalID,
                        const UnicodeString& mzID, UBool isLong, const UnicodeString& mzDisplayName,
                        UnicodeString& name) const {
    name.remove();

    PartialLocationKey key;
    key.tzID = ZoneMeta::findTimeZoneID(tzCanonicalID);
    key.mzID = ZoneMeta::findMetaZoneID(mzID);
    key.isLong = isLong;
    U_ASSERT(key.tzID != NULL && key.mzID != NULL);

    const UChar* plname = NULL;
    TimeZoneGenericNames *nonConstThis = const_cast<TimeZoneGenericNames *>(this);
    umtx_lock(&nonConstThis->fLock);
    {
        plname = (const UChar*)uhash_get(fPartialLocationNamesMap, (void *)&key);
    }
    umtx_unlock(&nonConstThis->fLock);
    if (plname != NULL) {
        name.setTo(TRUE, plname, -1);
        return name;
    }

    UnicodeString location;
    UnicodeString usCountryCode;
    ZoneMeta::getSingleCountry(tzCanonicalID, usCountryCode);
    if (!usCountryCode.isEmpty()) {
        char countryCode[ULOC_COUNTRY_CAPACITY];
        U_ASSERT(usCountryCode.length() < ULOC_COUNTRY_CAPACITY);
        int32_t ccLen = usCountryCode.extract(0, usCountryCode.length(), countryCode, sizeof(countryCode), US_INV);
        countryCode[ccLen] = 0;
        fLocaleDisplayNames->regionDisplayName(countryCode, location);
    } else {
        fTimeZoneNames->getExemplarLocationName(tzCanonicalID, location);
        if (location.isEmpty()) {
            // This could happen when the time zone is not associated with a country,
            // and its ID is not hierarchical, for example, CST6CDT.
            // We use the canonical ID itself as the location for this case.
            location.setTo(tzCanonicalID);
        }
    }

    UErrorCode status = U_ZERO_ERROR;

    FieldPosition fpos;
    Formattable param[] = {
        Formattable(location),
        Formattable(mzDisplayName)
    };
    fFallbackFormat->format(param, 2, name, fpos, status);
    if (U_FAILURE(status)) {
        name.remove();
        return name;
    }

    // Add the name to cache
    umtx_lock(&nonConstThis->fLock);
    {
        plname = (const UChar*)uhash_get(fPartialLocationNamesMap, (void *)&key);
        if (plname == NULL) {
            int32_t len = name.length();
            UChar* newVal = (UChar *)uprv_malloc(sizeof(UChar) * (len + 1));
            if (newVal != NULL) {
                PartialLocationKey* newKey = (PartialLocationKey *)uprv_malloc(sizeof(PartialLocationKey));
                if (newKey == NULL) {
                    uprv_free(newVal);
                } else {
                    name.extract(0, len, newVal);
                    newVal[len] = 0;

                    newKey->tzID = key.tzID;
                    newKey->mzID = key.mzID;
                    newKey->isLong = key.isLong;

                    uhash_put(fPartialLocationNamesMap, (void *)newKey, (void *)newVal, &status);
                    if (U_FAILURE(status)) {
                        uprv_free(newKey);
                        uprv_free(newVal);
                    }
                }
            }
        }
    }
    umtx_unlock(&nonConstThis->fLock);
    return name;
}

U_NAMESPACE_END
#endif
