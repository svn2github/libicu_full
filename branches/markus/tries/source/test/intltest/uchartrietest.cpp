/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uchartrietest.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov16
*   created by: Markus W. Scherer
*/

#include <string.h>

#include "unicode/utypes.h"
#include "uchartrie.h"
#include "uchartriebuilder.h"
#include "uchartrieiterator.h"
#include "intltest.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

struct StringAndValue {
    const char *s;
    int32_t value;
};

class UCharTrieTest : public IntlTest {
public:
    UCharTrieTest() {}
    virtual ~UCharTrieTest();

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);
    void TestBuilder();
    void TestEmpty();
    void Test_a();
    void Test_a_ab();
    void TestShortestListBranch();
    void TestLongestListBranch();
    void TestLongSequence();
    void TestLongBranch();
    void TestValuesForMarkAndReset();
    void TestHasUniqueValue();

    void checkData(const StringAndValue data[], int32_t dataLength);
    UnicodeString buildTrie(const StringAndValue data[], int32_t dataLength, UCharTrieBuilder &builder);
    void checkHasValue(const UnicodeString &trieUChars, const StringAndValue data[], int32_t dataLength);
    void checkMarkAndReset(const UnicodeString &trieUChars, const StringAndValue data[], int32_t dataLength);
    void checkIterator(const UnicodeString &trieUChars, const StringAndValue data[], int32_t dataLength);
};

extern IntlTest *createUCharTrieTest() {
    return new UCharTrieTest();
}

UCharTrieTest::~UCharTrieTest() {
}

void UCharTrieTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite UCharTrieTest: ");
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
    TESTCASE_AUTO(TestValuesForMarkAndReset);
    TESTCASE_AUTO(TestHasUniqueValue);
    TESTCASE_AUTO_END;
}

void UCharTrieTest::TestBuilder() {
    IcuTestErrorCode errorCode(*this, "TestBuilder()");
    UCharTrieBuilder builder;
    builder.build(errorCode);
    if(errorCode.reset()!=U_INDEX_OUTOFBOUNDS_ERROR) {
        errln("UCharTrieBuilder().build() did not set U_INDEX_OUTOFBOUNDS_ERROR");
        return;
    }
    builder.add("=", 0, errorCode).add("=", 1, errorCode).build(errorCode);
    if(errorCode.reset()!=U_ILLEGAL_ARGUMENT_ERROR) {
        errln("UCharTrieBuilder.build() did not detect duplicates");
        return;
    }
}

void UCharTrieTest::TestEmpty() {
    static const StringAndValue data[]={
        { "", 0 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharTrieTest::Test_a() {
    static const StringAndValue data[]={
        { "a", 1 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharTrieTest::Test_a_ab() {
    static const StringAndValue data[]={
        { "a", 1 },
        { "ab", 100 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharTrieTest::TestShortestListBranch() {
    static const StringAndValue data[]={
        { "a", 1000 },
        { "b", 2000 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharTrieTest::TestLongestListBranch() {
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
    checkData(data, LENGTHOF(data));
}

void UCharTrieTest::TestLongSequence() {
    static const StringAndValue data[]={
        { "a", -1 },
        // sequence of linear-match nodes
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -2 },
        // more than 256 units
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -3 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharTrieTest::TestLongBranch() {
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

void UCharTrieTest::TestValuesForMarkAndReset() {
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

enum {
    u_a=0x61,
    u_j=0x6a,
    u_n=0x6e,
    u_u=0x75
};

void UCharTrieTest::TestHasUniqueValue() {
    // All types of nodes leading to the same value,
    // for code coverage of the recursive hasUniqueValue() implementation.
    // In particular, we need a lot of branches on some single level
    // to exercise a three-way-branch node.
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
        { "jun", 6 },
        { "jun.", 6 },
        { "june", 6 },
        { "july", 7 }
    };
    UCharTrieBuilder builder;
    UnicodeString s=buildTrie(data, LENGTHOF(data), builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    if(trie.hasUniqueValue()) {
        errln("unique value at root");
    }
    trie.next(u_j);
    trie.next(u_a);
    trie.next(u_n);
    // hasUniqueValue() directly after next()
    if(!trie.hasUniqueValue() || 1!=trie.getValue()) {
        errln("not unique value 1 after \"jan\"");
    }
    trie.reset().next(u_j);
    trie.next(u_u);
    if(trie.hasUniqueValue()) {
        errln("unique value after \"ju\"");
    }
    trie.next(u_n);
    if(!trie.hasValue() || 6!=trie.getValue()) {
        errln("not normal value 6 after \"jun\"");
    }
    // hasUniqueValue() after hasValue()
    if(!trie.hasUniqueValue() || 6!=trie.getValue()) {
        errln("not unique value 6 after \"jun\"");
    }
    // hasUniqueValue() from within a linear-match node
    trie.reset().next(u_a);
    trie.next(u_u);
    if(!trie.hasUniqueValue() || 8!=trie.getValue()) {
        errln("not unique value 8 after \"au\"");
    }
}

void UCharTrieTest::checkData(const StringAndValue data[], int32_t dataLength) {
    UCharTrieBuilder builder;
    UnicodeString s=buildTrie(data, dataLength, builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    checkHasValue(s, data, dataLength);
    checkMarkAndReset(s, data, dataLength);
    checkIterator(s, data, dataLength);
}

UnicodeString UCharTrieTest::buildTrie(const StringAndValue data[], int32_t dataLength,
                                       UCharTrieBuilder &builder) {
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
        builder.add(UnicodeString(data[index].s, -1, US_INV).unescape(),
                    data[index].value, errorCode);
        index=(index+step)%dataLength;
    }
    UnicodeString s(builder.build(errorCode));
    if(!errorCode.logIfFailureAndReset("add()/build()")) {
        builder.add("zzz", 999, errorCode);
        if(errorCode.reset()!=U_NO_WRITE_PERMISSION) {
            errln("builder.build().add(zzz) did not set U_NO_WRITE_PERMISSION");
        }
    }
    return s;
}

void UCharTrieTest::checkHasValue(const UnicodeString &trieUChars,
                                  const StringAndValue data[], int32_t dataLength) {
    UCharTrie trie(trieUChars.getBuffer());
    for(int32_t i=0; i<dataLength; ++i) {
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        int32_t stringLength= (i&1) ? -1 : expectedString.length();
        if(!trie.hasValue(expectedString.getTerminatedBuffer(), stringLength)) {
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
        stringLength=expectedString.length();
        for(int32_t j=0; j<stringLength; ++j) {
            if(!trie.hasNext() || (trie.hasValue(), !trie.hasNext())) {
                errln("trie.hasNext()=FALSE before end of %s (at index %d)", data[i].s, j);
                break;
            }
            if(!trie.next(expectedString[j])) {
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
        trie.mark();
        UBool nextContinues=FALSE;
        for(int32_t c=0x20; c<0x7f; ++c) {
            if(trie.resetToMark().next(c)) {
                nextContinues=TRUE;
                break;
            }
        }
        if(hasNext!=nextContinues) {
            errln("trie.hasNext()=trie.next(some UChar) after end of %s", data[i].s);
        }
        trie.reset();
    }
}

void UCharTrieTest::checkMarkAndReset(const UnicodeString &trieUChars,
                                      const StringAndValue data[], int32_t dataLength) {
    UCharTrie trie(trieUChars.getBuffer());
    for(int32_t i=0; i<dataLength; ++i) {
        if((i&1)==0) {
            // resetToMark() from initial state or after reset()
            // should have no effect.
            trie.resetToMark();
        }
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        int32_t stringLength=expectedString.length();
        int32_t partialLength=stringLength/3;
        for(int32_t j=0; j<partialLength; ++j) {
            if(!trie.next(expectedString[j])) {
                errln("trie.next()=false for a prefix of %s", data[i].s);
                return;
            }
        }
        trie.mark();
        UBool hasValueAtMark=trie.hasValue();
        int32_t valueAtMark=-99;
        if(hasValueAtMark) {
            valueAtMark=trie.getValue();
        }
        trie.next(0);  // mismatch
        if(hasValueAtMark!=trie.resetToMark().hasValue() || (hasValueAtMark && valueAtMark!=trie.getValue())) {
            errln("trie.next(part of %s) changes hasValue()/getValue() after mark/next(0)/resetToMark", data[i].s);
        } else if(!trie.hasValue(expectedString.getTerminatedBuffer()+partialLength, stringLength-partialLength)) {
            errln("trie.next(part of %s) does not seem to contain %s after mark/next(0)/resetToMark", data[i].s);
        } else if(!trie.resetToMark().hasValue(expectedString.getTerminatedBuffer()+partialLength,
                                               stringLength-partialLength)) {
            errln("trie does not seem to contain %s after mark/hasValue(rest)/resetToMark", data[i].s);
        } else if(trie.getValue()!=data[i].value) {
            errln("trie value for %s is %ld=0x%lx instead of expected %ld=0x%lx",
                  data[i].s,
                  (long)trie.getValue(), (long)trie.getValue(),
                  (long)data[i].value, (long)data[i].value);
        }
        trie.reset();
    }
}

void UCharTrieTest::checkIterator(const UnicodeString &trieUChars,
                                  const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    UCharTrieIterator iter(trieUChars.getBuffer(), errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator constructor")) {
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
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        if(iter.getString()!=expectedString) {
            char buffer[1000];
            UnicodeString invString(prettify(iter.getString()));
            errln("trie iterator next().getString()=%s but expected %s for item %d",
                  invString.extract(0, invString.length(), buffer, LENGTHOF(buffer), US_INV),
                  data[i].s, (int)i);
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
