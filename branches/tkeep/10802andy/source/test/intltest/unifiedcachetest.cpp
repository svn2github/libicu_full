/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File UNIFIEDCACHETEST.CPP
*
********************************************************************************
*/
#include "cstring.h"
#include "intltest.h"
#include "unifiedcache.h"

class UCTItem : public SharedObject {
  public:
    const char *value;
    UCTItem(const char *x) : value(x) { };
    virtual ~UCTItem() { }
};

template<>
UCTItem *LocaleCacheKey<UCTItem>::createObject() const {
    return new UCTItem(fLoc.getName());
}

template<>
CacheKeyBase *LocaleCacheKey<UCTItem>::createKey() const {
    if (uprv_strcmp(fLoc.getLanguage(), fLoc.getName()) == 0) {
        return NULL;
    }
    return new LocaleCacheKey<UCTItem>(fLoc.getLanguage());
}

class UnifiedCacheTest : public IntlTest {
public:
    UnifiedCacheTest() {
    }
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestBasic();
};

void UnifiedCacheTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestBasic);
  TESTCASE_AUTO_END;
}

void UnifiedCacheTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = UnifiedCache::getInstance(status);
    assertSuccess("", status);
    const UCTItem *enGb = NULL;
    const UCTItem *enUs = NULL;
    const UCTItem *fr = NULL;
    const UCTItem *frFr = NULL;
    LocaleCacheKey<UCTItem> foo("en_US");
    cache->get(LocaleCacheKey<UCTItem>("en_US"), enUs, status);
    cache->get(LocaleCacheKey<UCTItem>("en_GB"), enGb, status);
    cache->get(LocaleCacheKey<UCTItem>("fr_FR"), frFr, status);
    cache->get(LocaleCacheKey<UCTItem>("fr"), fr, status);
    if (enGb != enUs) {
        errln("Expected en_GB and en_US to resolve to same object.");
    } 
    if (fr != frFr) {
        errln("Expected fr and fr_FR to resolve to same object.");
    } 
    assertSuccess("", status);
    SharedObject::clearPtr(enGb);
    SharedObject::clearPtr(enUs);
    SharedObject::clearPtr(fr);
    SharedObject::clearPtr(frFr);
}

extern IntlTest *createUnifiedCacheTest() {
    return new UnifiedCacheTest();
}
