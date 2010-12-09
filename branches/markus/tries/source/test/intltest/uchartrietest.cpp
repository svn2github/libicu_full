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
    void TestValuesForState();
    void TestNextForCodePoint();

    UnicodeString buildMonthsTrie(UCharTrieBuilder &builder);
    void TestHasUniqueValue();
    void TestGetNextUChars();
    void TestIteratorFromBranch();
    void TestIteratorFromLinearMatch();
    void TestTruncatingIteratorFromRoot();
    void TestTruncatingIteratorFromLinearMatchShort();
    void TestTruncatingIteratorFromLinearMatchLong();

    void checkData(const StringAndValue data[], int32_t dataLength);
    UnicodeString buildTrie(const StringAndValue data[], int32_t dataLength, UCharTrieBuilder &builder);
    void checkHasValue(const UnicodeString &trieUChars, const StringAndValue data[], int32_t dataLength);
    void checkHasValueWithState(const UnicodeString &trieUChars, const StringAndValue data[], int32_t dataLength);
    void checkIterator(const UnicodeString &trieUChars, const StringAndValue data[], int32_t dataLength);
    void checkIterator(UCharTrieIterator &iter, const StringAndValue data[], int32_t dataLength);
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
    TESTCASE_AUTO(TestValuesForState);
    TESTCASE_AUTO(TestNextForCodePoint);
    TESTCASE_AUTO(TestHasUniqueValue);
    TESTCASE_AUTO(TestGetNextUChars);
    TESTCASE_AUTO(TestIteratorFromBranch);
    TESTCASE_AUTO(TestIteratorFromLinearMatch);
    TESTCASE_AUTO(TestTruncatingIteratorFromRoot);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchShort);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchLong);
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

void UCharTrieTest::TestValuesForState() {
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

void UCharTrieTest::TestNextForCodePoint() {
    static const StringAndValue data[]={
        { "\\u4dff\\U00010000\\u9999\\U00020000\\udfff\\U0010ffff", 2000000000 },
        { "\\u4dff\\U00010000\\u9999\\U00020002", 44444 },
        { "\\u4dff\\U000103ff", 99999 }
    };
    UCharTrieBuilder builder;
    UnicodeString s=buildTrie(data, LENGTHOF(data), builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    if( !trie.nextForCodePoint(0x4dff) || trie.hasValue() ||
        !trie.nextForCodePoint(0x10000) || trie.hasValue() ||
        !trie.nextForCodePoint(0x9999) || trie.hasValue() ||
        !trie.nextForCodePoint(0x20000) || trie.hasValue() ||
        !trie.nextForCodePoint(0xdfff) || trie.hasValue() ||
        !trie.nextForCodePoint(0x10ffff) || !trie.hasValue() || trie.getValue()!=2000000000
    ) {
        errln("UCharTrie.nextForCodePoint() fails for %s", data[0].s);
    }
    if( !trie.reset().nextForCodePoint(0x4dff) || trie.hasValue() ||
        !trie.nextForCodePoint(0x10000) || trie.hasValue() ||
        !trie.nextForCodePoint(0x9999) || trie.hasValue() ||
        !trie.nextForCodePoint(0x20002) || !trie.hasValue() || trie.getValue()!=44444
    ) {
        errln("UCharTrie.nextForCodePoint() fails for %s", data[1].s);
    }
    if( !trie.reset().nextForCodePoint(0x4dff) || trie.hasValue() ||
        !trie.nextForCodePoint(0x10000) || trie.hasValue() ||
        !trie.nextForCodePoint(0x9999) || trie.hasValue() ||
        trie.nextForCodePoint(0x20222) || trie.hasValue()  // no match for trail surrogate
    ) {
        errln("UCharTrie.nextForCodePoint() fails for \\u4dff\\U00010000\\u9999\\U00020222");
    }
    if( !trie.reset().nextForCodePoint(0x4dff) || trie.hasValue() ||
        !trie.nextForCodePoint(0x103ff) || !trie.hasValue() || trie.getValue()!=99999
    ) {
        errln("UCharTrie.nextForCodePoint() fails for %s", data[2].s);
    }
}

enum {
    u_a=0x61,
    u_b=0x62,
    u_c=0x63,
    u_j=0x6a,
    u_n=0x6e,
    u_r=0x72,
    u_u=0x75
};

UnicodeString UCharTrieTest::buildMonthsTrie(UCharTrieBuilder &builder) {
    // All types of nodes leading to the same value,
    // for code coverage of recursive functions.
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
        { "july", 7 },
        { "jun", 6 },
        { "jun.", 6 },
        { "june", 6 }
    };
    return buildTrie(data, LENGTHOF(data), builder);
}

void UCharTrieTest::TestHasUniqueValue() {
    UCharTrieBuilder builder;
    UnicodeString s=buildMonthsTrie(builder);
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

class UnicodeStringAppendable : public Appendable {
public:
    UnicodeStringAppendable(UnicodeString &dest) : str(dest) {}
    virtual Appendable &append(UChar c) { str.append(c); return *this; }
    UnicodeStringAppendable &reset() { str.remove(); return *this; }
private:
    UnicodeString &str;
};

void UCharTrieTest::TestGetNextUChars() {
    UCharTrieBuilder builder;
    UnicodeString s=buildMonthsTrie(builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    UnicodeString buffer;
    UnicodeStringAppendable app(buffer);
    int32_t count=trie.getNextUChars(app);
    if(count!=2 || buffer.length()!=2 || buffer[0]!=u_a || buffer[1]!=u_j) {
        errln("months getNextUChars()!=[aj] at root");
    }
    trie.next(u_j);
    trie.next(u_a);
    trie.next(u_n);
    // getNextUChars() directly after next()
    count=trie.getNextUChars(app.reset());
    if(count!=20 || buffer!=UNICODE_STRING_SIMPLE(".abcdefghijklmnopqru")) {
        errln("months getNextUChars()!=[.abcdefghijklmnopqru] after \"jan\"");
    }
    // getNextUChars() after hasValue()
    trie.hasValue();
    count=trie.getNextUChars(app.reset());
    if(count!=20 || buffer!=UNICODE_STRING_SIMPLE(".abcdefghijklmnopqru")) {
        errln("months getNextUChars()!=[.abcdefghijklmnopqru] after \"jan\"+hasValue()");
    }
    // getNextUChars() from a linear-match node
    trie.next(u_u);
    count=trie.getNextUChars(app.reset());
    if(count!=1 || buffer.length()!=1 || buffer[0]!=u_a) {
        errln("months getNextUChars()!=[a] after \"janu\"");
    }
    trie.next(u_a);
    count=trie.getNextUChars(app.reset());
    if(count!=1 || buffer.length()!=1 || buffer[0]!=u_r) {
        errln("months getNextUChars()!=[r] after \"janua\"");
    }
}

void UCharTrieTest::TestIteratorFromBranch() {
    UCharTrieBuilder builder;
    UnicodeString s=buildMonthsTrie(builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    // Go to a branch node.
    trie.next(u_j);
    trie.next(u_a);
    trie.next(u_n);
    IcuTestErrorCode errorCode(*this, "TestIteratorFromBranch()");
    UCharTrieIterator iter(trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator(trie) constructor")) {
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

void UCharTrieTest::TestIteratorFromLinearMatch() {
    UCharTrieBuilder builder;
    UnicodeString s=buildMonthsTrie(builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    // Go into a linear-match node.
    trie.next(u_j);
    trie.next(u_a);
    trie.next(u_n);
    trie.next(u_u);
    trie.next(u_a);
    IcuTestErrorCode errorCode(*this, "TestIteratorFromLinearMatch()");
    UCharTrieIterator iter(trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator(trie) constructor")) {
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

void UCharTrieTest::TestTruncatingIteratorFromRoot() {
    UCharTrieBuilder builder;
    UnicodeString s=buildMonthsTrie(builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromRoot()");
    UCharTrieIterator iter(s.getBuffer(), 4, errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator(trie) constructor")) {
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

void UCharTrieTest::TestTruncatingIteratorFromLinearMatchShort() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    UCharTrieBuilder builder;
    UnicodeString s=buildTrie(data, LENGTHOF(data), builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    // Go into a linear-match node.
    trie.next(u_a);
    trie.next(u_b);
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchShort()");
    // Truncate within the linear-match node.
    UCharTrieIterator iter(trie, 2, errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator(trie) constructor")) {
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

void UCharTrieTest::TestTruncatingIteratorFromLinearMatchLong() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    UCharTrieBuilder builder;
    UnicodeString s=buildTrie(data, LENGTHOF(data), builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    UCharTrie trie(s.getBuffer());
    // Go into a linear-match node.
    trie.next(u_a);
    trie.next(u_b);
    trie.next(u_c);
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchLong()");
    // Truncate after the linear-match node.
    UCharTrieIterator iter(trie, 3, errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator(trie) constructor")) {
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

void UCharTrieTest::checkData(const StringAndValue data[], int32_t dataLength) {
    UCharTrieBuilder builder;
    UnicodeString s=buildTrie(data, dataLength, builder);
    if(s.isEmpty()) {
        return;  // buildTrie() reported an error
    }
    checkHasValue(s, data, dataLength);
    checkHasValueWithState(s, data, dataLength);
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
    UCharTrie::State state; 
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
        trie.saveState(state);
        UBool nextContinues=FALSE;
        for(int32_t c=0x20; c<0x7f; ++c) {
            if(trie.resetToState(state).next(c)) {
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

void UCharTrieTest::checkHasValueWithState(const UnicodeString &trieUChars,
                                           const StringAndValue data[], int32_t dataLength) {
    UCharTrie trie(trieUChars.getBuffer());
    UCharTrie::State noState, state; 
    for(int32_t i=0; i<dataLength; ++i) {
        if((i&1)==0) {
            // This should have no effect.
            trie.resetToState(noState);
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
            errln("trie.next(part of %s) changes hasValue()/getValue() after mark/next(0)/resetToMark",
                  data[i].s);
        } else if(!trie.hasValue(expectedString.getTerminatedBuffer()+partialLength,
                                 stringLength-partialLength)) {
            errln("trie.next(part of %s) does not seem to contain %s after mark/next(0)/resetToMark",
                  data[i].s);
        } else if(!trie.resetToState(state).hasValue(expectedString.getTerminatedBuffer()+partialLength,
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
    UCharTrieIterator iter(trieUChars.getBuffer(), 0, errorCode);
    if(errorCode.logIfFailureAndReset("UCharTrieIterator(trieUChars) constructor")) {
        return;
    }
    checkIterator(iter, data, dataLength);
}

void UCharTrieTest::checkIterator(UCharTrieIterator &iter,
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
