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

struct UHashtable;
struct UMutex;

U_NAMESPACE_BEGIN

/**
 * LRUCache keyed by locale ID.
 */

class LRUCache;

typedef int32_t u_atomic_int32_t;

typedef UObject *(*CloneFunc)(const UObject& other);
typedef UObject *(*CreateFunc)(const char *localeId, UErrorCode &status);

class Handle : public UMemory {
  public:
    // Default constructor to contain NULL pointer
    inline Handle() :
        readOnly(NULL), readWrite(NULL), refCount(NULL) {
    }

    void init(const UObject *ptr, u_atomic_int32_t *rc, CloneFunc cf);
    void init(UObject *ptr);

    // Destructor
    ~Handle();

    // Return read-only pointer
    const UObject *ptr() const;

    // Return pointer for mutating
    UObject *mutablePtr();
  private:
    const UObject *readOnly;
    UObject *readWrite;
    u_atomic_int32_t *refCount;
    CloneFunc cloneFunc;

    void release();

    // copy constructor
    Handle(const Handle& other);

    // assignment
    Handle& operator=(const Handle& other);
};

struct CacheEntry;

class LRUCache : public UMemory {
  public:
    static LRUCache *newCache(
        int32_t maxSize,
        UMutex *mutex,
        CreateFunc createFunc,
        CloneFunc cloneFunc,
        UErrorCode &status);
    
    // On cache miss, get holds the mutex while creating the data
    // from scratch.
    void get(const char *localeId, Handle &handle, UErrorCode &status);
    ~LRUCache();
  private:
    CacheEntry *mostRecentlyUsedMarker;
    CacheEntry *leastRecentlyUsedMarker;
    UHashtable *localeIdToEntries;
    int32_t maxSize;
    UMutex *mutex;
    CreateFunc createFunc;
    CloneFunc cloneFunc;

    LRUCache(int32_t maxSize, UMutex *mutex,
        CreateFunc createFunc, CloneFunc cloneFunc, UErrorCode &status);
    void moveToMostRecent(CacheEntry *cacheEntry);
};
    
U_NAMESPACE_END

#endif
