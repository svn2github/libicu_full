/*
*******************************************************************************
* Copyright (C) 2011, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#if !UCONFIG_NO_FORMATTING

#include "unicode/locdspnm.h"
#include "unicode/msgfmt.h"

#include "tzgnames.h"
#include "cmemory.h"
#include "uassert.h"
//#include "ucln_in.h"
#include "umutex.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "zonemeta.h"

U_NAMESPACE_BEGIN

#define ZID_KEY_MAX  128

//static UMTX gTZGNamesImplLock = NULL;

static const char gZoneStrings[]                = "zoneStrings";

static const char gRegionFormatTag[]            = "regionFormat";
static const char gFallbackRegionFormatTag[]    = "fallbackFormat";
static const char gFallbackFormatTag[]          = "fallbackFormat";

static const UChar gEmpty[]                     = {0x00};

static const UChar gDefRegionPattern[]          = {0x7B, 0x30, 0x7D, 0x00}; // "{0}"
static const UChar gDefFallbackRegionPattern[]  = {0x7B, 0x31, 0x7D, 0x20, 0x28, 0x7B, 0x30, 0x7D, 0x29, 0x00}; // "{1} ({0})"
static const UChar gDefFallbackPattern[]        = {0x7B, 0x31, 0x7D, 0x20, 0x28, 0x7B, 0x30, 0x7D, 0x29, 0x00}; // "{1} ({0})"

U_CDECL_BEGIN

typedef struct PartialLocationNames {
    const UChar* longName;
    const UChar* shortName;
} PartialLocationNames;

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
 * Deleter for PartialLocationNames
 */
static void U_CALLCONV
deletePartialLocationNames(void *obj) {
    PartialLocationNames *plnames = (PartialLocationNames *)obj;
    if (plnames->longName != NULL) {
        uprv_free((void *)plnames->longName);
    }
    if (plnames->shortName != NULL) {
        uprv_free((void *)plnames->shortName);
    }
    uprv_free(plnames);
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

    fPartialLocationNamesMap = uhash_open(uhash_hashUChars, uhash_compareUChars, NULL, &status);
    if (U_FAILURE(status)) {
        cleanup();
        return;
    }
    uhash_setValueDeleter(fPartialLocationNamesMap, deletePartialLocationNames);

    fLock = NULL;
//    ucln_i18n_registerCleanup(UCLN_I18N_TZGNAMES, tzGenericNames_cleanup);
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
                        } else {
                            // use the cached value
                            name.setTo(TRUE, newVal, -1);
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
    // TODO
    return name;
}

U_NAMESPACE_END
#endif
