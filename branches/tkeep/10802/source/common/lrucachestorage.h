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

/**
 * A function that creates a shared object. id is the key; data is extra
 * data for creating SharedObject, may be NULL; memory footprint of
 * created object is returned at size; any error is stored in status.
 */
typedef SharedObject * (*SharedObjectCreater)(
        const UnicodeString &id,
        void *data,
        int64_t &size,
        UErrorCode &status);

/**
 * Specifies a particular cache.
 */
struct CacheSpec {
    /**
     * Unique id for this cache
     */
    const UChar *cacheId;

    /**
     *  Creates objects for this cache.
     */
    SharedObjectCreater creater;

    /**
     * Arbitrary data for creating objects for this cache.
     */
    void *data;

    /**
     * Creates a shared object for this cache.
     * id is the key; memory footprint ofcreated object is returned at size;
     * any error is stored in status.
     */
    inline SharedObject *create(
            const UnicodeString &id, int64_t &size, UErrorCode &status) const {
        return creater(id, data, size, status);
    }

    /**
     * Creates a full key that includes cache type based on given key.
     * Caller owns returned key.
     * id is the original key; any error returned at status.
     */
    UnicodeString &createKey(
            const UnicodeString &id, UnicodeString &result) const;
};

/**
 * Storage for caches of SharedObjects keyed by an arbitrary ID.
 *
 * LRUCacheStorage has one main method, get(), which fetches a SharedObject
 * for a particular cache. If no such SharedObject exists in the particular
 * cache, get() creates and stores the new SharedObject for that cache.
 *
 * In addition to ID, get() accepts a CacheSpec which describes the particular
 * cache. CacheSpec consists of a cache ID, and a function pointer that
 * LRUCacheStorage uses to create the new object on cache miss.
 *
 * LRUCacheStorage has a maximum size in bytes. Whenever adding a new item to
 * the cache would exceed this maximum size, LRUCacheStorage evicts the
 * SharedObjects that were least recently fetched via the get() method.
 */
class U_COMMON_API LRUCacheStorage : public UMemory {
public:
    /**
     * Constructor.
     * @param maxSize the maximum size of the LRUCacheStorage in bytes
     * @param status any error is set here.
     */
    LRUCacheStorage(int64_t maxSize, UErrorCode &status);

    /**
     * Fetches a SharedObject by locale ID. On success, get() makes ptr point
     * to the fetched SharedObject while automatically updating reference
     * counts; on failure, get() leaves ptr unchanged and sets status.
     * When get() is called, ptr must either be NULL or be included in the
     * reference count of what it points to. After get() returns successfully,
     * caller must eventually call removeRef() on ptr to avoid memory leaks.
     *
     * T must be a subclass of SharedObject.
     */ 
    template<typename T>
    void get(
            const UnicodeString &id,
            const CacheSpec &spec,
            const T *&ptr,
            UErrorCode &status) {
        const T *value = (const T *) _get(id, spec, status);
        if (U_FAILURE(status)) {
            return;
        }
        SharedObject::copyPtr(value, ptr);
    }

    /**
     * Returns TRUE if a SharedObject for given ID is cached. Used
     * primarily for testing purposes.
     */
    UBool contains(const UnicodeString &id, const CacheSpec &spec) const;

    /**
     * Returns current footprint of cache in bytes. Used primarily for
     * testing purposes.
     */
    int64_t size() const { return currentSize; }

    /**
     * Returns the size in bytes of an empty cache entry. Used for testing
     * only.
     */
    static int64_t sizeOfEmptyEntry() {
        return sizeof(CacheEntry);
    }
    virtual ~LRUCacheStorage();
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
