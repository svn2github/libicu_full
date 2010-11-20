/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetrietest.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov16
*   created by: Markus W. Scherer
*/

#include <string.h>

#include "unicode/utypes.h"
#include "unicode/stringpiece.h"
#include "bytetrie.h"
#include "bytetriebuilder.h"
#include "bytetrieiterator.h"
#include "intltest.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

struct StringAndValue {
    const char *s;
    int32_t value;
};

class ByteTrieTest : public IntlTest {
public:
    ByteTrieTest() {}
    virtual ~ByteTrieTest();

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);
    void TestBuilder();
    void TestEmpty();
    void Test_a();
    void Test_a_ab();
    void TestShortestListBranch();
    void TestLongestListBranch();
    void TestLongSequence();
    void TestLongBranch();

    void checkData(const StringAndValue data[], int32_t dataLength);
    StringPiece buildTrie(const StringAndValue data[], int32_t dataLength, ByteTrieBuilder &builder);
    void checkContains(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
    void checkIterator(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
};

extern IntlTest *createByteTrieTest() {
    return new ByteTrieTest();
}

ByteTrieTest::~ByteTrieTest() {
}

void ByteTrieTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite ByteTrieTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBuilder);
    TESTCASE_AUTO(TestEmpty);
    TESTCASE_AUTO(Test_a);
    TESTCASE_AUTO(Test_a_ab);
    TESTCASE_AUTO(TestShortestListBranch);
    TESTCASE_AUTO(TestLongestListBranch);
    TESTCASE_AUTO(TestLongSequence);
    TESTCASE_AUTO(TestLongBranch);
    TESTCASE_AUTO_END;
}

void ByteTrieTest::TestBuilder() {
    IcuTestErrorCode errorCode(*this, "TestBuilder()");
    ByteTrieBuilder builder;
    builder.build(errorCode);
    if(errorCode.reset()!=U_INDEX_OUTOFBOUNDS_ERROR) {
        errln("ByteTrieBuilder().build() did not set U_INDEX_OUTOFBOUNDS_ERROR");
        return;
    }
    builder.add("=", 0, errorCode).add("=", 1, errorCode).build(errorCode);
    if(errorCode.reset()!=U_ILLEGAL_ARGUMENT_ERROR) {
        errln("ByteTrieBuilder.build() did not detect duplicates");
        return;
    }
}

void ByteTrieTest::TestEmpty() {
    static const StringAndValue data[]={
        { "", 0 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::Test_a() {
    static const StringAndValue data[]={
        { "a", 1 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::Test_a_ab() {
    static const StringAndValue data[]={
        { "a", 1 },
        { "ab", 100 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::TestShortestListBranch() {
    static const StringAndValue data[]={
        { "a", 1000 },
        { "b", 2000 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::TestLongestListBranch() {
    static const StringAndValue data[]={
        { "a", 0x10 },
        { "cc", 0x40 },
        { "e", 0x100 },
        { "ggg", 0x400 },
        { "i", 0x1000 },
        { "kkkk", 0x4000 },
        { "n", 0x10000 },
        { "ppppp", 0x40000 },
        { "r", 0x100000 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::TestLongSequence() {
    static const StringAndValue data[]={
        { "a", -1 },
        // sequence of linear-match nodes
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -2 },
        // more than 256 bytes
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -3 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::TestLongBranch() {
    // Three-way branch and interesting compact-integer values.
    static const StringAndValue data[]={
        { "a", -2 },
        { "b", -1 },
        { "c", 0 },
        { "d2", 1 },
        { "f", 0x3f },
        { "g", 0x40 },
        { "h", 0x41 },
        { "j23", 0x1900 },
        { "j24", 0x19ff },
        { "j25", 0x1a00 },
        { "k2", 0x1a80 },
        { "k3", 0x1aff },
        { "l234567890", 0x1b00 },
        { "l234567890123", 0x1b01 },
        { "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn", 0x10ffff },
        { "oooooooooooooooooooooooooooooooooooooooooooooooooooooo", 0x110000 },
        { "pppppppppppppppppppppppppppppppppppppppppppppppppppppp", 0x120000 },
        { "r", 0x333333 },
        { "s2345", 0x4444444 },
        { "t234567890", 0x77777777 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::checkData(const StringAndValue data[], int32_t dataLength) {
    ByteTrieBuilder builder;
    StringPiece sp=buildTrie(data, dataLength, builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    checkContains(sp, data, dataLength);
    checkIterator(sp, data, dataLength);
}

StringPiece ByteTrieTest::buildTrie(const StringAndValue data[], int32_t dataLength,
                                    ByteTrieBuilder &builder) {
    IcuTestErrorCode errorCode(*this, "buildTrie()");
    builder.clear();
    // Add the items to the trie builder in an interesting (not trivial, not random) order.
    int32_t index, step;
    if(dataLength&1) {
        // Odd number of items.
        index=dataLength/2;
        step=2;
    } else if((dataLength%3)!=0) {
        // Not a multiple of 3.
        index=dataLength/5;
        step=3;
    } else {
        index=dataLength-1;
        step=-1;
    }
    builder.clear();
    for(int32_t i=0; i<dataLength; ++i) {
        builder.add(data[index].s, data[index].value, errorCode);
        index=(index+step)%dataLength;
    }
    StringPiece sp(builder.build(errorCode));
    if(!errorCode.logIfFailureAndReset("add()/build()")) {
        builder.add("zzz", 999, errorCode);
        if(errorCode.reset()!=U_NO_WRITE_PERMISSION) {
            errln("builder.build().add(zzz) did not set U_NO_WRITE_PERMISSION");
        }
    }
    return sp;
}

void ByteTrieTest::checkContains(const StringPiece &trieBytes,
                                 const StringAndValue data[], int32_t dataLength) {
    ByteTrie trie(trieBytes.data());
    for(int32_t i=0; i<dataLength; ++i) {
        int32_t stringLength= (i&1) ? -1 : strlen(data[i].s);
        if(!trie.containsNext(data[i].s, stringLength)) {
            errln("trie does not seem to contain %s", data[i].s);
        } else if(trie.getValue()!=data[i].value) {
            errln("trie value for %s is %ld=0x%lx instead of expected %ld=0x%lx",
                  data[i].s,
                  (long)trie.getValue(), (long)trie.getValue(),
                  (long)data[i].value, (long)data[i].value);
        }
        trie.reset();
    }
}

void ByteTrieTest::checkIterator(const StringPiece &trieBytes,
                                 const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    ByteTrieIterator iter(trieBytes.data(), errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator constructor")) {
        return;
    }
    for(int32_t i=0; i<dataLength; ++i) {
        if(!iter.hasNext()) {
            errln("trie iterator hasNext()=FALSE for item %d: %s", (int)i, data[i].s);
            break;
        }
        UBool hasNext=iter.next(errorCode);
        if(errorCode.logIfFailureAndReset("trie iterator next() for item %d: %s", (int)i, data[i].s)) {
            break;
        }
        if(!hasNext) {
            errln("trie iterator next()=FALSE for item %d: %s", (int)i, data[i].s);
            break;
        }
        if(iter.getString()!=StringPiece(data[i].s)) {
            errln("trie iterator next().getString()=%s but expected %s for item %d",
                  iter.getString().data(), data[i].s, (int)i);
        }
        if(iter.getValue()!=data[i].value) {
            errln("trie iterator next().getValue()=%ld=0x%lx but expected %ld=0x%lx for item %d: %s",
                  (long)iter.getValue(), (long)iter.getValue(),
                  (long)data[i].value, (long)data[i].value,
                  (int)i, data[i].s);
        }
    }
    if(iter.hasNext()) {
        errln("trie iterator hasNext()=TRUE after all items");
    }
    UBool hasNext=iter.next(errorCode);
    errorCode.logIfFailureAndReset("trie iterator next() after all items");
    if(hasNext) {
        errln("trie iterator next()=TRUE after all items");
    }
}
