/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtest.cpp
*
* created on: 2012apr27
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationiterator.h"
#include "intltest.h"
#include "utf16collationiterator.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

class CollationTest : public IntlTest {
public:
    CollationTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);

    void TestMinMax();
    void TestImplicits();
    // TODO: void TestHiragana();
};

extern IntlTest *createCollationTest() {
    return new CollationTest();
}

void CollationTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite CollationTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestMinMax);
    TESTCASE_AUTO(TestImplicits);
    TESTCASE_AUTO_END;
}


void CollationTest::TestMinMax() {
    IcuTestErrorCode errorCode(*this, "TestMinMax");

    CollationDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationData> cd(builder.build(errorCode));
    if(errorCode.logIfFailureAndReset("CollationDataBuilder.build()")) {
        return;
    }

    UChar s[2] = { 0xfffe, 0xffff };
    UTF16CollationIterator ci(cd.getAlias(), 0, s, s + 2);
    int64_t ce = ci.nextCE(errorCode);
    int64_t expected =
        ((int64_t)Collation::MERGE_SEPARATOR_PRIMARY << 32) |
        (Collation::MERGE_SEPARATOR_BYTE << 24) |
        (Collation::MERGE_SEPARATOR_BYTE << 8);
    if(ce != expected) {
        errln("CollationIterator.nextCE(U+fffe)=%04lx != 02.02.02", (long)ce);
    }

    ce = ci.nextCE(errorCode);
    expected =
        ((int64_t)Collation::MAX_PRIMARY << 32) |
        Collation::COMMON_SEC_AND_TER_CE;
    if(ce != expected) {
        errln("CollationIterator.nextCE(U+ffff)=%04lx != max..", (long)ce);
    }

    ce = ci.nextCE(errorCode);
    if(ce != Collation::NO_CE) {
        errln("CollationIterator.nextCE(limit)=%04lx != NO_CE", (long)ce);
    }
}

void CollationTest::TestImplicits() {
    IcuTestErrorCode errorCode(*this, "TestImplicits");

    CollationDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationData> cd(builder.build(errorCode));
    if(errorCode.logIfFailureAndReset("CollationDataBuilder.build()")) {
        return;
    }

    // Implicit primary weights should be assigned for the following sets,
    // and sort in ascending order by set and then code point.
    // See http://www.unicode.org/reports/tr10/#Implicit_Weights
    // core Han Unified Ideographs
    UnicodeSet coreHan("[\\p{unified_ideograph}&"
                            "[\\p{Block=CJK_Unified_Ideographs}"
                            "\\p{Block=CJK_Compatibility_Ideographs}]]",
                       errorCode);
    // all other Unified Han ideographs
    UnicodeSet otherHan("[\\p{unified ideograph}-"
                            "[\\p{Block=CJK_Unified_Ideographs}"
                            "\\p{Block=CJK_Compatibility_Ideographs}]]",
                        errorCode);
    UnicodeSet unassigned("[:^assigned:]", errorCode);
    unassigned.remove(0xfffe, 0xffff);
    if(errorCode.logIfFailureAndReset("UnicodeSet")) {
        return;
    }
    const UnicodeSet *sets[] = { &coreHan, &otherHan, &unassigned };
    UChar32 prev;
    uint32_t prevPrimary = 0;
    UTF16CollationIterator ci(cd.getAlias(), 0, NULL, NULL);
    for(int32_t i = 0; i < LENGTHOF(sets); ++i) {
        LocalPointer<UnicodeSetIterator> iter(new UnicodeSetIterator(*sets[i]));
        while(iter->next()) {
            UChar32 c = iter->getCodepoint();
            UnicodeString s(c);
            ci.setText(s.getBuffer(), s.getBuffer() + s.length());
            int64_t ce = ci.nextCE(errorCode);
            int64_t ce2 = ci.nextCE(errorCode);
            if(errorCode.logIfFailureAndReset("CollationIterator.nextCE()")) {
                return;
            }
            if(ce == Collation::NO_CE || ce2 != Collation::NO_CE) {
                errln("CollationIterator.nextCE(U+%04lx) did not yield exactly one CE", (long)c);
                continue;
            }
            if((ce & 0xffffffff) != Collation::COMMON_SEC_AND_TER_CE) {
                errln("CollationIterator.nextCE(U+%04lx) has non-common sec/ter weights: %04lx",
                      (long)c, (long)(ce & 0xffffffff));
                continue;
            }
            uint32_t primary = (uint32_t)(ce >> 32);
            if(!(primary > prevPrimary)) {
                errln("CE(U+%04lx)=%04lx.. not greater than CE(U+%04lx)=%04lx..",
                      (long)c, (long)primary, (long)prev, (long)prevPrimary);
            }
            prev = c;
            prevPrimary = primary;
        }
    }
}
