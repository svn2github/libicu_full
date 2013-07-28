/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
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
#include "unicode/sortkey.h"
#include "unicode/std_string.h"
#include "unicode/tblcoll.h"
#include "unicode/uiter.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "charstr.h"
#include "collation.h"
#include "collationbasedatabuilder.h"
#include "collationbuilder.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationiterator.h"
#include "collationroot.h"
#include "collationruleparser.h"
#include "collationtailoring.h"
#include "collationweights.h"
#include "cstring.h"
#include "intltest.h"
#include "normalizer2impl.h"
#include "rulebasedcollator.h"
#include "ucbuf.h"
#include "uitercollationiterator.h"
#include "utf16collationiterator.h"
#include "utf8collationiterator.h"
#include "uvectr32.h"
#include "writesrc.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

// TODO: Move to ucbuf.h
U_DEFINE_LOCAL_OPEN_POINTER(LocalUCHARBUFPointer, UCHARBUF, ucbuf_close);

class CodePointIterator;

class CollationTest : public IntlTest {
public:
    CollationTest()
            : fcd(NULL),
              fileLineNumber(0),
              collData(NULL),
              coll(NULL) {}

    ~CollationTest() {
        delete coll;
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);

    void TestMinMax();
    void TestImplicits();
    void TestNulTerminated();
    void TestIllegalUTF8();
    void TestFCD();
    void TestCollationWeights();
    void TestDataDriven();

private:
    void checkFCD(const char *name, CollationIterator &ci, CodePointIterator &cpi);
    void checkAllocWeights(CollationWeights &cw,
                           uint32_t lowerLimit, uint32_t upperLimit, int32_t n,
                           int32_t someLength, int32_t minCount);

    static UnicodeString printSortKey(const uint8_t *p, int32_t length);
    static UnicodeString printCollationKey(const CollationKey &key);

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
    Collation::Level parseRelationAndString(UnicodeString &s, IcuTestErrorCode &errorCode);
    void parseAndSetAttribute(IcuTestErrorCode &errorCode);
    void parseAndSetReorderCodes(int32_t start, IcuTestErrorCode &errorCode);
    void buildBase(UCHARBUF *f, IcuTestErrorCode &errorCode);
    void buildTailoring(UCHARBUF *f, IcuTestErrorCode &errorCode);
    void setRootCollator(IcuTestErrorCode &errorCode);

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

    UBool getSortKeyParts(const UChar *s, int32_t length,
                          CharString &dest, int32_t partSize,
                          IcuTestErrorCode &errorCode);
    UBool getCollationKey(const UnicodeString &line,
                          const UChar *s, int32_t length,
                          CollationKey &key, IcuTestErrorCode &errorCode);
    UBool checkCompareTwo(const UnicodeString &prevFileLine,
                          const UnicodeString &prevString, const UnicodeString &s,
                          UCollationResult expectedOrder, Collation::Level expectedLevel,
                          IcuTestErrorCode &errorCode);
    void checkCompareStrings(UCHARBUF *f, IcuTestErrorCode &errorCode);

    const Normalizer2 *fcd;
    UnicodeString fileLine;
    int32_t fileLineNumber;
    UnicodeString fileTestName;
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
    TESTCASE_AUTO(TestIllegalUTF8);
    TESTCASE_AUTO(TestFCD);
    TESTCASE_AUTO(TestCollationWeights);
    TESTCASE_AUTO(TestDataDriven);
    TESTCASE_AUTO_END;
}

void CollationTest::TestMinMax() {
    IcuTestErrorCode errorCode(*this, "TestMinMax");

    CollationBaseDataBuilder builder(errorCode);
    builder.init(errorCode);
    CollationData data(*Normalizer2Factory::getNFCImpl(errorCode));
    builder.build(data, errorCode);
    if(errorCode.logIfFailureAndReset("CollationBaseDataBuilder.build()")) {
        return;
    }

    static const UChar s[2] = { 0xfffe, 0xffff };
    UTF16CollationIterator ci(&data, FALSE, s, s, s + 2);
    int64_t ce = ci.nextCE(errorCode);
    int64_t expected =
        ((int64_t)Collation::MERGE_SEPARATOR_PRIMARY << 32) |
        Collation::MERGE_SEPARATOR_LOWER32;
    if(ce != expected) {
        errln("CollationIterator.nextCE(U+fffe)=%04lx != 02.02.02", (long)ce);
    }

    ce = ci.nextCE(errorCode);
    expected = Collation::makeCE(Collation::MAX_PRIMARY);
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

    const CollationData *cd = CollationRoot::getData(errorCode);
    if(errorCode.logIfFailureAndReset("CollationRoot::getBaseData()")) {
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
    unassigned.remove(0xfffe, 0xffff);  // These have special CLDR root mappings.
    unassigned.remove(0x2066, 0x2069);  // TODO: remove this when UCD 6.3 is integrated
    unassigned.remove(0x061c);  // TODO: remove this when UCD 6.3 is integrated
    if(errorCode.logIfFailureAndReset("UnicodeSet")) {
        return;
    }
    const UnicodeSet *sets[] = { &coreHan, &otherHan, &unassigned };
    UChar32 prev = 0;
    uint32_t prevPrimary = 0;
    UTF16CollationIterator ci(cd, FALSE, NULL, NULL, NULL);
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
                errln("CollationIterator.nextCE(U+%04lx) has non-common sec/ter weights: %08lx",
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
    builder.init(errorCode);
    CollationData data(*Normalizer2Factory::getNFCImpl(errorCode));
    builder.build(data, errorCode);
    if(errorCode.logIfFailureAndReset("CollationBaseDataBuilder.build()")) {
        return;
    }

    static const UChar s[] = { 0x61, 0x62, 0x61, 0x62, 0 };

    UTF16CollationIterator ci1(&data, FALSE, s, s, s + 2);
    UTF16CollationIterator ci2(&data, FALSE, s + 2, s + 2, NULL);
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

void CollationTest::TestIllegalUTF8() {
    IcuTestErrorCode errorCode(*this, "TestIllegalUTF8");

    // Set the root collator.
    setRootCollator(errorCode);
    if(errorCode.logIfFailureAndReset("setting the root collator")) {
        return;
    }
    coll->setAttribute(UCOL_STRENGTH, UCOL_IDENTICAL, errorCode);

    static const char *strings[] = {
        // U+FFFD
        "a\xef\xbf\xbdz",
        // illegal byte sequences
        "a\x80z",  // trail byte
        "a\xc1\x81z",  // non-shortest form
        "a\xe0\x82\x83z",  // non-shortest form
        "a\xed\xa0\x80z",  // lead surrogate: would be U+D800
        "a\xed\xbf\xbfz",  // trail surrogate: would be U+DFFF
        "a\xf0\x8f\xbf\xbfz",  // non-shortest form
        "a\xf4\x90\x80\x80z"  // out of range: would be U+110000
    };

    StringPiece fffd(strings[0]);
    for(int32_t i = 1; i < LENGTHOF(strings); ++i) {
        StringPiece illegal(strings[i]);
        UCollationResult order = coll->compareUTF8(fffd, illegal, errorCode);
        if(order != UCOL_EQUAL) {
            errln("compareUTF8(U+FFFD, string %d with illegal UTF-8)=%d != UCOL_EQUAL",
                  (int)i, order);
        }
    }
}

class CodePointIterator {
public:
    CodePointIterator(const UChar32 *cp, int32_t length) : cp(cp), length(length), pos(0) {}
    void resetToStart() { pos = 0; }
    UChar32 next() { return (pos < length) ? cp[pos++] : U_SENTINEL; }
    UChar32 previous() { return (pos > 0) ? cp[--pos] : U_SENTINEL; }
    int32_t getLength() const { return length; }
    int getIndex() const { return (int)pos; }
private:
    const UChar32 *cp;
    int32_t length;
    int32_t pos;
};

void CollationTest::checkFCD(const char *name,
                             CollationIterator &ci, CodePointIterator &cpi) {
    IcuTestErrorCode errorCode(*this, "checkFCD");

    // Iterate forward to the limit.
    for(;;) {
        UChar32 c1 = ci.nextCodePoint(errorCode);
        UChar32 c2 = cpi.next();
        if(c1 != c2) {
            errln("%s.nextCodePoint(to limit, 1st pass) = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }

    // Iterate backward most of the way.
    for(int32_t n = (cpi.getLength() * 2) / 3; n > 0; --n) {
        UChar32 c1 = ci.previousCodePoint(errorCode);
        UChar32 c2 = cpi.previous();
        if(c1 != c2) {
            errln("%s.previousCodePoint() = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
    }

    // Forward again.
    for(;;) {
        UChar32 c1 = ci.nextCodePoint(errorCode);
        UChar32 c2 = cpi.next();
        if(c1 != c2) {
            errln("%s.nextCodePoint(to limit again) = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }

    // Iterate backward to the start.
    for(;;) {
        UChar32 c1 = ci.previousCodePoint(errorCode);
        UChar32 c2 = cpi.previous();
        if(c1 != c2) {
            errln("%s.previousCodePoint(to start) = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }
}

void CollationTest::TestFCD() {
    IcuTestErrorCode errorCode(*this, "TestFCD");

    CollationBaseDataBuilder builder(errorCode);
    builder.init(errorCode);
    CollationData data(*Normalizer2Factory::getNFCImpl(errorCode));
    builder.build(data, errorCode);
    if(errorCode.logIfFailureAndReset("CollationBaseDataBuilder.build()")) {
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

    FCDUTF16CollationIterator u16ci(&data, FALSE, s, s, NULL);
    if(errorCode.logIfFailureAndReset("FCDUTF16CollationIterator constructor")) {
        return;
    }
    CodePointIterator cpi(cp, LENGTHOF(cp));
    checkFCD("FCDUTF16CollationIterator", u16ci, cpi);

#if U_HAVE_STD_STRING
    cpi.resetToStart();
    std::string utf8;
    UnicodeString(s).toUTF8String(utf8);
    FCDUTF8CollationIterator u8ci(&data, FALSE,
                                  reinterpret_cast<const uint8_t *>(utf8.c_str()), 0, -1);
    if(errorCode.logIfFailureAndReset("FCDUTF8CollationIterator constructor")) {
        return;
    }
    checkFCD("FCDUTF8CollationIterator", u8ci, cpi);
#endif

    cpi.resetToStart();
    UCharIterator iter;
    uiter_setString(&iter, s, LENGTHOF(s) - 1);  // -1: without the terminating NUL
    FCDUIterCollationIterator uici(&data, FALSE, iter, 0);
    if(errorCode.logIfFailureAndReset("FCDUIterCollationIterator constructor")) {
        return;
    }
    checkFCD("FCDUIterCollationIterator", uici, cpi);
}

void CollationTest::checkAllocWeights(CollationWeights &cw,
                                      uint32_t lowerLimit, uint32_t upperLimit, int32_t n,
                                      int32_t someLength, int32_t minCount) {
    if(!cw.allocWeights(lowerLimit, upperLimit, n)) {
        errln("CollationWeights::allocWeights(%lx, %lx, %ld) = FALSE",
              (long)lowerLimit, (long)upperLimit, (long)n);
        return;
    }
    uint32_t previous = lowerLimit;
    int32_t count = 0;  // number of weights that have someLength
    for(int32_t i = 0; i < n; ++i) {
        uint32_t w = cw.nextWeight();
        if(w == 0xffffffff) {
            errln("CollationWeights::allocWeights(%lx, %lx, %ld).nextWeight() "
                  "returns only %ld weights",
                  (long)lowerLimit, (long)upperLimit, (long)n, (long)i);
            return;
        }
        if(!(previous < w && w < upperLimit)) {
            errln("CollationWeights::allocWeights(%lx, %lx, %ld).nextWeight() "
                  "number %ld -> %lx not between %lx and %lx",
                  (long)lowerLimit, (long)upperLimit, (long)n,
                  (long)(i + 1), (long)w, (long)previous, (long)upperLimit);
            return;
        }
        if(CollationWeights::lengthOfWeight(w) == someLength) { ++count; }
    }
    if(count < minCount) {
        errln("CollationWeights::allocWeights(%lx, %lx, %ld).nextWeight() "
              "returns only %ld < %ld weights of length %d",
              (long)lowerLimit, (long)upperLimit, (long)n,
              (long)count, (long)minCount, (int)someLength);
    }
}

void CollationTest::TestCollationWeights() {
    CollationWeights cw;

    // Non-compressible primaries use 254 second bytes 02..FF.
    logln("CollationWeights.initForPrimary(non-compressible)");
    cw.initForPrimary(FALSE);
    // Expect 1 weight 11 and 254 weights 12xx.
    checkAllocWeights(cw, 0x10000000, 0x13000000, 255, 1, 1);
    checkAllocWeights(cw, 0x10000000, 0x13000000, 255, 2, 254);
    // Expect 255 two-byte weights from the ranges 10ff, 11xx, 1202.
    checkAllocWeights(cw, 0x10fefe40, 0x12030300, 260, 2, 255);
    // Expect 254 two-byte weights from the ranges 10ff and 11xx.
    checkAllocWeights(cw, 0x10fefe40, 0x12030300, 600, 2, 254);
    // Expect 254^2=64516 three-byte weights.
    // During computation, there should be 3 three-byte ranges
    // 10ffff, 11xxxx, 120202.
    // The middle one should be split 64515:1,
    // and the newly-split-off range and the last ranged lengthened.
    checkAllocWeights(cw, 0x10fffe00, 0x12020300, 1 + 64516 + 254 + 1, 3, 64516);
    // Expect weights 1102 & 1103.
    checkAllocWeights(cw, 0x10ff0000, 0x11040000, 2, 2, 2);
    // Expect weights 102102 & 102103.
    checkAllocWeights(cw, 0x1020ff00, 0x10210400, 2, 3, 2);

    // Compressible primaries use 251 second bytes 04..FE.
    logln("CollationWeights.initForPrimary(compressible)");
    cw.initForPrimary(TRUE);
    // Expect 1 weight 11 and 251 weights 12xx.
    checkAllocWeights(cw, 0x10000000, 0x13000000, 252, 1, 1);
    checkAllocWeights(cw, 0x10000000, 0x13000000, 252, 2, 251);
    // Expect 252 two-byte weights from the ranges 10fe, 11xx, 1204.
    checkAllocWeights(cw, 0x10fdfe40, 0x12050300, 260, 2, 252);
    // Expect weights 1104 & 1105.
    checkAllocWeights(cw, 0x10fe0000, 0x11060000, 2, 2, 2);
    // Expect weights 102102 & 102103.
    checkAllocWeights(cw, 0x1020ff00, 0x10210400, 2, 3, 2);

    // Secondary and tertiary weights use only bytes 3 & 4.
    logln("CollationWeights.initForSecondary()");
    cw.initForSecondary();
    // Expect weights fbxx and all four fc..ff.
    checkAllocWeights(cw, 0xfb20, 0x10000, 20, 3, 4);

    logln("CollationWeights.initForTertiary()");
    cw.initForTertiary();
    // Expect weights 3dxx and both 3e & 3f.
    checkAllocWeights(cw, 0x3d02, 0x4000, 10, 3, 2);
}

UnicodeString CollationTest::printSortKey(const uint8_t *p, int32_t length) {
    UnicodeString s;
    for(int32_t i = 0; i < length; ++i) {
        if(i > 0) { s.append((UChar)0x20); }
        uint8_t b = p[i];
        if(b == 0) {
            s.append((UChar)0x2e);  // period
        } else if(b == 1) {
            s.append((UChar)0x7c);  // vertical bar
        } else {
            appendHex(b, 2, s);
        }
    }
    return s;
}

UnicodeString CollationTest::printCollationKey(const CollationKey &key) {
    int32_t length;
    const uint8_t *p = key.getByteArray(length);
    return printSortKey(p, length);
}

UBool CollationTest::readLine(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    int32_t lineLength;
    const UChar *line = ucbuf_readline(f, &lineLength, errorCode);
    if(line == NULL || errorCode.isFailure()) {
        fileLine.remove();
        return FALSE;
    }
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

Collation::Level CollationTest::parseRelationAndString(UnicodeString &s, IcuTestErrorCode &errorCode) {
    Collation::Level relation;
    int32_t start;
    if(fileLine[0] == 0x3c) {  // <
        UChar second = fileLine[1];
        start = 2;
        switch(second) {
        case 0x31:  // <1
            relation = Collation::PRIMARY_LEVEL;
            break;
        case 0x32:  // <2
            relation = Collation::SECONDARY_LEVEL;
            break;
        case 0x33:  // <3
            relation = Collation::TERTIARY_LEVEL;
            break;
        case 0x34:  // <4
            relation = Collation::QUATERNARY_LEVEL;
            break;
        case 0x63:  // <c
            relation = Collation::CASE_LEVEL;
            break;
        case 0x69:  // <i
            relation = Collation::IDENTICAL_LEVEL;
            break;
        default:  // just <
            relation = Collation::NO_LEVEL;
            start = 1;
            break;
        }
    } else if(fileLine[0] == 0x3d) {  // =
        relation = Collation::ZERO_LEVEL;
        start = 1;
    } else {
        start = 0;
    }
    if(start == 0 || !isSpace(fileLine[start])) {
        errln("no relation (= < <1 <2 <c <3 <4 <i) at beginning of line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return Collation::NO_LEVEL;
    }
    start = skipSpaces(start);
    UnicodeString prefix;
    parseString(start, prefix, s, errorCode);
    if(errorCode.isSuccess() && !prefix.isEmpty()) {
        errln("prefix string not allowed for test string: on line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return Collation::NO_LEVEL;
    }
    if(start < fileLine.length()) {
        errln("unexpected line contents after test string on line %d", (int)fileLineNumber);
        errln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return Collation::NO_LEVEL;
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
    // TODO: handle  % maxVariable symbol
    if(equalPos < 0) {
        if(fileLine.compare(start, 7, UNICODE_STRING("reorder", 7)) == 0) {
            parseAndSetReorderCodes(start + 7, errorCode);
            return;
        }
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
        return;
    }
    fileLine.remove();
}

void CollationTest::parseAndSetReorderCodes(int32_t start, IcuTestErrorCode &errorCode) {
    UVector32 reorderCodes(errorCode);
    while(start < fileLine.length()) {
        start = skipSpaces(start);
        int32_t limit = start;
        while(limit < fileLine.length() && !isSpace(fileLine[limit])) { ++limit; }
        CharString name;
        name.appendInvariantChars(fileLine.tempSubStringBetween(start, limit), errorCode);
        int32_t code = CollationRuleParser::getReorderCode(name.data());
        if(code < -1) {
            errln("invalid reorder code '%s' on line %d", name.data(), (int)fileLineNumber);
            errln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return;
        }
        reorderCodes.addElement(code, errorCode);
        start = limit;
    }
    coll->setReorderCodes(reorderCodes.getBuffer(), reorderCodes.size(), errorCode);
    if(errorCode.isFailure()) {
        errln("setReorderCodes() failed on line %d: %s", (int)fileLineNumber, errorCode.errorName());
        errln(fileLine);
        return;
    }
    fileLine.remove();
}

void CollationTest::buildBase(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    LocalPointer<CollationBaseDataBuilder> baseBuilder(new CollationBaseDataBuilder(errorCode));
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("new CollationBaseDataBuilder() failed");
        return;
    }
    baseBuilder->init(errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("CollationBaseDataBuilder.init() failed");
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
    LocalPointer<CollationTailoring> tailoring(new CollationTailoring(NULL));
    if(tailoring.isNull()) {
        errorCode.set(U_MEMORY_ALLOCATION_ERROR);
        return;
    }
    if(!tailoring->ensureOwnedData(errorCode)) { return; }
    baseBuilder->build(*tailoring->ownedData, errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("CollationBaseDataBuilder.build() failed");
    }
    tailoring->builder = baseBuilder.orphan();
    collData = tailoring->data;
    Collator *newColl = new RuleBasedCollator2(tailoring.getAlias());
    if(newColl == NULL) {
        errln("unable to allocate a new collator");
        errorCode.set(U_MEMORY_ALLOCATION_ERROR);
        return;
    }
    tailoring.orphan();  // newColl adopted the tailoring.
    delete coll;
    coll = newColl;
}

void CollationTest::buildTailoring(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    UnicodeString rules;
    while(readLine(f, errorCode)) {
        if(fileLine.isEmpty()) { continue; }
        if(isSectionStarter(fileLine[0])) { break; }
        rules.append(fileLine.unescape());
    }
    if(errorCode.isFailure()) { return; }
    logln(rules);
    // Duplicate of RuleBasedCollator2::buildTailoring(),
    // to get more error information.
    CollationBuilder builder(CollationRoot::getRoot(errorCode), errorCode);
    UParseError parseError;
    LocalPointer<CollationTailoring> tailoring(
            builder.parseAndBuild(rules, NULL, &parseError, errorCode));
    if(errorCode.isFailure()) {
        errln("CollationBuilder.parseAndBuild() failed - %s", errorCode.errorName());
        const char *reason = builder.getErrorReason();
        if(reason != NULL) { errln("  reason: %s", reason); }
        if(parseError.offset >= 0) { errln("  rules offset: %d", (int)parseError.offset); }
        if(parseError.preContext[0] != 0 || parseError.postContext[0] != 0) {
            errln(UnicodeString("  snippet: ...") +
                parseError.preContext + "(!)" + parseError.postContext + "...");
        }
        return;
    }
    Collator *newColl = new RuleBasedCollator2(tailoring.getAlias());
    if(newColl == NULL) {
        errln("unable to allocate a new collator");
        errorCode.set(U_MEMORY_ALLOCATION_ERROR);
        return;
    }
    tailoring.orphan();  // newColl adopted the tailoring.
    delete coll;
    collData = NULL;
    coll = newColl;
}

void CollationTest::setRootCollator(IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    Collator *newColl = CollationRoot::createCollator(errorCode);
    if(errorCode.isFailure()) {
        errln("unable to create a root collator");
        return;
    }
    delete coll;
    collData = NULL;
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
    UTF16CollationIterator ci(collData, FALSE, buffer, buffer, buffer + s.length());
    if(!checkCEs(ci, "UTF-16", ces, cesLength, errorCode)) { return FALSE; }
    // Test NUL-termination if s does not contain a NUL.
    if(s.indexOf((UChar)0) >= 0) { return TRUE; }
    UTF16CollationIterator ci0(collData, FALSE, buffer, buffer, NULL);
    return checkCEs(ci0, "UTF-16-NUL", ces, cesLength, errorCode);
}

UBool CollationTest::checkCEsFCD(const UnicodeString &s,
                                 int64_t ces[], int32_t cesLength,
                                 IcuTestErrorCode &errorCode) {
    const UChar *buffer = s.getBuffer();
    FCDUTF16CollationIterator ci(collData, FALSE, buffer, buffer, buffer + s.length());
    if(!checkCEs(ci, "FCD-UTF-16", ces, cesLength, errorCode)) { return FALSE; }
    // Test NUL-termination if s does not contain a NUL.
    if(s.indexOf((UChar)0) >= 0) { return TRUE; }
    FCDUTF16CollationIterator ci0(collData, FALSE, buffer, buffer, NULL);
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

UBool CollationTest::getSortKeyParts(const UChar *s, int32_t length,
                                     CharString &dest, int32_t partSize,
                                     IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return FALSE; }
    uint8_t part[32];
    U_ASSERT(partSize <= LENGTHOF(part));
    UCharIterator iter;
    uint32_t state[2] = { 0, 0 };
    for(;;) {
        uiter_setString(&iter, s, length);
        int32_t partLength = coll->nextSortKeyPart(&iter, state, part, partSize, errorCode);
        UBool done = partLength < partSize;
        if(done) {
            // At the end, append the next byte as well which should be 00.
            ++partLength;
        }
        dest.append(reinterpret_cast<char *>(part), partLength, errorCode);
        if(done) {
            return errorCode.isSuccess();
        }
    }
}

UBool CollationTest::getCollationKey(const UnicodeString &line,
                                     const UChar *s, int32_t length,
                                     CollationKey &key, IcuTestErrorCode &errorCode) {
    const char *norm =
        (coll->getAttribute(UCOL_NORMALIZATION_MODE, errorCode) == UCOL_ON) ?
        "on" : "off";
    if(errorCode.isFailure()) { return FALSE; }
    coll->getCollationKey(s, length, key, errorCode);
    if(errorCode.isFailure()) {
        errln(fileTestName);
        errln("Collator(normalization=%s).getCollationKey() failed: %s",
              norm, errorCode.errorName());
        errln(line);
        errorCode.reset();
        return FALSE;
    }
    int32_t keyLength;
    const uint8_t *keyBytes = key.getByteArray(keyLength);

    // If s contains U+FFFE, check that merged segments make the same key.
    LocalMemory<uint8_t> mergedKey;
    int32_t mergedKeyLength = 0;
    int32_t mergedKeyCapacity = 0;
    int32_t sLength = (length >= 0) ? length : u_strlen(s);
    int32_t segmentStart = 0;
    for(int32_t i = 0;;) {
        if(i == sLength) {
            if(segmentStart == 0) {
                // s does not contain any U+FFFE.
                break;
            }
        } else if(s[i] != 0xfffe) {
            ++i;
            continue;
        }
        // Get the sort key for another segment and merge it into mergedKey.
        CollationKey key1(mergedKey.getAlias(), mergedKeyLength);  // copies the bytes
        CollationKey key2;
        coll->getCollationKey(s + segmentStart, i - segmentStart, key2, errorCode);
        int32_t key1Length, key2Length;
        const uint8_t *key1Bytes = key1.getByteArray(key1Length);
        const uint8_t *key2Bytes = key2.getByteArray(key2Length);
        uint8_t *dest;
        int32_t minCapacity = key1Length + key2Length;
        if(key1Length > 0) { --minCapacity; }
        if(minCapacity <= mergedKeyCapacity) {
            dest = mergedKey.getAlias();
        } else {
            if(minCapacity <= 200) {
                mergedKeyCapacity = 200;
            } else if(minCapacity <= 2 * mergedKeyCapacity) {
                mergedKeyCapacity *= 2;
            } else {
                mergedKeyCapacity = minCapacity;
            }
            dest = mergedKey.allocateInsteadAndReset(mergedKeyCapacity);
        }
        U_ASSERT(dest != NULL);
        if(key1Length == 0) {
            // key2 is the sort key for the first segment.
            uprv_memcpy(dest, key2Bytes, key2Length);
            mergedKeyLength = key2Length;
        } else {
            mergedKeyLength =
                ucol_mergeSortkeys(key1Bytes, key1Length, key2Bytes, key2Length,
                                   dest, mergedKeyCapacity);
        }
        if(i == sLength) { break; }
        segmentStart = ++i;
    }
    if(segmentStart != 0 &&
            (mergedKeyLength != keyLength ||
            uprv_memcmp(mergedKey.getAlias(), keyBytes, keyLength) != 0)) {
        errln(fileTestName);
        errln("Collator(normalization=%s).getCollationKey(with U+FFFE) != "
              "ucol_mergeSortkeys(segments)",
              norm);
        errln(line);
        errln(printCollationKey(key));
        errln(printSortKey(mergedKey.getAlias(), mergedKeyLength));
        errorCode.reset();
        return FALSE;
    }

    // Check that nextSortKeyPart() makes the same key, with several part sizes.
    static const int32_t partSizes[] = { 32, 3, 1 };
    for(int32_t psi = 0; psi < LENGTHOF(partSizes); ++psi) {
        int32_t partSize = partSizes[psi];
        CharString parts;
        if(!getSortKeyParts(s, length, parts, 32, errorCode)) {
            errln(fileTestName);
            errln("Collator(normalization=%s).nextSortKeyPart(%d) failed: %s",
                  norm, (int)partSize, errorCode.errorName());
            errln(line);
            errorCode.reset();  // TODO: no need to reset except in IcuTestErrorCode-owning caller?
            return FALSE;
        }
        if(keyLength != parts.length() || uprv_memcmp(keyBytes, parts.data(), keyLength) != 0) {
            errln(fileTestName);
            errln("Collator(normalization=%s).getCollationKey() != nextSortKeyPart(%d)",
                  norm, (int)partSize);
            errln(line);
            errln(printCollationKey(key));
            errln(printSortKey(reinterpret_cast<uint8_t *>(parts.data()), parts.length()));
            errorCode.reset();
            return FALSE;
        }
    }
    return TRUE;
}

namespace {

/**
 * Replaces unpaired surrogates with U+FFFD.
 * Returns s if no replacement was made, otherwise buffer.
 */
const UnicodeString &surrogatesToFFFD(const UnicodeString &s, UnicodeString &buffer) {
    int32_t i = 0;
    while(i < s.length()) {
        UChar32 c = s.char32At(i);
        if(U_IS_SURROGATE(c)) {
            if(buffer.length() < i) {
                buffer.append(s, buffer.length(), i - buffer.length());
            }
            buffer.append((UChar)0xfffd);
        }
        i += U16_LENGTH(c);
    }
    if(buffer.isEmpty()) {
        return s;
    }
    if(buffer.length() < i) {
        buffer.append(s, buffer.length(), i - buffer.length());
    }
    return buffer;
}

}

UBool CollationTest::checkCompareTwo(const UnicodeString &prevFileLine,
                                     const UnicodeString &prevString, const UnicodeString &s,
                                     UCollationResult expectedOrder, Collation::Level expectedLevel,
                                     IcuTestErrorCode &errorCode) {
    const char *norm =
        (coll->getAttribute(UCOL_NORMALIZATION_MODE, errorCode) == UCOL_ON) ?
        "on" : "off";
    if(errorCode.isFailure()) { return FALSE; }

    // Get the sort keys first, for error debug output.
    CollationKey prevKey;
    if(!getCollationKey(prevFileLine, prevString.getBuffer(), prevString.length(),
                        prevKey, errorCode)) {
        return FALSE;
    }
    CollationKey key;
    if(!getCollationKey(fileLine, s.getBuffer(), s.length(), key, errorCode)) { return FALSE; }

// TODO: remove -- printf("line %d coll(%s).compare(prev, s)\n", fileLineNumber, norm);
    UCollationResult order = coll->compare(prevString, s, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compare(previous, current) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errln(printCollationKey(prevKey));
        errln(printCollationKey(key));
        errorCode.reset();
        return FALSE;
    }
// TODO: remove -- printf("line %d coll(%s).compare(s, prev)\n", fileLineNumber, norm);
    order = coll->compare(s, prevString, errorCode);
    if(order != -expectedOrder || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compare(current, previous) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, -expectedOrder, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errln(printCollationKey(prevKey));
        errln(printCollationKey(key));
        errorCode.reset();
        return FALSE;
    }
    // Test NUL-termination if the strings do not contain NUL characters.
    UBool containNUL = prevString.indexOf((UChar)0) >= 0 || s.indexOf((UChar)0) >= 0;
    if(!containNUL) {
// TODO: remove -- printf("line %d coll(%s).compare(prev-NUL, s-NUL)\n", fileLineNumber, norm);
        order = coll->compare(prevString.getBuffer(), -1, s.getBuffer(), -1, errorCode);
        if(order != expectedOrder || errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).compare(previous-NUL, current-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
            errln(prevFileLine);
            errln(fileLine);
            errln(printCollationKey(prevKey));
            errln(printCollationKey(key));
            errorCode.reset();
            return FALSE;
        }
// TODO: remove -- printf("line %d coll(%s).compare(s-NUL, prev-NUL)\n", fileLineNumber, norm);
        order = coll->compare(s.getBuffer(), -1, prevString.getBuffer(), -1, errorCode);
        if(order != -expectedOrder || errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).compare(current-NUL, previous-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, -expectedOrder, errorCode.errorName());
            errln(prevFileLine);
            errln(fileLine);
            errln(printCollationKey(prevKey));
            errln(printCollationKey(key));
            errorCode.reset();
            return FALSE;
        }
    }

#if U_HAVE_STD_STRING
    // compare(UTF-16) treats unpaired surrogates like unassigned code points.
    // Unpaired surrogates cannot be converted to UTF-8.
    // Create valid UTF-16 strings if necessary, and use those for
    // both the expected compare() result and for the input to compare(UTF-8).
    UnicodeString prevBuffer, sBuffer;
    const UnicodeString &prevValid = surrogatesToFFFD(prevString, prevBuffer);
    const UnicodeString &sValid = surrogatesToFFFD(s, sBuffer);
    std::string prevUTF8, sUTF8;
    UnicodeString(prevValid).toUTF8String(prevUTF8);
    UnicodeString(sValid).toUTF8String(sUTF8);
    UCollationResult expectedUTF8Order;
    if(&prevValid == &prevString && &sValid == &s) {
        expectedUTF8Order = expectedOrder;
    } else {
        expectedUTF8Order = coll->compare(prevValid, sValid, errorCode);
    }

    order = coll->compareUTF8(prevUTF8, sUTF8, errorCode);
    if(order != expectedUTF8Order || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compareUTF8(previous, current) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedUTF8Order, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errln(printCollationKey(prevKey));
        errln(printCollationKey(key));
        errorCode.reset();
        return FALSE;
    }
    order = coll->compareUTF8(sUTF8, prevUTF8, errorCode);
    if(order != -expectedUTF8Order || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compareUTF8(current, previous) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, -expectedUTF8Order, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errln(printCollationKey(prevKey));
        errln(printCollationKey(key));
        errorCode.reset();
        return FALSE;
    }
    // Test NUL-termination if the strings do not contain NUL characters.
    if(!containNUL) {
        order = coll->compareUTF8(prevUTF8.c_str(), -1, sUTF8.c_str(), -1, errorCode);
        if(order != expectedUTF8Order || errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).compareUTF8(previous-NUL, current-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, expectedUTF8Order, errorCode.errorName());
            errln(prevFileLine);
            errln(fileLine);
            errln(printCollationKey(prevKey));
            errln(printCollationKey(key));
            errorCode.reset();
            return FALSE;
        }
        order = coll->compareUTF8(sUTF8.c_str(), -1, prevUTF8.c_str(), -1, errorCode);
        if(order != -expectedUTF8Order || errorCode.isFailure()) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).compareUTF8(current-NUL, previous-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, -expectedUTF8Order, errorCode.errorName());
            errln(prevFileLine);
            errln(fileLine);
            errln(printCollationKey(prevKey));
            errln(printCollationKey(key));
            errorCode.reset();
            return FALSE;
        }
    }
#endif

    UCharIterator leftIter;
    UCharIterator rightIter;
    uiter_setString(&leftIter, prevString.getBuffer(), prevString.length());
    uiter_setString(&rightIter, s.getBuffer(), s.length());
    order = coll->compare(leftIter, rightIter, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).compare(UCharIterator: previous, current) "
              "wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errln(printCollationKey(prevKey));
        errln(printCollationKey(key));
        errorCode.reset();
        return FALSE;
    }

    order = prevKey.compareTo(key, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        errln(fileTestName);
        errln("line %d Collator(normalization=%s).getCollationKey(previous, current).compareTo() wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        errln(prevFileLine);
        errln(fileLine);
        errln(printCollationKey(prevKey));
        errln(printCollationKey(key));
        errorCode.reset();
        return FALSE;
    }
    if(order != UCOL_EQUAL && expectedLevel != Collation::NO_LEVEL) {
        int32_t prevKeyLength;
        const uint8_t *prevBytes = prevKey.getByteArray(prevKeyLength);
        int32_t keyLength;
        const uint8_t *bytes = key.getByteArray(keyLength);
        int32_t level = Collation::PRIMARY_LEVEL;
        for(int32_t i = 0;; ++i) {
            uint8_t b = prevBytes[i];
            if(b != bytes[i]) { break; }
            if(b == Collation::LEVEL_SEPARATOR_BYTE) {
                ++level;
                if(level == Collation::CASE_LEVEL &&
                        coll->getAttribute(UCOL_CASE_LEVEL, errorCode) == UCOL_OFF) {
                    ++level;
                }
            }
        }
        if(level != expectedLevel) {
            errln(fileTestName);
            errln("line %d Collator(normalization=%s).getCollationKey(previous, current).compareTo()=%d wrong level: %d != %d",
                  (int)fileLineNumber, norm, order, level, expectedLevel);
            errln(prevFileLine);
            errln(fileLine);
            errln(printCollationKey(prevKey));
            errln(printCollationKey(key));
            return FALSE;
        }
    }
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
        Collation::Level relation = parseRelationAndString(s, errorCode);
        if(errorCode.isFailure()) {
            errorCode.reset();
            break;
        }
        UCollationResult expectedOrder = (relation == Collation::ZERO_LEVEL) ? UCOL_EQUAL : UCOL_LESS;
        Collation::Level expectedLevel = relation;
        s.getTerminatedBuffer();  // Ensure NUL-termination.
        UBool isOk = TRUE;
        if(!needsNormalization(prevString, errorCode) && !needsNormalization(s, errorCode)) {
            coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF, errorCode);
            isOk = checkCompareTwo(prevFileLine, prevString, s, expectedOrder, expectedLevel, errorCode);
        }
        if(isOk) {
            coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, errorCode);
            checkCompareTwo(prevFileLine, prevString, s, expectedOrder, expectedLevel, errorCode);
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
            setRootCollator(errorCode);
            fileLine.remove();
        } else if(fileLine == UNICODE_STRING("@ rawbase", 9)) {
            buildBase(f.getAlias(), errorCode);
        } else if(fileLine == UNICODE_STRING("@ rules", 7)) {
            buildTailoring(f.getAlias(), errorCode);
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
