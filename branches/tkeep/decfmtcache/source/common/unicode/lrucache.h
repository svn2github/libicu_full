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

class CacheEntry2;

class LRUCache : public UObject {
  public:
    template<typename T>
    void get(const char *localeId, SharedPtr<T> &ptr, UErrorCode &status) {
        UObject *p;
        u_atomic_int32_t *rp;
        _get(localeId, p, rp, status);
        if (U_FAILURE(status)) {
            return;
        }
        if (rp == NULL) {
            // Cache does not hold reference to returned data.
            ptr = SharedPtr<T>((T *) p);
            return;
        }
        ptr = SharedPtr<T>((T *) p, rp);

        // _get incremented our reference counter already to avoid races.
        // Now that we have our own shared pointer, we can decrement it
        // back.
        umtx_atomic_dec(rp);
    }
    UBool contains(const char *localeId) const;
    virtual ~LRUCache();
  protected:
    virtual UObject *create(const char *localeId, UErrorCode &status)=0;
    LRUCache(int32_t maxSize, UMutex *mutex, UErrorCode &status);
  private:
    LRUCache();
    LRUCache(const LRUCache &other);
    LRUCache &operator=(const LRUCache &other);
    UObject *safeCreate(const char *localeId, UErrorCode &status);
    CacheEntry2 *mostRecentlyUsedMarker;
    CacheEntry2 *leastRecentlyUsedMarker;
    UHashtable *localeIdToEntries;
    int32_t maxSize;
    UMutex *mutex;

    void moveToMostRecent(CacheEntry2 *cacheEntry);
    UBool init(const char *localeId, CacheEntry2 *cacheEntry);
    void _get(const char *localeId, UObject *&ptr, u_atomic_int32_t *&refPtr, UErrorCode &status);
};
    
U_NAMESPACE_END

#endif
