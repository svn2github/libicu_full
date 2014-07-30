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
UCTItem *LocaleCacheKey<UCTItem>::createObject(UErrorCode &status) const {
    if (uprv_strcmp(fLoc.getName(), "zh") == 0) {
        status = U_MISSING_RESOURCE_ERROR;
        return NULL;
    }
    return new UCTItem(fLoc.getName());
}

template<>
CacheKeyBase *LocaleCacheKey<UCTItem>::createKey(UErrorCode &status) const {
    if (uprv_strcmp(fLoc.getName(), "zh") == 0) {
        status = U_MISSING_RESOURCE_ERROR;
        return NULL;
    }
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
    void TestError();
};

void UnifiedCacheTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestBasic);
  TESTCASE_AUTO(TestError);
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
    // en_US, en_GB, en share one object; fr_FR and fr share another.
    // 5 keys in all.
    assertEquals("", 5, cache->keyCount());
    SharedObject::clearPtr(enGb);
    cache->flush();
    assertEquals("", 5, cache->keyCount());
    SharedObject::clearPtr(enUs);
    cache->flush();
    // With en_GB and en_US cleared there are no more hard references to
    // the "en" object, so it gets flushed and the keys that refer to it
    // get removed from the cache.
    assertEquals("", 2, cache->keyCount());
    SharedObject::clearPtr(fr);
    SharedObject::clearPtr(frFr);
    cache->flush();
    assertEquals("", 0, cache->keyCount());
}

void UnifiedCacheTest::TestError() {
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = UnifiedCache::getInstance(status);
    assertSuccess("", status);
    const UCTItem *zhCn = NULL;
    const UCTItem *zhTw = NULL;
    const UCTItem *zhHk = NULL;
    cache->get(LocaleCacheKey<UCTItem>("zh_CN"), zhCn, status);
    if (status != U_MISSING_RESOURCE_ERROR) {
        errln("Expected U_MISSING_RESOURCE_ERROR");
    }
    status = U_ZERO_ERROR;
    cache->get(LocaleCacheKey<UCTItem>("zh_TW"), zhTw, status);
    if (status != U_MISSING_RESOURCE_ERROR) {
        errln("Expected U_MISSING_RESOURCE_ERROR");
    }
    status = U_ZERO_ERROR;
    cache->get(LocaleCacheKey<UCTItem>("zh_HK"), zhHk, status);
    if (status != U_MISSING_RESOURCE_ERROR) {
        errln("Expected U_MISSING_RESOURCE_ERROR");
    }
    // 4 keys in cache zhCN, zh, zhTW, zhHk all pointing to error placeholders
    assertEquals("", 4, cache->keyCount());
    cache->flush();
    // error placeholders have no hard references so they always get flushed. 
    assertEquals("", 0, cache->keyCount());
}

extern IntlTest *createUnifiedCacheTest() {
    return new UnifiedCacheTest();
}
