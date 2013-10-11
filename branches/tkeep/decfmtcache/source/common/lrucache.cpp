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

class CacheEntry : public UMemory {
public:
    CacheEntry *moreRecent;
    CacheEntry *lessRecent;
    char *localeId;
    UObject *cachedData;
    UErrorCode status;  // This is the error if any from creating cachedData.
    u_atomic_int32_t refCount;

    CacheEntry();
    ~CacheEntry();

    UBool canEvict();
    void unlink();
    void uninit();
    UBool init(const char *localeId, CreateFunc createFunc);
private:
    CacheEntry(const CacheEntry& other);
    CacheEntry &operator=(const CacheEntry& other);
};

CacheEntry::CacheEntry() 
    : moreRecent(NULL), lessRecent(NULL), localeId(NULL), cachedData(NULL),
      status(U_ZERO_ERROR) {

    // Count this cache entry as first reference to data.
    umtx_storeRelease(refCount, 1);
}

CacheEntry::~CacheEntry() {
    uninit();
}

UBool CacheEntry::canEvict() {
    // We can evict if there are no other references.
    return umtx_loadAcquire(refCount) == 1;
}

void CacheEntry::unlink() {
    if (moreRecent != NULL) {
        moreRecent->lessRecent = lessRecent;
    }
    if (lessRecent != NULL) {
        lessRecent->moreRecent = moreRecent;
    }
    moreRecent = NULL;
    lessRecent = NULL;
}

void CacheEntry::uninit() {
    delete cachedData;
    cachedData = NULL;
    status = U_ZERO_ERROR;
    if (localeId != NULL) {
        uprv_free(localeId);
    }
    localeId = NULL;
}

UBool CacheEntry::init(const char *locId, CreateFunc createFunc) {
    uninit();
    localeId = (char *) uprv_malloc(strlen(locId) + 1);
    if (localeId == NULL) {
        return FALSE;
    }
    uprv_strcpy(localeId, locId);
    cachedData = createFunc(locId, status);
}

LRUCache *LRUCache::newCache(
        int32_t maxSize,
        UMutex *mutex,
        CreateFunc createFunc,
        UErrorCode &status) {
    LRUCache *result = new LRUCache(maxSize, mutex, createFunc, status);
    if (U_FAILURE(status)) {
        delete result;
        return NULL;
    }
    return result;
} 

void LRUCache::moveToMostRecent(CacheEntry *entry) {
    entry->unlink();
    entry->moreRecent = mostRecentlyUsedMarker;
    entry->lessRecent = mostRecentlyUsedMarker->lessRecent;
    mostRecentlyUsedMarker->lessRecent->moreRecent = entry;
    mostRecentlyUsedMarker->lessRecent = entry;
}


void LRUCache::_get(const char *localeId, UObject *&ptr, u_atomic_int32_t *&refPtr, UErrorCode &status) {
    // Begin mutex
    {
        Mutex lock(mutex);
        CacheEntry *entry = (CacheEntry *) uhash_get(localeIdToEntries, localeId);
        if (entry != NULL) {
            moveToMostRecent(entry);
        } else {
            // Its a cache miss.

            if (uhash_count(localeIdToEntries) < maxSize) {
                entry = new CacheEntry;
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
                if (!entry->init(localeId, createFunc)) {
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
            return;
        }
    }
    // End mutex

    // Our data is not cached, just create it from scratch.
    UObject *newData = createFunc(localeId, status);
    if (U_FAILURE(status)) {
        return;
    }
    ptr = newData;
    refPtr = NULL;
}

LRUCache::LRUCache(int32_t size, UMutex *mtx, CreateFunc crf, UErrorCode &status) :
        maxSize(size), mutex(mtx), createFunc(crf) {
    mostRecentlyUsedMarker = new CacheEntry;
    leastRecentlyUsedMarker = new CacheEntry;
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
    for (CacheEntry *i = mostRecentlyUsedMarker; i != NULL;) {
        CacheEntry *next = i->lessRecent;
        delete i;
        i = next;
    }
}

U_NAMESPACE_END
