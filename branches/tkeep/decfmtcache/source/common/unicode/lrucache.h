/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
*******************************************************************************
*                                                                             
* File LRUCACHE.H                                                             
*******************************************************************************
*/

#ifndef __LRU_CACHE_H__
#define __LRU_CACHE_H__

#include "unicode/uobject.h"
#include "umutex.h"
#include "sharedptr.h"

struct UHashtable;

U_NAMESPACE_BEGIN

/**
 * LRUCache keyed by locale ID.
 */

typedef UObject *(*CreateFunc)(const char *localeId, UErrorCode &status);

struct CacheEntry;

class LRUCache : public UMemory {
  public:
    static LRUCache *newCache(
        int32_t maxSize,
        UMutex *mutex,
        CreateFunc createFunc,
        UErrorCode &status);
    
    template<typename T>
    void get(const char *localeId, SharedPtr<T> &ptr, UErrorCode &status) {
        UObject *p;
        u_atomic_int32_t *rp;
        _get(localeId, p, rp, status);
        if (U_FAILURE(status)) {
            return;
        }
        if (rp == NULL) {
            ptr = SharedPtr<T>(p);
            return;
        }
        ptr = SharedPtr<T>(p, rp);
    }
    ~LRUCache();
  private:
    CacheEntry *mostRecentlyUsedMarker;
    CacheEntry *leastRecentlyUsedMarker;
    UHashtable *localeIdToEntries;
    int32_t maxSize;
    UMutex *mutex;
    CreateFunc createFunc;

    LRUCache(int32_t maxSize, UMutex *mutex,
        CreateFunc createFunc, UErrorCode &status);
    void moveToMostRecent(CacheEntry *cacheEntry);
    void _get(const char *localeId, UObject *&ptr, u_atomic_int32_t *&refPtr, UErrorCode &status);
};
    
U_NAMESPACE_END

#endif
