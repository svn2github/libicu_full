/*
******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
******************************************************************************
*                                                                             
* File UNIFIEDCACHE.CPP 
******************************************************************************
*/

#include "uhash.h"
#include "unifiedcache.h"
#include "umutex.h"
#include "mutex.h"
#include "uassert.h"
#include "ucln_cmn.h"

static icu::UnifiedCache *gCache = NULL;
static icu::SharedObject *gInCreation = NULL;
static UMutex gCacheMutex = U_MUTEX_INITIALIZER;
static icu::UInitOnce gCacheInitOnce = U_INITONCE_INITIALIZER;

U_CDECL_BEGIN
static UBool U_CALLCONV unifiedcache_cleanup() {
    gCacheInitOnce.reset();
    if (gCache) {
        delete gCache;
        gCache = NULL;
    }
    return TRUE;
}
U_CDECL_END


U_NAMESPACE_BEGIN

U_CAPI int32_t U_EXPORT2
ucache_hashKeys(const UHashTok key) {
    const CacheKeyBase *ckey = (const CacheKeyBase *) key.pointer;
    return ckey->hashCode();
}

U_CAPI UBool U_EXPORT2
ucache_compareKeys(const UHashTok key1, const UHashTok key2) {
    const CacheKeyBase *p1 = (const CacheKeyBase *) key1.pointer;
    const CacheKeyBase *p2 = (const CacheKeyBase *) key2.pointer;
    return *p1 == *p2;
}

U_CAPI void U_EXPORT2
ucache_deleteKey(void *obj) {
    CacheKeyBase *p = (CacheKeyBase *) obj;
    delete p;
}

static void waitOnCreation() {
}

static void notifyCreation() {
}

CacheKeyBase *CacheKeyBase::createKey() const {
  return NULL;
}

CacheKeyBase::~CacheKeyBase() {
}

static void U_CALLCONV cacheInit(UErrorCode &status) {
    U_ASSERT(gCache == NULL);
    ucln_common_registerCleanup(
            UCLN_COMMON_UNIFIED_CACHE, unifiedcache_cleanup);
    gCache = new UnifiedCache(status);
    if (U_FAILURE(status)) {
        delete gCache;
        gCache = NULL;
    }
    gInCreation = new SharedObject();
    // Ensure that this sentinel object never gets garbage collected.
    gInCreation->addRef();
}

const UnifiedCache *UnifiedCache::getInstance(UErrorCode &status) {
    umtx_initOnce(gCacheInitOnce, &cacheInit, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    U_ASSERT(gCache != NULL && gInCreation != NULL);
    return gCache;
}

UnifiedCache::UnifiedCache(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fHashtable = uhash_open(
            &ucache_hashKeys,
            &ucache_compareKeys,
            NULL,
            &status);
    if (U_FAILURE(status)) {
        return;
    }
    uhash_setKeyDeleter(fHashtable, &ucache_deleteKey);
}

UBool UnifiedCache::contains(const CacheKeyBase &key) const {
    Mutex lock(&gCacheMutex);
    return uhash_get(fHashtable, &key) != NULL;
}

void UnifiedCache::_flush(UBool all, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return;
    }
    {
        Mutex lock(&gCacheMutex);
        int32_t pos = -1;
        const UHashElement *element = uhash_nextElement(fHashtable, &pos);
        for (; element != NULL; element = uhash_nextElement(fHashtable, &pos)) {
            const SharedObject *sharedObject =
                    (const SharedObject *) element->value.pointer;
            if (all || sharedObject->allSoftReferences()) {
                sharedObject->removeSoftRef();
                uhash_removeElement(fHashtable, element);
            }
        }
    }
}

void UnifiedCache::flush(UErrorCode &status) const {
    _flush(FALSE, status);
}

UnifiedCache::~UnifiedCache() {
    UErrorCode status = U_ZERO_ERROR;
    _flush(TRUE, status);
    uhash_close(fHashtable);
}

void UnifiedCache::_addToCache(
        const CacheKeyBase &key,
        const SharedObject *adoptedValue,
        UBool broadcastCreation,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return;
    }
    adoptedValue->addSoftRef();
    CacheKeyBase *clonedKey = key.clone();
    if (clonedKey == NULL) {
        adoptedValue->removeSoftRef();
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    {
        Mutex lock(&gCacheMutex);
        uhash_put(fHashtable, clonedKey, (void *) adoptedValue, &status);
        if (broadcastCreation) {
            notifyCreation();
        }
    }
    if (U_FAILURE(status)) {
        adoptedValue->removeSoftRef();
        return;
    }
}

const SharedObject *UnifiedCache::_get(
        const CacheKeyBase &key, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    U_ASSERT(gInCreation != NULL);
    const SharedObject *result;
    {
        Mutex lock(&gCacheMutex);
        result = (const SharedObject *) uhash_get(
            fHashtable, &key);
        while (result == gInCreation) {
            waitOnCreation();
            result = (const SharedObject *) uhash_get(
                fHashtable, &key);
        }
        if (result != NULL) {
            // Prevent flush() from garbage collecting this result before
            // caller sees it.
            result->addRef();
            return result;
        }
    }
    _addToCache(key, gInCreation, FALSE, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    CacheKeyBase *nextKey = key.createKey();
    if (nextKey == NULL) {
        result = key.createObject();
        if (result == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        // Prevent flush() from garbage collecting this result before
        // caller sees it.
        result->addRef();
        _addToCache(key, result, TRUE, status);
        if (U_FAILURE(status)) {
            // We have to manually remove the reference we added apart from
            // _addToCache
            result->removeRef();
            return NULL;
        }
    } else {
        result = _get(*nextKey, status);
        delete nextKey;
        if (U_FAILURE(status)) {
            return NULL;
        }
        _addToCache(key, result, TRUE, status);
        if (U_FAILURE(status)) {
            return NULL;
        }
    }
    return result;
}

U_NAMESPACE_END
