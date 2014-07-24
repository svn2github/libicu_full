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

CacheKeyBase *CacheKeyBase::createKey() const {
  return NULL;
}

CacheKeyBase::~CacheKeyBase() {
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
    return uhash_get(fHashtable, &key) != NULL;
}

void UnifiedCache::_flush(UBool all, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
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

void UnifiedCache::flush(UErrorCode &status) {
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
        UErrorCode &status) {
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
    uhash_put(fHashtable, clonedKey, (void *) adoptedValue, &status);
    if (U_FAILURE(status)) {
        adoptedValue->removeSoftRef();
        return;
    }
}

const SharedObject *UnifiedCache::_get(
        const CacheKeyBase &key, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    const SharedObject *result = (const SharedObject *) uhash_get(
            fHashtable, &key);
    if (result != NULL) {
        return result;
    }
    CacheKeyBase *nextKey = key.createKey();
    if (nextKey == NULL) {
        result = key.createObject();
        if (result == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        _addToCache(key, result, status);
        if (U_FAILURE(status)) {
            return NULL;
        }
    } else {
        result = _get(*nextKey, status);
        delete nextKey;
        if (U_FAILURE(status)) {
            return NULL;
        }
        _addToCache(key, result, status);
        if (U_FAILURE(status)) {
            return NULL;
        }
    }
    return result;
}

U_NAMESPACE_END
