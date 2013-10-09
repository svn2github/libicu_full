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
#include "umutex.h"
#include "uhash.h"
#include "cstring.h"

U_NAMESPACE_BEGIN

const UObject *Handle::ptr() const {
    return readWrite != NULL ? readWrite : readOnly;
}

UObject *Handle::mutablePtr() {
    if (readWrite == NULL && refCount != NULL &&
        readOnly != NULL && cloneFunc != NULL) {
        readWrite = cloneFunc(*readOnly);
        if (readWrite != NULL) {
            umtx_atomic_dec(refCount);
            refCount = NULL;
            readOnly = NULL;
        }
    }
    return readWrite;
}

Handle::~Handle() {
  release();
}

void Handle::init(const UObject *ptr, u_atomic_int32_t *rc, CloneFunc cf) {
    release();
    readOnly = ptr;
    readWrite = NULL;
    refCount = rc;
    cloneFunc = cf;
    if (refCount != NULL) {
        umtx_atomic_inc(refCount);
    }
}

void Handle::init(UObject *ptr) {
    if (ptr == readWrite) {
        return;
    }
    release();
    readWrite = ptr;
    refCount = NULL;
    readOnly = NULL;
}

void Handle::release() {
    if (readWrite != NULL) {
        delete readWrite;
    }
    if (refCount != NULL) {
        umtx_atomic_dec(refCount);
    }
}

struct CacheEntry : public UMemory {
  CacheEntry *moreRecent;
  CacheEntry *lessRecent;
  char *localeId;
  UObject *cachedData;
  UErrorCode status;  // This is the error if any from creating cachedData.
  int32_t refCount;

  CacheEntry();
  ~CacheEntry();

  UBool canEvict();
  void unlink();
  void uninit();
  UBool init(const char *localeId, CreateFunc createFunc);
};

CacheEntry::CacheEntry() 
    : moreRecent(NULL), lessRecent(NULL), localeId(NULL), cachedData(NULL),
      status(U_ZERO_ERROR), refCount(0) {
}

CacheEntry::~CacheEntry() {
  uninit();
}

UBool CacheEntry::canEvict() {
  return umtx_loadAcquire(*((u_atomic_int32_t *) &refCount)) == 0;
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
        CloneFunc cloneFunc,
        UErrorCode &status) {
    LRUCache *result = new LRUCache(maxSize, mutex, createFunc, cloneFunc, status);
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


void LRUCache::get(const char *localeId, Handle &handle, UErrorCode &status) {
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
            handle.init(entry->cachedData, (u_atomic_int32_t *) &entry->refCount, cloneFunc);
            return;
        }
    }
    // End mutex

    // Our data is not cached, just create it from scratch.
    UObject *newData = createFunc(localeId, status);
    if (U_FAILURE(status)) {
        return;
    }
    handle.init(newData);
}

LRUCache::LRUCache(int32_t size, UMutex *mtx, CreateFunc crf, CloneFunc clf, UErrorCode &status) :
        maxSize(size), mutex(mtx), createFunc(crf), cloneFunc(clf) {
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
