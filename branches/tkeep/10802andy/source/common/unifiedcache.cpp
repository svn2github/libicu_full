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
static icu::SharedObject *gNoValue = NULL;
static UMutex gCacheMutex = U_MUTEX_INITIALIZER;
static UConditionVar gInProgressValueAddedCond = U_CONDITION_INITIALIZER;
static icu::UInitOnce gCacheInitOnce = U_INITONCE_INITIALIZER;

U_CDECL_BEGIN
static UBool U_CALLCONV unifiedcache_cleanup() {
    gCacheInitOnce.reset();
    if (gCache) {
        delete gCache;
        gCache = NULL;
    }
    if (gNoValue) {
        delete gNoValue;
        gNoValue = NULL;
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

CacheKeyBase::~CacheKeyBase() {
}

static void U_CALLCONV cacheInit(UErrorCode &status) {
    U_ASSERT(gCache == NULL);
    ucln_common_registerCleanup(
            UCLN_COMMON_UNIFIED_CACHE, unifiedcache_cleanup);

    // gNoValue must be created first to avoid assertion error in
    // cache constructor.
    gNoValue = new SharedObject();
    gCache = new UnifiedCache(status);
    if (gCache == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        delete gCache;
        delete gNoValue;
        gCache = NULL;
        gNoValue = NULL;
        return;
    }
    // We add a softref because we want hash elements with gNoValue to be
    // elligible for purging but we don't ever want gNoValue to be deleted.
    gNoValue->addSoftRef();
}

const UnifiedCache *UnifiedCache::getInstance(UErrorCode &status) {
    umtx_initOnce(gCacheInitOnce, &cacheInit, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    U_ASSERT(gCache != NULL);
    return gCache;
}

UnifiedCache::UnifiedCache(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(gNoValue != NULL);
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

int32_t UnifiedCache::keyCount() const {
    Mutex lock(&gCacheMutex);
    return uhash_count(fHashtable);
}

void UnifiedCache::putErrorIfAbsent(
        const CacheKeyBase &key, UErrorCode status) const {
    U_ASSERT(U_FAILURE(status));
    Mutex lock(&gCacheMutex);
    const UHashElement *element = uhash_find(fHashtable, &key);
    if (element == NULL) {
        CacheKeyBase *clonedKey = key.clone();
        if (clonedKey == NULL) {
            return;
        }
        clonedKey->status = status;
        _put(clonedKey, gNoValue);
    }
    if (_inProgress(element)) {
        _writeError(element, status);

        // Tell waiting threads that we replace in-progress status with
        // an error.
        umtx_condBroadcast(&gInProgressValueAddedCond);
    }
}

void UnifiedCache::flush() const {
    _flush(FALSE);
}

UnifiedCache::~UnifiedCache() {
    _flush(TRUE);
    uhash_close(fHashtable);
}

void UnifiedCache::_flush(UBool all) const {
    Mutex lock(&gCacheMutex);
    int32_t pos = -1;
    const UHashElement *element = uhash_nextElement(fHashtable, &pos);
    for (; element != NULL; element = uhash_nextElement(fHashtable, &pos)) {
        const SharedObject *sharedObject =
                (const SharedObject *) element->value.pointer;
        if (all || sharedObject->allSoftReferences()) {
            uhash_removeElement(fHashtable, element);
            sharedObject->removeSoftRef();
        }
    }
}

UBool UnifiedCache::_put(
        CacheKeyBase *keyToAdopt, const SharedObject *value) const {
    UErrorCode status = U_ZERO_ERROR;
    uhash_put(fHashtable, keyToAdopt, (void *) value, &status);
    if (U_SUCCESS(status)) {
        value->addSoftRef();
        return TRUE;
    }
    return FALSE;
}

void UnifiedCache::_putIfAbsent(
        const CacheKeyBase &key, const SharedObject *value) const {
    U_ASSERT(value != NULL);
    Mutex lock(&gCacheMutex);
    const UHashElement *element = uhash_find(fHashtable, &key);
    if (element != NULL && !_inProgress(element)) {

        // Something has already been put here, just return
        return;
    }
    CacheKeyBase *clonedKey = key.clone();
    if (clonedKey == NULL) {
        return;
    }
    if (!_put(clonedKey, value) && element != NULL) {

        // If put fails to replace in-progress status, write an error
        // so that waiting threads will wake up.
        _writeError(element, U_MEMORY_ALLOCATION_ERROR);
    }
    if (element != NULL) {

        // Tell other threads that we replaced in-progress with either
        // a value or an error.
        umtx_condBroadcast(&gInProgressValueAddedCond);
    }
}

const SharedObject *UnifiedCache::_poll(
        const CacheKeyBase &key, UBool pollForGet, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    Mutex lock(&gCacheMutex);
    const UHashElement *element = uhash_find(fHashtable, &key);
    while (element != NULL && _inProgress(element)) {
        umtx_condWait(&gInProgressValueAddedCond, &gCacheMutex);
        element = uhash_find(fHashtable, &key);
    }
    if (element != NULL) {
        const SharedObject *result = _fetch(element, status);
        if (result != NULL) {
            result->addRef();
        }
        return result;
    }
    if (pollForGet) {
        CacheKeyBase *clonedKey = key.clone();
        if (clonedKey == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        if (!_put(clonedKey, gNoValue)) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
    }
    return NULL;
}

const SharedObject *UnifiedCache::_get(
        const CacheKeyBase &key, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const SharedObject *result = _poll(key, TRUE, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (result != NULL) {
        return result;
    }
    result = key.createObject(status);
    if (U_FAILURE(status)) {
        putErrorIfAbsent(key, status);
        status = U_ZERO_ERROR;
    } else if (result != NULL) {
        _putIfAbsent(key, result);
    }
    return _get(key, status);
}

void UnifiedCache::_writeError(const UHashElement *element, UErrorCode status) {
    const CacheKeyBase *theKey = (const CacheKeyBase *) element->key.pointer;
    theKey->status = status;
}

const SharedObject *UnifiedCache::_fetch(
        const UHashElement *element, UErrorCode &status) {
    const CacheKeyBase *theKey = (const CacheKeyBase *) element->key.pointer;
    status = theKey->status;
    if (U_FAILURE(status)) {
        return NULL;
    }
    return (const SharedObject *) element->value.pointer;
}
    
UBool UnifiedCache::_inProgress(const UHashElement *element) {
    UErrorCode status = U_ZERO_ERROR;
    const SharedObject *result = _fetch(element, status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return (result == gNoValue);
}

U_NAMESPACE_END
