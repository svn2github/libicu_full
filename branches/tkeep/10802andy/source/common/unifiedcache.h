/*
******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*
* File UNIFIEDCACHE.H
******************************************************************************
*/

#ifndef __UNIFIED_CACHE_H__
#define __UNIFIED_CACHE_H__

#include "unicode/uobject.h"
#include "unicode/locid.h"
#include "sharedobject.h"

U_NAMESPACE_BEGIN

class U_COMMON_API CacheKeyBase: public UObject {
 public:
   virtual ~CacheKeyBase();
   virtual int32_t hashCode() const = 0;
   virtual CacheKeyBase *clone() const = 0;
   virtual UBool operator == (const CacheKeyBase &other) const = 0;
   virtual SharedObject *createObject() const = 0;  // Cannot be void * return
   virtual CacheKeyBase *createKey();
};

template<typename T>
class U_COMMON_API CacheKey : public CacheKeyBase {
 public:
   virtual ~CacheKey();
   virtual int32_t hashCode() const;
   virtual UBool operator == (const CacheKeyBase &other) const;
};
   
template<typename T>
class U_COMMON_API LocaleCacheKey : public CacheKey<T> {
 protected:
   Locale   fLoc;
 public:
   LocaleCacheKey(const Locale &loc) : fLoc(loc) {};
   virtual ~LocaleCacheKey();
   virtual int32_t hashCode() const;
   virtual UBool operator == (const CacheKeyBase &other) const;
   virtual CacheKeyBase *clone() const;
   virtual T *createObject() const;
};


class U_COMMON_API UnifiedCache : public UObject {
 public:
   UnifiedCache(UErrorCode &status);
   template<typename T>
   void get(const CacheKey<T>& key, const T *&ptr, UErrorCode &status) {
       const T *value = (const T *) _get(key, status);
       if (U_FAILURE(status)) {
            return;
        }
        SharedObject::copyPtr(value, ptr);
        SharedObject::clearPtr(value);
   }
   UBool contains(const CacheKeyBase& key) const;
   void flush(UErrorCode &status);
   virtual ~UnifiedCache();
 private:
   const SharedObject *_get(const CacheKeyBase &key, UErrorCode &status);
};

U_NAMESPACE_END

#endif
