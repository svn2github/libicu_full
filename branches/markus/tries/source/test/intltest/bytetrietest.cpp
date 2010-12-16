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
    void TestShortestBranch();
    void TestBranches();
    void TestLongSequence();
    void TestLongBranch();
    void TestValuesForState();

    StringPiece buildMonthsTrie(ByteTrieBuilder &builder);
    void TestHasUniqueValue();
    void TestGetNextBytes();
    void TestIteratorFromBranch();
    void TestIteratorFromLinearMatch();
    void TestTruncatingIteratorFromRoot();
    void TestTruncatingIteratorFromLinearMatchShort();
    void TestTruncatingIteratorFromLinearMatchLong();

    void checkData(const StringAndValue data[], int32_t dataLength);
    StringPiece buildTrie(const StringAndValue data[], int32_t dataLength, ByteTrieBuilder &builder);
    void checkFirst(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
    void checkHasValue(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
    void checkHasValueWithState(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
    void checkNextString(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
    void checkIterator(const StringPiece &trieBytes, const StringAndValue data[], int32_t dataLength);
    void checkIterator(ByteTrieIterator &iter, const StringAndValue data[], int32_t dataLength);
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
    TESTCASE_AUTO(TestShortestBranch);
    TESTCASE_AUTO(TestBranches);
    TESTCASE_AUTO(TestLongSequence);
    TESTCASE_AUTO(TestLongBranch);
    TESTCASE_AUTO(TestValuesForState);
/*    TESTCASE_AUTO(TestHasUniqueValue);
    TESTCASE_AUTO(TestGetNextBytes);
    TESTCASE_AUTO(TestIteratorFromBranch);
    TESTCASE_AUTO(TestIteratorFromLinearMatch);
    TESTCASE_AUTO(TestTruncatingIteratorFromRoot);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchShort);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchLong);*/
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

void ByteTrieTest::TestShortestBranch() {
    static const StringAndValue data[]={
        { "a", 1000 },
        { "b", 2000 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::TestBranches() {
    static const StringAndValue data[]={
        { "a", 0x10 },
        { "cc", 0x40 },
        { "e", 0x100 },
        { "ggg", 0x400 },
        { "i", 0x1000 },
        { "kkkk", 0x4000 },
        { "n", 0x10000 },
        { "ppppp", 0x40000 },
        { "r", 0x100000 },
        { "sss", 0x200000 },
        { "t", 0x400000 },
        { "uu", 0x800000 },
        { "vv", 0x7fffffff },
        { "zz", 0x80000000 }
    };
    for(int32_t length=2; length<=LENGTHOF(data); ++length) {
        infoln("TestBranches length=%d", (int)length);
        checkData(data, length);
    }
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
    // Split-branch and interesting compact-integer values.
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
        { "t234567890", 0x77777777 },
        { "z", 0x80000001 }
    };
    checkData(data, LENGTHOF(data));
}

void ByteTrieTest::TestValuesForState() {
    // Check that saveState() and resetToState() interact properly
    // with next() and hasValue().
    static const StringAndValue data[]={
        { "a", -1 },
        { "ab", -2 },
        { "abc", -3 },
        { "abcd", -4 },
        { "abcde", -5 },
        { "abcdef", -6 }
    };
    checkData(data, LENGTHOF(data));
}

StringPiece ByteTrieTest::buildMonthsTrie(ByteTrieBuilder &builder) {
    // All types of nodes leading to the same value,
    // for code coverage of recursive functions.
    // In particular, we need a lot of branches on some single level
    // to exercise a split-branch node.
    static const StringAndValue data[]={
        { "august", 8 },
        { "jan", 1 },
        { "jan.", 1 },
        { "jana", 1 },
        { "janbb", 1 },
        { "janc", 1 },
        { "janddd", 1 },
        { "janee", 1 },
        { "janef", 1 },
        { "janf", 1 },
        { "jangg", 1 },
        { "janh", 1 },
        { "janiiii", 1 },
        { "janj", 1 },
        { "jankk", 1 },
        { "jankl", 1 },
        { "jankmm", 1 },
        { "janl", 1 },
        { "janm", 1 },
        { "jannnnnnnnnnnnnnnnnnnnnnnnnnnnn", 1 },
        { "jano", 1 },
        { "janpp", 1 },
        { "janqqq", 1 },
        { "janr", 1 },
        { "januar", 1 },
        { "january", 1 },
        { "july", 7 },
        { "jun", 6 },
        { "jun.", 6 },
        { "june", 6 }
    };
    return buildTrie(data, LENGTHOF(data), builder);
}

void ByteTrieTest::TestHasUniqueValue() {
    ByteTrieBuilder builder;
    StringPiece sp=buildMonthsTrie(builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    ByteTrie trie(sp.data());
    if(trie.hasUniqueValue()) {
        errln("unique value at root");
    }
    trie.next('j');
    trie.next('a');
    trie.next('n');
    // hasUniqueValue() directly after next()
    if(!trie.hasUniqueValue() || 1!=trie.getValue()) {
        errln("not unique value 1 after \"jan\"");
    }
    trie.reset().next('j');
    trie.next('u');
    if(trie.hasUniqueValue()) {
        errln("unique value after \"ju\"");
    }
    trie.next('n');
    if(!trie.hasValue() || 6!=trie.getValue()) {
        errln("not normal value 6 after \"jun\"");
    }
    // hasUniqueValue() after hasValue()
    if(!trie.hasUniqueValue() || 6!=trie.getValue()) {
        errln("not unique value 6 after \"jun\"");
    }
    // hasUniqueValue() from within a linear-match node
    trie.reset().next('a');
    trie.next('u');
    if(!trie.hasUniqueValue() || 8!=trie.getValue()) {
        errln("not unique value 8 after \"au\"");
    }
}

void ByteTrieTest::TestGetNextBytes() {
    ByteTrieBuilder builder;
    StringPiece sp=buildMonthsTrie(builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    ByteTrie trie(sp.data());
    char buffer[40];
    CheckedArrayByteSink sink(buffer, LENGTHOF(buffer));
    int32_t count=trie.getNextBytes(sink);
    if(count!=2 || sink.NumberOfBytesAppended()!=2 || buffer[0]!='a' || buffer[1]!='j') {
        errln("months getNextBytes()!=[aj] at root");
    }
    trie.next('j');
    trie.next('a');
    trie.next('n');
    // getNextBytes() directly after next()
    count=trie.getNextBytes(sink.Reset());
    buffer[count]=0;
    if(count!=20 || sink.NumberOfBytesAppended()!=20 || 0!=strcmp(buffer, ".abcdefghijklmnopqru")) {
        errln("months getNextBytes()!=[.abcdefghijklmnopqru] after \"jan\"");
    }
    // getNextBytes() after hasValue()
    trie.hasValue();
    memset(buffer, 0, sizeof(buffer));
    count=trie.getNextBytes(sink.Reset());
    if(count!=20 || sink.NumberOfBytesAppended()!=20 || 0!=strcmp(buffer, ".abcdefghijklmnopqru")) {
        errln("months getNextBytes()!=[.abcdefghijklmnopqru] after \"jan\"+hasValue()");
    }
    // getNextBytes() from a linear-match node
    trie.next('u');
    memset(buffer, 0, sizeof(buffer));
    count=trie.getNextBytes(sink.Reset());
    if(count!=1 || sink.NumberOfBytesAppended()!=1 || buffer[0]!='a') {
        errln("months getNextBytes()!=[a] after \"janu\"");
    }
    trie.next('a');
    memset(buffer, 0, sizeof(buffer));
    count=trie.getNextBytes(sink.Reset());
    if(count!=1 || sink.NumberOfBytesAppended()!=1 || buffer[0]!='r') {
        errln("months getNextBytes()!=[r] after \"janua\"");
    }
}

void ByteTrieTest::TestIteratorFromBranch() {
    ByteTrieBuilder builder;
    StringPiece sp=buildMonthsTrie(builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    ByteTrie trie(sp.data());
    // Go to a branch node.
    trie.next('j');
    trie.next('a');
    trie.next('n');
    IcuTestErrorCode errorCode(*this, "TestIteratorFromBranch()");
    ByteTrieIterator iter(trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the suffixes
    // following "jan".
    static const StringAndValue data[]={
        { "", 1 },
        { ".", 1 },
        { "a", 1 },
        { "bb", 1 },
        { "c", 1 },
        { "ddd", 1 },
        { "ee", 1 },
        { "ef", 1 },
        { "f", 1 },
        { "gg", 1 },
        { "h", 1 },
        { "iiii", 1 },
        { "j", 1 },
        { "kk", 1 },
        { "kl", 1 },
        { "kmm", 1 },
        { "l", 1 },
        { "m", 1 },
        { "nnnnnnnnnnnnnnnnnnnnnnnnnnnn", 1 },
        { "o", 1 },
        { "pp", 1 },
        { "qqq", 1 },
        { "r", 1 },
        { "uar", 1 },
        { "uary", 1 }
    };
    checkIterator(iter, data, LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, LENGTHOF(data));
}

void ByteTrieTest::TestIteratorFromLinearMatch() {
    ByteTrieBuilder builder;
    StringPiece sp=buildMonthsTrie(builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    ByteTrie trie(sp.data());
    // Go into a linear-match node.
    trie.next('j');
    trie.next('a');
    trie.next('n');
    trie.next('u');
    trie.next('a');
    IcuTestErrorCode errorCode(*this, "TestIteratorFromLinearMatch()");
    ByteTrieIterator iter(trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the suffixes
    // following "janua".
    static const StringAndValue data[]={
        { "r", 1 },
        { "ry", 1 }
    };
    checkIterator(iter, data, LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, LENGTHOF(data));
}

void ByteTrieTest::TestTruncatingIteratorFromRoot() {
    ByteTrieBuilder builder;
    StringPiece sp=buildMonthsTrie(builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromRoot()");
    ByteTrieIterator iter(sp.data(), 4, errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the first 4 characters
    // of each string, and no string duplicates from the truncation.
    static const StringAndValue data[]={
        { "augu", -1 },
        { "jan", 1 },
        { "jan.", 1 },
        { "jana", 1 },
        { "janb", -1 },
        { "janc", 1 },
        { "jand", -1 },
        { "jane", -1 },
        { "janf", 1 },
        { "jang", -1 },
        { "janh", 1 },
        { "jani", -1 },
        { "janj", 1 },
        { "jank", -1 },
        { "janl", 1 },
        { "janm", 1 },
        { "jann", -1 },
        { "jano", 1 },
        { "janp", -1 },
        { "janq", -1 },
        { "janr", 1 },
        { "janu", -1 },
        { "july", 7 },
        { "jun", 6 },
        { "jun.", 6 },
        { "june", 6 }
    };
    checkIterator(iter, data, LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, LENGTHOF(data));
}

void ByteTrieTest::TestTruncatingIteratorFromLinearMatchShort() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    ByteTrieBuilder builder;
    StringPiece sp=buildTrie(data, LENGTHOF(data), builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    ByteTrie trie(sp.data());
    // Go into a linear-match node.
    trie.next('a');
    trie.next('b');
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchShort()");
    // Truncate within the linear-match node.
    ByteTrieIterator iter(trie, 2, errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator(trie) constructor")) {
        return;
    }
    static const StringAndValue expected[]={
        { "cd", -1 }
    };
    checkIterator(iter, expected, LENGTHOF(expected));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), expected, LENGTHOF(expected));
}

void ByteTrieTest::TestTruncatingIteratorFromLinearMatchLong() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    ByteTrieBuilder builder;
    StringPiece sp=buildTrie(data, LENGTHOF(data), builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    ByteTrie trie(sp.data());
    // Go into a linear-match node.
    trie.next('a');
    trie.next('b');
    trie.next('c');
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchLong()");
    // Truncate after the linear-match node.
    ByteTrieIterator iter(trie, 3, errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator(trie) constructor")) {
        return;
    }
    static const StringAndValue expected[]={
        { "def", 10 },
        { "dep", -1 },
        { "dey", -1 }
    };
    checkIterator(iter, expected, LENGTHOF(expected));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), expected, LENGTHOF(expected));
}

void ByteTrieTest::checkData(const StringAndValue data[], int32_t dataLength) {
    ByteTrieBuilder builder;
    StringPiece sp=buildTrie(data, dataLength, builder);
    if(sp.empty()) {
        return;  // buildTrie() reported an error
    }
    checkFirst(sp, data, dataLength);
    checkHasValue(sp, data, dataLength);
    checkHasValueWithState(sp, data, dataLength);
    checkNextString(sp, data, dataLength);
    // checkIterator(sp, data, dataLength);
}

StringPiece ByteTrieTest::buildTrie(const StringAndValue data[], int32_t dataLength,
                                    ByteTrieBuilder &builder) {
    IcuTestErrorCode errorCode(*this, "buildTrie()");
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

void ByteTrieTest::checkFirst(const StringPiece &trieBytes,
                              const StringAndValue data[], int32_t dataLength) {
    ByteTrie trie(trieBytes.data());
    for(int32_t i=0; i<dataLength; ++i) {
        int c=(uint8_t)*data[i].s;
        if(c==0) {
            continue;  // skip empty string
        }
        UBool firstOk=trie.first(c);
        UBool firstHasValue=firstOk && trie.hasValue();
        int32_t firstValue=firstHasValue ? trie.getValue() : -1;
        UBool nextOk=trie.next((uint8_t)data[i].s[1]);
        if(firstOk!=trie.reset().next(c) ||
           firstHasValue!=(firstOk && trie.hasValue()) ||
           firstValue!=(firstHasValue ? trie.getValue() : -1) ||
           nextOk!=trie.next((uint8_t)data[i].s[1])
        ) {
            errln("trie.first(%c)!=trie.reset().next(same) for %s",
                  c, data[i].s);
        }
    }
}

void ByteTrieTest::checkHasValue(const StringPiece &trieBytes,
                                 const StringAndValue data[], int32_t dataLength) {
    ByteTrie trie(trieBytes.data());
    ByteTrie::State state;
    for(int32_t i=0; i<dataLength; ++i) {
        int32_t stringLength= (i&1) ? -1 : strlen(data[i].s);
        if(!trie.next(data[i].s, stringLength) || !trie.hasValue()) {
            errln("trie does not seem to contain %s", data[i].s);
        } else if(trie.getValue()!=data[i].value) {
            errln("trie value for %s is %ld=0x%lx instead of expected %ld=0x%lx",
                  data[i].s,
                  (long)trie.getValue(), (long)trie.getValue(),
                  (long)data[i].value, (long)data[i].value);
        } else if(!trie.hasValue() || trie.getValue()!=data[i].value) {
            errln("trie value for %s changes when repeating hasValue()/getValue()", data[i].s);
        }
        trie.reset();
        stringLength=strlen(data[i].s);
        for(int32_t j=0; j<stringLength; ++j) {
            if(!trie.hasNext() || (trie.hasValue(), !trie.hasNext())) {
                errln("trie.hasNext()=FALSE before end of %s (at index %d)", data[i].s, j);
                break;
            }
            if(!trie.next(data[i].s[j])) {
                errln("trie.next()=FALSE before end of %s (at index %d)", data[i].s, j);
                break;
            }
        }
        // Compare the final hasNext() with whether next() can actually continue.
        UBool hasNext=trie.hasNext();
        trie.hasValue();
        if(hasNext!=trie.hasNext()) {
            errln("trie.hasNext() != hasNext()+hasValue()+hasNext() after end of %s", data[i].s);
        }
        trie.saveState(state);
        UBool nextContinues=FALSE;
        for(int32_t c=0x20; c<0x7f; ++c) {
            if(trie.resetToState(state).next(c)) {
                nextContinues=TRUE;
                break;
            }
        }
        if(hasNext!=nextContinues) {
            errln("trie.hasNext()=trie.next(some byte) after end of %s", data[i].s);
        }
        trie.reset();
    }
}

void ByteTrieTest::checkHasValueWithState(const StringPiece &trieBytes,
                                          const StringAndValue data[], int32_t dataLength) {
    ByteTrie trie(trieBytes.data());
    ByteTrie::State noState, state;
    for(int32_t i=0; i<dataLength; ++i) {
        if((i&1)==0) {
            // This should have no effect.
            trie.resetToState(noState);
        }
        const char *expectedString=data[i].s;
        int32_t stringLength=strlen(expectedString);
        int32_t partialLength=stringLength/3;
        for(int32_t j=0; j<partialLength; ++j) {
            if(!trie.next(expectedString[j])) {
                errln("trie.next()=false for a prefix of %s", expectedString);
                return;
            }
        }
        trie.saveState(state);
        UBool hasValueAtState=trie.hasValue();
        int32_t valueAtState=-99;
        if(hasValueAtState) {
            valueAtState=trie.getValue();
        }
        trie.next(0);  // mismatch
        if( hasValueAtState!=trie.resetToState(state).hasValue() ||
            (hasValueAtState && valueAtState!=trie.getValue())
        ) {
            errln("trie.next(part of %s) changes hasValue()/getValue() after saveState/next(0)/resetToState",
                  expectedString);
        } else if(!trie.next(expectedString+partialLength, stringLength-partialLength) ||
                  !trie.hasValue()) {
            errln("trie.next(part of %s) does not seem to contain %s after saveState/next(0)/resetToState",
                  expectedString);
        } else if(!trie.resetToState(state).
                        next(expectedString+partialLength, stringLength-partialLength) ||
                  !trie.hasValue()) {
            errln("trie does not seem to contain %s after saveState/next(rest)/resetToState",
                  expectedString);
        } else if(trie.getValue()!=data[i].value) {
            errln("trie value for %s is %ld=0x%lx instead of expected %ld=0x%lx",
                  expectedString,
                  (long)trie.getValue(), (long)trie.getValue(),
                  (long)data[i].value, (long)data[i].value);
        }
        trie.reset();
    }
}

// next(string) is also tested in other functions,
// but here we try to go partway through the string, and then beyond it.
void ByteTrieTest::checkNextString(const StringPiece &trieBytes,
                                   const StringAndValue data[], int32_t dataLength) {
    ByteTrie trie(trieBytes.data());
    for(int32_t i=0; i<dataLength; ++i) {
        const char *expectedString=data[i].s;
        int32_t stringLength=strlen(expectedString);
        if(!trie.next(expectedString, stringLength/2)) {
            errln("trie.next(up to middle of string)=FALSE for %s", expectedString);
            continue;
        }
        // Test that we stop properly at the end of the string.
        if(trie.next(expectedString+stringLength/2, stringLength+1-stringLength/2)) {
            errln("trie.next(string+NUL)=TRUE for %s", expectedString);
        }
        trie.reset();
    }
}

void ByteTrieTest::checkIterator(const StringPiece &trieBytes,
                                 const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    ByteTrieIterator iter(trieBytes.data(), 0, errorCode);
    if(errorCode.logIfFailureAndReset("ByteTrieIterator(trieBytes) constructor")) {
        return;
    }
    checkIterator(iter, data, dataLength);
}

void ByteTrieTest::checkIterator(ByteTrieIterator &iter,
                                 const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
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
