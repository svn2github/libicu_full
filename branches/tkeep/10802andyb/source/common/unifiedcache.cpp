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
static UMutex gCacheMutex = U_MUTEX_INITIALIZER;
static UConditionVar gCreationCompleteCond = U_CONDITION_INITIALIZER;
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
    umtx_condWait(&gCreationCompleteCond, &gCacheMutex);
}

static void notifyCreation() {
    umtx_condBroadcast(&gCreationCompleteCond);
}

static UBool inCreation(const SharedObject *obj) {
    return (obj != NULL && obj->getCreationError() == U_STANDARD_ERROR_LIMIT);
}

static UBool isReal(const SharedObject *obj) {
    return (obj != NULL && obj->getCreationError() == U_ZERO_ERROR);
}

static SharedObject *newInCreation() {
    return new SharedObject(U_STANDARD_ERROR_LIMIT);
}

CacheKeyBase *CacheKeyBase::createKey(UErrorCode & /* status */) const {
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

void UnifiedCache::flush() const {
    _flush(FALSE);
}

UnifiedCache::~UnifiedCache() {
    _flush(TRUE);
    uhash_close(fHashtable);
}

// _getCompleted atomically fetches a shared object from the cache and
// adds a reference to it that caller must eventually clear.
// _getCompleted is guaranteed not to return a placeholder
// object. If it finds an error placeholder object, it sets status to
// that error. If there is a cache miss for a particular key, the first
// call to _getCompleted will return NULL with no error while remaining
// calls with that key will block until the first caller attempts
// either to store an object or an error for that key.
const SharedObject *UnifiedCache::_getCompleted(
        const CacheKeyBase &key, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    Mutex lock(&gCacheMutex);
    const SharedObject *result = (const SharedObject *) uhash_get(
            fHashtable, &key);
    if (result == NULL) {
        SharedObject *obj = newInCreation();
        if (!_putOrClear(key, obj)) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        // Count this thread that will receive NULL and create the
        // object as one of the waiting threads.
        obj->addRef();
        return NULL;
    }
    // If its an in creation placeholder object, we add a reference to count
    // this thread as a waiting thread. If its a real shared object, we add
    // a reference as required by _getCompleted. If its an error placeholder,
    // we don't add a reference.
    if (inCreation(result) || isReal(result)) {
        result->addRef();
    }

    // Wait for the thread that got NULL back to create the object.
    while (inCreation(result)) {
        waitOnCreation();
        result = (const SharedObject *) uhash_get(fHashtable, &key);
        if (result == NULL) {
            // This can happen if the thread creating the object was
            // unable to replace the in creation placeholder with 
            // either a real object or an error placeholder.
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
    }
    if (result->getCreationError() != U_ZERO_ERROR) {
        status = result->getCreationError();
        // Error placeholders don't get extra references to remove.
        return NULL;
    }
    // If we had to wait on result, it got the reference we added to
    // the in creation placeholder object when the thread creating it
    // added it to the cache. 
    return result;
}

// _putOrClear is used by _replaceWithRealObject and _replaceWithError to
// modify the cache. It attempts to store adoptedObj under key. On success,
// it returns TRUE and adds one soft reference to adoptedObj. On failure,
// it returns FALSE and clears the value stored at key and frees adoptedObj
// if no references to it are held. Caution: _putOrClear does not make
// any attempt to free any previous object stored at key.
// adoptedObj may be NULL in which case _putOrClear just clears the
// value stored at key and returns FALSE.
UBool UnifiedCache::_putOrClear(
        const CacheKeyBase &key, const SharedObject *adoptedObj) const {
    if (adoptedObj == NULL) {
        uhash_remove(fHashtable, &key);
        return FALSE;
    }
    adoptedObj->addSoftRef();
    CacheKeyBase *clonedKey = key.clone();
    if (clonedKey == NULL) {
        adoptedObj->removeSoftRef();
        uhash_remove(fHashtable, &key);
        return FALSE;
    }
    UErrorCode status = U_ZERO_ERROR;
    uhash_put(fHashtable, clonedKey, (void *) adoptedObj, &status);
    if (U_FAILURE(status)) {
        adoptedObj->removeSoftRef();
        uhash_remove(fHashtable, &key);
        return FALSE;
    }
    return TRUE;
}
   
// _replaceWithRealObject atomically replaces an in creation placeholder with
// a shared object and notifies any waiting threads. It adds as many
// references to adoptedObj as there are threads waiting on it including the
// one making this call. Once other threads can see adoptedObj in the cache,
// it will have all these references added to it. It returns TRUE on success
// or FALSE on failue. On failure, it just clears the creation placeholder
// so that waiting threads will give up. On failure, it also frees adoptedObj
// if no references to it are held. key is the cache key; adoptedObj is the
// shared object.
UBool UnifiedCache::_replaceWithRealObject(
        const CacheKeyBase &key,
        const SharedObject *adoptedObj) const {
    Mutex lock(&gCacheMutex);
    const SharedObject *placeholder = (const SharedObject *) uhash_get(
            fHashtable, &key);
    U_ASSERT(inCreation(placeholder));
    if (!_putOrClear(key, adoptedObj)) {
        delete placeholder;
        notifyCreation();
        return FALSE;
    }

    // This is how many threads are waiting on the real object. We
    // subtract one because the cache itself holds a soft reference.
    int32_t threadsWaiting = placeholder->getRefCount() - 1;

    // Assign a reference for each waiting thread.
    for (int32_t i = 0; i < threadsWaiting; ++i) {
        adoptedObj->addRef();
    }
    delete placeholder;
    notifyCreation();
    return TRUE;
}

// _replaceWithError atomically replaces an in creation placeholder with
// an error placeholder and notifies any waiting threads. On failure, it
// just clears the creation placeholder so that waiting threads will give
// up. key is the cache key; status is the error.
void UnifiedCache::_replaceWithError(
        const CacheKeyBase &key,
        UErrorCode &status) const {
    Mutex lock(&gCacheMutex);
    const SharedObject *placeholder = (const SharedObject *) uhash_get(
            fHashtable, &key);
    U_ASSERT(inCreation(placeholder));
    _putOrClear(key, new SharedObject(status));
    delete placeholder;
    notifyCreation();
}

// _get works like _getCompleted except that it will always return a real
// shared object or set an error. When _getCompleted returns NULL, _get
// creates the missing object and adds it to the cache.
const SharedObject *UnifiedCache::_get(
        const CacheKeyBase &key, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const SharedObject *result = _getCompleted(key, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (result != NULL) {
        return result;
    }
    // We need to create the object.
    CacheKeyBase *nextKey = key.createKey(status);
    if (U_FAILURE(status)) {
        _replaceWithError(key, status);
        return NULL;
    }
    if (nextKey == NULL) {
        result = key.createObject(status);
        if (U_FAILURE(status)) {
            _replaceWithError(key, status);
            return NULL;
        }
        result->addRef();
    } else {
        result = _get(*nextKey, status);
        delete nextKey;
        if (U_FAILURE(status)) {
            _replaceWithError(key, status);
            return NULL;
        }
    }
    if (_replaceWithRealObject(key, result)) {
      // If we successfully stored result under key, we can remove the
      // extra reference from the _get call. 
      result->removeRef();
    }
    return result;
}

U_NAMESPACE_END
