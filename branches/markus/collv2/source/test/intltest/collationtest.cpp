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

#include <stdio.h>  // TODO: remove
#include "unicode/utypes.h"
#include "unicode/coll.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "charstr.h"
#include "collation.h"
#include "collationbasedatabuilder.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationiterator.h"
#include "intltest.h"
#include "normalizer2impl.h"
#include "rulebasedcollator.h"
#include "ucbuf.h"
#include "utf16collationiterator.h"
#include "writesrc.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

// TODO: Move to ucbuf.h
U_DEFINE_LOCAL_OPEN_POINTER(LocalUCHARBUFPointer, UCHARBUF, ucbuf_close);

extern const CollationBaseData *getCollationBaseData(UErrorCode &errorCode);

class CollationTest : public IntlTest {
public:
    CollationTest()
            : fcd(NULL),
              fileLineNumber(0),
              baseBuilder(NULL), baseData(NULL),
              collData(NULL),
              coll(NULL) {}

    ~CollationTest() {
        delete baseBuilder;
        delete baseData;
        delete coll;
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);

    void TestMinMax();
    void TestImplicits();
    void TestNulTerminated();
    void TestFCD();
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
                              int64_t ces[], int32_t capacity,
                              IcuTestErrorCode &errorCode);
    int32_t parseRelationAndString(UnicodeString &s, IcuTestErrorCode &errorCode);
    void parseAndSetAttribute(IcuTestErrorCode &errorCode);
    void buildBase(UCHARBUF *f, IcuTestErrorCode &errorCode);
    void replaceCollator(IcuTestErrorCode &errorCode);

    UBool needsNormalization(const UnicodeString &s, UErrorCode &errorCode) const;

    UBool checkCEs(CollationIterator &ci, const char *mode,
                   int64_t ces[], int32_t cesLength,
                   IcuTestErrorCode &errorCode);
    UBool checkCEsNormal(const UnicodeString &s,
                         int64_t ces[], int32_t cesLength,
                         IcuTestErrorCode &errorCode);
    UBool checkCEsFCD(const UnicodeString &s,
                      int64_t ces[], int32_t cesLength,
                      IcuTestErrorCode &errorCode);
    void checkCEs(UCHARBUF *f, IcuTestErrorCode &errorCode);

    UBool checkCompareTwo(const UnicodeString &prevFileLine,
                          const UnicodeString &prevString, const UnicodeString &s,
                          UCollationResult expectedOrder, UColAttributeValue expectedStrength,
                          IcuTestErrorCode &errorCode);
    void checkCompareStrings(UCHARBUF *f, IcuTestErrorCode &errorCode);

    const Normalizer2 *fcd;
    UnicodeString fileLine;
    int32_t fileLineNumber;
    UnicodeString fileTestName;
    CollationBaseDataBuilder *baseBuilder;
    CollationBaseData *baseData;
    const CollationData *collData;
    Collator *coll;
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
    TESTCASE_AUTO(TestFCD);
    TESTCASE_AUTO(TestDataDriven);
    TESTCASE_AUTO_END;
}

void CollationTest::TestMinMax() {
    IcuTestErrorCode errorCode(*this, "TestMinMax");

    CollationBaseDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationBaseData> cd(builder.buildBaseData(errorCode));
    if(errorCode.logIfFailureAndReset("CollationBaseDataBuilder.buildBaseData()")) {
        return;
    }

    static const UChar s[2] = { 0xfffe, 0xffff };
    UTF16CollationIterator ci(cd.getAlias(), s, s + 2);
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

    const CollationBaseData *cd = getCollationBaseData(errorCode);
    if(errorCode.logIfFailureAndReset("getCollationBaseData()")) {
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
    UnicodeSet unassigned("[[:Cn:][:Cs:][:Co:]]", errorCode);
    unassigned.remove(0xfffe, 0xffff);
    if(errorCode.logIfFailureAndReset("UnicodeSet")) {
        return;
    }
    const UnicodeSet *sets[] = { &coreHan, &otherHan, &unassigned };
    UChar32 prev = 0;
    uint32_t prevPrimary = 0;
    UTF16CollationIterator ci(cd, NULL, NULL);
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

    CollationBaseDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationBaseData> cd(builder.buildBaseData(errorCode));
    if(errorCode.logIfFailureAndReset("CollationBaseDataBuilder.buildBaseData()")) {
        return;
    }

    static const UChar s[] = { 0x61, 0x62, 0x61, 0x62, 0 };

    UTF16CollationIterator ci1(cd.getAlias(), s, s + 2);
    UTF16CollationIterator ci2(cd.getAlias(), s + 2, NULL);
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

class CodePointIterator {
public:
    CodePointIterator(const UChar32 *cp, int32_t length) : cp(cp), length(length), pos(0) {}
    UChar32 next() { return (pos < length) ? cp[pos++] : U_SENTINEL; }
    UChar32 previous() { return (pos > 0) ? cp[--pos] : U_SENTINEL; }
    int getIndex() const { return (int)pos; }
private:
    const UChar32 *cp;
    int32_t length;
    int32_t pos;
};

void CollationTest::TestFCD() {
    IcuTestErrorCode errorCode(*this, "TestFCD");

    CollationBaseDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    LocalPointer<CollationBaseData> cd(builder.buildBaseData(errorCode));
    if(errorCode.logIfFailureAndReset("CollationBaseDataBuilder.buildBaseData()")) {
        return;
    }

    // Input string, not FCD, NUL-terminated.
    static const UChar s[] = {
        0x308, 0xe1, 0x62, 0x301, 0x327, 0x430, 0x62,
        U16_LEAD(0x1D15F), U16_TRAIL(0x1D15F),  // MUSICAL SYMBOL QUARTER NOTE=1D158 1D165, ccc=0, 216
        0x327, 0x308,  // ccc=202, 230
        U16_LEAD(0x1D16D), U16_TRAIL(0x1D16D),  // MUSICAL SYMBOL COMBINING AUGMENTATION DOT, ccc=226
        U16_LEAD(0x1D15F), U16_TRAIL(0x1D15F),
        U16_LEAD(0x1D16D), U16_TRAIL(0x1D16D),
        0xac01,
        0xe7,  // Character with tccc!=0 decomposed together with mis-ordered sequence.
        U16_LEAD(0x1D16D), U16_TRAIL(0x1D16D), U16_LEAD(0x1D165), U16_TRAIL(0x1D165),
        0xe1,  // Character with tccc!=0 decomposed together with decomposed sequence.
        0xf73, 0xf75,  // Tibetan composite vowels must be decomposed.
        0x4e00, 0xf81,
        0
    };
    // Expected code points.
    static const UChar32 cp[] = {
        0x308, 0xe1, 0x62, 0x327, 0x301, 0x430, 0x62,
        0x1D158, 0x327, 0x1D165, 0x1D16D, 0x308,
        0x1D15F, 0x1D16D,
        0xac01,
        0x63, 0x327, 0x1D165, 0x1D16D,
        0x61,
        0xf71, 0xf71, 0xf72, 0xf74, 0x301,
        0x4e00, 0xf71, 0xf80
    };

    FCDUTF16CollationIterator ci(cd.getAlias(), s, NULL, errorCode);
    if(errorCode.logIfFailureAndReset("FCDUTF16CollationIterator constructor")) {
        return;
    }
    CodePointIterator cpi(cp, LENGTHOF(cp));
    // Iterate forward to the NUL terminator.
    for(;;) {
        UChar32 c1 = ci.nextCodePoint(errorCode);
        UChar32 c2 = cpi.next();
        if(c1 != c2) {
            errln("FCDUTF16CollationIterator.nextCodePoint(NUL-terminated) = U+%04lx != U+%04lx at %d",
                  (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }

    // Iterate backward most of the way.
    for(int32_t n = (LENGTHOF(cp) * 2) / 3; n > 0; --n) {
        UChar32 c1 = ci.previousCodePoint(errorCode);
        UChar32 c2 = cpi.previous();
        if(c1 != c2) {
            errln("FCDUTF16CollationIterator.previousCodePoint() = U+%04lx != U+%04lx at %d",
                  (long)c1, (long)c2, cpi.getIndex());
            return;
        }
    }

    // Forward again.
    for(;;) {
        UChar32 c1 = ci.nextCodePoint(errorCode);
        UChar32 c2 = cpi.next();
        if(c1 != c2) {
            errln("FCDUTF16CollationIterator.nextCodePoint(to limit) = U+%04lx != U+%04lx at %d",
                  (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }

    // Iterate backward to the start.
    for(int32_t n = (LENGTHOF(cp) * 2) / 3; n > 0; --n) {
        UChar32 c1 = ci.previousCodePoint(errorCode);
        UChar32 c2 = cpi.previous();
        if(c1 != c2) {
            errln("FCDUTF16CollationIterator.previousCodePoint(to start) = U+%04lx != U+%04lx at %d",
                  (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
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
                                         int64_t ces[], int32_t capacity,
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
        ++cesLength;
    }
    return cesLength;
}

int32_t CollationTest::parseRelationAndString(UnicodeString &s, IcuTestErrorCode &errorCode) {
    int32_t relation;
    int32_t start;
    if(fileLine[0] == 0x3c) {  // <
        UChar second = fileLine[1];
        if(0x31 <= second && second <= 0x34) {  // <1..<4
            relation = second - 0x31;  // UCOL_PRIMARY..UCOL_QUATERNARY
            start = 2;
        } else if(second == 0x63) {  // <c
            relation = UCOL_LOWER_FIRST;  // case level difference
            start = 2;
        } else if(second == 0x69) {  // <i
            relation = UCOL_IDENTICAL;
            start = 2;
        } else {  // just <
            relation = UCOL_STRENGTH_LIMIT;
            start = 1;
        }
    } else if(fileLine[0] == 0x3d) {  // =
        relation = UCOL_DEFAULT;
        start = 1;
    } else {
        start = 0;
    }
    if(start == 0 || !isSpace(fileLine[start])) {
        errln("no relation (= < <1 <2 <c <3 <4 <i) at beginning of line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return 0;
    }
    start = skipSpaces(start);
    UnicodeString prefix;
    parseString(start, prefix, s, errorCode);
    if(errorCode.isSuccess() && !prefix.isEmpty()) {
        errln("prefix string not allowed for test string: on line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return 0;
    }
    if(start < fileLine.length()) {
        errln("unexpected line contents after test string on line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return 0;
    }
    return relation;
}

static const struct {
    const char *name;
    UColAttribute attr;
} attributes[] = {
    { "backwards", UCOL_FRENCH_COLLATION },
    { "alternate", UCOL_ALTERNATE_HANDLING },
    { "caseFirst", UCOL_CASE_FIRST },
    { "caseLevel", UCOL_CASE_LEVEL },
    // UCOL_NORMALIZATION_MODE is turned on and off automatically.
    { "strength", UCOL_STRENGTH },
    // UCOL_HIRAGANA_QUATERNARY_MODE is deprecated.
    { "numeric", UCOL_NUMERIC_COLLATION }
};

static const struct {
    const char *name;
    UColAttributeValue value;
} attributeValues[] = {
    { "default", UCOL_DEFAULT },
    { "primary", UCOL_PRIMARY },
    { "secondary", UCOL_SECONDARY },
    { "tertiary", UCOL_TERTIARY },
    { "quaternary", UCOL_QUATERNARY },
    { "identical", UCOL_IDENTICAL },
    { "off", UCOL_OFF },
    { "on", UCOL_ON },
    { "shifted", UCOL_SHIFTED },
    { "non-ignorable", UCOL_NON_IGNORABLE },
    { "lower", UCOL_LOWER_FIRST },
    { "upper", UCOL_UPPER_FIRST }
};

void CollationTest::parseAndSetAttribute(IcuTestErrorCode &errorCode) {
    int32_t start = skipSpaces(1);
    int32_t equalPos = fileLine.indexOf(0x3d);
    // TODO: handle  % reorder Grek Zzzz digit
    // TODO: handle  % maxVariable symbol
    if(equalPos < 0) {
        errln("missing '=' on line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return;
    }

    UnicodeString attrString = fileLine.tempSubStringBetween(start, equalPos);
    UColAttribute attr;
    for(int32_t i = 0;; ++i) {
        if(i == LENGTHOF(attributes)) {
            errln("invalid attribute name on line %d", (int)fileLineNumber);
            errln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return;
        }
        if(attrString == UnicodeString(attributes[i].name, -1, US_INV)) {
            attr = attributes[i].attr;
            break;
        }
    }

    UnicodeString valueString = fileLine.tempSubString(equalPos+1);
    UColAttributeValue value;
    for(int32_t i = 0;; ++i) {
        if(i == LENGTHOF(attributeValues)) {
            errln("invalid attribute value name on line %d", (int)fileLineNumber);
            errln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return;
        }
        if(valueString == UnicodeString(attributeValues[i].name, -1, US_INV)) {
            value = attributeValues[i].value;
            break;
        }
    }

    coll->setAttribute(attr, value, errorCode);
    if(errorCode.isFailure()) {
        errln("illegal attribute=value combination on line %d: %s",
              (int)fileLineNumber, errorCode.errorName());
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return;
    }
    fileLine.remove();
}

void CollationTest::buildBase(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    delete baseBuilder;
    baseBuilder = new CollationBaseDataBuilder(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("new CollationBaseDataBuilder() failed");
        return;
    }
    baseBuilder->initBase(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("CollationBaseDataBuilder.initBase() failed");
        return;
    }

    UnicodeString prefix, s;
    int64_t ces[32];
    while(readLine(f, errorCode)) {
        if(fileLine.isEmpty()) { continue; }
        if(isSectionStarter(fileLine[0])) { break; }
// printf("-- buildBase line %d\n", (int)fileLineNumber);  // TODO: remove
        int32_t cesLength = parseStringAndCEs(prefix, s, ces, LENGTHOF(ces), errorCode);
        if(errorCode.isFailure()) { return; }
        baseBuilder->add(prefix, s, ces, cesLength, errorCode);
        if(errorCode.isFailure()) {
            errln(fileTestName);
            errln("CollationBaseDataBuilder.add() failed");
            errln(fileLine);
            return;
        }
    }
    if(errorCode.isFailure()) { return; }
    delete baseData;
    baseData = baseBuilder->buildBaseData(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("CollationBaseDataBuilder.buildBaseData() failed");
    }
    collData = baseData;
}

void CollationTest::replaceCollator(IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    Collator *newColl = new RuleBasedCollator2(collData);
    if(newColl == NULL) {
        errln("unable to allocate a new collator");
        errorCode.set(U_MEMORY_ALLOCATION_ERROR);
        return;
    }
    delete coll;
    coll = newColl;
}

UBool CollationTest::needsNormalization(const UnicodeString &s, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode) || !fcd->isNormalized(s, errorCode)) { return TRUE; }
    // In some sequences with Tibetan composite vowel signs,
    // even if the string passes the FCD check,
    // those composites must be decomposed.
    // Check if s contains 0F71 immediately followed by 0F73 or 0F75 or 0F81.
    int32_t index = 0;
    while((index = s.indexOf((UChar)0xf71, index)) >= 0) {
        if(++index < s.length()) {
            UChar c = s[index];
            if(c == 0xf73 || c == 0xf75 || c == 0xf81) { return TRUE; }
        }
    }
    return FALSE;
}

UBool CollationTest::checkCEs(CollationIterator &ci, const char *mode,
                              int64_t ces[], int32_t cesLength,
                              IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return FALSE; }
    for(int32_t i = 0;; ++i) {
        int64_t ce = ci.nextCE(errorCode);
        if(errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d CE index %d: %s CollationIterator.nextCE() failed: %s",
                  (int)fileLineNumber, (int)i, mode, errorCode.errorName());
            errln(fileLine);
            errorCode.reset();
            return FALSE;
        }
        int64_t expected = (i < cesLength) ? ces[i] : Collation::NO_CE;
        if(ce != expected) {
            errln(fileTestName);
            errln("line %d CE index %d: %s CollationIterator.nextCE()=%lx != %lx",
                  (int)fileLineNumber, (int)i, mode, (long)ce, (long)expected);
            errln(fileLine);
            return FALSE;
        }
        if(ce == Collation::NO_CE) { return TRUE; }
    }
}

UBool CollationTest::checkCEsNormal(const UnicodeString &s,
                                    int64_t ces[], int32_t cesLength,
                                    IcuTestErrorCode &errorCode) {
    const UChar *buffer = s.getBuffer();
    UTF16CollationIterator ci(collData, buffer, buffer + s.length());
    if(!checkCEs(ci, "UTF-16", ces, cesLength, errorCode)) { return FALSE; }
    // Test NUL-termination if s does not contain a NUL.
    if(s.indexOf((UChar)0) >= 0) { return TRUE; }
    UTF16CollationIterator ci0(collData, buffer, NULL);
    return checkCEs(ci0, "UTF-16-NUL", ces, cesLength, errorCode);
}

UBool CollationTest::checkCEsFCD(const UnicodeString &s,
                                 int64_t ces[], int32_t cesLength,
                                 IcuTestErrorCode &errorCode) {
    const UChar *buffer = s.getBuffer();
    FCDUTF16CollationIterator ci(collData, buffer, buffer + s.length(), errorCode);
    if(!checkCEs(ci, "FCD-UTF-16", ces, cesLength, errorCode)) { return FALSE; }
    // Test NUL-termination if s does not contain a NUL.
    if(s.indexOf((UChar)0) >= 0) { return TRUE; }
    FCDUTF16CollationIterator ci0(collData, buffer, NULL, errorCode);
    return checkCEs(ci0, "FCD-UTF-16-NUL", ces, cesLength, errorCode);
}

void CollationTest::checkCEs(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    UnicodeString prefix, s;
    int64_t ces[32];
    while(readLine(f, errorCode)) {
        if(fileLine.isEmpty()) { continue; }
        if(isSectionStarter(fileLine[0])) { break; }
        int32_t cesLength = parseStringAndCEs(prefix, s, ces, LENGTHOF(ces), errorCode);
        if(errorCode.isFailure()) {
            errorCode.reset();
            continue;
        }
        if(!prefix.isEmpty()) {
            errln("prefix string not allowed for test string: on line %d", (int)fileLineNumber);
            errln(fileLine);
            continue;
        }
        s.getTerminatedBuffer();  // Ensure NUL-termination.
        if(!needsNormalization(s, errorCode)) {
            if(!checkCEsNormal(s, ces, cesLength, errorCode)) { continue; }
        }
        checkCEsFCD(s, ces, cesLength, errorCode);
    }
}

UBool CollationTest::checkCompareTwo(const UnicodeString &prevFileLine,
                                     const UnicodeString &prevString, const UnicodeString &s,
                                     UCollationResult expectedOrder, UColAttributeValue /*expectedStrength*/,
                                     IcuTestErrorCode &errorCode) {
    const char *norm =
        (coll->getAttribute(UCOL_NORMALIZATION_MODE, errorCode) == UCOL_ON) ?
        "on" : "off";
    if(errorCode.isFailure()) { return FALSE; }
// TODO: remove -- printf("line %d coll(%s).compare(prev, s)\n", fileLineNumber, norm);
    UCollationResult order = coll->compare(prevString, s, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compare(previous, current) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errorCode.reset();
        return FALSE;
    }
// TODO: remove -- printf("line %d coll(%s).compare(s, prev)\n", fileLineNumber, norm);
    order = coll->compare(s, prevString, errorCode);
    if(order != -expectedOrder || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compare(current, previous) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, -expectedOrder, errorCode.errorName());
        errln(fileLine);
        errln(prevFileLine);
        errorCode.reset();
        return FALSE;
    }
    // Test NUL-termination if the strings do not contain NUL characters.
    UBool containNUL = prevString.indexOf((UChar)0) >= 0 || s.indexOf((UChar)0) >= 0;
    if(!containNUL) {
// TODO: remove -- printf("line %d coll(%s).compare(prev-NUL, s-NUL)\n", fileLineNumber, norm);
        UCollationResult order = coll->compare(prevString.getBuffer(), -1, s.getBuffer(), -1, errorCode);
        if(order != expectedOrder || errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).compare(previous-NUL, current-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
            errln(prevFileLine);
            errln(fileLine);
            errorCode.reset();
            return FALSE;
        }
// TODO: remove -- printf("line %d coll(%s).compare(s-NUL, prev-NUL)\n", fileLineNumber, norm);
        order = coll->compare(s.getBuffer(), -1, prevString.getBuffer(), -1, errorCode);
        if(order != -expectedOrder || errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).compare(current-NUL, previous-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, -expectedOrder, errorCode.errorName());
            errln(fileLine);
            errln(prevFileLine);
            errorCode.reset();
            return FALSE;
        }
    }
    // TODO: test getSortKey(), check expectedStrength (UCOL_STRENGTH_LIMIT is unspecified)
    // TODO: test getSortKeyPart()
    return TRUE;
}

void CollationTest::checkCompareStrings(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    UnicodeString prevFileLine = UNICODE_STRING("(none)", 6);
    UnicodeString prevString, s;
    prevString.getTerminatedBuffer();  // Ensure NUL-termination.
    while(readLine(f, errorCode)) {
        if(fileLine.isEmpty()) { continue; }
        if(isSectionStarter(fileLine[0])) { break; }
        int32_t relation = parseRelationAndString(s, errorCode);
        if(errorCode.isFailure()) {
            errorCode.reset();
            break;
        }
        UCollationResult expectedOrder = (relation == UCOL_DEFAULT) ? UCOL_EQUAL : UCOL_LESS;
        UColAttributeValue expectedStrength = (UColAttributeValue)relation;
        s.getTerminatedBuffer();  // Ensure NUL-termination.
        UBool isOk = TRUE;
        if(!needsNormalization(prevString, errorCode) && !needsNormalization(s, errorCode)) {
            coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF, errorCode);
            isOk = checkCompareTwo(prevFileLine, prevString, s, expectedOrder, expectedStrength, errorCode);
        }
        if(isOk) {
            coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, errorCode);
            checkCompareTwo(prevFileLine, prevString, s, expectedOrder, expectedStrength, errorCode);
        }
        prevFileLine = fileLine;
        prevString = s;
        prevString.getTerminatedBuffer();  // Ensure NUL-termination.
    }
}

void CollationTest::TestDataDriven() {
    IcuTestErrorCode errorCode(*this, "TestDataDriven");

    fcd = Normalizer2Factory::getFCDInstance(errorCode);
    if(errorCode.logIfFailureAndReset("Normalizer2Factory::getFCDInstance()")) {
        return;
    }

    CharString path(getSourceTestData(errorCode), errorCode);
    path.appendPathPart("collationtest.txt", errorCode);
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
        } else if(fileLine == UNICODE_STRING("@ root", 6)) {
            collData = getCollationBaseData(errorCode);
            replaceCollator(errorCode);
            fileLine.remove();
        } else if(fileLine == UNICODE_STRING("@ rawbase", 9)) {
            buildBase(f.getAlias(), errorCode);
            replaceCollator(errorCode);
        } else if(fileLine[0] == 0x25 && isSpace(fileLine[1])) {  // %
            parseAndSetAttribute(errorCode);
        } else if(fileLine == UNICODE_STRING("* CEs", 5)) {
            checkCEs(f.getAlias(), errorCode);
        } else if(fileLine == UNICODE_STRING("* compare", 9)) {
            checkCompareStrings(f.getAlias(), errorCode);
        } else {
            errln("syntax error on line %d", (int)fileLineNumber);
            errln(fileLine);
            return;
        }
    }
}

// TODO
// Partial, modified copy of genuca.cpp, initially to aid testing.

#include "cstring.h"
#include "ucol_bld.h"
#include "ucol_elm.h"
#include "uparse.h"

static UBool beVerbose = FALSE;

static UVersionInfo UCAVersion;

static UCAElements le;

static uint32_t gVariableTop;  // TODO: store in the base builder, or otherwise make this not be a global variable

// returns number of characters read
static int32_t readElement(char **from, char *to, char separator, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return 0;
    }
    char buffer[1024];
    int32_t i = 0;
    for(;;) {
        char c = **from;
        if(c == separator || (separator == ' ' && c == '\t')) {
            break;
        }
        if (c == '\0') {
            return 0;
        }
        if(c != ' ') {
            *(buffer+i++) = c;
        }
        (*from)++;
    }
    (*from)++;
    *(buffer + i) = 0;
    strcpy(to, buffer);
    return i;
}

/* TODO: remove?!
static int32_t skipUntilWhiteSpace(char **from, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    int32_t count = 0;
    while (**from != ' ' && **from != '\t' && **from != '\0') {
        (*from)++;
        count++;
    }
    return count;
}
*/

static int32_t skipWhiteSpace(char **from, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    int32_t count = 0;
    while (**from == ' ' || **from == '\t') {
        (*from)++;
        count++;
    }
    return count;
}

static uint32_t parseWeight(char *s, uint32_t maxWeight, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || *s == 0) { return 0; }
    if(*s == '0' && s[1] == '0') {
        // leading 0 byte
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    char *end;
    unsigned long weight = uprv_strtoul(s, &end, 16);
    if(end == s || *end != 0 || weight > maxWeight) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    // Left-align the weight.
    if(weight != 0) {
        while(weight <= (maxWeight >> 8)) { weight <<= 8; }
    }
    return (uint32_t)weight;
}

static int64_t getSingleCEValue(const CollationDataBuilder &builder,
                                char *primary, char *secondary, char *tertiary, UErrorCode *status) {
    uint32_t p = parseWeight(primary, 0xffffffff, *status);
    uint32_t s = parseWeight(secondary, 0xffff, *status);
    uint32_t t = parseWeight(tertiary, 0xffff, *status);
    if(U_FAILURE(*status)) {
        return 0;
    }
    // Transform an implicit primary weight.
    if(0xe0000000 <= p && p <= 0xe4ffffff) {
        UChar32 c = uprv_uca_getCodePointFromRaw(uprv_uca_getRawFromImplicit(p));
        U_ASSERT(c >= 0);
        int64_t ce = builder.getSingleCE(c, *status);
// TODO: remove debug code -- printf("implicit p %08lx -> U+%04lx -> new p %08lx\n", (long)p, (long)c, (long)(uint32_t)(ce >> 32));
        p = (uint32_t)(ce >> 32);
    }
    return ((int64_t)p << 32) | (s << 16) | t;
}

static int32_t hex2num(char hex) {
    if(hex>='0' && hex <='9') {
        return hex-'0';
    } else if(hex>='a' && hex<='f') {
        return hex-'a'+10;
    } else if(hex>='A' && hex<='F') {
        return hex-'A'+10;
    } else {
        return 0;
    }
}

int32_t getReorderCode(const char* name) {
    int32_t code = ucol_findReorderingEntry(name);
    if (code >= 0) {
        return code;
    }
    code = u_getPropertyValueEnum(UCHAR_SCRIPT, name);
    if (code >= 0) {
        return code;
    }
    // Exception: The standard name is "PUNCT" but FractionalUCA.txt uses the long form.
    if (0 == uprv_strcmp(name, "PUNCTUATION")) {
        return UCOL_REORDER_CODE_PUNCTUATION;
    }
    return -1;  // Same as UCHAR_INVALID_CODE or USCRIPT_INVALID_CODE.
}

UCAElements *readAnElement(FILE *data,
                           CollationBaseDataBuilder &builder,
                           UCAConstants *consts,
                           int64_t ces[32], int32_t &cesLength,
                           UErrorCode *status) {
    char buffer[2048], primary[100], secondary[100], tertiary[100];
    UChar leadByte[100], scriptCode[100];
    int32_t i = 0;
    char *pointer = NULL;
    char *commentStart = NULL;
    char *startCodePoint = NULL;
    char *endCodePoint = NULL;
    char *result = fgets(buffer, 2048, data);
    int32_t buflen = (int32_t)uprv_strlen(buffer);
    if(U_FAILURE(*status)) {
        return 0;
    }
    *primary = *secondary = *tertiary = '\0';
    *leadByte = *scriptCode = '\0';
    if(result == NULL) {
        if(feof(data)) {
            return NULL;
        } else {
            fprintf(stderr, "empty line but no EOF!\n");
            *status = U_INVALID_FORMAT_ERROR;
            return NULL;
        }
    }
    while(buflen>0 && (buffer[buflen-1] == '\r' || buffer[buflen-1] == '\n')) {
      buffer[--buflen] = 0;
    }

    if(buffer[0] == 0 || buffer[0] == '#') {
        return NULL; // just a comment, skip whole line
    }

    UCAElements *element = &le;
    memset(element, 0, sizeof(*element));

    enum ActionType {
      READCE,
      READPRIMARY,
      READFIRSTPRIMARY,
      READUNIFIEDIDEOGRAPH,
      READUCAVERSION,
      READLEADBYTETOSCRIPTS,
      IGNORE
    };

    // Directives.
    if(buffer[0] == '[') {
      uint32_t cnt = 0;
      static const struct {
        char name[128];
        uint32_t *what;
        ActionType what_to_do;
      } vt[]  = { {"[first tertiary ignorable",  consts->UCA_FIRST_TERTIARY_IGNORABLE,  READCE},
                  {"[last tertiary ignorable",   consts->UCA_LAST_TERTIARY_IGNORABLE,   READCE},
                  {"[first secondary ignorable", consts->UCA_FIRST_SECONDARY_IGNORABLE, READCE},
                  {"[last secondary ignorable",  consts->UCA_LAST_SECONDARY_IGNORABLE,  READCE},
                  {"[first primary ignorable",   consts->UCA_FIRST_PRIMARY_IGNORABLE,   READCE},
                  {"[last primary ignorable",    consts->UCA_LAST_PRIMARY_IGNORABLE,    READCE},
                  {"[first variable",            consts->UCA_FIRST_VARIABLE,            READCE},
                  {"[last variable",             consts->UCA_LAST_VARIABLE,             READCE},
                  {"[first regular",             consts->UCA_FIRST_NON_VARIABLE,        READCE},
                  {"[last regular",              consts->UCA_LAST_NON_VARIABLE,         READCE},
                  {"[first implicit",            consts->UCA_FIRST_IMPLICIT,            READCE},
                  {"[last implicit",             consts->UCA_LAST_IMPLICIT,             READCE},
                  {"[first trailing",            consts->UCA_FIRST_TRAILING,            READCE},
                  {"[last trailing",             consts->UCA_LAST_TRAILING,             READCE},

                  {"[Unified_Ideograph",            NULL, READUNIFIEDIDEOGRAPH},
                  {"[first_primary",                NULL, READFIRSTPRIMARY},

                  {"[fixed top",                    NULL, IGNORE},
                  {"[fixed first implicit byte",    NULL, IGNORE},
                  {"[fixed last implicit byte",     NULL, IGNORE},
                  {"[fixed first trail byte",       NULL, IGNORE},
                  {"[fixed last trail byte",        NULL, IGNORE},
                  {"[fixed first special byte",     NULL, IGNORE},
                  {"[fixed last special byte",      NULL, IGNORE},
                  {"[variable top = ",              &gVariableTop,                      READPRIMARY},
                  {"[UCA version = ",               NULL,                               READUCAVERSION},
                  {"[top_byte",                     NULL,                               READLEADBYTETOSCRIPTS},
                  {"[reorderingTokens",             NULL,                               IGNORE},
                  {"[categories",                   NULL,                               IGNORE},
                  // TODO: Do not ignore these -- they are important for tailoring,
                  // for creating well-formed Collation Element Tables.
                  {"[first tertiary in secondary non-ignorable",                 NULL,                               IGNORE},
                  {"[last tertiary in secondary non-ignorable",                 NULL,                               IGNORE},
                  {"[first secondary in primary non-ignorable",                 NULL,                               IGNORE},
                  {"[last secondary in primary non-ignorable",                 NULL,                               IGNORE},
      };
      for (cnt = 0; cnt<LENGTHOF(vt); cnt++) {
        uint32_t vtLen = (uint32_t)uprv_strlen(vt[cnt].name);
        if(uprv_strncmp(buffer, vt[cnt].name, vtLen) == 0) {
            ActionType what_to_do = vt[cnt].what_to_do;
            if (what_to_do == IGNORE) { //vt[cnt].what_to_do == IGNORE
                return NULL;
            } else if(what_to_do == READPRIMARY) {
              pointer = buffer+vtLen;
              readElement(&pointer, primary, ']', status);
              *(vt[cnt].what) = parseWeight(primary, 0xffffffff, *status);
              if(U_FAILURE(*status)) {
                  fprintf(stderr, "Value of \"%s\" is not a primary weight\n", buffer);
                  return NULL;
              }
            } else if(what_to_do == READFIRSTPRIMARY) {
                pointer = buffer+vtLen;
                skipWhiteSpace(&pointer, status);
                UBool firstInGroup = FALSE;
                if(uprv_strncmp(pointer, "group", 5) == 0) {
                    firstInGroup = TRUE;
                    pointer += 5;
                    skipWhiteSpace(&pointer, status);
                }
                char scriptName[100];
                int32_t length = readElement(&pointer, scriptName, ' ', status);
                if (length <= 0) {
                    fprintf(stderr, "Syntax error: unable to parse reorder code from line '%s'\n", buffer);
                    *status = U_INVALID_FORMAT_ERROR;
                    return NULL;
                }
                int32_t reorderCode = getReorderCode(scriptName);
                if (reorderCode < 0) {
                    fprintf(stderr, "Syntax error: unable to parse reorder code from line '%s'\n", buffer);
                    *status = U_INVALID_FORMAT_ERROR;
                    return NULL;
                }
                if (CollationBaseData::scriptByteFromInt(reorderCode) < 0) {
                    fprintf(stderr, "reorder code %d for \"%s\" cannot be encoded by CollationBaseData::scriptByteFromInt(reorderCode)\n",
                            reorderCode, scriptName);
                    *status = U_INTERNAL_PROGRAM_ERROR;
                    return NULL;
                }
                readElement(&pointer, primary, ']', status);
                uint32_t weight = parseWeight(primary, 0xffffffff, *status);
                if(U_FAILURE(*status)) {
                    fprintf(stderr, "Value of \"%s\" is not a primary weight\n", buffer);
                    return NULL;
                }
                builder.addFirstPrimary(reorderCode, firstInGroup, weight, *status);
                if(U_FAILURE(*status)) {
                    fprintf(stderr, "Failure setting data for \"%s\" - %s -- [first_primary] data out of order?\n",
                            buffer, u_errorName(*status));
                    return NULL;
                }
            } else if(what_to_do == READUNIFIEDIDEOGRAPH) {
                pointer = buffer+vtLen;
                UVector32 unihan(*status);
                if(U_FAILURE(*status)) { return NULL; }
                for(;;) {
                    skipWhiteSpace(&pointer, status);
                    if(*pointer == ']') { break; }
                    if(*pointer == 0) {
                        // Missing ] after ranges.
                        *status = U_INVALID_FORMAT_ERROR;
                        return NULL;
                    }
                    char *s = pointer;
                    while(*s != ' ' && *s != '\t' && *s != ']' && *s != '\0') { ++s; }
                    char c = *s;
                    *s = 0;
                    uint32_t start, end;
                    u_parseCodePointRange(pointer, &start, &end, status);
                    *s = c;
                    if(U_FAILURE(*status)) {
                        fprintf(stderr, "Syntax error: unable to parse one of the ranges from line '%s'\n", buffer);
                        *status = U_INVALID_FORMAT_ERROR;
                        return NULL;
                    }
                    unihan.addElement((UChar32)start, *status);
                    unihan.addElement((UChar32)end, *status);
                    pointer = s;
                }
                builder.initHanRanges(unihan.getBuffer(), unihan.size(), *status);
            } else if (what_to_do == READCE) {
              // TODO: combine & clean up the two CE parsers
#if 0  // TODO
              pointer = strchr(buffer+vtLen, '[');
              if(pointer) {
                pointer++;
                element->sizePrim[0]=readElement(&pointer, primary, ',', status) / 2;
                element->sizeSec[0]=readElement(&pointer, secondary, ',', status) / 2;
                element->sizeTer[0]=readElement(&pointer, tertiary, ']', status) / 2;
                vt[cnt].what[0] = getSingleCEValue(primary, secondary, tertiary, status);
                if(element->sizePrim[0] > 2 || element->sizeSec[0] > 1 || element->sizeTer[0] > 1) {
                  uint32_t CEi = 1;
                  uint32_t value = UCOL_CONTINUATION_MARKER; /* Continuation marker */
                    if(2*CEi<element->sizePrim[i]) {
                        value |= ((hex2num(*(primary+4*CEi))&0xF)<<28);
                        value |= ((hex2num(*(primary+4*CEi+1))&0xF)<<24);
                    }

                    if(2*CEi+1<element->sizePrim[i]) {
                        value |= ((hex2num(*(primary+4*CEi+2))&0xF)<<20);
                        value |= ((hex2num(*(primary+4*CEi+3))&0xF)<<16);
                    }

                    if(CEi<element->sizeSec[i]) {
                        value |= ((hex2num(*(secondary+2*CEi))&0xF)<<12);
                        value |= ((hex2num(*(secondary+2*CEi+1))&0xF)<<8);
                    }

                    if(CEi<element->sizeTer[i]) {
                        value |= ((hex2num(*(tertiary+2*CEi))&0x3)<<4);
                        value |= (hex2num(*(tertiary+2*CEi+1))&0xF);
                    }

                    CEi++;

                    vt[cnt].what[1] = value;
                    //element->CEs[CEindex++] = value;
                } else {
                  vt[cnt].what[1] = 0;
                }
              } else {
                fprintf(stderr, "Failed to read a CE from line %s\n", buffer);
              }
#endif
            } else if (what_to_do == READUCAVERSION) { //vt[cnt].what_to_do == READUCAVERSION
              u_versionFromString(UCAVersion, buffer+vtLen);
              if(beVerbose) {
                char uca[U_MAX_VERSION_STRING_LENGTH];
                u_versionToString(UCAVersion, uca);
                printf("UCA version %s\n", uca);
              }
              UVersionInfo UCDVersion;
              u_getUnicodeVersion(UCDVersion);
              if (UCAVersion[0] != UCDVersion[0] || UCAVersion[1] != UCDVersion[1]) {
                char uca[U_MAX_VERSION_STRING_LENGTH];
                char ucd[U_MAX_VERSION_STRING_LENGTH];
                u_versionToString(UCAVersion, uca);
                u_versionToString(UCDVersion, ucd);
                // Warning, not error, to permit bootstrapping during a version upgrade.
                fprintf(stderr, "warning: UCA version %s != UCD version %s (temporarily change the FractionalUCA.txt UCA version during Unicode version upgrade)\n", uca, ucd);
                // *status = U_INVALID_FORMAT_ERROR;
                // return NULL;
              }
            } else if (what_to_do == READLEADBYTETOSCRIPTS) { //vt[cnt].what_to_do == READLEADBYTETOSCRIPTS
                pointer = buffer + vtLen;
                skipWhiteSpace(&pointer, status);

                uint16_t leadByte = (hex2num(*pointer++) * 16);
                leadByte += hex2num(*pointer++);
                //printf("~~~~ processing lead byte = %02x\n", leadByte);
                skipWhiteSpace(&pointer, status);
                if (uprv_strstr(pointer, "COMPRESS") != NULL) {
                    builder.setCompressibleLeadByte(leadByte);
                }
            }
            return NULL;
        }
      }
      fprintf(stderr, "Warning: unrecognized option: %s\n", buffer);
      //*status = U_INVALID_FORMAT_ERROR;
      return NULL;
    }

    startCodePoint = buffer;
    endCodePoint = strchr(startCodePoint, ';');

    if(endCodePoint == 0) {
        fprintf(stderr, "error - line with no code point!\n");
        *status = U_INVALID_FORMAT_ERROR; /* No code point - could be an error, but probably only an empty line */
        return NULL;
    } else {
        *(endCodePoint) = 0;
    }

    char *pipePointer = strchr(buffer, '|');
    if (pipePointer != NULL) {
        // Read the prefix string which precedes the actual string.
        *pipePointer = 0;
        element->prefixSize =
            u_parseString(startCodePoint,
                          element->prefixChars, LENGTHOF(element->prefixChars),
                          NULL, status);
        if(U_FAILURE(*status)) {
            fprintf(stderr, "error - parsing of prefix \"%s\" failed: %s\n",
                    startCodePoint, u_errorName(*status));
            *status = U_INVALID_FORMAT_ERROR;
            return NULL;
        }
        element->prefix = element->prefixChars;
        startCodePoint = pipePointer + 1;
    }

    // Read the string which gets the CE(s) assigned.
    element->cSize =
        u_parseString(startCodePoint,
                      element->uchars, LENGTHOF(element->uchars),
                      NULL, status);
    if(U_FAILURE(*status)) {
        fprintf(stderr, "error - parsing of code point(s) \"%s\" failed: %s\n",
                startCodePoint, u_errorName(*status));
        *status = U_INVALID_FORMAT_ERROR;
        return NULL;
    }
    element->cPoints = element->uchars;

    startCodePoint = endCodePoint+1;

    commentStart = strchr(startCodePoint, '#');
    if(commentStart == NULL) {
        commentStart = strlen(startCodePoint) + startCodePoint;
    }

    i = 0;
    cesLength = 0;
    element->noOfCEs = 0;
    for(;;) {
        endCodePoint = strchr(startCodePoint, ']');
        if(endCodePoint == NULL || endCodePoint >= commentStart) {
            break;
        }
        pointer = strchr(startCodePoint, '[');
        pointer++;

        readElement(&pointer, primary, ',', status);
        readElement(&pointer, secondary, ',', status);
        readElement(&pointer, tertiary, ']', status);
        ces[cesLength++] = getSingleCEValue(builder, primary, secondary, tertiary, status);
        // TODO: max 31 CEs

        startCodePoint = endCodePoint+1;
        i++;
    }

    // we don't want any strange stuff after useful data!
    if (pointer == NULL) {
        /* huh? Did we get ']' without the '['? Pair your brackets! */
        *status=U_INVALID_FORMAT_ERROR;
    }
    else {
        while(pointer < commentStart)  {
            if(*pointer != ' ' && *pointer != '\t')
            {
                *status=U_INVALID_FORMAT_ERROR;
                break;
            }
            pointer++;
        }
    }
    if(element->cSize == 1 && element->cPoints[0] == 0xfffe) {
        // UCA 6.0 gives U+FFFE a special minimum weight using the
        // byte 02 which is the merge-sort-key separator and illegal for any
        // other characters.
    } else {
        // Rudimentary check for valid bytes in CE weights.
        // For a more comprehensive check see cintltst /tscoll/citertst/TestCEValidity
        for (i = 0; i < cesLength; ++i) {
            int64_t ce = ces[i];
            for (int j = 7; j >= 0; --j) {
                uint8_t b = (uint8_t)(ce >> (j * 8));
                if(j <= 1) { b &= 0x3f; }  // tertiary bytes use 6 bits
                if (b == 1) {
                    fprintf(stderr, "Warning: invalid UCA weight byte 01 for %s\n", buffer);
                    return NULL;
                }
                if ((j == 7 || j == 3 || j == 1) && b == 2) {
                    fprintf(stderr, "Warning: invalid UCA weight lead byte 02 for %s\n", buffer);
                    return NULL;
                }
                // Primary second bytes 03 and FF are compression terminators.
                // TODO: 02 & 03 are usable when the lead byte is not compressible.
                // 02 is unusable and 03 is the low compression terminator when the lead byte is compressible.
                if (j == 6 && b == 0xff) {
                    fprintf(stderr, "Warning: invalid UCA primary second weight byte %02X for %s\n",
                            b, buffer);
                    return NULL;
                }
            }
        }
    }

    if(U_FAILURE(*status)) {
        fprintf(stderr, "problem putting stuff in hash table %s\n", u_errorName(*status));
        *status = U_INTERNAL_PROGRAM_ERROR;
        return NULL;
    }

    return element;
}

static int32_t
diffThreeBytePrimaries(uint32_t p1, uint32_t p2, UBool isCompressible) {
    if((p1 & 0xffff0000) == (p2 & 0xffff0000)) {
        // Same first two bytes.
        return (int32_t)(p2 - p1) >> 8;
    } else {
        // Third byte: 254 bytes 02..FF
        int32_t linear1 = (int32_t)((p1 >> 8) & 0xff) - 2;
        int32_t linear2 = (int32_t)((p2 >> 8) & 0xff) - 2;
        int32_t factor;
        if(isCompressible) {
            // Second byte for compressible lead byte: 251 bytes 04..FE
            linear1 += 254 * ((int32_t)((p1 >> 16) & 0xff) - 4);
            linear2 += 254 * ((int32_t)((p2 >> 16) & 0xff) - 4);
            factor = 251 * 254;
        } else {
            // Second byte for incompressible lead byte: 254 bytes 02..FF
            linear1 += 254 * ((int32_t)((p1 >> 16) & 0xff) - 2);
            linear2 += 254 * ((int32_t)((p2 >> 16) & 0xff) - 2);
            factor = 254 * 254;
        }
        linear1 += factor * (int32_t)((p1 >> 24) & 0xff);
        linear2 += factor * (int32_t)((p2 >> 24) & 0xff);
        return linear2 - linear1;
    }
}

static void
write_uca_table(const char *filename,
                CollationBaseDataBuilder &builder,
                UErrorCode *status)
{
    FILE *data = fopen(filename, "r");
    if(data == NULL) {
        fprintf(stderr, "Couldn't open file: %s\n", filename);
        *status = U_FILE_ACCESS_ERROR;
        return;
    }
    uint32_t line = 0;
    UCAElements *element = NULL;
    // TODO: Turn UCAConstants into fields in the inverse-base table.
    // Pass the inverse-base builder into the parsing function.
    UCAConstants consts;
    uprv_memset(&consts, 0, sizeof(consts));

    /* first set up constants for implicit calculation */
    uprv_uca_initImplicitConstants(status);

    // TODO: uprv_memset(inverseTable, 0xDA, sizeof(int32_t)*3*0xFFFF);

    if(U_FAILURE(*status))
    {
        fprintf(stderr, "Failed to initialize data structures for parsing FractionalUCA.txt - %s\n", u_errorName(*status));
        fclose(data);
        return;
    }

    UChar32 maxCodePoint = 0;
    int32_t numArtificialSecondaries = 0;
    int32_t numExtraSecondaryExpansions = 0;
    int32_t numExtraExpansionCEs = 0;
    int32_t numDiffTertiaries = 0;
    int32_t surrogateCount = 0;
    while(!feof(data)) {
        if(U_FAILURE(*status)) {
            fprintf(stderr, "Something returned an error %i (%s) while processing line %u of %s. Exiting...\n",
                *status, u_errorName(*status), (int)line, filename);
            exit(*status);
        }

        line++;
        if(beVerbose) {
          printf("%u ", (int)line);
        }
        int64_t ces[32];
        int32_t cesLength = 0;
        element = readAnElement(data, builder, &consts, ces, cesLength, status);
        if(element != NULL) {
            // we have read the line, now do something sensible with the read data!

#if 0  // TODO
            // if element is a contraction, we want to add it to contractions[]
            int32_t length = (int32_t)element->cSize;
            if(length > 1 && element->cPoints[0] != 0xFDD0) { // this is a contraction
              if(U16_IS_LEAD(element->cPoints[0]) && U16_IS_TRAIL(element->cPoints[1]) && length == 2) {
                surrogateCount++;
              } else {
                if(noOfContractions>=MAX_UCA_CONTRACTIONS) {
                  fprintf(stderr,
                          "\nMore than %d contractions. Please increase MAX_UCA_CONTRACTIONS in genuca.cpp. "
                          "Exiting...\n",
                          (int)MAX_UCA_CONTRACTIONS);
                  exit(U_BUFFER_OVERFLOW_ERROR);
                }
                if(length > MAX_UCA_CONTRACTION_LENGTH) {
                  fprintf(stderr,
                          "\nLine %d: Contraction of length %d is too long. Please increase MAX_UCA_CONTRACTION_LENGTH in genuca.cpp. "
                          "Exiting...\n",
                          (int)line, (int)length);
                  exit(U_BUFFER_OVERFLOW_ERROR);
                }
                UChar *t = &contractions[noOfContractions][0];
                u_memcpy(t, element->cPoints, length);
                t += length;
                for(; length < MAX_UCA_CONTRACTION_LENGTH; ++length) {
                    *t++ = 0;
                }
                noOfContractions++;
              }
            }
            else {
                // TODO (claireho): does this work? Need more tests
                // The following code is to handle the UCA pre-context rules
                // for L/l with middle dot. We share the structures for contractionCombos.
                // The format for pre-context character is
                // contractions[0]: codepoint in element->cPoints[0]
                // contractions[1]: '\0' to differentiate from a contraction
                // contractions[2]: prefix char
                if (element->prefixSize>0) {
                    if(length > 1 || element->prefixSize > 1) {
                        fprintf(stderr,
                                "\nLine %d: Character with prefix, "
                                "either too many characters or prefix too long.\n",
                                (int)line);
                        exit(U_INTERNAL_PROGRAM_ERROR);
                    }
                    if(noOfContractions>=MAX_UCA_CONTRACTIONS) {
                      fprintf(stderr,
                              "\nMore than %d contractions. Please increase MAX_UCA_CONTRACTIONS in genuca.cpp. "
                              "Exiting...\n",
                              (int)MAX_UCA_CONTRACTIONS);
                      exit(U_BUFFER_OVERFLOW_ERROR);
                    }
                    UChar *t = &contractions[noOfContractions][0];
                    t[0]=element->cPoints[0];
                    t[1]=0;
                    t[2]=element->prefixChars[0];
                    t += 3;
                    for(length = 3; length < MAX_UCA_CONTRACTION_LENGTH; ++length) {
                        *t++ = 0;
                    }
                    noOfContractions++;
                }
            }
#endif

#if 0  // TODO
            /* we're first adding to inverse, because addAnElement will reverse the order */
            /* of code points and stuff... we don't want that to happen */
            if((element->CEs[0] >> 24) != 2) {
                // Add every element except for the special minimum-weight character U+FFFE
                // which has 02 weights.
                // If we had 02 weights in the invuca table, then tailoring primary
                // after an ignorable would try to put a weight before 02 which is not valid.
                // We could fix this in a complicated way in the from-rule-string builder,
                // but omitting this special element from invuca is simple and effective.
                addToInverse(element, status);
            }
#endif
            if(!((int32_t)element->cSize > 1 && element->cPoints[0] == 0xFDD0)) {
                // TODO: We ignore the maximum CE for U+FFFF which has [EF FE, 05, 05] in v1.
                // CollationBaseDataBuilder::initBase() sets a different (higher) CE.
                uint32_t p = (uint32_t)(ces[0] >> 32);
                if(p == 0xeffe0000) { continue; }
                UnicodeString prefix(FALSE, element->prefixChars, (int32_t)element->prefixSize);
                UnicodeString s(FALSE, element->cPoints, (int32_t)element->cSize);
                builder.add(prefix, s, ces, cesLength, *status);

                UChar32 c = s.char32At(0);
                if(c > maxCodePoint) { maxCodePoint = c; }

                if(cesLength >= 2 && p != 0) {
                    int64_t prevCE = ces[0];
                    for(int32_t i = 1; i < cesLength; ++i) {
                        int64_t ce = ces[i];
                        if((uint32_t)(ce >> 32) == 0 && ((uint32_t)ce >> 16) > 0xdb99 && (uint32_t)(prevCE >> 32) != 0) {
                            ++numArtificialSecondaries;
                            if(cesLength == 2) {
                                ++numExtraSecondaryExpansions;
                                numExtraExpansionCEs += 2;
                            } else {
                                ++numExtraExpansionCEs;
                            }
                            if(((uint32_t)prevCE & 0x3f3f) != ((uint32_t)ce & 0x3f3f)) {
                                ++numDiffTertiaries;
                            }
                        }
                        prevCE = ce;
                    }
                }
            }
        }
    }

    // TODO: Remove analysis & output.
    printf("*** number of artificial secondary CEs:                 %6ld\n", (long)numArtificialSecondaries);
    printf("    number of 2-CE expansions that could be single CEs: %6ld\n", (long)numExtraSecondaryExpansions);
    printf("    number of extra expansion CEs:                      %6ld\n", (long)numExtraExpansionCEs);
    printf("    number of artificial sec. CEs w. diff. tertiaries:  %6ld\n", (long)numDiffTertiaries);

    int32_t numRanges = 0;
    int32_t numRangeCodePoints = 0;
    UChar32 rangeFirst = U_SENTINEL;
    UChar32 rangeLast = U_SENTINEL;
    uint32_t rangeFirstPrimary = 0;
    uint32_t rangeLastPrimary = 0;
    int32_t rangeStep = -1;

    // Detect ranges of characters in primary code point order,
    // with 3-byte primaries and
    // with consistent "step" differences between adjacent primaries.
    // TODO: Modify the FractionalUCA generator to use the same primary-weight incrementation.
    // Start at U+0180: No ranges for common Latin characters.
    // Go one beyond maxCodePoint in case a range ends there.
    for(UChar32 c = 0x180; c <= (maxCodePoint + 1); ++c) {
        UBool action;
        uint32_t p = builder.getLongPrimaryIfSingleCE(c);
        if(p != 0) {
            // p is a "long" (three-byte) primary.
            if(rangeFirst >= 0 && c == (rangeLast + 1) && p > rangeLastPrimary) {
                // Find the offset between the two primaries.
                int32_t step = diffThreeBytePrimaries(
                    rangeLastPrimary, p, builder.isCompressiblePrimary(p));
                if(rangeFirst == rangeLast && step >= 2) {
                    // c == rangeFirst + 1, store the "step" between range primaries.
                    rangeStep = step;
                    rangeLast = c;
                    rangeLastPrimary = p;
                    action = 0;  // continue range
                } else if(rangeStep == step) {
                    // Continue the range with the same "step" difference.
                    rangeLast = c;
                    rangeLastPrimary = p;
                    action = 0;  // continue range
                } else {
                    action = 1;  // maybe finish range, start a new one
                }
            } else {
                action = 1;  // maybe finish range, start a new one
            }
        } else {
            action = -1;  // maybe finish range, do not start a new one
        }
        if(action != 0 && rangeFirst >= 0) {
            // Finish a range.
            // Set offset CE32s for a long range, leave single CEs for a short range.
            UBool didSetRange = builder.maybeSetPrimaryRange(
                rangeFirst, rangeLast,
                rangeFirstPrimary, rangeStep, *status);
            if(U_FAILURE(*status)) {
                fprintf(stderr,
                        "failure setting code point order range U+%04lx..U+%04lx "
                        "%08lx..%08lx step %d - %s\n",
                        (long)rangeFirst, (long)rangeLast,
                        (long)rangeFirstPrimary, (long)rangeLastPrimary,
                        (int)rangeStep, u_errorName(*status));
            } else if(didSetRange) {
                int32_t rangeLength = rangeLast - rangeFirst + 1;
                /*printf("* set code point order range U+%04lx..U+%04lx [%d] "
                        "%08lx..%08lx step %d\n",
                        (long)rangeFirst, (long)rangeLast,
                        (int)rangeLength,
                        (long)rangeFirstPrimary, (long)rangeLastPrimary,
                        (int)rangeStep);*/
                ++numRanges;
                numRangeCodePoints += rangeLength;
            }
            rangeFirst = U_SENTINEL;
            rangeStep = -1;
        }
        if(action > 0) {
            // Start a new range.
            rangeFirst = rangeLast = c;
            rangeFirstPrimary = rangeLastPrimary = p;
        }
    }
    printf("** set %d ranges with %d code points\n", (int)numRanges, (int)numRangeCodePoints);

    // TODO: Probably best to work in two passes.
    // Pass 1 for reading all data, setting isCompressible flags (and reordering groups)
    // and finding ranges.
    // Then set the ranges in a newly initialized builder
    // for optimal compression (makes sure that adjacent blocks can overlap easily).
    // Then set all mappings outside the ranges.
    //
    // In the first pass, we could store mappings in a simple list,
    // with single-character/single-long-primary-CE mappings in a UTrie2;
    // or store the mappings in a temporary builder;
    // or we could just parse the input file again in the second pass.
    //
    // Ideally set/copy U+0000..U+017F before setting anything else,
    // then set default Han/Hangul, then set the ranges, then copy non-range mappings.
    // It should be easy to copy mappings from an un-built builder to a new one.
    // Add CollationDataBuilder::copyFrom(builder, code point, errorCode) -- copy contexts & expansions.

    // TODO: Analyse why CLDR root collation data is large.
    // Size of expansions for canonical decompositions?
    // Size of expansions for compatibility decompositions?
    // For the latter, consider storing some of them as specials,
    //   decompose at runtime?? Store unusual tertiary in special CE32?

    if(UCAVersion[0] == 0 && UCAVersion[1] == 0 && UCAVersion[2] == 0 && UCAVersion[3] == 0) {
        fprintf(stderr, "UCA version not specified. Cannot create data file!\n");
        fclose(data);
        return;
    }

    if (beVerbose) {
        printf("\nLines read: %u\n", (int)line);
        printf("Surrogate count: %i\n", (int)surrogateCount);
        printf("Raw data breakdown:\n");
        /*printf("Compact array stage1 top: %i, stage2 top: %i\n", t->mapping->stage1Top, t->mapping->stage2Top);*/
        // TODO: printf("Number of contractions: %u\n", (int)noOfContractions);
        // TODO: printf("Contraction image size: %u\n", (int)t->image->contractionSize);
        // TODO: printf("Expansions size: %i\n", (int)t->expansions->position);
    }


    /* produce canonical closure for table */
#if 0  // TODO
    /* do the closure */
    UnicodeSet closed;
    int32_t noOfClosures = uprv_uca_canonicalClosure(t, NULL, &closed, status);
    if(noOfClosures != 0) {
        fprintf(stderr, "Warning: %i canonical closures occured!\n", (int)noOfClosures);
        UnicodeString pattern;
        std::string utf8;
        closed.toPattern(pattern, TRUE).toUTF8String(utf8);
        fprintf(stderr, "UTF-8 pattern string: %s\n", utf8.c_str());
    }

    /* test */
    UCATableHeader *myData = uprv_uca_assembleTable(t, status);  

    if (beVerbose) {
        printf("Compacted data breakdown:\n");
        /*printf("Compact array stage1 top: %i, stage2 top: %i\n", t->mapping->stage1Top, t->mapping->stage2Top);*/
        printf("Number of contractions: %u\n", (int)noOfContractions);
        printf("Contraction image size: %u\n", (int)t->image->contractionSize);
        printf("Expansions size: %i\n", (int)t->expansions->position);
    }

    if(U_FAILURE(*status)) {
        fprintf(stderr, "Error creating table: %s\n", u_errorName(*status));
        fclose(data);
        return -1;
    }

    /* populate the version info struct with version info*/
    myData->version[0] = UCOL_BUILDER_VERSION;
    myData->version[1] = UCAVersion[0];
    myData->version[2] = UCAVersion[1];
    myData->version[3] = UCAVersion[2];
    /*TODO:The fractional rules version should be taken from FractionalUCA.txt*/
    // Removed this macro. Instead, we use the fields below
    //myD->version[1] = UCOL_FRACTIONAL_UCA_VERSION;
    //myD->UCAVersion = UCAVersion; // out of FractionalUCA.txt
    uprv_memcpy(myData->UCAVersion, UCAVersion, sizeof(UVersionInfo));
    u_getUnicodeVersion(myData->UCDVersion);

    writeOutData(myData, &consts, &leadByteConstants, contractions, noOfContractions, outputDir, copyright, status);
    uprv_free(myData);

    InverseUCATableHeader *inverse = assembleInverseTable(status);
    uprv_memcpy(inverse->UCAVersion, UCAVersion, sizeof(UVersionInfo));
    writeOutInverseData(inverse, outputDir, copyright, status);
    uprv_free(inverse);
#endif

    fclose(data);

    return;
}

static CollationBaseData *
makeBaseDataFromFractionalUCA(CollationBaseDataBuilder &builder, UErrorCode &errorCode) {
    CharString path(IntlTest::getSourceTestData(errorCode), errorCode);
    path.appendPathPart("..", errorCode);
    path.appendPathPart("..", errorCode);
    path.appendPathPart("data", errorCode);
    path.appendPathPart("unidata", errorCode);
    path.appendPathPart("FractionalUCA.txt", errorCode);

    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "unable to build file path to FractionalUCA.txt - %s\n",
                u_errorName(errorCode));
        return NULL;
    }

    builder.initBase(errorCode);
    write_uca_table(path.data(), builder, &errorCode);
    CollationBaseData *cd = builder.buildBaseData(errorCode);
    printf("*** CLDR root collation part sizes ***\n");
    UErrorCode trieErrorCode = U_ZERO_ERROR;
    int32_t length = builder.serializeTrie(NULL, 0, trieErrorCode);
    printf("  trie size:                    %6ld\n", (long)length);
    int32_t totalSize = length;
    length = builder.lengthOfCE32s();
    printf("  CE32s:            %6ld *4 = %6ld\n", (long)length, (long)length * 4);
    totalSize += length * 4;
    length = builder.lengthOfCEs();
    printf("  CEs:              %6ld *8 = %6ld\n", (long)length, (long)length * 8);
    totalSize += length * 8;
    length = builder.lengthOfContexts();
    printf("  contexts:         %6ld *2 = %6ld\n", (long)length, (long)length * 2);
    totalSize += length * 2;
    // TODO: Allocate Jamo CEs inside the array of CEs?! Then we just need to store an offset. -1 if from base.
    length = 19+21+27;
    printf("  Jamo CEs:         %6ld *8 = %6ld\n", (long)length, (long)length * 8);
    totalSize += length * 8;
    // TODO: Combine compressibleBytes with reordering group data.
    printf("  compressibleBytes:               256\n");
    totalSize += 256;
    length = cd->scriptsLength;
    printf("  scripts data:     %6ld *4 = %6ld\n", (long)length, (long)length * 4);
    totalSize += length * 4;
    UErrorCode setErrorCode = U_ZERO_ERROR;
    length = builder.serializeUnsafeBackwardSet(NULL, 0, setErrorCode);
    printf("  unsafeBwdSet:     %6ld *2 = %6ld\n", (long)length, (long)length * 2);
    totalSize += length * 2;
    printf("*** CLDR root collation size:   %6ld\n", (long)totalSize);
    printf("  missing: headers, options\n");
    return cd;
}

// TODO: Put this into genuca.
// Make preparseucd.py write fcd_data.h mapping code point ranges to FCD16 values,
// use that in genuca rather than properties APIs.

/**
 * Adds each lead surrogate to the bmp set if any of the 1024
 * associated supplementary code points is in the supp set.
 * These can be one and the same set.
 */
static void
setLeadSurrogatesForAssociatedSupplementary(UnicodeSet &bmp, const UnicodeSet &supp) {
    UChar32 c = 0x10000;
    for(UChar lead = 0xd800; lead < 0xdc00; ++lead, c += 0x400) {
        if(supp.containsSome(c, c + 0x3ff)) {
            bmp.add(lead);
        }
    }
}

static int32_t
makeBMPFoldedBitSet(const UnicodeSet &set, uint8_t index[0x800], uint32_t bits[256],
                    UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    bits[0] = 0;  // no bits set
    bits[1] = 0xffffffff;  // all bits set
    int32_t bitsLength = 2;
    int32_t i = 0;
    for(UChar32 c = 0; c <= 0xffff; c += 0x20, ++i) {
        if(set.containsNone(c, c + 0x1f)) {
            index[i] = 0;
        } else if(set.contains(c, c + 0x1f)) {
            index[i] = 1;
        } else {
            uint32_t b = 0;
            for(int32_t j = 0; j <= 0x1f; ++j) {
                if(set.contains(c + j)) {
                    b |= (uint32_t)1 << j;
                }
            }
            int32_t k;
            for(k = 2;; ++k) {
                if(k == bitsLength) {
                    // new bit combination
                    if(bitsLength == 256) {
                        errorCode = U_BUFFER_OVERFLOW_ERROR;
                        return 0;
                    }
                    bits[bitsLength++] = b;
                    break;
                }
                if(bits[k] == b) {
                    // duplicate bit combination
                    break;
                }
            }
            index[i] = k;
        }
    }
    return bitsLength;
}

/**
 * Builds data for the FCD check fast path.
 * For details see the CollationFCD class comments.
 */
static void
buildFCDData(UErrorCode &errorCode) {
    UnicodeSet lcccSet(UNICODE_STRING_SIMPLE("[[:^lccc=0:][\\udc00-\\udfff]]"), errorCode);
    UnicodeSet tcccSet(UNICODE_STRING_SIMPLE("[:^tccc=0:]"), errorCode);
    setLeadSurrogatesForAssociatedSupplementary(tcccSet, tcccSet);
    // The following supp(lccc)->lead(tccc) should be unnecessary
    // after the previous supp(tccc)->lead(tccc)
    // because there should not be any characters with lccc!=0 and tccc=0.
    // It is safe and harmless.
    setLeadSurrogatesForAssociatedSupplementary(tcccSet, lcccSet);
    setLeadSurrogatesForAssociatedSupplementary(lcccSet, lcccSet);
    uint8_t lcccIndex[0x800], tcccIndex[0x800];
    uint32_t lcccBits[256], tcccBits[256];
    int32_t lcccBitsLength = makeBMPFoldedBitSet(lcccSet, lcccIndex, lcccBits, errorCode);
    int32_t tcccBitsLength = makeBMPFoldedBitSet(tcccSet, tcccIndex, tcccBits, errorCode);
    printf("@@@ lcccBitsLength=%d -> %d bytes\n", lcccBitsLength, 0x800 + lcccBitsLength * 4);
    printf("@@@ tcccBitsLength=%d -> %d bytes\n", tcccBitsLength, 0x800 + tcccBitsLength * 4);

    if(U_FAILURE(errorCode)) { return; }

    FILE *f=usrc_create("", "collationfcd.cpp",  // TODO: path
                        "icu/tools/unicode/c/genuca/genuca.cpp");  // TODO: generator .cpp
    if(f==NULL) {
        errorCode=U_FILE_ACCESS_ERROR;
        return;
    }
    fputs("#include \"unicode/utypes.h\"\n\n", f);
    fputs("#if !UCONFIG_NO_COLLATION\n\n", f);
    fputs("#include \"collationfcd.h\"\n\n", f);
    usrc_writeArray(f,
        "const uint8_t CollationFCD::lcccIndex[%ld]={\n",
        lcccIndex, 8, 0x800,
        "\n};\n\n");
    usrc_writeArray(f,
        "const uint32_t CollationFCD::lcccBits[%ld]={\n",
        lcccBits, 32, lcccBitsLength,
        "\n};\n\n");
    usrc_writeArray(f,
        "const uint8_t CollationFCD::tcccIndex[%ld]={\n",
        tcccIndex, 8, 0x800,
        "\n};\n\n");
    usrc_writeArray(f,
        "const uint32_t CollationFCD::tcccBits[%ld]={\n",
        tcccBits, 32, tcccBitsLength,
        "\n};\n\n");
    fputs("#endif  // !UCONFIG_NO_COLLATION\n", f);
    fclose(f);
}

extern const CollationBaseData *
getCollationBaseData(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    static CollationBaseDataBuilder *gBaseDataBuilder = NULL;
    static CollationBaseData *gBaseData = NULL;
    {
        Mutex lock;
        if(gBaseData != NULL) { return gBaseData; }
    }
    buildFCDData(errorCode);
    LocalPointer<CollationBaseDataBuilder> builder(new CollationBaseDataBuilder(errorCode));
    LocalPointer<CollationBaseData> baseData(makeBaseDataFromFractionalUCA(*builder, errorCode));
    baseData->variableTop = gVariableTop;
    if(U_SUCCESS(errorCode)) {
        Mutex lock;
        if(gBaseData == NULL) {
            gBaseDataBuilder = builder.orphan();
            gBaseData = baseData.orphan();
        }
    }
    return gBaseData;
}
