/*
******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
******************************************************************************
*                                                                             
* File LRUCACHESTORAGE.H                                                             
******************************************************************************
*/

#ifndef __LRU_CACHE_STORAGE_H__
#define __LRU_CACHE_STORAGE_H__

#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "sharedobject.h"

struct UHashtable;


U_NAMESPACE_BEGIN

class UnifiedCache;
class Alias;

/**
 * A function that creates a new shared object for a particular cache view.
 * called on cache miss. 
 * @param id the key which is unique for the cache view;
 * @param cache points to the unified cache instance primarily used for
 * getting aliases.
 * @param data extra data for creating SharedObject and may be NULL; 
 * @param alias instead of creating a new SharedObject, this function may
 * specify an alias by returning NULL and storing the alias here. function
 * can get the alias using the provided cache pointer.
 * @param status any error returned here.
 * @return the newly created SharedObject or NULL to create an alias or
 * report an error.
 */
typedef SharedObject * (*SharedObjectCreater)(
        const UnicodeString &id,
        UnifiedCache *cache,
        void *data,
        const Alias **alias,
        UErrorCode &status);

/**
 * Specifies a particular cache view.
 */
struct CacheView {
    /**
     * Unique id for this cache view
     */
    const UChar *cacheViewId;

    /**
     *  Creates objects for this cache view.
     */
    SharedObjectCreater creater;

    /**
     * Arbitrary data for creating objects for this cache view. It is
     * passed as-is to creater.
     */
    void *viewData;

    /**
     * Creates a shared object for this cache view.
     * id is the key unique for this cache view; any error is stored in status.
     */
    inline SharedObject *create(
            const UnicodeString &id,
            UnifiedCache *cache,
            const Alias **alias,
            UErrorCode &status) const {
        return creater(id, cache, viewData, alias, status);
    }

    /**
     * Creates a full key that unique across all cache views.
     * id is the original key unique for this cache view.
     */
    UnicodeString &createKey(
            const UnicodeString &id, UnicodeString &result) const;
};

/**
 * UnifiedCache is the ICU unified cache.
 */
class U_COMMON_API UnifiedCache : public UMemory {
public:
    /**
     * Constructor.
     * @param status any error is set here.
     */
    UnifiedCache(UErrorCode &status);

    /**
     * get() Fetches a SharedObject by cache view and cache view ID.
     * On success, get() makes ptr point to the fetched SharedObject while
     * automatically updating reference counts; on failure, get() leaves ptr
     * unchanged and sets status. When get() is called, ptr must either be
     * NULL or be included in the reference count of what it points to.
     * After get() returns successfully, caller must eventually call
     * removeRef() on ptr to avoid memory leaks.
     *
     * T must be a subclass of SharedObject.
     */ 
    template<typename T>
    void get(
            const UnicodeString &id,
            const CacheView &view,
            const T *&ptr,
            UErrorCode &status) {
        const T *value = (const T *) _get(id, view, status);
        if (U_FAILURE(status)) {
            return;
        }
        SharedObject::copyPtr(value, ptr);
    }

    /**
     * getAlias returns an alias to a SharedObject by cache view and id. 
     * If the this results in a cache miss, getAlias creates the SharedObject
     * in the usual way before returning an alias to it. Returned alias is
     * owned by this cache, client must not attempt to free it.
     */
    const Alias *getAlias(
            const UnicodeString &id,
            const CacheView &view,
            UErrorCode &status);

    /**
     * Returns TRUE if a SharedObject for given ID is cached. Used
     * primarily for testing purposes.
     */
    UBool contains(const UnicodeString &id, const CacheSpec &spec) const;

    /**
     * Flush removes all objects from this cache that are not currently in
     * use.
     */
    void flush(UErrorCode &stats);

    virtual ~UnifiedCache();
protected:
private:
    class CacheEntry : public UMemory {
    public:
        CacheEntry *moreRecent;
        CacheEntry *lessRecent;
        UnicodeString fullId;
        const SharedObject *cachedData;
        int64_t size;
        UErrorCode status;  // This is the error if any from creating
                            // cachedData.
        CacheEntry();
        ~CacheEntry();

        void unlink();
        void init(const UnicodeString &id, const CacheSpec &spec);
    private:
        CacheEntry(const CacheEntry& other);
        CacheEntry &operator=(const CacheEntry& other);
    };
    LRUCacheStorage(const LRUCacheStorage &other);
    LRUCacheStorage &operator=(const LRUCacheStorage &other);

    // TODO (Travis Keep): Consider replacing both of these end nodes with a
    // single sentinel.
    CacheEntry *mostRecentlyUsedMarker;
    CacheEntry *leastRecentlyUsedMarker;
    UHashtable *fullIdToEntries;
    int64_t currentSize;
    int64_t maxSize;

    void moveToMostRecent(CacheEntry *cacheEntry);
    const SharedObject *_get(const UnicodeString &id, const CacheSpec &spec, UErrorCode &status);
};

U_NAMESPACE_END

#endif
