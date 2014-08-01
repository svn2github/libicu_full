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
    char *value;
    UCTItem(const char *x) : value(NULL) { 
        value = uprv_strdup(x);
    }
    virtual ~UCTItem() {
        uprv_free(value);
    }
};

class UCTItem2 : public SharedObject {
};

template<>
UCTItem2 *LocaleCacheKey<UCTItem2>::createObject(UErrorCode &status) const {
    return NULL;
}

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
    if (uprv_strcmp(fLoc.getName(), "zh_CN") == 0) {
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
    void TestHashEquals();
};

void UnifiedCacheTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestBasic);
  TESTCASE_AUTO(TestError);
  TESTCASE_AUTO(TestHashEquals);
  TESTCASE_AUTO_END;
}

void UnifiedCacheTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = UnifiedCache::getInstance(status);
    assertSuccess("", status);
    cache->flush();
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
    if (enGb == fr) {
        errln("Expected en_GB and fr to return different objects.");
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
    cache->flush();
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

void UnifiedCacheTest::TestHashEquals() {
    LocaleCacheKey<UCTItem> key1("en_US");
    LocaleCacheKey<UCTItem> key2("en_US");
    LocaleCacheKey<UCTItem> diffKey1("en_UT");
    LocaleCacheKey<UCTItem2> diffKey2("en_US");
    assertTrue("", key1.hashCode() == key2.hashCode());
    assertTrue("", key1.hashCode() != diffKey1.hashCode());
    assertTrue("", key1.hashCode() != diffKey2.hashCode());
    assertTrue("", diffKey1.hashCode() != diffKey2.hashCode());
    assertTrue("", key1 == key2);
    assertTrue("", key1 != diffKey1);
    assertTrue("", key1 != diffKey2);
    assertTrue("", diffKey1 != diffKey2);
}

extern IntlTest *createUnifiedCacheTest() {
    return new UnifiedCacheTest();
}
