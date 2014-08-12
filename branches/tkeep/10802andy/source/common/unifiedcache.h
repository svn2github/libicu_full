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

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/uobject.h"
#include "unicode/locid.h"
#include "sharedobject.h"
#include "unicode/unistr.h"
#include "cstring.h"
#include "ustr_imp.h"

struct UHashtable;
struct UHashElement;

U_NAMESPACE_BEGIN

class UnifiedCache;

class U_COMMON_API CacheKeyBase : public UObject {
 public:
   CacheKeyBase() : creationStatus(U_ZERO_ERROR) {}
   CacheKeyBase(const CacheKeyBase &other) 
           : creationStatus(other.creationStatus) { }
   virtual ~CacheKeyBase();
   virtual int32_t hashCode() const = 0;
   virtual CacheKeyBase *clone() const = 0;
   virtual UBool operator == (const CacheKeyBase &other) const = 0;
   virtual const SharedObject *createObject(
           const void *creationContext, UErrorCode &status) const = 0;
   UBool operator != (const CacheKeyBase &other) const {
       return !(*this == other);
   }
 private:
   mutable UErrorCode creationStatus;
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
   virtual const T *createObject(
           const void *creationContext, UErrorCode &status) const;
};

class U_COMMON_API UnifiedCache : public UObject {
 public:
   /**
    * @internal
    */
   UnifiedCache(UErrorCode &status);

   static const UnifiedCache *getInstance(UErrorCode &status);

   template<typename T>
   void get(const CacheKey<T>& key, const T *&ptr, UErrorCode &status) const {
       get(key, NULL, ptr, status);
   }

   template<typename T>
   void get(
           const CacheKey<T>& key,
           const void *creationContext,
           const T *&ptr,
           UErrorCode &status) const {
       if (U_FAILURE(status)) {
           return;
       }
       UErrorCode creationStatus = U_ZERO_ERROR;
       const SharedObject *value = NULL;
       _get(key, value, creationContext, creationStatus);
       const T *tvalue = (const T *) value;
       if (U_SUCCESS(creationStatus)) {
           SharedObject::copyPtr(tvalue, ptr);
       }
       SharedObject::clearPtr(tvalue);
       // Take care not to overwrite a warning status passed in with
       // another warning or U_ZERO_ERROR.
       if (status == U_ZERO_ERROR || U_FAILURE(creationStatus)) {
           status = creationStatus;
       }
   }

   template<typename T>
   static void getByLocale(
           const Locale &loc, const T *&ptr, UErrorCode &status) {
       const UnifiedCache *cache = getInstance(status);
       if (U_FAILURE(status)) {
           return;
       }
       cache->get(LocaleCacheKey<T>(loc), ptr, status);
   }
   int32_t keyCount() const;
   void flush() const;
   virtual ~UnifiedCache();
 private:
   UHashtable *fHashtable;
   UnifiedCache(const UnifiedCache &other);
   UnifiedCache &operator=(const UnifiedCache &other);
   void _flush(UBool all) const;
   void _get(
           const CacheKeyBase &key,
           const SharedObject *&value,
           const void *creationContext,
           UErrorCode &status) const;
   UBool _poll(
           const CacheKeyBase &key,
           const SharedObject *&value,
           UErrorCode &status) const;
   void _putNew(
           const CacheKeyBase &key,
           const SharedObject *value,
           const UErrorCode creationStatus,
           UErrorCode &status) const;
   void _putIfAbsentAndGet(
           const CacheKeyBase &key,
           const SharedObject *&value,
           UErrorCode &status) const;
   static void _put(
           const UHashElement *element,
           const SharedObject *value,
           const UErrorCode status);
   static void _fetch(
           const UHashElement *element,
           const SharedObject *&value,
           UErrorCode &status);
   static UBool _inProgress(const UHashElement *element);
};

U_NAMESPACE_END

#endif
