/*
******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
******************************************************************************
*                                                                             
* File UNIFIEDCACHE.CPP 
******************************************************************************
*/

#include "unifiedcache.h"
#include "utypeinfo.h"

U_NAMESPACE_BEGIN

CacheKeyBase *CacheKeyBase::createKey() {
  return NULL;
}

CacheKeyBase::~CacheKeyBase() {
}

template<typename T>
int32_t CacheKey<T>::hashCode() const {
    return UnicodeString(typeid(T).name()).hashCode(); 
}

template<typename T>
UBool CacheKey<T>::operator == (const CacheKeyBase &other) const {
    return typeid(*this) == typeid(other);
}

template<typename T>
CacheKey<T>::~CacheKey<T>() {
}

template<typename T>
int32_t LocaleCacheKey<T>::hashCode() const {
    return 37 *CacheKey<T>::hashCode() + fLoc.hashCode();
}

template<typename T>
UBool LocaleCacheKey<T>::operator == (const CacheKeyBase &other) const {
    // reflexive
    if (this == &other) {
        return TRUE;
    }
    if (!CacheKey<T>::operator == (other)) {
        return FALSE;
    }
    // We know this and other are of same class because operator== on
    // CacheKey returned true.
    LocaleCacheKey<T> *fOther = static_cast<LocaleCacheKey<T> *>(&other);
    return fLoc == fOther->fLoc;
}

template<typename T>
CacheKeyBase *LocaleCacheKey<T>::clone() const {
    return new LocaleCacheKey<T>(*this);
}

template<typename T>
LocaleCacheKey<T>::~LocaleCacheKey<T>() {
}

UnifiedCache::UnifiedCache(UErrorCode &status) {
}

UBool UnifiedCache::contains(const CacheKeyBase &key) const {
    return FALSE;
}

void UnifiedCache::flush(UErrorCode &status) {
}

UnifiedCache::~UnifiedCache() {
}

const SharedObject *UnifiedCache::_get(
        const CacheKeyBase &key, UErrorCode &status) {
    return NULL;
}

U_NAMESPACE_END
