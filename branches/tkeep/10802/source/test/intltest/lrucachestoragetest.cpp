/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File LRUCACHESTORAGETEST.CPP
*
********************************************************************************
*/
#include "cstring.h"
#include "intltest.h"
#include "lrucachestorage.h"
#include "sharedptr.h"

const UChar kCacheId[] = {0x46, 0x4F, 0x4F, 0x0};
const UChar kError[] = {0x65, 0x72, 0x72, 0x6F, 0x72, 0x0};

static int64_t parseId(const UnicodeString &id, UnicodeString &localeId) {
    int32_t idx = id.indexOf(0x5F);
    localeId.setTo(id, 0, idx);
    int64_t result = 0;
    int32_t len = id.length();
    for (int32_t i = idx + 1; i < len; ++i) {
        result *= 10;
        result += (id[i] - 0x30);
    }
    return result;
}

class CopyOnWriteForTesting : public SharedObject {
public:
    CopyOnWriteForTesting() : SharedObject(), localeNamePtr(), formatStrPtr(), length(0) {
    }

    CopyOnWriteForTesting(const CopyOnWriteForTesting &other) :
        SharedObject(other),
        localeNamePtr(other.localeNamePtr),
        formatStrPtr(other.formatStrPtr),
        length(other.length) {
    }

    virtual ~CopyOnWriteForTesting() {
    }

    SharedPtr<UnicodeString> localeNamePtr;
    SharedPtr<UnicodeString> formatStrPtr;
    int32_t length;
private:
    CopyOnWriteForTesting &operator=(const CopyOnWriteForTesting &rhs);
};

// A cache specification for testing. Keys are of the form "xyz_500" where
// "xyz" is the locale and 500 is the size of the data including the
// cache entry and the key.
class CacheSpecForTesting : public UMemory {
public:
    CacheSpecForTesting(const UnicodeString &dfs, int64_t emptySize);
    const CacheSpec &getSpec() const { return spec; }
protected:
private:
    SharedPtr<UnicodeString> defaultFormatStr;
    int64_t emptyEntrySize;
    CacheSpec spec;
    SharedObject *create(const UnicodeString &id, int64_t &size, UErrorCode &status);
    static SharedObject *createCallback(
            const UnicodeString &id, void *data, int64_t &size, UErrorCode &status) {
        return ((CacheSpecForTesting *) data)->create(id, size, status);
    }
};

CacheSpecForTesting::CacheSpecForTesting(
        const UnicodeString &dfs, int64_t emptySize)
        : defaultFormatStr(), emptyEntrySize(emptySize) {
    defaultFormatStr.reset(new UnicodeString(dfs));
    spec.cacheId = kCacheId;
    spec.creater = &CacheSpecForTesting::createCallback;
    spec.data = this;
}

SharedObject *CacheSpecForTesting::create(
        const UnicodeString &id, int64_t &size, UErrorCode &status) {
     if (id == UnicodeString(kError)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    UnicodeString localeId;
    size = parseId(id, localeId);
    size -= emptyEntrySize;
    UnicodeString temp;
    size -= spec.createKey(id, temp).length() * sizeof(UChar);
    U_ASSERT(size > 0);
    CopyOnWriteForTesting *result = new CopyOnWriteForTesting;
    result->localeNamePtr.reset(new UnicodeString(localeId));
    result->formatStrPtr = defaultFormatStr;
    result->length = 5;
    return result;
}

class LRUCacheStorageTest : public IntlTest {
public:
    LRUCacheStorageTest() {
    }
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestCreateKey();
    void TestSharedPointer();
    void TestErrorCallingConstructor();
    void TestLRUCacheStorage();
    void TestLRUCacheStorageVariable();
    void TestLRUCacheStorageError();
    void verifySharedPointer(
            const CopyOnWriteForTesting* ptr,
            const UnicodeString& name,
            const UnicodeString& format);
    void verifyString(
            const UnicodeString &expected, const UnicodeString &actual);
    void verifyReferences(
            const CopyOnWriteForTesting* ptr,
            int32_t count, int32_t nameCount, int32_t formatCount);
};

void LRUCacheStorageTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestCreateKey);
  TESTCASE_AUTO(TestSharedPointer);
  TESTCASE_AUTO(TestErrorCallingConstructor);
  TESTCASE_AUTO(TestLRUCacheStorage);
  TESTCASE_AUTO(TestLRUCacheStorageVariable);
  TESTCASE_AUTO(TestLRUCacheStorageError);
  TESTCASE_AUTO_END;
}

void LRUCacheStorageTest::TestCreateKey() {
    CacheSpecForTesting spec("little", LRUCacheStorage::sizeOfEmptyEntry());
    UnicodeString result;
    spec.getSpec().createKey("aKey", result);
    UChar expected[] = {0x46, 0x4F, 0x4F, 0x0, 0x61, 0x4B, 0x65, 0x79, 0x0};
    assertEquals("", UnicodeString(expected, 8), result);
}

void LRUCacheStorageTest::TestSharedPointer() {
    UErrorCode status = U_ZERO_ERROR;
    LRUCacheStorage cache(1500, status);
    CacheSpecForTesting spec("little", LRUCacheStorage::sizeOfEmptyEntry());
    const CopyOnWriteForTesting* ptr = NULL;
    cache.get("boo_500", spec.getSpec(), ptr, status);
    verifySharedPointer(ptr, "boo", "little");
    const CopyOnWriteForTesting* ptrCopy = ptr;
    ptrCopy->addRef();
    {
        const CopyOnWriteForTesting* ptrCopy2(ptrCopy);
        ptrCopy2->addRef();
        verifyReferences(ptr, 4, 1, 2);
        ptrCopy2->removeRef();
    }
    
    verifyReferences(ptr, 3, 1, 2);
    CopyOnWriteForTesting *wPtrCopy = SharedObject::copyOnWrite(ptrCopy);
    *wPtrCopy->localeNamePtr.readWrite() = UnicodeString("hi there");
    *wPtrCopy->formatStrPtr.readWrite() = UnicodeString("see you");
    verifyReferences(ptr, 2, 1, 2);
    verifyReferences(ptrCopy, 1, 1, 1);
    verifySharedPointer(ptr, "boo", "little");
    verifySharedPointer(ptrCopy, "hi there", "see you");
    ptrCopy->removeRef();
    ptr->removeRef();
}

void LRUCacheStorageTest::TestErrorCallingConstructor() {
    UErrorCode status = U_MEMORY_ALLOCATION_ERROR;
    LRUCacheStorage cache(1500,  status);
} 

void LRUCacheStorageTest::TestLRUCacheStorage() {
    UErrorCode status = U_ZERO_ERROR;
    LRUCacheStorage cache(1500, status);
    CacheSpecForTesting specForTesting("little", LRUCacheStorage::sizeOfEmptyEntry());
    const CacheSpec &spec = specForTesting.getSpec();
    const CopyOnWriteForTesting* ptr1 = NULL;
    const CopyOnWriteForTesting* ptr2 = NULL;
    const CopyOnWriteForTesting* ptr3 = NULL;
    const CopyOnWriteForTesting* ptr4 = NULL;
    const CopyOnWriteForTesting* ptr5 = NULL;
    cache.get("foo_500", spec, ptr1, status);
    cache.get("bar_500", spec, ptr2, status);
    assertEquals("cache size with 2", (int64_t) 1000, cache.size());
    cache.get("baz_500", spec, ptr3, status);
    assertEquals("cache size", (int64_t) 1500, cache.size());
    verifySharedPointer(ptr1, "foo", "little");
    verifySharedPointer(ptr2, "bar", "little");
    verifySharedPointer(ptr3, "baz", "little");

    // Cache holds a reference to returned data which explains the 2s
    // Note the '4'. each cached data has a reference to "little" and the
    // cache itself also has a reference to "little"
    verifyReferences(ptr1, 2, 1, 4);
    verifyReferences(ptr2, 2, 1, 4);
    verifyReferences(ptr3, 2, 1, 4);
    
    // (Most recent) "baz", "bar", "foo" (Least Recent) 
    // Cache is now full but thanks to shared pointers we can still evict.
    cache.get("full_500", spec, ptr4, status);
    verifySharedPointer(ptr4, "full", "little");

    verifyReferences(ptr4, 2, 1, 5);

    // (Most Recent) "full" "baz", "bar" (Least Recent)
    cache.get("baz_500", spec, ptr5, status);
    verifySharedPointer(ptr5, "baz", "little");
    // ptr5, ptr3, and cache have baz data
    verifyReferences(ptr5, 3, 1, 5);

    // This should delete foo data since it got evicted from cache.
    ptr1->removeRef();
    ptr1 = NULL;
    // Reference count for little drops to 4 because foo data was deleted.
    verifyReferences(ptr5, 3, 1, 4);

    // (Most Recent) "baz" "full" "bar" (Least Recent)
    cache.get("baz_500", spec, ptr5, status);
    verifySharedPointer(ptr5, "baz", "little");
    verifyReferences(ptr5, 3, 1, 4);

    // (Most Recent) "baz", "full", "bar" (Least Recent)
    // ptr3, ptr5 -> "baz" ptr4 -> "full" ptr2 -> "bar"
    if (!cache.contains("baz_500", spec)
        || !cache.contains("full_500", spec)
        || !cache.contains("bar_500", spec)
        || cache.contains("foo_500", spec)) {
        errln("Unexpected keys in cache.");
    }
    cache.get("new1_500", spec, ptr5, status);
    verifySharedPointer(ptr5, "new1", "little");
    verifyReferences(ptr5, 2, 1, 5);

    // Since bar was evicted, clearing its pointer should delete its data.
    // Notice that the reference count to 'little' dropped from 5 to 4.
    ptr2->removeRef();
    ptr2 = NULL;
    verifyReferences(ptr5, 2, 1, 4);
    if (cache.contains("bar_500", spec) || !cache.contains("full_500", spec)) {
        errln("Unexpected 'bar' in cache.");
    }

    // (Most Recent) "new1", "baz", "full" (Least Recent)
    // ptr3 -> "baz" ptr4 -> "full" ptr5 -> "new1"
    cache.get("new2_500", spec, ptr5, status);
    verifySharedPointer(ptr5, "new2", "little");
    verifyReferences(ptr5, 2, 1, 5);

    // since "full" was evicted, clearing its pointer should delete its data.
    ptr4->removeRef();
    ptr4 = NULL;
    verifyReferences(ptr5, 2, 1, 4);
    if (cache.contains("full_500", spec) || !cache.contains("baz_500", spec)) {
        errln("Unexpected 'full' in cache.");
    }

    // (Most Recent) "new2", "new1", "baz" (Least Recent)
    // ptr3 -> "baz" ptr5 -> "new2"
    cache.get("new3_500", spec, ptr5, status);
    verifySharedPointer(ptr5, "new3", "little");
    verifyReferences(ptr5, 2, 1, 5);

    // since "baz" was evicted, clearing its pointer should delete its data.
    ptr3->removeRef();
    ptr3 = NULL;
    verifyReferences(ptr5, 2, 1, 4);
    if (cache.contains("baz_500", spec) || !cache.contains("new3_500", spec)) {
        errln("Unexpected 'baz' in cache.");
    }
    assertEquals("cache size", (int64_t) 1500, cache.size());
    SharedObject::clearPtr(ptr1);
    SharedObject::clearPtr(ptr2);
    SharedObject::clearPtr(ptr3);
    SharedObject::clearPtr(ptr4);
    SharedObject::clearPtr(ptr5);
}

void LRUCacheStorageTest::TestLRUCacheStorageVariable() {
    UErrorCode status = U_ZERO_ERROR;
    LRUCacheStorage cache(1500, status);
    CacheSpecForTesting specForTesting("little", LRUCacheStorage::sizeOfEmptyEntry());
    const CacheSpec &spec = specForTesting.getSpec();
    const CopyOnWriteForTesting* ptr1 = NULL;
    cache.get("foo_200", spec, ptr1, status);
    cache.get("bar_300", spec, ptr1, status);
    cache.get("baz_500", spec, ptr1, status);
    assertEquals("cache size", (int64_t) 1000, cache.size());
    cache.get("full_701", spec, ptr1, status);
    assertEquals("cache size 2", (int64_t) 1201, cache.size());

    if (!cache.contains("full_701", spec)
        || !cache.contains("baz_500", spec)
        || cache.contains("bar_300", spec)
        || cache.contains("foo_200", spec)) {
        errln("Unexpected keys in cache.");
    }
    cache.get("new_1500", spec, ptr1, status);
    assertEquals("cache size 3", (int64_t) 1500, cache.size());

    if (!cache.contains("new_1500", spec)
        || cache.contains("full_701", spec)
        || cache.contains("baz_500", spec)) {
        errln("Unexpected keys in cache 2.");
    }
    
    assertSuccess("", status);
    cache.get("new_1501", spec, ptr1, status);
    if (status != U_MEMORY_ALLOCATION_ERROR) {
        errln("Expected memory allocation error.");
    }
    assertEquals("cache size 4", (int64_t) 1500, cache.size());

    if (!cache.contains("new_1500", spec)
        || cache.contains("new_1501", spec)) {
        errln("Unexpected keys in cache 3.");
    }
    SharedObject::clearPtr(ptr1);
}
void LRUCacheStorageTest::TestLRUCacheStorageError() {
    UErrorCode status = U_ZERO_ERROR;
    LRUCacheStorage cache(1500, status);
    CacheSpecForTesting specForTesting("little", LRUCacheStorage::sizeOfEmptyEntry());
    const CopyOnWriteForTesting *ptr1;
    cache.get("error", specForTesting.getSpec(), ptr1, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected an error.");
    }
}

void LRUCacheStorageTest::verifySharedPointer(
        const CopyOnWriteForTesting* ptr,
        const UnicodeString& name,
        const UnicodeString& format) {
    const UnicodeString *strPtr = ptr->localeNamePtr.readOnly();
    verifyString(name, *strPtr);
    strPtr = ptr->formatStrPtr.readOnly();
    verifyString(format, *strPtr);
}

void LRUCacheStorageTest::verifyString(const UnicodeString &expected, const UnicodeString &actual) {
    if (expected != actual) {
        errln(UnicodeString("Expected '") + expected + "', got '"+ actual+"'");
    }
}

void LRUCacheStorageTest::verifyReferences(const CopyOnWriteForTesting* ptr, int32_t count, int32_t nameCount, int32_t formatCount) {
    int32_t actual = ptr->getRefCount();
    if (count != actual) {
        errln("Main reference count wrong: Expected %d, got %d", count, actual);
    }
    actual = ptr->localeNamePtr.count();
    if (nameCount != actual) {
        errln("name reference count wrong: Expected %d, got %d", nameCount, actual);
    }
    actual = ptr->formatStrPtr.count();
    if (formatCount != actual) {
        errln("format reference count wrong: Expected %d, got %d", formatCount, actual);
    }
}

extern IntlTest *createLRUCacheStorageTest() {
    return new LRUCacheStorageTest();
}
