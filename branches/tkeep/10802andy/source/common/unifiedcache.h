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
#include "cstring.h"
#include "ustr_imp.h"

struct UHashtable;
struct UHashElement;

U_NAMESPACE_BEGIN

class UnifiedCache;

class U_COMMON_API CacheKeyBase: public UObject {
 public:
   CacheKeyBase() : status(U_ZERO_ERROR) {}
   virtual ~CacheKeyBase();
   virtual int32_t hashCode() const = 0;
   virtual CacheKeyBase *clone() const = 0;
   virtual UBool operator == (const CacheKeyBase &other) const = 0;
   virtual SharedObject *createObject(UErrorCode &status) const = 0;
   UBool operator != (const CacheKeyBase &other) const {
       return !(*this == other);
   }
 private:
   mutable UErrorCode status;
   friend class UnifiedCache;
};



template<typename T>
class U_COMMON_API CacheKey : public CacheKeyBase {
 public:
   virtual ~CacheKey() { }
   virtual int32_t hashCode() const {
       const char *s = typeid(T).name();
       return ustr_hashCharsN(s, uprv_strlen(s));
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
   virtual T *createObject(UErrorCode &status) const;
};

class U_COMMON_API UnifiedCache : public UObject {
 public:
   /**
    * @internal
    */
   UnifiedCache(UErrorCode &status);
   static const UnifiedCache *getInstance(UErrorCode &status);
   template<typename T>
   UBool get(const CacheKey<T>& key, const T *&ptr, UErrorCode &status) const {
       const T *value = (const T *) _get(key, status);
       if (U_FAILURE(status)) {
            return FALSE;
       }
       SharedObject::copyPtr(value, ptr);
       // We have to manually remove the reference that _get adds.
       value->removeRef();
       return TRUE;
   }

   template<typename T>
   UBool poll(const CacheKey<T>& key, const T *&ptr, UErrorCode &status) const {
       const T *value = (const T *) _poll(key, FALSE, status);
       if (U_FAILURE(status) || value == NULL) {
            return FALSE;
       }
       SharedObject::copyPtr(value, ptr);

       // We have to manually remove the reference that _poll adds.
       value->removeRef();
       return TRUE;
   }
  
   template<typename T>
   void putIfAbsent(const CacheKey<T>& key, const T *ptr) const {
       _putIfAbsent(key, ptr);
   }

   void putErrorIfAbsent(const CacheKeyBase& key, UErrorCode status) const;

   template<typename T>
   static UBool getByLocale(
           const Locale &loc, const T *&ptr, UErrorCode &status) {
       const UnifiedCache *cache = getInstance(status);
       if (U_FAILURE(status)) {
           return FALSE;
       }
       return cache->get(LocaleCacheKey<T>(loc), ptr, status);
   }
   int32_t keyCount() const;
   void flush() const;
   virtual ~UnifiedCache();
 private:
   UHashtable *fHashtable;
   UnifiedCache(const UnifiedCache &other);
   UnifiedCache &operator=(const UnifiedCache &other);
   static void _writeError(
           const UHashElement *element, UErrorCode status);
   static const SharedObject *_fetch(
           const UHashElement *element, UErrorCode &status);
   static UBool _inProgress(const UHashElement *element);
   UBool _put(CacheKeyBase *keyToBeAdopted, const SharedObject *value) const;
   void _flush(UBool all) const;
   void _putIfAbsent(const CacheKeyBase &key, const SharedObject *value) const;
   const SharedObject *_poll(
           const CacheKeyBase &key,
           UBool pollForGet,
           UErrorCode &status) const;
   const SharedObject *_get(const CacheKeyBase &key, UErrorCode &status) const;
};

U_NAMESPACE_END

#endif
