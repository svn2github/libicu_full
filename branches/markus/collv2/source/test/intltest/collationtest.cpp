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
#include "unicode/ustring.h"
#include "charstr.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationiterator.h"
#include "intltest.h"
#include "ucbuf.h"
#include "utf16collationiterator.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

// TODO: Move to ucbuf.h
U_DEFINE_LOCAL_OPEN_POINTER(LocalUCHARBUFPointer, UCHARBUF, ucbuf_close);

class CollationTest : public IntlTest {
public:
    CollationTest()
            : fileLineNumber(0),
              baseBuilder(NULL), baseData(NULL) {}

    ~CollationTest() {
        delete baseBuilder;
        delete baseData;
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);

    void TestMinMax();
    void TestImplicits();
    void TestNulTerminated();
    void TestHiragana();
    void TestDataDriven();

private:
    // Helpers & fields for data-driven test.
    static UBool isCROrLF(UChar c) { return c == 0xa || c == 0xd; }
    static UBool isSpace(UChar c) { return c == 9 || c == 0x20 || c == 0x3000; }
    static UBool isSectionStarter(UChar c) { return c == 0x25 || c == 0x2a || c == 0x40; }  // %*@
    static UBool isHexDigit(UChar c) {
        return (0x30 <= c && c <= 0x39) || (0x61 <= c && c <= 0x66) || (0x41 <= c && c <= 0x46);
    }
    int32_t skipSpaces(int32_t i) {
        while(isSpace(fileLine[i])) { ++i; }
        return i;
    }

    UBool readLine(UCHARBUF *f, IcuTestErrorCode &errorCode);
    uint32_t parseHex(int32_t &start, int32_t maxBytes, UErrorCode &errorCode);
    int64_t parseCE(int32_t &start, UErrorCode &errorCode);
    void parseString(int32_t &start, UnicodeString &prefix, UnicodeString &s, UErrorCode &errorCode);
    int32_t parseStringAndCEs(UnicodeString &prefix, UnicodeString &s,
                              int64_t ces[], int8_t hira[], int32_t capacity,
                              IcuTestErrorCode &errorCode);
    void buildBase(UCHARBUF *f, IcuTestErrorCode &errorCode);
    void checkCEs(UCHARBUF *f, IcuTestErrorCode &errorCode);

    UnicodeString fileLine;
    int32_t fileLineNumber;
    UnicodeString fileTestName;
    CollationDataBuilder *baseBuilder;
    CollationData *baseData;
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
    TESTCASE_AUTO(TestNulTerminated);
    TESTCASE_AUTO(TestHiragana);
    TESTCASE_AUTO(TestDataDriven);
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

    static const UChar s[2] = { 0xfffe, 0xffff };
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

void CollationTest::TestNulTerminated() {
    IcuTestErrorCode errorCode(*this, "TestNulTerminated");

    CollationDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationData> cd(builder.build(errorCode));
    if(errorCode.logIfFailureAndReset("CollationDataBuilder.build()")) {
        return;
    }

    static const UChar s[] = { 0x61, 0x62, 0x61, 0x62, 0 };

    UTF16CollationIterator ci1(cd.getAlias(), 0, s, s + 2);
    UTF16CollationIterator ci2(cd.getAlias(), 0, s + 2, NULL);
    for(int32_t i = 0;; ++i) {
        int64_t ce1 = ci1.nextCE(errorCode);
        int64_t ce2 = ci2.nextCE(errorCode);
        if(errorCode.logIfFailureAndReset("CollationIterator.nextCE()")) {
            return;
        }
        if(ce1 != ce2) {
            errln("CollationIterator.nextCE(with length) != nextCE(NUL-terminated) at CE %d", (int)i);
            break;
        }
        if(ce1 == Collation::NO_CE) { break; }
    }
}

void CollationTest::TestHiragana() {
    IcuTestErrorCode errorCode(*this, "TestHiragana");

    CollationDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationData> cd(builder.build(errorCode));
    if(errorCode.logIfFailureAndReset("CollationDataBuilder.build()")) {
        return;
    }

    static const UChar s[] = { 0x61, 0x309a, 0x304b, 0x3099, 0x7a };
    static const int8_t hira[] = { 0, -1, 1, -1, 0 };

    UTF16CollationIterator ci(cd.getAlias(), 0, s, s + LENGTHOF(s));
    UBool expectAnyHiragana = FALSE;
    for(int32_t i = 0; i < LENGTHOF(s); ++i) {
        ci.nextCE(errorCode);
        if(ci.getHiragana() != hira[i]) {
            errln("CollationIterator.nextCE(U+%04lx).getHiragana()=%d != %d",
                  (long)s[i], ci.getHiragana(), hira[i]);
        }
        if(ci.getHiragana() > 0) { expectAnyHiragana = TRUE; }
        if(ci.getAnyHiragana() != expectAnyHiragana) {
            errln("CollationIterator.nextCE(U+%04lx).getAnyHiragana()=%d != %d",
                  (long)s[i], ci.getAnyHiragana(), expectAnyHiragana);
        }
    }
}

UBool CollationTest::readLine(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    int32_t lineLength;
    const UChar *line = ucbuf_readline(f, &lineLength, errorCode);
    if(line == NULL || errorCode.isFailure()) { return FALSE; }
    ++fileLineNumber;
    // Strip trailing CR/LF, comments, and spaces.
    const UChar *comment = u_memchr(line, 0x23, lineLength);  // '#'
    if(comment != NULL) {
        lineLength = (int32_t)(comment - line);
    } else {
        while(lineLength > 0 && isCROrLF(line[lineLength - 1])) { --lineLength; }
    }
    while(lineLength > 0 && isSpace(line[lineLength - 1])) { --lineLength; }
    fileLine.setTo(FALSE, line, lineLength);
    return TRUE;
}

uint32_t CollationTest::parseHex(int32_t &start, int32_t maxBytes, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    int32_t limit = start + 2 * maxBytes;
    uint32_t value = 0;
    int32_t i;
    for(i = start; i < limit; ++i) {
        UChar c = fileLine[i];
        uint32_t digit;
        if(0x30 <= c && c <= 0x39) {
            digit = c - 0x30;
        } else if(0x61 <= c && c <= 0x66) {
            digit = c - (0x61 - 10);
        } else if(0x41 <= c && c <= 0x46) {
            digit = c - (0x41 - 10);
        } else {
            break;
        }
        value = (value << 4) | digit;
    }
    int32_t length = i - start;
    if(length < 2 || (length & 1) != 0) {
        errln("error parsing hex value at line %d index %d", (int)fileLineNumber, (int)start);
        errln(fileLine);
        errorCode = U_PARSE_ERROR;
        return 0;
    }
    start = i;
    return value;
}

int64_t CollationTest::parseCE(int32_t &start, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    int64_t ce = 0;
    if(isHexDigit(fileLine[start])) {
        int32_t oldStart = start;
        uint32_t p = parseHex(start, 4, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        ce = (int64_t)p << (64 - (start - oldStart) * 4);
    }
    if(fileLine[start] != 0x2e) {
        errln("primary weight missing dot at line %d index %d", (int)fileLineNumber, (int)start);
        errln(fileLine);
        errorCode = U_PARSE_ERROR;
        return 0;
    }
    ++start;

    if(isHexDigit(fileLine[start])) {
        int32_t oldStart = start;
        uint32_t s = parseHex(start, 2, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        ce |= s << (32 - (start - oldStart) * 4);
    } else if(ce != 0) {
        ce |= Collation::COMMON_SECONDARY_CE;
    }
    if(fileLine[start] != 0x2e) {
        errln("secondary weight missing dot at line %d index %d", (int)fileLineNumber, (int)start);
        errln(fileLine);
        errorCode = U_PARSE_ERROR;
        return 0;
    }
    ++start;

    if(isHexDigit(fileLine[start])) {
        int32_t oldStart = start;
        uint32_t t = parseHex(start, 2, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        ce |= t << (16 - (start - oldStart) * 4);
    } else if(ce != 0) {
        ce |= Collation::COMMON_TERTIARY_CE;
    }
    return ce;
}

void CollationTest::parseString(int32_t &start, UnicodeString &prefix, UnicodeString &s,
                                UErrorCode &errorCode) {
    int32_t length = fileLine.length();
    int32_t i;
    for(i = start; i < length && !isSpace(fileLine[i]); ++i) {}
    int32_t pipeIndex = fileLine.indexOf((UChar)0x7c, start, i - start);  // '|'
    if(pipeIndex >= 0) {
        prefix = fileLine.tempSubStringBetween(start, pipeIndex).unescape();
        if(prefix.isEmpty()) {
            errln("empty prefix on line %d", (int)fileLineNumber);
            errln(fileLine);
            errorCode = U_PARSE_ERROR;
            return;
        }
        start = pipeIndex + 1;
    } else {
        prefix.remove();
    }
    s = fileLine.tempSubStringBetween(start, i).unescape();
    if(s.isEmpty()) {
        errln("empty string on line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode = U_PARSE_ERROR;
        return;
    }
    start = i;
}

int32_t CollationTest::parseStringAndCEs(UnicodeString &prefix, UnicodeString &s,
                                         int64_t ces[], int8_t hira[], int32_t capacity,
                                         IcuTestErrorCode &errorCode) {
    int32_t start = 0;
    parseString(start, prefix, s, errorCode);
    if(errorCode.isFailure()) { return 0; }
    int32_t cesLength = 0;
    for(;;) {
        if(start == fileLine.length()) {
            if(cesLength == 0) {
                errln("no CEs on line %d", (int)fileLineNumber);
                errln(fileLine);
                errorCode.set(U_PARSE_ERROR);
                return 0;
            }
            break;
        }
        if(!isSpace(fileLine[start])) {
            errln("no spaces separating CEs at line %d index %d", (int)fileLineNumber, (int)start);
            errln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return 0;
        }
        if(cesLength == capacity) {
            errln("too many CEs on line %d", (int)fileLineNumber);
            errln(fileLine);
            errorCode.set(U_BUFFER_OVERFLOW_ERROR);
            return 0;
        }
        start = skipSpaces(start);
        ces[cesLength] = parseCE(start, errorCode);
        if(errorCode.isFailure()) { return 0; }
        if(hira != NULL) {
            if(fileLine[start] == 0x48) {  // 'H' for Hiragana
                hira[cesLength] = 1;
                ++start;
            } else if(fileLine[start] == 0x68) {  // 'h' for inherited Hiragana
                hira[cesLength] = -1;
                ++start;
            } else {
                hira[cesLength] = 0;
            }
        }
        ++cesLength;
    }
    return cesLength;
}

void CollationTest::buildBase(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    delete baseBuilder;
    baseBuilder = new CollationDataBuilder(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("new CollationDataBuilder() failed");
        return;
    }
    baseBuilder->initBase(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("CollationDataBuilder.initBase() failed");
        return;
    }

    UnicodeString prefix, s;
    int64_t ces[32];
    while(readLine(f, errorCode)) {
        if(fileLine.isEmpty()) { continue; }
        if(isSectionStarter(fileLine[0])) { break; }
        int32_t cesLength = parseStringAndCEs(prefix, s, ces, NULL, LENGTHOF(ces), errorCode);
        if(errorCode.isFailure()) { return; }
        baseBuilder->add(prefix, s, ces, cesLength, errorCode);
        if(errorCode.isFailure()) {
            errln(fileTestName);
            errln("CollationDataBuilder.add() failed");
            errln(fileLine);
            return;
        }
    }
    if(errorCode.isFailure()) { return; }
    delete baseData;
    baseData = baseBuilder->build(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("CollationDataBuilder.build() failed");
    }
}

void CollationTest::checkCEs(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    // TODO: tailoring vs. baseData
    // TODO: optional FCD

    UnicodeString prefix, s;
    int64_t ces[32];
    int8_t hira[32];
    while(readLine(f, errorCode)) {
        if(fileLine.isEmpty()) { continue; }
        if(isSectionStarter(fileLine[0])) { break; }
        int32_t cesLength = parseStringAndCEs(prefix, s, ces, hira, LENGTHOF(ces), errorCode);
        if(errorCode.isFailure()) { return; }
        int32_t prefixLength = prefix.length();
        if(prefixLength != 0) { s.insert(0, prefix); }
        const UChar *buffer = s.getBuffer();
        // TODO: set flags
        // TODO: test NUL-termination if s does not contain a NUL
        UTF16CollationIterator ci(baseData, 0, buffer + prefixLength, buffer + s.length());
        for(int32_t i = 0;; ++i) {
            int64_t ce = ci.nextCE(errorCode);
            int64_t expected = (i < cesLength) ? ces[i] : Collation::NO_CE;
            if(ce != expected) {
                errln(fileTestName);
                errln("line %d CE index %d: CollationIterator.nextCE()=%lx != %lx",
                      (int)fileLineNumber, (int)i, (long)ce, (long)expected);
                errln(fileLine);
                break;
            }
            int8_t h = ci.getHiragana();
            int8_t expectedHira = (i < cesLength) ? hira[i] : 0;
            if(h != expectedHira) {
                errln(fileTestName);
                errln("line %d CE index %d: CollationIterator.getHiragana()=%d != %d",
                      (int)fileLineNumber, (int)i, h, expectedHira);
                errln(fileLine);
                break;
            }
            if(ce == Collation::NO_CE) { break; }
        }
    }
}

void CollationTest::TestDataDriven() {
    IcuTestErrorCode errorCode(*this, "TestDataDriven");

    CharString path(getSourceTestData(errorCode), errorCode);
    path.append("collationtest.txt", errorCode);
    const char *codePage = "UTF-8";
    LocalUCHARBUFPointer f(ucbuf_open(path.data(), &codePage, TRUE, FALSE, errorCode));
    if(errorCode.logIfFailureAndReset("ucbuf_open(collationtest.txt)")) {
        return;
    }
    while(errorCode.isSuccess()) {
        // Read a new line if necessary.
        // Sub-parsers leave the first line set that they do not handle.
        if(fileLine.isEmpty()) {
            if(!readLine(f.getAlias(), errorCode)) { break; }
            continue;
        }
        if(!isSectionStarter(fileLine[0])) {
            errln("syntax error on line %d", (int)fileLineNumber);
            errln(fileLine);
            return;
        }
        if(fileLine.startsWith(UNICODE_STRING("** test: ", 9))) {
            fileTestName = fileLine;
            logln(fileLine);
            fileLine.remove();
        } else if(fileLine == UNICODE_STRING("@ rawbase", 9)) {
            buildBase(f.getAlias(), errorCode);
        } else if(fileLine == UNICODE_STRING("* CEs", 5)) {
            checkCEs(f.getAlias(), errorCode);
        } else {
            errln("syntax error on line %d", (int)fileLineNumber);
            errln(fileLine);
            return;
        }
    }
}
