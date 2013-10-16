/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
*******************************************************************************
*                                                                             
* File LRUCACHE.CPP                                                             
*******************************************************************************
*/

#include "unicode/lrucache.h"
#include "mutex.h"
#include "uhash.h"
#include "cstring.h"

U_NAMESPACE_BEGIN

// Named CacheEntry2 to avoid conflict with CacheEntry in serv.cpp
// Don't know how to make truly private class that the linker can't see.
class CacheEntry2 : public UMemory {
public:
    CacheEntry2 *moreRecent;
    CacheEntry2 *lessRecent;
    char *localeId;
    UObject *cachedData;
    UErrorCode status;  // This is the error if any from creating cachedData.
    u_atomic_int32_t refCount;

    CacheEntry2();
    ~CacheEntry2();

    UBool canEvict();
    void unlink();
    void uninit();
    UBool init(const char *localeId, UObject *dataToAdopt, UErrorCode err);
private:
    CacheEntry2(const CacheEntry2& other);
    CacheEntry2 &operator=(const CacheEntry2& other);
};

CacheEntry2::CacheEntry2() 
    : moreRecent(NULL), lessRecent(NULL), localeId(NULL), cachedData(NULL),
      status(U_ZERO_ERROR) {

    // Count this cache entry as first reference to data.
    umtx_storeRelease(refCount, 1);
}

CacheEntry2::~CacheEntry2() {
    uninit();
}

UBool CacheEntry2::canEvict() {
    // We can evict if there are no other references.
    return umtx_loadAcquire(refCount) == 1;
}

void CacheEntry2::unlink() {
    if (moreRecent != NULL) {
        moreRecent->lessRecent = lessRecent;
    }
    if (lessRecent != NULL) {
        lessRecent->moreRecent = moreRecent;
    }
    moreRecent = NULL;
    lessRecent = NULL;
}

void CacheEntry2::uninit() {
    delete cachedData;
    cachedData = NULL;
    status = U_ZERO_ERROR;
    if (localeId != NULL) {
        uprv_free(localeId);
    }
    localeId = NULL;
}

UBool CacheEntry2::init(const char *locId, UObject *dataToAdopt, UErrorCode err) {
    uninit();
    localeId = (char *) uprv_malloc(strlen(locId) + 1);
    if (localeId == NULL) {
        delete dataToAdopt;
        return FALSE;
    }
    uprv_strcpy(localeId, locId);
    cachedData = dataToAdopt;
    status = err;
    return TRUE;
}

void LRUCache::moveToMostRecent(CacheEntry2 *entry) {
    entry->unlink();
    entry->moreRecent = mostRecentlyUsedMarker;
    entry->lessRecent = mostRecentlyUsedMarker->lessRecent;
    mostRecentlyUsedMarker->lessRecent->moreRecent = entry;
    mostRecentlyUsedMarker->lessRecent = entry;
}

UObject *LRUCache::safeCreate(const char *localeId, UErrorCode &status) {
    UObject *result = create(localeId, status);

    // Safe guard to ensure that some error is reported for missing data in
    // case subclass forgets to set status.
    if (result == NULL && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    } 

    // Safe guard to ensure that if subclass reports an error and returns
    // data that we don't leak memory.
    if (result != NULL && U_FAILURE(status)) {
        delete result;
        return NULL;
    }
    return result;
}

UBool LRUCache::init(const char *localeId, CacheEntry2 *entry) {
    UErrorCode status = U_ZERO_ERROR;
    UObject *result = safeCreate(localeId, status);
    return entry->init(localeId, result, status);
}

UBool LRUCache::contains(const char *localeId) const {
    return (uhash_get(localeIdToEntries, localeId) != NULL);
}


void LRUCache::_get(const char *localeId, UObject *&ptr, u_atomic_int32_t *&refPtr, UErrorCode &status) {
    // Begin mutex
    {
        Mutex lock(mutex);
        CacheEntry2 *entry = (CacheEntry2 *) uhash_get(localeIdToEntries, localeId);
        if (entry != NULL) {
            moveToMostRecent(entry);
        } else {
            // Its a cache miss.

            if (uhash_count(localeIdToEntries) < maxSize) {
                entry = new CacheEntry2;
            } else {
                entry = leastRecentlyUsedMarker->moreRecent;
                while (entry != mostRecentlyUsedMarker && !entry->canEvict()) {
                    entry = entry->moreRecent;
                }
                if (entry != mostRecentlyUsedMarker) {
                    // We found an entry to evict.
                    uhash_remove(localeIdToEntries, entry->localeId);
                    entry->unlink();
                    entry->uninit();
                } else {
                    // Cache is full, but nothing can be evicted.
                    entry = NULL;
                }
            }
 
            // Either there is room in the cache and entry != NULL or the cache is
            // full and entry == NULL.
            if (entry != NULL) {
                // If we were able to get an entry, prepare it with the data.
                if (!init(localeId, entry)) {
                    delete entry;
                    entry = NULL;
                }
            }
            if (entry != NULL) {
                // Add to hashtable
                uhash_put(localeIdToEntries, entry->localeId, entry, &status);
                if (U_FAILURE(status)) {
                    delete entry;
                    entry = NULL;
                }
            }
            if (entry != NULL) {
                moveToMostRecent(entry);
            }
        }
        if (entry != NULL) {
            // Our data is cached
            if (U_FAILURE(entry->status)) {
                status = entry->status;
                return;
            }
            ptr = entry->cachedData;
            refPtr = &entry->refCount;

            // Increment reference counter while we have mutex to avoid
            // possible race where two different threads evict the same
            // cache entry.
            umtx_atomic_inc(refPtr);
            return;
        }
    }
    // End mutex

    // Our data is not cached, just create it from scratch.
    UObject *newData = safeCreate(localeId, status);
    if (U_FAILURE(status)) {
        return;
    }
    ptr = newData;
    refPtr = NULL;
}

LRUCache::LRUCache(int32_t size, UMutex *mtx, UErrorCode &status) :
        maxSize(size), mutex(mtx) {
    mostRecentlyUsedMarker = new CacheEntry2;
    leastRecentlyUsedMarker = new CacheEntry2;
    if (mostRecentlyUsedMarker == NULL || leastRecentlyUsedMarker == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    mostRecentlyUsedMarker->moreRecent = NULL;
    mostRecentlyUsedMarker->lessRecent = leastRecentlyUsedMarker;
    leastRecentlyUsedMarker->moreRecent = mostRecentlyUsedMarker;
    leastRecentlyUsedMarker->lessRecent = NULL;
    localeIdToEntries = uhash_openSize(
        uhash_hashChars,
        uhash_compareChars,
        NULL,
        maxSize + maxSize / 5,
        &status);
    if (U_FAILURE(status)) {
        return;
    }
}

LRUCache::~LRUCache() {
    uhash_close(localeIdToEntries);
    for (CacheEntry2 *i = mostRecentlyUsedMarker; i != NULL;) {
        CacheEntry2 *next = i->lessRecent;
        delete i;
        i = next;
    }
}

UObject *SimpleLRUCache::create(const char *localeId, UErrorCode &status) {
    return createFunc(localeId, status);
}

U_NAMESPACE_END
