/*
******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
******************************************************************************
*                                                                             
* File LRUCACHE.CPP                                                             
******************************************************************************
*/

#include "lrucachestorage.h"
#include "uhash.h"
#include "cstring.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

UnicodeString &CacheSpec::createKey(
        const UnicodeString &id, UnicodeString &result) const {
    result.remove();
    result.append(cacheId, -1);
    result.append((UChar) 0x0);
    return result.append(id);
}

// TODO (Travis Keep): Consider building synchronization into this cache
// instead of leaving synchronization up to the clients.

LRUCacheStorage::CacheEntry::CacheEntry() 
    : moreRecent(NULL), lessRecent(NULL), fullId(), cachedData(NULL), size(0),
      status(U_ZERO_ERROR) {
}

LRUCacheStorage::CacheEntry::~CacheEntry() {
    SharedObject::clearPtr(cachedData);
}

void LRUCacheStorage::CacheEntry::unlink() {
    if (moreRecent != NULL) {
        moreRecent->lessRecent = lessRecent;
    }
    if (lessRecent != NULL) {
        lessRecent->moreRecent = moreRecent;
    }
    moreRecent = NULL;
    lessRecent = NULL;
}


void LRUCacheStorage::CacheEntry::init(
        const UnicodeString &id, const CacheSpec &spec) {
    spec.createKey(id, fullId);
    status = U_ZERO_ERROR;
    SharedObject::copyPtr(spec.create(id, size, status), cachedData);
    int64_t cacheEntryFootprint =
            sizeof(CacheEntry) + fullId.length() * sizeof(UChar);
    if (U_SUCCESS(status)) {
        size += cacheEntryFootprint;
    } else {
        size = cacheEntryFootprint;
    }
}

void LRUCacheStorage::moveToMostRecent(CacheEntry *entry) {
    if (entry->moreRecent == mostRecentlyUsedMarker) {
        return;
    }
    entry->unlink();
    entry->moreRecent = mostRecentlyUsedMarker;
    entry->lessRecent = mostRecentlyUsedMarker->lessRecent;
    mostRecentlyUsedMarker->lessRecent->moreRecent = entry;
    mostRecentlyUsedMarker->lessRecent = entry;
}

UBool LRUCacheStorage::contains(const UnicodeString &id, const CacheSpec &spec) const {
    UnicodeString fullId;
    spec.createKey(id, fullId);
    return (uhash_get(fullIdToEntries, &fullId) != NULL);
}


const SharedObject *LRUCacheStorage::_get(
        const UnicodeString &id, const CacheSpec &spec, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    UnicodeString fullId;
    spec.createKey(id, fullId);

    CacheEntry *entry = static_cast<CacheEntry *>(uhash_get(
            fullIdToEntries, &fullId));
    if (entry == NULL) {
        // Its a cache miss.

        entry = new CacheEntry;
        if (entry == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        entry->init(id, spec);

        // If entry we created is itself larger than memory limit then we
        // fail immediately.
        if (entry->size > maxSize) {
            delete entry;
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }

        // Add this new entry to the cache making it most recent
        uhash_put(fullIdToEntries, &entry->fullId, entry, &status);
        if (U_FAILURE(status)) {
            delete entry;
            return NULL;
        }
        moveToMostRecent(entry);
        currentSize += entry->size;

        // Evict as needed to stay within memory limits
        CacheEntry *idx = leastRecentlyUsedMarker->moreRecent;
        while (currentSize > maxSize) {
            CacheEntry *nextIdx = idx->moreRecent;
            currentSize -= idx->size;
            idx->unlink();
            uhash_remove(fullIdToEntries, &idx->fullId);
            delete idx;
            idx = nextIdx;
        }
    } else {
        // Re-link entry so that it is the most recent.
        moveToMostRecent(entry);
    }
    if (U_FAILURE(entry->status)) {
        status = entry->status;
        return NULL;
    }
    return entry->cachedData;
}

LRUCacheStorage::LRUCacheStorage(int64_t size, UErrorCode &status) :
        mostRecentlyUsedMarker(NULL),
        leastRecentlyUsedMarker(NULL),
        fullIdToEntries(NULL),
        currentSize(0),
        maxSize(size) {
    if (U_FAILURE(status)) {
        return;
    }
    mostRecentlyUsedMarker = new CacheEntry;
    leastRecentlyUsedMarker = new CacheEntry;
    if (mostRecentlyUsedMarker == NULL || leastRecentlyUsedMarker == NULL) {
        delete mostRecentlyUsedMarker;
        delete leastRecentlyUsedMarker;
        mostRecentlyUsedMarker = leastRecentlyUsedMarker = NULL;
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    mostRecentlyUsedMarker->moreRecent = NULL;
    mostRecentlyUsedMarker->lessRecent = leastRecentlyUsedMarker;
    leastRecentlyUsedMarker->moreRecent = mostRecentlyUsedMarker;
    leastRecentlyUsedMarker->lessRecent = NULL;
    fullIdToEntries = uhash_open(
        uhash_hashUnicodeString,
        uhash_compareUnicodeString,
        NULL,
        &status);
    if (U_FAILURE(status)) {
        return;
    }
}

LRUCacheStorage::~LRUCacheStorage() {
    uhash_close(fullIdToEntries);
    for (CacheEntry *i = mostRecentlyUsedMarker; i != NULL;) {
        CacheEntry *next = i->lessRecent;
        delete i;
        i = next;
    }
}

U_NAMESPACE_END
