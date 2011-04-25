/*
*******************************************************************************
* Copyright (C) 2011, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/locid.h"
#include "unicode/uenum.h"
#include "cmemory.h"
#include "cstring.h"
#include "putilimp.h"
#include "uassert.h"
#include "ucln_in.h"
#include "uhash.h"
#include "umutex.h"

#include "tznames.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

class TimeZoneNamesImpl : public TimeZoneNames {
public:
    TimeZoneNamesImpl(const Locale& locale);
    virtual ~TimeZoneNamesImpl();

    StringEnumeration* getAvailableMetaZoneIDs() const;
    StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID) const;
    UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const;
    UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const;

    UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const;
    UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const;

    UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

    UEnumeration* find(const UnicodeString& text, int32_t start, int32_t types) const;
private:
    Locale fLocale;
};

TimeZoneNamesImpl::TimeZoneNamesImpl(const Locale& locale)
:   fLocale(locale) {
    //TODO
}

TimeZoneNamesImpl::~TimeZoneNamesImpl() {
    //TODO
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs() const {
    //TODO
    return NULL;
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


// TimeZoneNames object cache handling
static UMTX gTimeZoneNamesLock = NULL;
static UHashtable *gTimeZoneNamesCache = NULL;
static UBool gTimeZoneNamesCacheInitialized = FALSE;

// Access count - incremented every time up to SWEEP_INTERVAL,
// then reset to 0
static int32_t gAccessCount = 0;

// Interval for calling the cache sweep function
#define SWEEP_INTERVAL 100

// Cache expiration in millisecond. When a cached entry is no
// longer referenced and exceeding this threshold since last
// access time, then the cache entry will be deleted by the sweep
// function.
#define CACHE_EXPIRATION 600000.0

typedef struct TimeZoneNamesCacheEntry {
    TimeZoneNames*  names;
    int32_t         refCount;
    double          lastAccess;
} TimeZoneNamesCacheEntry;

U_CDECL_BEGIN
/**
 * Cleanup callback func
 */
static UBool U_CALLCONV timeZoneNames_cleanup(void)
{
    if (gTimeZoneNamesCache != NULL) {
        uhash_close(gTimeZoneNamesCache);
        gTimeZoneNamesCache = NULL;
    }
    gTimeZoneNamesCacheInitialized = FALSE;
    return TRUE;
}

/**
 * Deleter for locake key
 */
static void U_CALLCONV
deleteChars(void *obj) {
    uprv_free(obj);
}

/**
 * Deleter for TimeZoneNamesCacheEntry
 */
static void U_CALLCONV
deleteTimeZoneNamesCacheEntry(void *obj) {
    U_NAMESPACE_QUALIFIER TimeZoneNamesCacheEntry *entry = (U_NAMESPACE_QUALIFIER TimeZoneNamesCacheEntry*)obj;
    delete (U_NAMESPACE_QUALIFIER TimeZoneNamesImpl*) entry->names;
    uprv_free(entry);
}
U_CDECL_END

/**
 * Function used for removing unreferrenced cache entries exceeding
 * the expiration time. This function must be called with in the mutex
 * block.
 */
static void sweepCache() {
    int32_t pos = -1;
    const UHashElement* elem;
    double now = (double)uprv_getUTCtime();

    while (elem = uhash_nextElement(gTimeZoneNamesCache, &pos)) {
        TimeZoneNamesCacheEntry *entry = (TimeZoneNamesCacheEntry *)elem->value.pointer;
        if (entry->refCount <= 0 && (now - entry->lastAccess) > CACHE_EXPIRATION) {
            // delete this entry
            uhash_removeElement(gTimeZoneNamesCache, elem);
        }
    }
}

class TimeZoneNamesDelegate : public TimeZoneNames {
public:
    TimeZoneNamesDelegate(const Locale& locale, UErrorCode& status);
    virtual ~TimeZoneNamesDelegate();

    StringEnumeration* getAvailableMetaZoneIDs() const;
    StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID) const;
    UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const;
    UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const;

    UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const;
    UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const;

    UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

    UEnumeration* find(const UnicodeString& text, int32_t start, int32_t types) const;
private:
    TimeZoneNamesCacheEntry*    tznamesCacheEntry;
};

TimeZoneNamesDelegate::TimeZoneNamesDelegate(const Locale& locale, UErrorCode& status) {
    UBool initialized;
    UMTX_CHECK(&gTimeZoneNamesLock, gTimeZoneNamesCacheInitialized, initialized);
    if (!initialized) {
        // Create empty hashtable
        umtx_lock(&gTimeZoneNamesLock);
        {
            if (!gTimeZoneNamesCacheInitialized) {
                gTimeZoneNamesCache = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
                if (gTimeZoneNamesLock == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                }
                if (U_FAILURE(status)) {
                    return;
                }
                uhash_setKeyDeleter(gTimeZoneNamesCache, deleteChars);
                uhash_setValueDeleter(gTimeZoneNamesCache, deleteTimeZoneNamesCacheEntry);
                gTimeZoneNamesCacheInitialized = TRUE;
                ucln_i18n_registerCleanup(UCLN_I18N_TIMEZONENAMES, timeZoneNames_cleanup);
            }
        }
        umtx_unlock(&gTimeZoneNamesLock);
    }

    // Check the cache, if not available, create new one and cache
    TimeZoneNamesCacheEntry *cacheEntry = NULL;
    umtx_lock(&gTimeZoneNamesLock);
    {
        const char *key = locale.getName();
        cacheEntry = (TimeZoneNamesCacheEntry *)uhash_get(gTimeZoneNamesCache, key);
        if (cacheEntry == NULL) {
            TimeZoneNames *tznames = new TimeZoneNamesImpl(locale);
            if (tznames == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            char *newKey = (char *)uprv_malloc(uprv_strlen(key) + 1);
            if (newKey == NULL) {
                uprv_free(tznames);
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            uprv_strcpy(newKey, key);
            cacheEntry = (TimeZoneNamesCacheEntry *)uprv_malloc(sizeof(TimeZoneNamesCacheEntry));
            if (cacheEntry == NULL) {
                uprv_free(tznames);
                uprv_free(newKey);
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            cacheEntry->names = tznames;
            cacheEntry->refCount = 1;
            cacheEntry->lastAccess = (double)uprv_getUTCtime();

            uhash_put(gTimeZoneNamesCache, newKey, cacheEntry, &status);
            if (U_FAILURE(status)) {
                //TODO
            }
        } else {
            // Update the reference count
            cacheEntry->refCount++;
            cacheEntry->lastAccess = (double)uprv_getUTCtime();
        }
        gAccessCount++;
        if (gAccessCount >= SWEEP_INTERVAL) {
            // sweep
            sweepCache();
            gAccessCount = 0;
        }
    }
    umtx_unlock(&gTimeZoneNamesLock);

    tznamesCacheEntry = cacheEntry;
}

TimeZoneNamesDelegate::~TimeZoneNamesDelegate() {
    umtx_lock(&gTimeZoneNamesLock);
    {
        U_ASSERT(tznamesCacheEntry->refCount > 0);
        // Just decrement the reference count
        tznamesCacheEntry->refCount--;
    }
    umtx_unlock(&gTimeZoneNamesLock);
}

StringEnumeration*
TimeZoneNamesDelegate::getAvailableMetaZoneIDs() const {
    return tznamesCacheEntry->names->getAvailableMetaZoneIDs();
}

StringEnumeration*
TimeZoneNamesDelegate::getAvailableMetaZoneIDs(const UnicodeString& tzID) const {
    return tznamesCacheEntry->names->getAvailableMetaZoneIDs(tzID);
}

UnicodeString&
TimeZoneNamesDelegate::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    return tznamesCacheEntry->names->getMetaZoneID(tzID, date, mzID);
}

UnicodeString&
TimeZoneNamesDelegate::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    return tznamesCacheEntry->names->getReferenceZoneID(mzID, region, tzID);
}

UnicodeString&
TimeZoneNamesDelegate::getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const {
    return tznamesCacheEntry->names->getMetaZoneDisplayName(mzID, type, name);
}

UnicodeString&
TimeZoneNamesDelegate::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    return tznamesCacheEntry->names->getTimeZoneDisplayName(tzID, type, name);
}

UnicodeString&
TimeZoneNamesDelegate::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    return tznamesCacheEntry->names->getExemplarLocationName(tzID, name);
}

UEnumeration*
TimeZoneNamesDelegate::find(const UnicodeString& text, int32_t start, int32_t types) const {
    return tznamesCacheEntry->names->find(text, start, types);
}




TimeZoneNames::~TimeZoneNames() {
}

TimeZoneNames*
TimeZoneNames::createInstance(const Locale& locale, UErrorCode& status) {
    return new TimeZoneNamesDelegate(locale, status);
}

UnicodeString&
TimeZoneNames::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    //TODO
    return name;
}

UnicodeString&
TimeZoneNames::getZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UDate date, UnicodeString& name) const {
    //TODO
    return name;
}

U_NAMESPACE_END
#endif
