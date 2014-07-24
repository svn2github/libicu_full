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
#include "unicode/unistr.h"
#include "utypeinfo.h"

struct UHashtable;

U_NAMESPACE_BEGIN

class U_COMMON_API CacheKeyBase: public UObject {
 public:
   virtual ~CacheKeyBase();
   virtual int32_t hashCode() const = 0;
   virtual CacheKeyBase *clone() const = 0;
   virtual UBool operator == (const CacheKeyBase &other) const = 0;
   virtual SharedObject *createObject() const = 0;  // Cannot be void * return
   virtual CacheKeyBase *createKey() const;
   UBool operator != (const CacheKeyBase &other) const {
       return !(*this == other);
   }
};

template<typename T>
class U_COMMON_API CacheKey : public CacheKeyBase {
 public:
   virtual ~CacheKey() { }
   virtual int32_t hashCode() const {
       return UnicodeString(typeid(T).name()).hashCode();
   }
   virtual UBool operator == (const CacheKeyBase &other) const {
       return typeid(*this) == typeid(other);
   }
};

template<typename T>
class U_COMMON_API LocaleCacheKey : public CacheKey<T> {
 protected:
   Locale   fLoc;
 public:
   LocaleCacheKey(const Locale &loc) : fLoc(loc) {};
   virtual ~LocaleCacheKey() { }
   virtual int32_t hashCode() const {
       return 37 *CacheKey<T>::hashCode() + fLoc.hashCode();
   }
   virtual UBool operator == (const CacheKeyBase &other) const {
       // reflexive
       if (this == &other) {
           return TRUE;
       }
       if (!CacheKey<T>::operator == (other)) {
           return FALSE;
       }
       // We know this and other are of same class because operator== on
       // CacheKey returned true.
       const LocaleCacheKey<T> *fOther =
               static_cast<const LocaleCacheKey<T> *>(&other);
       return fLoc == fOther->fLoc;
   }
   virtual CacheKeyBase *clone() const {
       return new LocaleCacheKey<T>(*this);
   }
   virtual T *createObject() const;
   virtual CacheKeyBase *createKey() const {
       return NULL;
   }
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
   }
   UBool contains(const CacheKeyBase& key) const;
   void flush(UErrorCode &status);
   virtual ~UnifiedCache();
 private:
   UHashtable *fHashtable;
   void _flush(UBool all, UErrorCode &status);
   void _addToCache(
       const CacheKeyBase &key,
       const SharedObject *adoptedValue,
       UErrorCode &status);
   const SharedObject *_get(const CacheKeyBase &key, UErrorCode &status);
};

U_NAMESPACE_END

#endif
