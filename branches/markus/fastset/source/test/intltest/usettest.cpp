/*
********************************************************************************
*   Copyright (C) 1999-2007 International Business Machines Corporation and
*   others. All Rights Reserved.
********************************************************************************
*   Date        Name        Description
*   10/20/99    alan        Creation.
*   03/22/2000  Madhu       Added additional tests
********************************************************************************
*/

#include <stdio.h>

#include <string.h>
#include "unicode/utypes.h"
#include "usettest.h"
#include "unicode/ucnv.h"
#include "unicode/uniset.h"
#include "unicode/uchar.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "unicode/parsepos.h"
#include "unicode/symtable.h"
#include "unicode/uversion.h"
#include "hash.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

#define TEST_ASSERT_SUCCESS(status) {if (U_FAILURE(status)) { \
    errln("fail in file \"%s\", line %d: \"%s\"", __FILE__, __LINE__, \
    u_errorName(status));}}

#define TEST_ASSERT(expr) {if (!(expr)) { \
    errln("fail in file \"%s\", line %d", __FILE__, __LINE__); }}

UnicodeString operator+(const UnicodeString& left, const UnicodeSet& set) {
    UnicodeString pat;
    set.toPattern(pat);
    return left + UnicodeSetTest::escape(pat);
}

#define CASE(id,test) case id:                          \
                          name = #test;                 \
                          if (exec) {                   \
                              logln(#test "---");       \
                              logln();                  \
                              test();                   \
                          }                             \
                          break

UnicodeSetTest::UnicodeSetTest() : utf8Cnv(NULL) {
}

UConverter *UnicodeSetTest::openUTF8Converter() {
    if(utf8Cnv==NULL) {
        UErrorCode errorCode=U_ZERO_ERROR;
        utf8Cnv=ucnv_open("UTF-8", &errorCode);
    }
    return utf8Cnv;
}

UnicodeSetTest::~UnicodeSetTest() {
    ucnv_close(utf8Cnv);
}

void
UnicodeSetTest::runIndexedTest(int32_t index, UBool exec,
                               const char* &name, char* /*par*/) {
    // if (exec) logln((UnicodeString)"TestSuite UnicodeSetTest");
    switch (index) {
        CASE(0,TestPatterns);
        CASE(1,TestAddRemove);
        CASE(2,TestCategories);
        CASE(3,TestCloneEqualHash);
        CASE(4,TestMinimalRep);
        CASE(5,TestAPI);
        CASE(6,TestScriptSet);
        CASE(7,TestPropertySet);
        CASE(8,TestClone);
        CASE(9,TestExhaustive);
        CASE(10,TestToPattern);
        CASE(11,TestIndexOf);
        CASE(12,TestStrings);
        CASE(13,Testj2268);
        CASE(14,TestCloseOver);
        CASE(15,TestEscapePattern);
        CASE(16,TestInvalidCodePoint);
        CASE(17,TestSymbolTable);
        CASE(18,TestSurrogate);
        CASE(19,TestPosixClasses);
        CASE(20,TestIteration);
        CASE(21,TestFreezable);
        CASE(22,TestSpan);
        CASE(23,TestStringSpan);
        default: name = ""; break;
    }
}

static const char NOT[] = "%%%%";

/** 
 * UVector was improperly copying contents
 * This code will crash this is still true
 */
void UnicodeSetTest::Testj2268() {
  UnicodeSet t;
  t.add(UnicodeString("abc"));
  UnicodeSet test(t);
  UnicodeString ustrPat;
  test.toPattern(ustrPat, TRUE);
}

/**
 * Test toPattern().
 */
void UnicodeSetTest::TestToPattern() {
    UErrorCode ec = U_ZERO_ERROR;

    // Test that toPattern() round trips with syntax characters and
    // whitespace.
    {
        static const char* OTHER_TOPATTERN_TESTS[] = {
            "[[:latin:]&[:greek:]]", 
            "[[:latin:]-[:greek:]]",
            "[:nonspacing mark:]",
            NULL
        };

        for (int32_t j=0; OTHER_TOPATTERN_TESTS[j]!=NULL; ++j) {
            ec = U_ZERO_ERROR;
            UnicodeSet s(OTHER_TOPATTERN_TESTS[j], ec);
            if (U_FAILURE(ec)) {
                errln((UnicodeString)"FAIL: bad pattern " + OTHER_TOPATTERN_TESTS[j]);
                continue;
            }
            checkPat(OTHER_TOPATTERN_TESTS[j], s);
        }
    
        for (UChar32 i = 0; i <= 0x10FFFF; ++i) {
            if ((i <= 0xFF && !u_isalpha(i)) || u_isspace(i)) {

                // check various combinations to make sure they all work.
                if (i != 0 && !toPatternAux(i, i)){
                    continue;
                }
                if (!toPatternAux(0, i)){
                    continue;
                }
                if (!toPatternAux(i, 0xFFFF)){
                    continue;
                }
            }
        }
    }

    // Test pattern behavior of multicharacter strings.
    {
        ec = U_ZERO_ERROR;
        UnicodeSet* s = new UnicodeSet("[a-z {aa} {ab}]", ec);

        // This loop isn't a loop.  It's here to make the compiler happy.
        // If you're curious, try removing it and changing the 'break'
        // statements (except for the last) to goto's.
        for (;;) {
            if (U_FAILURE(ec)) break;
            const char* exp1[] = {"aa", "ab", NOT, "ac", NULL};
            expectToPattern(*s, "[a-z{aa}{ab}]", exp1);

            s->add("ac");
            const char* exp2[] = {"aa", "ab", "ac", NOT, "xy", NULL};
            expectToPattern(*s, "[a-z{aa}{ab}{ac}]", exp2);

            s->applyPattern("[a-z {\\{l} {r\\}}]", ec);
            if (U_FAILURE(ec)) break;
            const char* exp3[] = {"{l", "r}", NOT, "xy", NULL};
            expectToPattern(*s, "[a-z{r\\}}{\\{l}]", exp3);

            s->add("[]");
            const char* exp4[] = {"{l", "r}", "[]", NOT, "xy", NULL};
            expectToPattern(*s, "[a-z{\\[\\]}{r\\}}{\\{l}]", exp4);

            s->applyPattern("[a-z {\\u4E01\\u4E02}{\\n\\r}]", ec);
            if (U_FAILURE(ec)) break;
            const char* exp5[] = {"\\u4E01\\u4E02", "\n\r", NULL};
            expectToPattern(*s, "[a-z{\\u000A\\u000D}{\\u4E01\\u4E02}]", exp5);

            // j2189
            s->clear();
            s->add(UnicodeString("abc", ""));
            s->add(UnicodeString("abc", ""));
            const char* exp6[] = {"abc", NOT, "ab", NULL};
            expectToPattern(*s, "[{abc}]", exp6);

            break;
        }

        if (U_FAILURE(ec)) errln("FAIL: pattern parse error");
        delete s;
    }
 
    // JB#3400: For 2 character ranges prefer [ab] to [a-b]
    UnicodeSet s;
    s.add((UChar)97, (UChar)98); // 'a', 'b'
    expectToPattern(s, "[ab]", NULL);
}
    
UBool UnicodeSetTest::toPatternAux(UChar32 start, UChar32 end) {

    // use Integer.toString because Utility.hex doesn't handle ints
    UnicodeString pat = "";
    // TODO do these in hex
    //String source = "0x" + Integer.toString(start,16).toUpperCase();
    //if (start != end) source += "..0x" + Integer.toString(end,16).toUpperCase();
    UnicodeString source;
    source = source + (uint32_t)start;
    if (start != end) 
        source = source + ".." + (uint32_t)end;
    UnicodeSet testSet;
    testSet.add(start, end);
    return checkPat(source, testSet);
}
    
UBool UnicodeSetTest::checkPat(const UnicodeString& source,
                               const UnicodeSet& testSet) {
    // What we want to make sure of is that a pattern generated
    // by toPattern(), with or without escaped unprintables, can
    // be passed back into the UnicodeSet constructor.
    UnicodeString pat0;

    testSet.toPattern(pat0, TRUE);
    
    if (!checkPat(source + " (escaped)", testSet, pat0)) return FALSE;
    
    //String pat1 = unescapeLeniently(pat0);
    //if (!checkPat(source + " (in code)", testSet, pat1)) return false;
    
    UnicodeString pat2; 
    testSet.toPattern(pat2, FALSE);
    if (!checkPat(source, testSet, pat2)) return FALSE;
    
    //String pat3 = unescapeLeniently(pat2);
    // if (!checkPat(source + " (in code)", testSet, pat3)) return false;
    
    //logln(source + " => " + pat0 + ", " + pat1 + ", " + pat2 + ", " + pat3);
    logln((UnicodeString)source + " => " + pat0 + ", " + pat2);
    return TRUE;
}

UBool UnicodeSetTest::checkPat(const UnicodeString& source,
                               const UnicodeSet& testSet,
                               const UnicodeString& pat) {
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeSet testSet2(pat, ec);
    if (testSet2 != testSet) {
        errln((UnicodeString)"Fail toPattern: " + source + " => " + pat);
        return FALSE;
    }
    return TRUE;
}

void
UnicodeSetTest::TestPatterns(void) {
    UnicodeSet set;
    expectPattern(set, UnicodeString("[[a-m]&[d-z]&[k-y]]", ""),  "km");
    expectPattern(set, UnicodeString("[[a-z]-[m-y]-[d-r]]", ""),  "aczz");
    expectPattern(set, UnicodeString("[a\\-z]", ""),  "--aazz");
    expectPattern(set, UnicodeString("[-az]", ""),  "--aazz");
    expectPattern(set, UnicodeString("[az-]", ""),  "--aazz");
    expectPattern(set, UnicodeString("[[[a-z]-[aeiou]i]]", ""), "bdfnptvz");

    // Throw in a test of complement
    set.complement();
    UnicodeString exp;
    exp.append((UChar)0x0000).append("aeeoouu").append((UChar)(0x007a+1)).append((UChar)0xFFFF);
    expectPairs(set, exp);
}

void
UnicodeSetTest::TestCategories(void) {
    UErrorCode status = U_ZERO_ERROR;
    const char* pat = " [:Lu:] "; // Whitespace ok outside [:..:]
    UnicodeSet set(pat, status);
    if (U_FAILURE(status)) {
        errln((UnicodeString)"Fail: Can't construct set with " + pat);
    } else {
        expectContainment(set, pat, "ABC", "abc");
    }

    UChar32 i;
    int32_t failures = 0;
    // Make sure generation of L doesn't pollute cached Lu set
    // First generate L, then Lu
    set.applyPattern("[:L:]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    for (i=0; i<0x200; ++i) {
        UBool l = u_isalpha((UChar)i);
        if (l != set.contains(i)) {
            errln((UnicodeString)"FAIL: L contains " + (unsigned short)i + " = " + 
                  set.contains(i));
            if (++failures == 10) break;
        }
    }
    
    set.applyPattern("[:Lu:]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    for (i=0; i<0x200; ++i) {
        UBool lu = (u_charType((UChar)i) == U_UPPERCASE_LETTER);
        if (lu != set.contains(i)) {
            errln((UnicodeString)"FAIL: Lu contains " + (unsigned short)i + " = " + 
                  set.contains(i));
            if (++failures == 20) break;
        }
    }
}
void
UnicodeSetTest::TestCloneEqualHash(void) {
    UErrorCode status = U_ZERO_ERROR;
    // set1 and set2 used to be built with the obsolete constructor taking
    // UCharCategory values; replaced with pattern constructors
    // markus 20030502
    UnicodeSet *set1=new UnicodeSet("\\p{Lowercase Letter}", status); //  :Ll: Letter, lowercase
    UnicodeSet *set1a=new UnicodeSet("[:Ll:]", status); //  Letter, lowercase
    if (U_FAILURE(status)){
        errln((UnicodeString)"FAIL: Can't construst set with category->Ll");
        return;
    }
    UnicodeSet *set2=new UnicodeSet("\\p{Decimal Number}", status);   //Number, Decimal digit
    UnicodeSet *set2a=new UnicodeSet("[:Nd:]", status);   //Number, Decimal digit
    if (U_FAILURE(status)){
        errln((UnicodeString)"FAIL: Can't construct set with category->Nd");
        return;
    }

    if (*set1 != *set1a) {
        errln("FAIL: category constructor for Ll broken");
    }
    if (*set2 != *set2a) {
        errln("FAIL: category constructor for Nd broken");
    }
    delete set1a;
    delete set2a;

    logln("Testing copy construction");
    UnicodeSet *set1copy=new UnicodeSet(*set1);
    if(*set1 != *set1copy || *set1 == *set2 || 
        getPairs(*set1) != getPairs(*set1copy) ||
        set1->hashCode() != set1copy->hashCode()){
        errln("FAIL : Error in copy construction");
        return;
    }

    logln("Testing =operator");
    UnicodeSet set1equal=*set1;
    UnicodeSet set2equal=*set2;
    if(set1equal != *set1 || set1equal != *set1copy || set2equal != *set2 || 
        set2equal == *set1 || set2equal == *set1copy || set2equal == set1equal){
        errln("FAIL: Error in =operator");
    }

    logln("Testing clone()");
    UnicodeSet *set1clone=(UnicodeSet*)set1->clone();
    UnicodeSet *set2clone=(UnicodeSet*)set2->clone();
    if(*set1clone != *set1 || *set1clone != *set1copy || *set1clone != set1equal || 
        *set2clone != *set2 || *set2clone == *set1copy || *set2clone != set2equal || 
        *set2clone == *set1 || *set2clone == set1equal || *set2clone == *set1clone){
        errln("FAIL: Error in clone");
    }

    logln("Testing hashcode");
    if(set1->hashCode() != set1equal.hashCode() || set1->hashCode() != set1clone->hashCode() ||
        set2->hashCode() != set2equal.hashCode() || set2->hashCode() != set2clone->hashCode() ||
        set1copy->hashCode() != set1equal.hashCode() || set1copy->hashCode() != set1clone->hashCode() ||
        set1->hashCode() == set2->hashCode()  || set1copy->hashCode() == set2->hashCode() ||
        set2->hashCode() == set1clone->hashCode() || set2->hashCode() == set1equal.hashCode() ){
        errln("FAIL: Error in hashCode()");
    }

    delete set1;
    delete set1copy;
    delete set2;
    delete set1clone;
    delete set2clone;


}
void
UnicodeSetTest::TestAddRemove(void) {
    UnicodeSet set; // Construct empty set
    doAssert(set.isEmpty() == TRUE, "set should be empty");
    doAssert(set.size() == 0, "size should be 0");
    set.complement();
    doAssert(set.size() == 0x110000, "size should be 0x110000");
    set.clear();
    set.add(0x0061, 0x007a);
    expectPairs(set, "az");
    doAssert(set.isEmpty() == FALSE, "set should not be empty");
    doAssert(set.size() != 0, "size should not be equal to 0");
    doAssert(set.size() == 26, "size should be equal to 26");
    set.remove(0x006d, 0x0070);
    expectPairs(set, "alqz");
    doAssert(set.size() == 22, "size should be equal to 22");
    set.remove(0x0065, 0x0067);
    expectPairs(set, "adhlqz");
    doAssert(set.size() == 19, "size should be equal to 19");
    set.remove(0x0064, 0x0069);
    expectPairs(set, "acjlqz");
    doAssert(set.size() == 16, "size should be equal to 16");
    set.remove(0x0063, 0x0072);
    expectPairs(set, "absz");
    doAssert(set.size() == 10, "size should be equal to 10");
    set.add(0x0066, 0x0071);
    expectPairs(set, "abfqsz");
    doAssert(set.size() == 22, "size should be equal to 22");
    set.remove(0x0061, 0x0067);
    expectPairs(set, "hqsz");
    set.remove(0x0061, 0x007a);
    expectPairs(set, "");
    doAssert(set.isEmpty() == TRUE, "set should be empty");
    doAssert(set.size() == 0, "size should be 0");
    set.add(0x0061);
    doAssert(set.isEmpty() == FALSE, "set should not be empty");
    doAssert(set.size() == 1, "size should not be equal to 1");
    set.add(0x0062);
    set.add(0x0063);
    expectPairs(set, "ac");
    doAssert(set.size() == 3, "size should not be equal to 3");
    set.add(0x0070);
    set.add(0x0071);
    expectPairs(set, "acpq");
    doAssert(set.size() == 5, "size should not be equal to 5");
    set.clear();
    expectPairs(set, "");
    doAssert(set.isEmpty() == TRUE, "set should be empty");
    doAssert(set.size() == 0, "size should be 0");

    // Try removing an entire set from another set
    expectPattern(set, "[c-x]", "cx");
    UnicodeSet set2;
    expectPattern(set2, "[f-ky-za-bc[vw]]", "acfkvwyz");
    set.removeAll(set2);
    expectPairs(set, "deluxx");

    // Try adding an entire set to another set
    expectPattern(set, "[jackiemclean]", "aacceein");
    expectPattern(set2, "[hitoshinamekatajamesanderson]", "aadehkmort");
    set.addAll(set2);
    expectPairs(set, "aacehort");
    doAssert(set.containsAll(set2) == TRUE, "set should contain all the elements in set2");

    // Try retaining an set of elements contained in another set (intersection)
    UnicodeSet set3;
    expectPattern(set3, "[a-c]", "ac");
    doAssert(set.containsAll(set3) == FALSE, "set doesn't contain all the elements in set3");
    set3.remove(0x0062);
    expectPairs(set3, "aacc");
    doAssert(set.containsAll(set3) == TRUE, "set should contain all the elements in set3");
    set.retainAll(set3);
    expectPairs(set, "aacc");
    doAssert(set.size() == set3.size(), "set.size() should be set3.size()");
    doAssert(set.containsAll(set3) == TRUE, "set should contain all the elements in set3");
    set.clear();
    doAssert(set.size() != set3.size(), "set.size() != set3.size()");

    // Test commutativity
    expectPattern(set, "[hitoshinamekatajamesanderson]", "aadehkmort");
    expectPattern(set2, "[jackiemclean]", "aacceein");
    set.addAll(set2);
    expectPairs(set, "aacehort");
    doAssert(set.containsAll(set2) == TRUE, "set should contain all the elements in set2");




}

/**
 * Make sure minimal representation is maintained.
 */
void UnicodeSetTest::TestMinimalRep() {
    UErrorCode status = U_ZERO_ERROR;
    // This is pretty thoroughly tested by checkCanonicalRep()
    // run against the exhaustive operation results.  Use the code
    // here for debugging specific spot problems.

    // 1 overlap against 2
    UnicodeSet set("[h-km-q]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    UnicodeSet set2("[i-o]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set.addAll(set2);
    expectPairs(set, "hq");
    // right
    set.applyPattern("[a-m]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set2.applyPattern("[e-o]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set.addAll(set2);
    expectPairs(set, "ao");
    // left
    set.applyPattern("[e-o]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set2.applyPattern("[a-m]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set.addAll(set2);
    expectPairs(set, "ao");
    // 1 overlap against 3
    set.applyPattern("[a-eg-mo-w]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set2.applyPattern("[d-q]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    set.addAll(set2);
    expectPairs(set, "aw");
}

void UnicodeSetTest::TestAPI() {
    UErrorCode status = U_ZERO_ERROR;
    // default ct
    UnicodeSet set;
    if (!set.isEmpty() || set.getRangeCount() != 0) {
        errln((UnicodeString)"FAIL, set should be empty but isn't: " +
              set);
    }

    // clear(), isEmpty()
    set.add(0x0061);
    if (set.isEmpty()) {
        errln((UnicodeString)"FAIL, set shouldn't be empty but is: " +
              set);
    }
    set.clear();
    if (!set.isEmpty()) {
        errln((UnicodeString)"FAIL, set should be empty but isn't: " +
              set);
    }

    // size()
    set.clear();
    if (set.size() != 0) {
        errln((UnicodeString)"FAIL, size should be 0, but is " + set.size() +
              ": " + set);
    }
    set.add(0x0061);
    if (set.size() != 1) {
        errln((UnicodeString)"FAIL, size should be 1, but is " + set.size() +
              ": " + set);
    }
    set.add(0x0031, 0x0039);
    if (set.size() != 10) {
        errln((UnicodeString)"FAIL, size should be 10, but is " + set.size() +
              ": " + set);
    }

    // contains(first, last)
    set.clear();
    set.applyPattern("[A-Y 1-8 b-d l-y]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    for (int32_t i = 0; i<set.getRangeCount(); ++i) {
        UChar32 a = set.getRangeStart(i);
        UChar32 b = set.getRangeEnd(i);
        if (!set.contains(a, b)) {
            errln((UnicodeString)"FAIL, should contain " + (unsigned short)a + '-' + (unsigned short)b +
                  " but doesn't: " + set);
        }
        if (set.contains((UChar32)(a-1), b)) {
            errln((UnicodeString)"FAIL, shouldn't contain " +
                  (unsigned short)(a-1) + '-' + (unsigned short)b +
                  " but does: " + set);
        }
        if (set.contains(a, (UChar32)(b+1))) {
            errln((UnicodeString)"FAIL, shouldn't contain " +
                  (unsigned short)a + '-' + (unsigned short)(b+1) +
                  " but does: " + set);
        }
    }

    // Ported InversionList test.
    UnicodeSet a((UChar32)3,(UChar32)10);
    UnicodeSet b((UChar32)7,(UChar32)15);
    UnicodeSet c;

    logln((UnicodeString)"a [3-10]: " + a);
    logln((UnicodeString)"b [7-15]: " + b);
    c = a;
    c.addAll(b);
    UnicodeSet exp((UChar32)3,(UChar32)15);
    if (c == exp) {
        logln((UnicodeString)"c.set(a).add(b): " + c);
    } else {
        errln((UnicodeString)"FAIL: c.set(a).add(b) = " + c + ", expect " + exp);
    }
    c.complement();
    exp.set((UChar32)0, (UChar32)2);
    exp.add((UChar32)16, UnicodeSet::MAX_VALUE);
    if (c == exp) {
        logln((UnicodeString)"c.complement(): " + c);
    } else {
        errln((UnicodeString)"FAIL: c.complement() = " + c + ", expect " + exp);
    }
    c.complement();
    exp.set((UChar32)3, (UChar32)15);
    if (c == exp) {
        logln((UnicodeString)"c.complement(): " + c);
    } else {
        errln((UnicodeString)"FAIL: c.complement() = " + c + ", expect " + exp);
    }
    c = a;
    c.complementAll(b);
    exp.set((UChar32)3,(UChar32)6);
    exp.add((UChar32)11,(UChar32) 15);
    if (c == exp) {
        logln((UnicodeString)"c.set(a).exclusiveOr(b): " + c);
    } else {
        errln((UnicodeString)"FAIL: c.set(a).exclusiveOr(b) = " + c + ", expect " + exp);
    }

    exp = c;
    bitsToSet(setToBits(c), c);
    if (c == exp) {
        logln((UnicodeString)"bitsToSet(setToBits(c)): " + c);
    } else {
        errln((UnicodeString)"FAIL: bitsToSet(setToBits(c)) = " + c + ", expect " + exp);
    }

    // Additional tests for coverage JB#2118
    //UnicodeSet::complement(class UnicodeString const &)
    //UnicodeSet::complementAll(class UnicodeString const &)
    //UnicodeSet::containsNone(class UnicodeSet const &)
    //UnicodeSet::containsNone(long,long)
    //UnicodeSet::containsSome(class UnicodeSet const &)
    //UnicodeSet::containsSome(long,long)
    //UnicodeSet::removeAll(class UnicodeString const &)
    //UnicodeSet::retain(long)
    //UnicodeSet::retainAll(class UnicodeString const &)
    //UnicodeSet::serialize(unsigned short *,long,enum UErrorCode &)
    //UnicodeSetIterator::getString(void)
    set.clear();
    set.complement("ab");
    exp.applyPattern("[{ab}]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (set != exp) { errln("FAIL: complement(\"ab\")"); return; }
    
    UnicodeSetIterator iset(set);
    if (!iset.next() || !iset.isString()) {
        errln("FAIL: UnicodeSetIterator::next/isString");
    } else if (iset.getString() != "ab") {
        errln("FAIL: UnicodeSetIterator::getString");
    }

    set.add((UChar32)0x61, (UChar32)0x7A);
    set.complementAll("alan");
    exp.applyPattern("[{ab}b-kmo-z]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (set != exp) { errln("FAIL: complementAll(\"alan\")"); return; }

    exp.applyPattern("[a-z]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (set.containsNone(exp)) { errln("FAIL: containsNone(UnicodeSet)"); }
    if (!set.containsSome(exp)) { errln("FAIL: containsSome(UnicodeSet)"); }
    exp.applyPattern("[aln]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (!set.containsNone(exp)) { errln("FAIL: containsNone(UnicodeSet)"); }
    if (set.containsSome(exp)) { errln("FAIL: containsSome(UnicodeSet)"); }

    if (set.containsNone((UChar32)0x61, (UChar32)0x7A)) {
        errln("FAIL: containsNone(UChar32, UChar32)");
    }
    if (!set.containsSome((UChar32)0x61, (UChar32)0x7A)) {
        errln("FAIL: containsSome(UChar32, UChar32)");
    }
    if (!set.containsNone((UChar32)0x41, (UChar32)0x5A)) {
        errln("FAIL: containsNone(UChar32, UChar32)");
    }
    if (set.containsSome((UChar32)0x41, (UChar32)0x5A)) {
        errln("FAIL: containsSome(UChar32, UChar32)");
    }

    set.removeAll("liu");
    exp.applyPattern("[{ab}b-hj-kmo-tv-z]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (set != exp) { errln("FAIL: removeAll(\"liu\")"); return; }

    set.retainAll("star");
    exp.applyPattern("[rst]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (set != exp) { errln("FAIL: retainAll(\"star\")"); return; }

    set.retain((UChar32)0x73);
    exp.applyPattern("[s]", status);
    if (U_FAILURE(status)) { errln("FAIL"); return; }
    if (set != exp) { errln("FAIL: retain('s')"); return; }

    uint16_t buf[32];
    int32_t slen = set.serialize(buf, sizeof(buf)/sizeof(buf[0]), status);
    if (U_FAILURE(status)) { errln("FAIL: serialize"); return; }
    if (slen != 3 || buf[0] != 2 || buf[1] != 0x73 || buf[2] != 0x74) {
        errln("FAIL: serialize");
        return;
    }
}

void UnicodeSetTest::TestIteration() {
    UErrorCode ec = U_ZERO_ERROR;
    int i = 0;
    int outerLoop;
    
    // 6 code points, 3 ranges, 2 strings, 8 total elements
    //   Iteration will access them in sorted order -  a, b, c, y, z, U0001abcd, "str1", "str2"
    UnicodeSet set("[zabyc\\U0001abcd{str1}{str2}]", ec);
    TEST_ASSERT_SUCCESS(ec);
    UnicodeSetIterator it(set);

    for (outerLoop=0; outerLoop<3; outerLoop++) {
        // Run the test multiple times, to check that iterator.reset() is working.
        for (i=0; i<10; i++) {
            UBool         nextv        = it.next();
            UBool         isString     = it.isString();
            int32_t       codePoint    = it.getCodepoint();
            //int32_t       codePointEnd = it.getCodepointEnd();
            UnicodeString s   = it.getString();
            switch (i) {
            case 0:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == FALSE);
                TEST_ASSERT(codePoint==0x61);
                TEST_ASSERT(s == "a");
                break;
            case 1:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == FALSE);
                TEST_ASSERT(codePoint==0x62);
                TEST_ASSERT(s == "b");
                break;
            case 2:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == FALSE);
                TEST_ASSERT(codePoint==0x63);
                TEST_ASSERT(s == "c");
                break;
            case 3:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == FALSE);
                TEST_ASSERT(codePoint==0x79);
                TEST_ASSERT(s == "y");
                break;
            case 4:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == FALSE);
                TEST_ASSERT(codePoint==0x7a);
                TEST_ASSERT(s == "z");
                break;
            case 5:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == FALSE);
                TEST_ASSERT(codePoint==0x1abcd);
                TEST_ASSERT(s == UnicodeString((UChar32)0x1abcd));
                break;
            case 6:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == TRUE);
                TEST_ASSERT(s == "str1");
                break;
            case 7:
                TEST_ASSERT(nextv == TRUE);
                TEST_ASSERT(isString == TRUE);
                TEST_ASSERT(s == "str2");
                break;
            case 8:
                TEST_ASSERT(nextv == FALSE);
                break;
            case 9:
                TEST_ASSERT(nextv == FALSE);
                break;
            }
        }
        it.reset();  // prepare to run the iteration again.
    }
}
                



void UnicodeSetTest::TestStrings() {
    UErrorCode ec = U_ZERO_ERROR;
    
    UnicodeSet* testList[] = {
        UnicodeSet::createFromAll("abc"),
        new UnicodeSet("[a-c]", ec),
        
        &(UnicodeSet::createFrom("ch")->add('a','z').add("ll")),
        new UnicodeSet("[{ll}{ch}a-z]", ec),
    
        UnicodeSet::createFrom("ab}c"),
        new UnicodeSet("[{ab\\}c}]", ec),

        &((new UnicodeSet('a','z'))->add('A', 'Z').retain('M','m').complement('X')), 
        new UnicodeSet("[[a-zA-Z]&[M-m]-[X]]", ec),

        NULL
    };

    if (U_FAILURE(ec)) {
        errln("FAIL: couldn't construct test sets");
    }

    for (int32_t i = 0; testList[i] != NULL; i+=2) {
        if (U_SUCCESS(ec)) {
            UnicodeString pat0, pat1;
            testList[i]->toPattern(pat0, TRUE);
            testList[i+1]->toPattern(pat1, TRUE);
            if (*testList[i] == *testList[i+1]) {
                logln((UnicodeString)"Ok: " + pat0 + " == " + pat1);
            } else {
                logln((UnicodeString)"FAIL: " + pat0 + " != " + pat1);
            }
        }
        delete testList[i];
        delete testList[i+1];
    }        
}

/**
 * Test the [:Latin:] syntax.
 */
void UnicodeSetTest::TestScriptSet() {
    expectContainment("[:Latin:]", "aA", CharsToUnicodeString("\\u0391\\u03B1"));

    expectContainment("[:Greek:]", CharsToUnicodeString("\\u0391\\u03B1"), "aA");
    
    /* Jitterbug 1423 */
    expectContainment("[[:Common:][:Inherited:]]", CharsToUnicodeString("\\U00003099\\U0001D169\\u0000"), "aA");

}

/**
 * Test the [:Latin:] syntax.
 */
void UnicodeSetTest::TestPropertySet() {
    static const char* const DATA[] = {
        // Pattern, Chars IN, Chars NOT in

        "[:Latin:]",
        "aA",
        "\\u0391\\u03B1",

        "[\\p{Greek}]",
        "\\u0391\\u03B1",
        "aA",

        "\\P{ GENERAL Category = upper case letter }",
        "abc",
        "ABC",

        // Combining class: @since ICU 2.2
        // Check both symbolic and numeric
        "\\p{ccc=Nukta}",
        "\\u0ABC",
        "abc",

        "\\p{Canonical Combining Class = 11}",
        "\\u05B1",
        "\\u05B2",

        "[:c c c = iota subscript :]",
        "\\u0345",
        "xyz",

        // Bidi class: @since ICU 2.2
        "\\p{bidiclass=lefttoright}",
        "abc",
        "\\u0671\\u0672",

        // Binary properties: @since ICU 2.2
        "\\p{ideographic}",
        "\\u4E0A",
        "x",

        "[:math=false:]",
        "q)*(",
        // weiv: )(and * were removed from math in Unicode 4.0.1
        //"(*+)",
        "+<>^",

        // JB#1767 \N{}, \p{ASCII}
        "[:Ascii:]",
        "abc\\u0000\\u007F",
        "\\u0080\\u4E00",
        
        "[\\N{ latin small letter  a  }[:name= latin small letter z:]]",
        "az",
        "qrs",

        // JB#2015
        "[:any:]",
        "a\\U0010FFFF",
        "",

        "[:nv=0.5:]",
        "\\u00BD\\u0F2A",
        "\\u00BC",

        // JB#2653: Age
        "[:Age=1.1:]",
        "\\u03D6", // 1.1
        "\\u03D8\\u03D9", // 3.2
        
        "[:Age=3.1:]",
        "\\u1800\\u3400\\U0002f800",
        "\\u0220\\u034f\\u30ff\\u33ff\\ufe73\\U00010000\\U00050000",

        // JB#2350: Case_Sensitive
        "[:Case Sensitive:]",
        "A\\u1FFC\\U00010410",
        ";\\u00B4\\U00010500",

        // JB#2832: C99-compatibility props
        "[:blank:]",
        " \\u0009",
        "1-9A-Z",

        "[:graph:]",
        "19AZ",
        " \\u0003\\u0007\\u0009\\u000A\\u000D",

        "[:punct:]",
        "!@#%&*()[]{}-_\\/;:,.?'\"",
        "09azAZ",

        "[:xdigit:]",
        "09afAF",
        "gG!",

        // Regex compatibility test
        "[-b]", // leading '-' is literal
        "-b",
        "ac",

        "[^-b]", // leading '-' is literal
        "ac",
        "-b",

        "[b-]", // trailing '-' is literal
        "-b",
        "ac",

        "[^b-]", // trailing '-' is literal
        "ac",
        "-b",

        "[a-b-]", // trailing '-' is literal
        "ab-",
        "c=",
        
        "[[a-q]&[p-z]-]", // trailing '-' is literal
        "pq-",
        "or=",

        "[\\s|\\)|:|$|\\>]", // from regex tests
        "s|):$>",
        "abc",

        "[\\uDC00cd]", // JB#2906: isolated trail at start
        "cd\\uDC00",
        "ab\\uD800\\U00010000",
        
        "[ab\\uD800]", // JB#2906: isolated trail at start
        "ab\\uD800",
        "cd\\uDC00\\U00010000",
        
        "[ab\\uD800cd]", // JB#2906: isolated lead in middle
        "abcd\\uD800",
        "ef\\uDC00\\U00010000",
        
        "[ab\\uDC00cd]", // JB#2906: isolated trail in middle
        "abcd\\uDC00",
        "ef\\uD800\\U00010000",

        "[:^lccc=0:]", // Lead canonical class
        "\\u0300\\u0301",
        "abcd\\u00c0\\u00c5",

        "[:^tccc=0:]", // Trail canonical class
        "\\u0300\\u0301\\u00c0\\u00c5",
        "abcd",

        "[[:^lccc=0:][:^tccc=0:]]", // Lead and trail canonical class
        "\\u0300\\u0301\\u00c0\\u00c5",
        "abcd",

        "[[:^lccc=0:]-[:^tccc=0:]]", // Stuff that starts with an accent but ends with a base (none right now)
        "",
        "abcd\\u0300\\u0301\\u00c0\\u00c5",
        
        "[[:ccc=0:]-[:lccc=0:]-[:tccc=0:]]", // Weirdos. Complete canonical class is zero, but both lead and trail are not
        "\\u0F73\\u0F75\\u0F81",
        "abcd\\u0300\\u0301\\u00c0\\u00c5",

        "[:Assigned:]",
        "A\\uE000\\uF8FF\\uFDC7\\U00010000\\U0010FFFD",
        "\\u0888\\uFDD3\\uFFFE\\U00050005"
    };

    static const int32_t DATA_LEN = sizeof(DATA)/sizeof(DATA[0]);

    for (int32_t i=0; i<DATA_LEN; i+=3) {  
        expectContainment(DATA[i], CharsToUnicodeString(DATA[i+1]),
                          CharsToUnicodeString(DATA[i+2]));
    }
}

/**
  * Test that Posix style character classes [:digit:], etc.
  *   have the Unicode definitions from TR 18.
  */
void UnicodeSetTest::TestPosixClasses() {
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:alpha:]", status);
        UnicodeSet s2("\\p{Alphabetic}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:lower:]", status);
        UnicodeSet s2("\\p{lowercase}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:upper:]", status);
        UnicodeSet s2("\\p{Uppercase}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:punct:]", status);
        UnicodeSet s2("\\p{gc=Punctuation}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:digit:]", status);
        UnicodeSet s2("\\p{gc=DecimalNumber}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:xdigit:]", status);
        UnicodeSet s2("[\\p{DecimalNumber}\\p{HexDigit}]", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:alnum:]", status);
        UnicodeSet s2("[\\p{Alphabetic}\\p{DecimalNumber}]", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:space:]", status);
        UnicodeSet s2("\\p{Whitespace}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:blank:]", status);
        TEST_ASSERT_SUCCESS(status);
        UnicodeSet s2("[\\p{Whitespace}-[\\u000a\\u000B\\u000c\\u000d\\u0085\\p{LineSeparator}\\p{ParagraphSeparator}]]",
            status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:cntrl:]", status);
        TEST_ASSERT_SUCCESS(status);
        UnicodeSet s2("\\p{Control}", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:graph:]", status);
        TEST_ASSERT_SUCCESS(status);
        UnicodeSet s2("[^\\p{Whitespace}\\p{Control}\\p{Surrogate}\\p{Unassigned}]", status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeSet s1("[:print:]", status);
        TEST_ASSERT_SUCCESS(status);
        UnicodeSet s2("[[:graph:][:blank:]-[\\p{Control}]]" ,status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(s1==s2);
    }
}
/**
 * Test cloning of UnicodeSet.  For C++, we test the copy constructor.
 */
void UnicodeSetTest::TestClone() {
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeSet s("[abcxyz]", ec);
    UnicodeSet t(s);
    expectContainment(t, "abc", "def");
}

/**
 * Test the indexOf() and charAt() methods.
 */
void UnicodeSetTest::TestIndexOf() {
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeSet set("[a-cx-y3578]", ec);
    if (U_FAILURE(ec)) {
        errln("FAIL: UnicodeSet constructor");
        return;
    }
    for (int32_t i=0; i<set.size(); ++i) {
        UChar32 c = set.charAt(i);
        if (set.indexOf(c) != i) {
            errln("FAIL: charAt(%d) = %X => indexOf() => %d",
                i, c, set.indexOf(c));
        }
    }
    UChar32 c = set.charAt(set.size());
    if (c != -1) {
        errln("FAIL: charAt(<out of range>) = %X", c);
    }
    int32_t j = set.indexOf((UChar32)0x71/*'q'*/);
    if (j != -1) {
        errln((UnicodeString)"FAIL: indexOf('q') = " + j);
    }
}

/**
 * Test closure API.
 */
void UnicodeSetTest::TestCloseOver() {
    UErrorCode ec = U_ZERO_ERROR;

    char CASE[] = {(char)USET_CASE_INSENSITIVE};
    char CASE_MAPPINGS[] = {(char)USET_ADD_CASE_MAPPINGS};
    const char* DATA[] = {
        // selector, input, output
        CASE,
        "[aq\\u00DF{Bc}{bC}{Fi}]",
        "[aAqQ\\u00DF\\uFB01{ss}{bc}{fi}]",

        CASE,
        "[\\u01F1]", // 'DZ'
        "[\\u01F1\\u01F2\\u01F3]",

        CASE,
        "[\\u1FB4]",
        "[\\u1FB4{\\u03AC\\u03B9}]",

        CASE,
        "[{F\\uFB01}]",
        "[\\uFB03{ffi}]",            

        CASE, // make sure binary search finds limits
        "[a\\uFF3A]",
        "[aA\\uFF3A\\uFF5A]",

        CASE,
        "[a-z]","[A-Za-z\\u017F\\u212A]",
        CASE,
        "[abc]","[A-Ca-c]",
        CASE,
        "[ABC]","[A-Ca-c]",

        CASE, "[i]", "[iI]",

        CASE, "[\\u0130]",          "[\\u0130{i\\u0307}]", // dotted I
        CASE, "[{i\\u0307}]",       "[\\u0130{i\\u0307}]", // i with dot

        CASE, "[\\u0131]",          "[\\u0131]", // dotless i

        CASE, "[\\u0390]",          "[\\u0390\\u1FD3{\\u03B9\\u0308\\u0301}]",

        CASE, "[\\u03c2]",          "[\\u03a3\\u03c2\\u03c3]", // sigmas

        CASE, "[\\u03f2]",          "[\\u03f2\\u03f9]", // lunate sigmas

        CASE, "[\\u03f7]",          "[\\u03f7\\u03f8]",

        CASE, "[\\u1fe3]",          "[\\u03b0\\u1fe3{\\u03c5\\u0308\\u0301}]",

        CASE, "[\\ufb05]",          "[\\ufb05\\ufb06{st}]",
        CASE, "[{st}]",             "[\\ufb05\\ufb06{st}]",

        CASE, "[\\U0001044F]",      "[\\U00010427\\U0001044F]",

        CASE, "[{a\\u02BE}]",       "[\\u1E9A{a\\u02BE}]", // first in sorted table

        CASE, "[{\\u1f7c\\u03b9}]", "[\\u1ff2{\\u1f7c\\u03b9}]", // last in sorted table

        CASE_MAPPINGS,
        "[aq\\u00DF{Bc}{bC}{Fi}]",
        "[aAqQ\\u00DF{ss}{Ss}{SS}{Bc}{BC}{bC}{bc}{FI}{Fi}{fi}]",

        CASE_MAPPINGS,
        "[\\u01F1]", // 'DZ'
        "[\\u01F1\\u01F2\\u01F3]",
        
        CASE_MAPPINGS,
        "[a-z]",
        "[A-Za-z]",

        NULL
    };

    UnicodeSet s;
    UnicodeSet t;
    UnicodeString buf;
    for (int32_t i=0; DATA[i]!=NULL; i+=3) {
        int32_t selector = DATA[i][0];
        UnicodeString pat(DATA[i+1]);
        UnicodeString exp(DATA[i+2]);
        s.applyPattern(pat, ec);
        s.closeOver(selector);
        t.applyPattern(exp, ec);
        if (U_FAILURE(ec)) {
            errln("FAIL: applyPattern failed");
            continue;
        }
        if (s == t) {
            logln((UnicodeString)"Ok: " + pat + ".closeOver(" + selector + ") => " + exp);
        } else {
            errln((UnicodeString)"FAIL: " + pat + ".closeOver(" + selector + ") => " +
                  s.toPattern(buf, TRUE) + ", expected " + exp);
        }
    }

#if 0
    /*
     * Unused test code.
     * This was used to compare the old implementation (using USET_CASE)
     * with the new one (using 0x100 temporarily)
     * while transitioning from hardcoded case closure tables in uniset.cpp
     * (moved to uniset_props.cpp) to building the data by gencase into ucase.icu.
     * and using ucase.c functions for closure.
     * See Jitterbug 3432 RFE: Move uniset.cpp data to a data file
     *
     * Note: The old and new implementation never fully matched because
     * the old implementation turned out to not map U+0130 and U+0131 correctly
     * (dotted I and dotless i) and because the old implementation's data tables
     * were outdated compared to Unicode 4.0.1 at the time of the change to the
     * new implementation. (So sigmas and some other characters were not handled
     * according to the newer Unicode version.)
     */
    UnicodeSet sens("[:case_sensitive:]", ec), sens2, s2;
    UnicodeSetIterator si(sens);
    UnicodeString str, buf2;
    const UnicodeString *pStr;
    UChar32 c;
    while(si.next()) {
        if(!si.isString()) {
            c=si.getCodepoint();
            s.clear();
            s.add(c);

            str.setTo(c);
            str.foldCase();
            sens2.add(str);

            t=s;
            s.closeOver(USET_CASE);
            t.closeOver(0x100);
            if(s!=t) {
                errln("FAIL: closeOver(U+%04x) differs: ", c);
                errln((UnicodeString)"old "+s.toPattern(buf, TRUE)+" new: "+t.toPattern(buf2, TRUE));
            }
        }
    }
    // remove all code points
    // should contain all full case folding mapping strings
    sens2.remove(0, 0x10ffff);
    si.reset(sens2);
    while(si.next()) {
        if(si.isString()) {
            pStr=&si.getString();
            s.clear();
            s.add(*pStr);
            t=s2=s;
            s.closeOver(USET_CASE);
            t.closeOver(0x100);
            if(s!=t) {
                errln((UnicodeString)"FAIL: closeOver("+s2.toPattern(buf, TRUE)+") differs: ");
                errln((UnicodeString)"old "+s.toPattern(buf, TRUE)+" new: "+t.toPattern(buf2, TRUE));
            }
        }
    }
#endif

    // Test the pattern API
    s.applyPattern("[abc]", USET_CASE_INSENSITIVE, NULL, ec);
    if (U_FAILURE(ec)) {
        errln("FAIL: applyPattern failed");
    } else {
        expectContainment(s, "abcABC", "defDEF");
    }
    UnicodeSet v("[^abc]", USET_CASE_INSENSITIVE, NULL, ec);
    if (U_FAILURE(ec)) {
        errln("FAIL: constructor failed");
    } else {
        expectContainment(v, "defDEF", "abcABC");
    }
    UnicodeSet cm("[abck]", USET_ADD_CASE_MAPPINGS, NULL, ec);
    if (U_FAILURE(ec)) {
        errln("FAIL: construct w/case mappings failed");
    } else {
        expectContainment(cm, "abckABCK", CharsToUnicodeString("defDEF\\u212A"));
    }
}

void UnicodeSetTest::TestEscapePattern() {
    const char pattern[] =
        "[\\uFEFF \\u200A-\\u200E \\U0001D173-\\U0001D17A \\U000F0000-\\U000FFFFD ]";
    const char exp[] =
        "[\\u200A-\\u200E\\uFEFF\\U0001D173-\\U0001D17A\\U000F0000-\\U000FFFFD]";
    // We test this with two passes; in the second pass we
    // pre-unescape the pattern.  Since U+200E is rule whitespace,
    // this fails -- which is what we expect.
    for (int32_t pass=1; pass<=2; ++pass) {
        UErrorCode ec = U_ZERO_ERROR;
        UnicodeString pat(pattern);
        if (pass==2) {
            pat = pat.unescape();
        }
        // Pattern is only good for pass 1
        UBool isPatternValid = (pass==1);

        UnicodeSet set(pat, ec);
        if (U_SUCCESS(ec) != isPatternValid){
            errln((UnicodeString)"FAIL: applyPattern(" +
                  escape(pat) + ") => " +
                  u_errorName(ec));
            continue;
        }
        if (U_FAILURE(ec)) {
            continue;
        }
        if (set.contains((UChar)0x0644)){
            errln((UnicodeString)"FAIL: " + escape(pat) + " contains(U+0664)");
        }

        UnicodeString newpat;
        set.toPattern(newpat, TRUE);
        if (newpat == exp) {
            logln(escape(pat) + " => " + newpat);
        } else {
            errln((UnicodeString)"FAIL: " + escape(pat) + " => " + newpat);
        }

        for (int32_t i=0; i<set.getRangeCount(); ++i) {
            UnicodeString str("Range ");
            str.append((UChar)(0x30 + i))
                .append(": ")
                .append((UChar32)set.getRangeStart(i))
                .append(" - ")
                .append((UChar32)set.getRangeEnd(i));
            str = str + " (" + set.getRangeStart(i) + " - " +
                set.getRangeEnd(i) + ")";
            if (set.getRangeStart(i) < 0) {
                errln((UnicodeString)"FAIL: " + escape(str));
            } else {
                logln(escape(str));
            }
        }
    }
}

void UnicodeSetTest::expectRange(const UnicodeString& label,
                                 const UnicodeSet& set,
                                 UChar32 start, UChar32 end) {
    UnicodeSet exp(start, end);
    UnicodeString pat;
    if (set == exp) {
        logln(label + " => " + set.toPattern(pat, TRUE));
    } else {
        UnicodeString xpat;
        errln((UnicodeString)"FAIL: " + label + " => " +
              set.toPattern(pat, TRUE) +
              ", expected " + exp.toPattern(xpat, TRUE));
    }
}

void UnicodeSetTest::TestInvalidCodePoint() {

    const UChar32 DATA[] = {
        // Test range             Expected range
        0, 0x10FFFF,              0, 0x10FFFF,
        (UChar32)-1, 8,           0, 8,
        8, 0x110000,              8, 0x10FFFF
    };
    const int32_t DATA_LENGTH = sizeof(DATA)/sizeof(DATA[0]);

    UnicodeString pat;
    int32_t i;

    for (i=0; i<DATA_LENGTH; i+=4) {
        UChar32 start  = DATA[i];
        UChar32 end    = DATA[i+1];
        UChar32 xstart = DATA[i+2];
        UChar32 xend   = DATA[i+3];

        // Try various API using the test code points

        UnicodeSet set(start, end);
        expectRange((UnicodeString)"ct(" + start + "," + end + ")",
                    set, xstart, xend);
        
        set.clear();
        set.set(start, end);
        expectRange((UnicodeString)"set(" + start + "," + end + ")",
                    set, xstart, xend);
        
        UBool b = set.contains(start);
        b = set.contains(start, end);
        b = set.containsNone(start, end);
        b = set.containsSome(start, end);

        /*int32_t index = set.indexOf(start);*/
        
        set.clear();
        set.add(start);
        set.add(start, end);
        expectRange((UnicodeString)"add(" + start + "," + end + ")",
                    set, xstart, xend);

        set.set(0, 0x10FFFF);
        set.retain(start, end);
        expectRange((UnicodeString)"retain(" + start + "," + end + ")",
                    set, xstart, xend);
        set.retain(start);

        set.set(0, 0x10FFFF);
        set.remove(start);
        set.remove(start, end);
        set.complement();
        expectRange((UnicodeString)"!remove(" + start + "," + end + ")",
                    set, xstart, xend);

        set.set(0, 0x10FFFF);
        set.complement(start, end);
        set.complement();
        expectRange((UnicodeString)"!complement(" + start + "," + end + ")",
                    set, xstart, xend);
        set.complement(start);
    }

    const UChar32 DATA2[] = {
        0,
        0x10FFFF,
        (UChar32)-1,
        0x110000
    };
    const int32_t DATA2_LENGTH = sizeof(DATA2)/sizeof(DATA2[0]);

    for (i=0; i<DATA2_LENGTH; ++i) {
        UChar32 c = DATA2[i], end = 0x10FFFF;
        UBool valid = (c >= 0 && c <= 0x10FFFF);

        UnicodeSet set(0, 0x10FFFF);

        // For single-codepoint contains, invalid codepoints are NOT contained
        UBool b = set.contains(c);
        if (b == valid) {
            logln((UnicodeString)"[\\u0000-\\U0010FFFF].contains(" + c +
                  ") = " + b);
        } else {
            errln((UnicodeString)"FAIL: [\\u0000-\\U0010FFFF].contains(" + c +
                  ") = " + b);
        }

        // For codepoint range contains, containsNone, and containsSome,
        // invalid or empty (start > end) ranges have UNDEFINED behavior.
        b = set.contains(c, end);
        logln((UnicodeString)"* [\\u0000-\\U0010FFFF].contains(" + c +
              "," + end + ") = " + b);

        b = set.containsNone(c, end);
        logln((UnicodeString)"* [\\u0000-\\U0010FFFF].containsNone(" + c +
              "," + end + ") = " + b);

        b = set.containsSome(c, end);
        logln((UnicodeString)"* [\\u0000-\\U0010FFFF].containsSome(" + c +
              "," + end + ") = " + b);

        int32_t index = set.indexOf(c);
        if ((index >= 0) == valid) {
            logln((UnicodeString)"[\\u0000-\\U0010FFFF].indexOf(" + c +
                  ") = " + index);
        } else {
            errln((UnicodeString)"FAIL: [\\u0000-\\U0010FFFF].indexOf(" + c +
                  ") = " + index);
        }
    }
}

// Used by TestSymbolTable
class TokenSymbolTable : public SymbolTable {
public:
    Hashtable contents;

    TokenSymbolTable(UErrorCode& ec) : contents(FALSE, ec) {
        contents.setValueDeleter(uhash_deleteUnicodeString);
    }

    ~TokenSymbolTable() {}

    /**
     * (Non-SymbolTable API) Add the given variable and value to
     * the table.  Variable should NOT contain leading '$'.
     */
    void add(const UnicodeString& var, const UnicodeString& value,
             UErrorCode& ec) {
        if (U_SUCCESS(ec)) {
            contents.put(var, new UnicodeString(value), ec);
        }
    }

    /**
     * SymbolTable API
     */
    virtual const UnicodeString* lookup(const UnicodeString& s) const {
        return (const UnicodeString*) contents.get(s);
    }

    /**
     * SymbolTable API
     */
    virtual const UnicodeFunctor* lookupMatcher(UChar32 /*ch*/) const {
        return NULL;
    }

    /**
     * SymbolTable API
     */
    virtual UnicodeString parseReference(const UnicodeString& text,
                                         ParsePosition& pos, int32_t limit) const {
        int32_t start = pos.getIndex();
        int32_t i = start;
        UnicodeString result;
        while (i < limit) {
            UChar c = text.charAt(i);
            if ((i==start && !u_isIDStart(c)) || !u_isIDPart(c)) {
                break;
            }
            ++i;
        }
        if (i == start) { // No valid name chars
            return result; // Indicate failure with empty string
        }
        pos.setIndex(i);
        text.extractBetween(start, i, result);
        return result;
    }
};

void UnicodeSetTest::TestSymbolTable() {
    // Multiple test cases can be set up here.  Each test case
    // is terminated by null:
    // var, value, var, value,..., input pat., exp. output pat., null
    const char* DATA[] = {
        "us", "a-z", "[0-1$us]", "[0-1a-z]", NULL,
        "us", "[a-z]", "[0-1$us]", "[0-1[a-z]]", NULL,
        "us", "\\[a\\-z\\]", "[0-1$us]", "[-01\\[\\]az]", NULL,
        NULL
    };

    for (int32_t i=0; DATA[i]!=NULL; ++i) {
        UErrorCode ec = U_ZERO_ERROR;
        TokenSymbolTable sym(ec);
        if (U_FAILURE(ec)) {
            errln("FAIL: couldn't construct TokenSymbolTable");
            continue;
        }

        // Set up variables
        while (DATA[i+2] != NULL) {
            sym.add(DATA[i], DATA[i+1], ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: couldn't add to TokenSymbolTable");
                continue;
            }
            i += 2;
        }

        // Input pattern and expected output pattern
        UnicodeString inpat = DATA[i], exppat = DATA[i+1];
        i += 2;

        ParsePosition pos(0);
        UnicodeSet us(inpat, pos, USET_IGNORE_SPACE, &sym, ec);
        if (U_FAILURE(ec)) {
            errln("FAIL: couldn't construct UnicodeSet");
            continue;
        }

        // results
        if (pos.getIndex() != inpat.length()) {
            errln((UnicodeString)"Failed to read to end of string \""
                  + inpat + "\": read to "
                  + pos.getIndex() + ", length is "
                  + inpat.length());
        }

        UnicodeSet us2(exppat, ec);
        if (U_FAILURE(ec)) {
            errln("FAIL: couldn't construct expected UnicodeSet");
            continue;
        }
        
        UnicodeString a, b;
        if (us != us2) {
            errln((UnicodeString)"Failed, got " + us.toPattern(a, TRUE) +
                  ", expected " + us2.toPattern(b, TRUE));
        } else {
            logln((UnicodeString)"Ok, got " + us.toPattern(a, TRUE));
        }
    }
}

void UnicodeSetTest::TestSurrogate() {
    const char* DATA[] = {
        // These should all behave identically
        "[abc\\uD800\\uDC00]",
        // "[abc\uD800\uDC00]", // Can't do this on C -- only Java
        "[abc\\U00010000]",
        0
    };
    for (int i=0; DATA[i] != 0; ++i) {
        UErrorCode ec = U_ZERO_ERROR;
        logln((UnicodeString)"Test pattern " + i + " :" + DATA[i]);
        UnicodeSet set(DATA[i], ec);
        if (U_FAILURE(ec)) {
            errln("FAIL: UnicodeSet constructor");
            continue;
        }
        expectContainment(set,
                          CharsToUnicodeString("abc\\U00010000"),
                          CharsToUnicodeString("\\uD800;\\uDC00")); // split apart surrogate-pair
        if (set.size() != 4) {
            errln((UnicodeString)"FAIL: " + DATA[i] + ".size() == " + 
                  set.size() + ", expected 4");
        }
    }
}

void UnicodeSetTest::TestExhaustive() {
    // exhaustive tests. Simulate UnicodeSets with integers.
    // That gives us very solid tests (except for large memory tests).

    int32_t limit = 128;

    UnicodeSet x, y, z, aa;

    for (int32_t i = 0; i < limit; ++i) {
        bitsToSet(i, x);
        logln((UnicodeString)"Testing " + i + ", " + x);
        _testComplement(i, x, y);

        // AS LONG AS WE ARE HERE, check roundtrip
        checkRoundTrip(bitsToSet(i, aa));

        for (int32_t j = 0; j < limit; ++j) {
            _testAdd(i,j,  x,y,z);
            _testXor(i,j,  x,y,z);
            _testRetain(i,j,  x,y,z);
            _testRemove(i,j,  x,y,z);
        }
    }
}

void UnicodeSetTest::_testComplement(int32_t a, UnicodeSet& x, UnicodeSet& z) {
    bitsToSet(a, x);
    z = x;
    z.complement();
    int32_t c = setToBits(z);
    if (c != (~a)) {
        errln((UnicodeString)"FAILED: add: ~" + x +  " != " + z);
        errln((UnicodeString)"FAILED: add: ~" + a + " != " + c);
    }
    checkCanonicalRep(z, (UnicodeString)"complement " + a);
}

void UnicodeSetTest::_testAdd(int32_t a, int32_t b, UnicodeSet& x, UnicodeSet& y, UnicodeSet& z) {
    bitsToSet(a, x);
    bitsToSet(b, y);
    z = x;
    z.addAll(y);
    int32_t c = setToBits(z);
    if (c != (a | b)) {
        errln((UnicodeString)"FAILED: add: " + x + " | " + y + " != " + z);
        errln((UnicodeString)"FAILED: add: " + a + " | " + b + " != " + c);
    }
    checkCanonicalRep(z, (UnicodeString)"add " + a + "," + b);
}

void UnicodeSetTest::_testRetain(int32_t a, int32_t b, UnicodeSet& x, UnicodeSet& y, UnicodeSet& z) {
    bitsToSet(a, x);
    bitsToSet(b, y);
    z = x;
    z.retainAll(y);
    int32_t c = setToBits(z);
    if (c != (a & b)) {
        errln((UnicodeString)"FAILED: retain: " + x + " & " + y + " != " + z);
        errln((UnicodeString)"FAILED: retain: " + a + " & " + b + " != " + c);
    }
    checkCanonicalRep(z, (UnicodeString)"retain " + a + "," + b);
}

void UnicodeSetTest::_testRemove(int32_t a, int32_t b, UnicodeSet& x, UnicodeSet& y, UnicodeSet& z) {
    bitsToSet(a, x);
    bitsToSet(b, y);
    z = x;
    z.removeAll(y);
    int32_t c = setToBits(z);
    if (c != (a &~ b)) {
        errln((UnicodeString)"FAILED: remove: " + x + " &~ " + y + " != " + z);
        errln((UnicodeString)"FAILED: remove: " + a + " &~ " + b + " != " + c);
    }
    checkCanonicalRep(z, (UnicodeString)"remove " + a + "," + b);
}

void UnicodeSetTest::_testXor(int32_t a, int32_t b, UnicodeSet& x, UnicodeSet& y, UnicodeSet& z) {
    bitsToSet(a, x);
    bitsToSet(b, y);
    z = x;
    z.complementAll(y);
    int32_t c = setToBits(z);
    if (c != (a ^ b)) {
        errln((UnicodeString)"FAILED: complement: " + x + " ^ " + y + " != " + z);
        errln((UnicodeString)"FAILED: complement: " + a + " ^ " + b + " != " + c);
    }
    checkCanonicalRep(z, (UnicodeString)"complement " + a + "," + b);
}

/**
 * Check that ranges are monotonically increasing and non-
 * overlapping.
 */
void UnicodeSetTest::checkCanonicalRep(const UnicodeSet& set, const UnicodeString& msg) {
    int32_t n = set.getRangeCount();
    if (n < 0) {
        errln((UnicodeString)"FAIL result of " + msg +
              ": range count should be >= 0 but is " +
              n /*+ " for " + set.toPattern())*/);
        return;
    }
    UChar32 last = 0;
    for (int32_t i=0; i<n; ++i) {
        UChar32 start = set.getRangeStart(i);
        UChar32 end = set.getRangeEnd(i);
        if (start > end) {
            errln((UnicodeString)"FAIL result of " + msg +
                  ": range " + (i+1) +
                  " start > end: " + (int)start + ", " + (int)end +
                  " for " + set);
        }
        if (i > 0 && start <= last) {
            errln((UnicodeString)"FAIL result of " + msg +
                  ": range " + (i+1) +
                  " overlaps previous range: " + (int)start + ", " + (int)end +
                  " for " + set);
        }
        last = end;
    }
}

/**
 * Convert a bitmask to a UnicodeSet.
 */
UnicodeSet& UnicodeSetTest::bitsToSet(int32_t a, UnicodeSet& result) {
    result.clear();
    for (UChar32 i = 0; i < 32; ++i) {
        if ((a & (1<<i)) != 0) {
            result.add(i);
        }
    }
    return result;
}

/**
 * Convert a UnicodeSet to a bitmask.  Only the characters
 * U+0000 to U+0020 are represented in the bitmask.
 */
int32_t UnicodeSetTest::setToBits(const UnicodeSet& x) {
    int32_t result = 0;
    for (int32_t i = 0; i < 32; ++i) {
        if (x.contains((UChar32)i)) {
            result |= (1<<i);
        }
    }
    return result;
}

/**
 * Return the representation of an inversion list based UnicodeSet
 * as a pairs list.  Ranges are listed in ascending Unicode order.
 * For example, the set [a-zA-M3] is represented as "33AMaz".
 */
UnicodeString UnicodeSetTest::getPairs(const UnicodeSet& set) {
    UnicodeString pairs;
    for (int32_t i=0; i<set.getRangeCount(); ++i) {
        UChar32 start = set.getRangeStart(i);
        UChar32 end = set.getRangeEnd(i);
        if (end > 0xFFFF) {
            end = 0xFFFF;
            i = set.getRangeCount(); // Should be unnecessary
        }
        pairs.append((UChar)start).append((UChar)end);
    }
    return pairs;
}

/**
 * Basic consistency check for a few items.
 * That the iterator works, and that we can create a pattern and
 * get the same thing back
 */
void UnicodeSetTest::checkRoundTrip(const UnicodeSet& s) {
    UErrorCode ec = U_ZERO_ERROR;

    UnicodeSet t(s);
    checkEqual(s, t, "copy ct");

    t = s;
    checkEqual(s, t, "operator=");

    copyWithIterator(t, s, FALSE);
    checkEqual(s, t, "iterator roundtrip");

    copyWithIterator(t, s, TRUE); // try range
    checkEqual(s, t, "iterator roundtrip");
        
    UnicodeString pat; s.toPattern(pat, FALSE);
    t.applyPattern(pat, ec);
    if (U_FAILURE(ec)) {
        errln("FAIL: applyPattern");
        return;
    } else {
        checkEqual(s, t, "toPattern(false)");
    }
        
    s.toPattern(pat, TRUE);
    t.applyPattern(pat, ec);
    if (U_FAILURE(ec)) {
        errln("FAIL: applyPattern");
        return;
    } else {
        checkEqual(s, t, "toPattern(true)");
    }
}
    
void UnicodeSetTest::copyWithIterator(UnicodeSet& t, const UnicodeSet& s, UBool withRange) {
    t.clear();
    UnicodeSetIterator it(s);
    if (withRange) {
        while (it.nextRange()) {
            if (it.isString()) {
                t.add(it.getString());
            } else {
                t.add(it.getCodepoint(), it.getCodepointEnd());
            }
        }
    } else {
        while (it.next()) {
            if (it.isString()) {
                t.add(it.getString());
            } else {
                t.add(it.getCodepoint());
            }
        }
    }
}
    
UBool UnicodeSetTest::checkEqual(const UnicodeSet& s, const UnicodeSet& t, const char* message) {
    UnicodeString source; s.toPattern(source, TRUE);
    UnicodeString result; t.toPattern(result, TRUE);
    if (s != t) {
        errln((UnicodeString)"FAIL: " + message
              + "; source = " + source
              + "; result = " + result
              );
        return FALSE;
    } else {
        logln((UnicodeString)"Ok: " + message
              + "; source = " + source
              + "; result = " + result
              );
    }
    return TRUE;
}

void
UnicodeSetTest::expectContainment(const UnicodeString& pat,
                                  const UnicodeString& charsIn,
                                  const UnicodeString& charsOut) {
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeSet set(pat, ec);
    if (U_FAILURE(ec)) {
        errln((UnicodeString)"FAIL: pattern \"" +
              pat + "\" => " + u_errorName(ec));
        return;
    }
    expectContainment(set, pat, charsIn, charsOut);
}

void
UnicodeSetTest::expectContainment(const UnicodeSet& set,
                                  const UnicodeString& charsIn,
                                  const UnicodeString& charsOut) {
    UnicodeString pat;
    set.toPattern(pat);
    expectContainment(set, pat, charsIn, charsOut);
}

void
UnicodeSetTest::expectContainment(const UnicodeSet& set,
                                  const UnicodeString& setName,
                                  const UnicodeString& charsIn,
                                  const UnicodeString& charsOut) {
    UnicodeString bad;
    UChar32 c;
    int32_t i;

    for (i=0; i<charsIn.length(); i+=U16_LENGTH(c)) {
        c = charsIn.char32At(i);
        if (!set.contains(c)) {
            bad.append(c);
        }
    }
    if (bad.length() > 0) {
        errln((UnicodeString)"Fail: set " + setName + " does not contain " + prettify(bad) +
              ", expected containment of " + prettify(charsIn));
    } else {
        logln((UnicodeString)"Ok: set " + setName + " contains " + prettify(charsIn));
    }

    bad.truncate(0);
    for (i=0; i<charsOut.length(); i+=U16_LENGTH(c)) {
        c = charsOut.char32At(i);
        if (set.contains(c)) {
            bad.append(c);
        }
    }
    if (bad.length() > 0) {
        errln((UnicodeString)"Fail: set " + setName + " contains " + prettify(bad) +
              ", expected non-containment of " + prettify(charsOut));
    } else {
        logln((UnicodeString)"Ok: set " + setName + " does not contain " + prettify(charsOut));
    }
}

void
UnicodeSetTest::expectPattern(UnicodeSet& set,
                              const UnicodeString& pattern,
                              const UnicodeString& expectedPairs){
    UErrorCode status = U_ZERO_ERROR;
    set.applyPattern(pattern, status);
    if (U_FAILURE(status)) {
        errln(UnicodeString("FAIL: applyPattern(\"") + pattern +
              "\") failed");
        return;
    } else {
        if (getPairs(set) != expectedPairs ) {
            errln(UnicodeString("FAIL: applyPattern(\"") + pattern +
                  "\") => pairs \"" +
                  escape(getPairs(set)) + "\", expected \"" +
                  escape(expectedPairs) + "\"");
        } else {
            logln(UnicodeString("Ok:   applyPattern(\"") + pattern +
                  "\") => pairs \"" +
                  escape(getPairs(set)) + "\"");
        }
    }
    // the result of calling set.toPattern(), which is the string representation of
    // this set(set), is passed to a  UnicodeSet constructor, and tested that it 
    // will produce another set that is equal to this one.
    UnicodeString temppattern;
    set.toPattern(temppattern);
    UnicodeSet *tempset=new UnicodeSet(temppattern, status);
    if (U_FAILURE(status)) {
        errln(UnicodeString("FAIL: applyPattern(\""+ pattern + "\").toPattern() => " + temppattern + " => invalid pattern"));
        return;
    }
    if(*tempset != set || getPairs(*tempset) != getPairs(set)){
        errln(UnicodeString("FAIL: applyPattern(\""+ pattern + "\").toPattern() => " + temppattern + " => pairs \""+ escape(getPairs(*tempset)) + "\", expected pairs \"" +
            escape(getPairs(set)) + "\""));
    } else{
        logln(UnicodeString("Ok:   applyPattern(\""+ pattern + "\").toPattern() => " + temppattern + " => pairs \"" + escape(getPairs(*tempset)) + "\""));
    }

    delete tempset;

}

void
UnicodeSetTest::expectPairs(const UnicodeSet& set, const UnicodeString& expectedPairs) {
    if (getPairs(set) != expectedPairs) {
        errln(UnicodeString("FAIL: Expected pair list \"") +
              escape(expectedPairs) + "\", got \"" +
              escape(getPairs(set)) + "\"");
    }
}

void UnicodeSetTest::expectToPattern(const UnicodeSet& set,
                                     const UnicodeString& expPat,
                                     const char** expStrings) {
    UnicodeString pat;
    set.toPattern(pat, TRUE);
    if (pat == expPat) {
        logln((UnicodeString)"Ok:   toPattern() => \"" + pat + "\"");
    } else {
        errln((UnicodeString)"FAIL: toPattern() => \"" + pat + "\", expected \"" + expPat + "\"");
        return;
    }
    if (expStrings == NULL) {
        return;
    }
    UBool in = TRUE;
    for (int32_t i=0; expStrings[i] != NULL; ++i) {
        if (expStrings[i] == NOT) { // sic; pointer comparison
            in = FALSE;
            continue;
        }
        UnicodeString s = CharsToUnicodeString(expStrings[i]);
        UBool contained = set.contains(s);
        if (contained == in) {
            logln((UnicodeString)"Ok: " + expPat + 
                  (contained ? " contains {" : " does not contain {") +
                  escape(expStrings[i]) + "}");
        } else {
            errln((UnicodeString)"FAIL: " + expPat + 
                  (contained ? " contains {" : " does not contain {") +
                  escape(expStrings[i]) + "}");
        }
    }
}

static UChar toHexString(int32_t i) { return (UChar)(i + (i < 10 ? 0x30 : (0x41 - 10))); }

void
UnicodeSetTest::doAssert(UBool condition, const char *message)
{
    if (!condition) {
        errln(UnicodeString("ERROR : ") + message);
    }
}

UnicodeString
UnicodeSetTest::escape(const UnicodeString& s) {
    UnicodeString buf;
    for (int32_t i=0; i<s.length(); )
    {
        UChar32 c = s.char32At(i);
        if (0x0020 <= c && c <= 0x007F) {
            buf += c;
        } else {
            if (c <= 0xFFFF) {
                buf += (UChar)0x5c; buf += (UChar)0x75;
            } else {
                buf += (UChar)0x5c; buf += (UChar)0x55;
                buf += toHexString((c & 0xF0000000) >> 28);
                buf += toHexString((c & 0x0F000000) >> 24);
                buf += toHexString((c & 0x00F00000) >> 20);
                buf += toHexString((c & 0x000F0000) >> 16);
            }
            buf += toHexString((c & 0xF000) >> 12);
            buf += toHexString((c & 0x0F00) >> 8);
            buf += toHexString((c & 0x00F0) >> 4);
            buf += toHexString(c & 0x000F);
        }
        i += U16_LENGTH(c);
    }
    return buf;
}

void UnicodeSetTest::TestFreezable() {
    UErrorCode errorCode=U_ZERO_ERROR;
    UnicodeString idPattern=UNICODE_STRING("[:ID_Continue:]", 15);
    UnicodeSet idSet(idPattern, errorCode);
    if(U_FAILURE(errorCode)) {
        errln("FAIL: unable to create UnicodeSet([:ID_Continue:]) - %s", u_errorName(errorCode));
        return;
    }

    UnicodeString wsPattern=UNICODE_STRING("[:White_Space:]", 15);
    UnicodeSet wsSet(wsPattern, errorCode);
    if(U_FAILURE(errorCode)) {
        errln("FAIL: unable to create UnicodeSet([:White_Space:]) - %s", u_errorName(errorCode));
        return;
    }

    idSet.add(idPattern);
    UnicodeSet frozen(idSet);
    frozen.freeze();

    if(idSet.isFrozen() || !frozen.isFrozen()) {
        errln("FAIL: isFrozen() is wrong");
    }
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: a copy-constructed frozen set differs from its original");
    }

    frozen=wsSet;
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: a frozen set was modified by operator=");
    }

    UnicodeSet frozen2(frozen);
    if(frozen2!=frozen || frozen2!=idSet) {
        errln("FAIL: a copied frozen set differs from its frozen original");
    }
    if(!frozen2.isFrozen()) {
        errln("FAIL: copy-constructing a frozen set results in a thawed one");
    }
    UnicodeSet frozen3(5, 55);  // Set to some values to really test assignment below, not copy construction.
    if(frozen3.contains(0, 4) || !frozen3.contains(5, 55) || frozen3.contains(56, 0x10ffff)) {
        errln("FAIL: UnicodeSet(5, 55) failed");
    }
    frozen3=frozen;
    if(!frozen3.isFrozen()) {
        errln("FAIL: copying a frozen set results in a thawed one");
    }

    UnicodeSet *cloned=(UnicodeSet *)frozen.clone();
    if(!cloned->isFrozen() || *cloned!=frozen || cloned->containsSome(0xd802, 0xd805)) {
        errln("FAIL: clone() failed");
    }
    cloned->add(0xd802, 0xd805);
    if(cloned->containsSome(0xd802, 0xd805)) {
        errln("FAIL: unable to modify clone");
    }
    delete cloned;

    UnicodeSet *thawed=(UnicodeSet *)frozen.cloneAsThawed();
    if(thawed->isFrozen() || *thawed!=frozen || thawed->containsSome(0xd802, 0xd805)) {
        errln("FAIL: cloneAsThawed() failed");
    }
    thawed->add(0xd802, 0xd805);
    if(!thawed->contains(0xd802, 0xd805)) {
        errln("FAIL: unable to modify thawed clone");
    }
    delete thawed;

    frozen.set(5, 55);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::set() modified a frozen set");
    }

    frozen.clear();
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::clear() modified a frozen set");
    }

    frozen.closeOver(USET_CASE_INSENSITIVE);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::closeOver() modified a frozen set");
    }

    frozen.compact();
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::compact() modified a frozen set");
    }

    ParsePosition pos;
    frozen.
        applyPattern(wsPattern, errorCode).
        applyPattern(wsPattern, USET_IGNORE_SPACE, NULL, errorCode).
        applyPattern(wsPattern, pos, USET_IGNORE_SPACE, NULL, errorCode).
        applyIntPropertyValue(UCHAR_CANONICAL_COMBINING_CLASS, 230, errorCode).
        applyPropertyAlias(UNICODE_STRING_SIMPLE("Assigned"), UnicodeString(), errorCode);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::applyXYZ() modified a frozen set");
    }

    frozen.
        add(0xd800).
        add(0xd802, 0xd805).
        add(wsPattern).
        addAll(idPattern).
        addAll(wsSet);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::addXYZ() modified a frozen set");
    }

    frozen.
        retain(0x62).
        retain(0x64, 0x69).
        retainAll(wsPattern).
        retainAll(wsSet);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::retainXYZ() modified a frozen set");
    }

    frozen.
        remove(0x62).
        remove(0x64, 0x69).
        remove(idPattern).
        removeAll(idPattern).
        removeAll(idSet);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::removeXYZ() modified a frozen set");
    }

    frozen.
        complement().
        complement(0x62).
        complement(0x64, 0x69).
        complement(idPattern).
        complementAll(idPattern).
        complementAll(idSet);
    if(frozen!=idSet || !(frozen==idSet)) {
        errln("FAIL: UnicodeSet::complementXYZ() modified a frozen set");
    }
}

// Test span() etc. -------------------------------------------------------- ***

// Append the UTF-8 version of the string to t and return the appended UTF-8 length.
static int32_t
appendUTF8(const UChar *s, int32_t length, char *t, int32_t capacity) {
    UErrorCode errorCode=U_ZERO_ERROR;
    int32_t length8=0;
    u_strToUTF8(t, capacity, &length8, s, length, &errorCode);
    if(U_SUCCESS(errorCode)) {
        return length8;
    } else {
        // The string contains an unpaired surrogate.
        // Ignore this string.
        return 0;
    }
}

// Make the strings in a UnicodeSet easily accessible.
class UnicodeSetWithStrings {
public:
    UnicodeSetWithStrings(const UnicodeSet &normalSet) :
            set(normalSet), stringsLength(0), hasSurrogates(FALSE),
            nextStringIndex(0), nextUTF8Start(0) {
        int32_t size=set.size();
        if(size>0 && set.charAt(size-1)<0) {
            // If a set's last element is not a code point, then it must contain strings.
            // Iterate over the set, skip all code point ranges, and cache the strings.
            // Convert them to UTF-8 for spanUTF8().
            UnicodeSetIterator iter(set);
            const UnicodeString *s;
            char *s8=utf8;
            int32_t length8, utf8Count=0;
            while(iter.nextRange() && stringsLength<LENGTHOF(strings)) {
                if(iter.isString()) {
                    // Store the pointer to the set's string element
                    // which we happen to know is a stable pointer.
                    strings[stringsLength]=s=&iter.getString();
                    utf8Count+=
                        utf8Lengths[stringsLength]=length8=
                        appendUTF8(s->getBuffer(), s->length(),
                                   s8, (int32_t)(sizeof(utf8)-utf8Count));
                    if(length8==0) {
                        hasSurrogates=TRUE;
                    }
                    ++stringsLength;
                }
            }
        }
    }

    const UnicodeSet &getSet() const {
        return set;
    }

    UBool hasStrings() const {
        return (UBool)(stringsLength>0);
    }

    UBool hasStringsWithSurrogates() const {
        return hasSurrogates;
    }

    void resetStringIterator() {
        nextStringIndex=nextUTF8Start=0;
    }

    const UnicodeString *nextString() {
        if(nextStringIndex<stringsLength) {
            return strings[nextStringIndex++];
        } else {
            return NULL;
        }
    }

    const char *nextUTF8(int32_t &length) {
        if(nextStringIndex<stringsLength) {
            const char *s8=utf8+nextUTF8Start;
            nextUTF8Start+=length=utf8Lengths[nextStringIndex++];
            return s8;
        } else {
            length=0;
            return NULL;
        }
    }

    UBool spanNeedsStrings;  // Flag managed externally.

private:
    const UnicodeSet &set;

    const UnicodeString *strings[20];
    int32_t stringsLength;
    UBool hasSurrogates;

    char utf8[1024];
    int32_t utf8Lengths[20];

    int32_t nextStringIndex;
    int32_t nextUTF8Start;
};

// Implement span() with contains() for comparison.
static int32_t containsSpanUTF16(UnicodeSetWithStrings &set, const UChar *s, int32_t length, USetSpanCondition spanCondition) {
    const UnicodeSet &realSet(set.getSet());
    if(!set.hasStrings()) {
        if(spanCondition!=USET_SPAN_WHILE_NOT_CONTAINED) {
            spanCondition=USET_SPAN_WHILE_CONTAINED;  // Pin to 0/1 values.
        }

        UChar32 c;
        int32_t start=0, prev;
        while((prev=start)<length) {
            U16_NEXT(s, start, length, c);
            if(realSet.contains(c)!=spanCondition) {
                break;
            }
        }
        return prev;
    } else if(spanCondition==USET_SPAN_WHILE_NOT_CONTAINED) {
        UChar32 c;
        int32_t start, next;
        for(start=next=0; start<length;) {
            U16_NEXT(s, next, length, c);
            if(realSet.contains(c)) {
                break;
            }
            const UnicodeString *str;
            set.resetStringIterator();
            while((str=set.nextString())!=NULL) {
                if(str->length()<=(length-start) && 0==str->compare(s+start, str->length())) {
                    set.spanNeedsStrings=TRUE;
                    return start;
                }
            }
            start=next;
        }
        return start;
    } else /* USET_SPAN_WHILE_NOT_CONTAINED or USET_SPAN_WHILE_LONGEST_MATCH */ {
        UChar32 c;
        int32_t start, next, maxSpanLimit=0;
        for(start=next=0; start<length;) {
            U16_NEXT(s, next, length, c);
            if(!realSet.contains(c)) {
                next=start;  // Do not span this single, not-contained code point.
            }
            const UnicodeString *str;
            set.resetStringIterator();
            while((str=set.nextString())!=NULL) {
                if(str->length()<=(length-start) && 0==str->compare(s+start, str->length())) {
                    set.spanNeedsStrings=TRUE;
                    int32_t matchLimit=start+str->length();
                    if(matchLimit==length) {
                        return length;
                    }
                    if(spanCondition==USET_SPAN_WHILE_CONTAINED) {
                        // Iterate for the shortest match at each position.
                        // Recurse for each but the shortest match.
                        if(next==start) {
                            next=matchLimit;  // First match from start.
                        } else {
                            if(matchLimit<next) {
                                // Remember shortest match from start for iteration.
                                int32_t temp=next;
                                next=matchLimit;
                                matchLimit=temp;
                            }
                            // Recurse for non-shortest match from start.
                            int32_t spanLength=containsSpanUTF16(set, s+matchLimit, length-matchLimit, USET_SPAN_WHILE_CONTAINED);
                            if((matchLimit+spanLength)>maxSpanLimit) {
                                maxSpanLimit=matchLimit+spanLength;
                                if(maxSpanLimit==length) {
                                    return length;
                                }
                            }
                        }
                    } else /* spanCondition==USET_SPAN_WHILE_LONGEST_MATCH */ {
                        if(matchLimit>next) {
                            // Remember longest match from start.
                            next=matchLimit;
                        }
                    }
                }
            }
            if(next==start) {
                break;  // No match from start.
            }
            start=next;
        }
        if(start>maxSpanLimit) {
            return start;
        } else {
            return maxSpanLimit;
        }
    }
}

static int32_t containsSpanUTF8(UnicodeSetWithStrings &set, const char *s, int32_t length, USetSpanCondition spanCondition) {
    const UnicodeSet &realSet(set.getSet());
    if(!set.hasStrings()) {
        if(spanCondition!=USET_SPAN_WHILE_NOT_CONTAINED) {
            spanCondition=USET_SPAN_WHILE_CONTAINED;  // Pin to 0/1 values.
        }

        UChar32 c;
        int32_t start=0, prev;
        while((prev=start)<length) {
            U8_NEXT(s, start, length, c);
            if(c<0) {
                c=0xfffd;
            }
            if(realSet.contains(c)!=spanCondition) {
                break;
            }
        }
        return prev;
    } else if(spanCondition==USET_SPAN_WHILE_NOT_CONTAINED) {
        UChar32 c;
        int32_t start, next;
        for(start=next=0; start<length;) {
            U8_NEXT(s, next, length, c);
            if(c<0) {
                c=0xfffd;
            }
            if(realSet.contains(c)) {
                break;
            }
            const char *s8;
            int32_t length8;
            set.resetStringIterator();
            while((s8=set.nextUTF8(length8))!=NULL) {
                if(length8<=(length-start) && 0==memcmp(s+start, s8, length8)) {
                    set.spanNeedsStrings=TRUE;
                    return start;
                }
            }
            start=next;
        }
        return start;
    } else /* USET_SPAN_WHILE_NOT_CONTAINED or USET_SPAN_WHILE_LONGEST_MATCH */ {
        UChar32 c;
        int32_t start, next, maxSpanLimit=0;
        for(start=next=0; start<length;) {
            U8_NEXT(s, next, length, c);
            if(c<0) {
                c=0xfffd;
            }
            if(!realSet.contains(c)) {
                next=start;  // Do not span this single, not-contained code point.
            }
            const char *s8;
            int32_t length8;
            set.resetStringIterator();
            while((s8=set.nextUTF8(length8))!=NULL) {
                if(length8<=(length-start) && 0==memcmp(s+start, s8, length8)) {
                    set.spanNeedsStrings=TRUE;
                    int32_t matchLimit=start+length8;
                    if(matchLimit==length) {
                        return length;
                    }
                    if(spanCondition==USET_SPAN_WHILE_CONTAINED) {
                        // Iterate for the shortest match at each position.
                        // Recurse for each but the shortest match.
                        if(next==start) {
                            next=matchLimit;  // First match from start.
                        } else {
                            if(matchLimit<next) {
                                // Remember shortest match from start for iteration.
                                int32_t temp=next;
                                next=matchLimit;
                                matchLimit=temp;
                            }
                            // Recurse for non-shortest match from start.
                            int32_t spanLength=containsSpanUTF8(set, s+matchLimit, length-matchLimit, USET_SPAN_WHILE_CONTAINED);
                            if((matchLimit+spanLength)>maxSpanLimit) {
                                maxSpanLimit=matchLimit+spanLength;
                                if(maxSpanLimit==length) {
                                    return length;
                                }
                            }
                        }
                    } else /* spanCondition==USET_SPAN_WHILE_LONGEST_MATCH */ {
                        if(matchLimit>next) {
                            // Remember longest match from start.
                            next=matchLimit;
                        }
                    }
                }
            }
            if(next==start) {
                break;  // No match from start.
            }
            start=next;
        }
        if(start>maxSpanLimit) {
            return start;
        } else {
            return maxSpanLimit;
        }
    }
}

static inline USetSpanCondition invertSpanCondition(USetSpanCondition spanCondition) {
    return spanCondition == USET_SPAN_WHILE_NOT_CONTAINED ? USET_SPAN_WHILE_CONTAINED : USET_SPAN_WHILE_NOT_CONTAINED;
}

/*
 * Count spans on a string with the method according to type and set the span limits.
 * The set may be the complement of the original.
 * When using spanBack(), use lastSpanCondition for the first spanBack().
 * Type==0 and !isComplement sets the lastSpanCondition.
 * Returns -1 if there is no such type.
 */
static int32_t getSpans(UnicodeSetWithStrings &set, UBool isComplement,
                        const void *s, int32_t length, UBool isUTF16,
                        int type, const char *&typeName,
                        USetSpanCondition firstSpanCondition, USetSpanCondition &lastSpanCondition,
                        int32_t limits[], int32_t limitsCapacity) {
    const UnicodeSet &realSet(set.getSet());
    int32_t start, count;
    USetSpanCondition spanCondition;

    if(isComplement) {
        firstSpanCondition=invertSpanCondition(firstSpanCondition);
    }

    count=0;
    switch(type) {
    case 0:
        typeName= isUTF16 ? "contains" : "containsUTF8";
        start=0;
        if(length<0) {
            length= isUTF16 ? u_strlen((const UChar *)s) : strlen((const char *)s);
        }
        spanCondition=firstSpanCondition;
        for(;;) {
            start+= isUTF16 ? containsSpanUTF16(set, (const UChar *)s+start, length-start, spanCondition) :
                              containsSpanUTF8(set, (const char *)s+start, length-start, spanCondition);
            if(count<limitsCapacity) {
                limits[count]=start;
            }
            ++count;
            if(start>=length) {
                if(!isComplement) {
                    lastSpanCondition=spanCondition;
                }
                break;
            }
            spanCondition=invertSpanCondition(spanCondition);
        }
        break;
    case 1:
        typeName= isUTF16 ? "span" : "spanUTF8";
        start=0;
        spanCondition=firstSpanCondition;
        for(;;) {
            start+= isUTF16 ? realSet.span((const UChar *)s+start, length>=0 ? length-start : length, spanCondition) :
                              realSet.spanUTF8((const char *)s+start, length>=0 ? length-start : length, spanCondition);
            if(count<limitsCapacity) {
                limits[count]=start;
            }
            ++count;
            if(length>=0 ? start>=length :
                           isUTF16 ? ((const UChar *)s)[start]==0 :
                                     ((const char *)s)[start]==0
            ) {
                break;
            }
            spanCondition=invertSpanCondition(spanCondition);
        }
        break;
    case 2:
        typeName= isUTF16 ? "spanBack" : "spanBackUTF8";
        spanCondition= isComplement ? invertSpanCondition(lastSpanCondition) : lastSpanCondition;
        for(;;) {
            ++count;
            if(count<=limitsCapacity) {
                limits[limitsCapacity-count]= length >=0 ? length :
                                                           isUTF16 ? u_strlen((const UChar *)s) : strlen((const char *)s);
            }
            length= isUTF16 ? realSet.spanBack((const UChar *)s, length, spanCondition) :
                              realSet.spanBackUTF8((const char *)s, length, spanCondition);
            if(length==0 && spanCondition==firstSpanCondition) {
                break;
            }
            spanCondition=invertSpanCondition(spanCondition);
        }
        if(count<limitsCapacity) {
            memmove(limits, limits+(limitsCapacity-count), count*4);
        }
        break;
    default:
        typeName="";
        return -1;
    }

    return count;
}

// sets to be tested; odd index=isComplement
enum {
    SLOW,
    SLOW_NOT,
    FAST,
    FAST_NOT,
    SET_COUNT
};

static const char *const setNames[SET_COUNT]={
    "slow",
    "slow.not",
    "fast",
    "fast.not"
};

/*
 * Verify that we get the same results whether we look at text with contains(),
 * span() or spanBack(), using unfrozen or frozen versions of the set,
 * and using the set or its complement (switching the spanConditions accordingly).
 * The latter verifies that
 *   set.span(spanCondition) == set.complement().span(!spanCondition).
 * Also verify that using the inverse spanCondition yields inverse spans
 * with the same boundaries:
 *   set.span(spanCondition) == !set.span(!spanCondition).
 *
 * The expectLimits[] are either provided by the caller (with expectCount>=0)
 * or returned to the caller (with an input expectCount<0).
 */
void UnicodeSetTest::testSpan(UnicodeSetWithStrings *sets[4],
                              const void *s, int32_t length, UBool isUTF16,
                              int32_t expectLimits[], int32_t &expectCount,
                              const char *testName, int32_t index) {
    int32_t limits[500];
    int32_t limitsCount;
    int i, j, k;

    const char *typeName;
    int type;

    // Initialize to an arbitrary value for happy compilers.
    USetSpanCondition lastSpanCondition=USET_SPAN_WHILE_NOT_CONTAINED;

    sets[0]->spanNeedsStrings=FALSE;

    for(i=0; i<SET_COUNT; ++i) {
        if((i&1)!=0 && sets[0]->spanNeedsStrings) {
            // Skip the set complements if spanning the string requires the
            // use of the set's strings.
            // Strings are not complemented.
            continue;
        }
        for(type=0;; ++type) {
            limitsCount=getSpans(*sets[i], (UBool)(i&1),
                                 s, length, isUTF16,
                                 type, typeName,
                                 USET_SPAN_WHILE_NOT_CONTAINED, lastSpanCondition,
                                 limits, LENGTHOF(limits));
            if(limitsCount<0) {
                break;
            }
            if(expectCount<0) {
                expectCount=limitsCount;
                if(limitsCount>LENGTHOF(limits)) {
                    errln("FAIL: %s[0x%lx].%s.%s span count=%ld > %ld capacity - too many spans",
                          testName, (long)index, setNames[i], typeName, (long)limitsCount, (long)LENGTHOF(limits));
                    return;
                }
                memcpy(expectLimits, limits, limitsCount*4);
            } else if(limitsCount!=expectCount) {
                errln("FAIL: %s[0x%lx].%s.%s span count=%ld != %ld",
                      testName, (long)index, setNames[i], typeName, (long)limitsCount, (long)expectCount);
            } else {
                for(j=0; j<limitsCount; ++j) {
                    if(limits[j]!=expectLimits[j]) {
                        errln("FAIL: %s[0x%lx].%s.%s span count=%ld limits[%d]=%ld != %ld",
                              testName, (long)index, setNames[i], typeName, (long)limitsCount,
                              j, (long)limits[j], (long)expectLimits[j]);
                        break;
                    }
                }
            }

            // Verify that we get the opposite spans for the opposite spanCondition.
            limitsCount=getSpans(*sets[i], (UBool)(i&1),
                                 s, length, isUTF16,
                                 type, typeName,
                                 USET_SPAN_WHILE_CONTAINED, lastSpanCondition,
                                 limits, LENGTHOF(limits));
            if(expectLimits[0]==0) {
                j=0;
                k=1;
            } else {
                j=1;
                k=0;
            }
            if((limitsCount-j)!=(expectCount-k)) {
                errln("FAIL: %s[0x%lx].%s.%s span.not non-0 count=%ld != %ld",
                      testName, (long)index, setNames[i], typeName, (long)(limitsCount-j), (long)(expectCount-k));
            } else if(j>0 && limits[0]!=0) {
                errln("FAIL: %s[0x%lx].%s.%s span.not non-0 count=%ld limits[0]=%ld != 0",
                      testName, (long)index, setNames[i], typeName, (long)(limitsCount-j), (long)limits[0]);
            } else {
                for(; j<limitsCount; ++j, ++k) {
                    if(limits[j]!=expectLimits[k]) {
                        errln("FAIL: %s[0x%lx].%s.%s span.not non-0 count=%ld limits[%d]=%ld != %ld",
                              testName, (long)index, setNames[i], typeName, (long)(limitsCount-j),
                              j, (long)limits[j], (long)expectLimits[k]);
                        break;
                    }
                }
            }
        }
    }

    // Compare span() with containsAll()/containsNone().
    if(isUTF16) {
        const UChar *s16=(const UChar *)s;
        UnicodeString string;
        int32_t prev=0, limit, length;
        for(i=0; i<expectCount; ++i) {
            limit=expectLimits[i];
            length=limit-prev;
            if(length>0) {
                string.setTo(FALSE, s16+prev, length);  // read-only alias
                if(i&1) {
                    if(!sets[SLOW]->getSet().containsAll(string)) {
                        errln("FAIL: %s[0x%lx].%s.containsAll(%ld..%ld)==FALSE contradicts span()",
                              testName, (long)index, setNames[SLOW], (long)prev, (long)limit);
                        return;
                    }
                    if(!sets[FAST]->getSet().containsAll(string)) {
                        errln("FAIL: %s[0x%lx].%s.containsAll(%ld..%ld)==FALSE contradicts span()",
                              testName, (long)index, setNames[FAST], (long)prev, (long)limit);
                        return;
                    }
                } else {
                    if(!sets[SLOW]->getSet().containsNone(string)) {
                        errln("FAIL: %s[0x%lx].%s.containsNone(%ld..%ld)==FALSE contradicts span()",
                              testName, (long)index, setNames[SLOW], (long)prev, (long)limit);
                        return;
                    }
                    if(!sets[FAST]->getSet().containsNone(string)) {
                        errln("FAIL: %s[0x%lx].%s.containsNone(%ld..%ld)==FALSE contradicts span()",
                              testName, (long)index, setNames[FAST], (long)prev, (long)limit);
                        return;
                    }
                }
            }
            prev=limit;
        }
    }
}

void UnicodeSetTest::testSpan(UnicodeSetWithStrings *sets[4],
                              const void *s, int32_t length, UBool isUTF16,
                              const char *testName, int32_t index) {
    int32_t expectLimits[500];
    int32_t expectCount=-1;
    testSpan(sets, s, length, isUTF16, expectLimits, expectCount, testName, index);
}

UBool stringContainsUnpairedSurrogate(const UChar *s, int32_t length) {
    UChar c, c2;

    if(length>=0) {
        while(length>0) {
            c=*s++;
            --length;
            if(0xd800<=c && c<0xe000) {
                if(c>=0xdc00 || length==0 || !U16_IS_TRAIL(c2=*s++)) {
                    return TRUE;
                }
                --length;
            }
        }
    } else {
        while((c=*s++)!=0) {
            if(0xd800<=c && c<0xe000) {
                if(c>=0xdc00 || !U16_IS_TRAIL(c2=*s++)) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

// Test both UTF-16 and UTF-8 versions of span() etc. on the same sets and text.
// Testing UTF-16 and UTF-8 together requires that surrogate code points
// have the same contains(c) value as U+FFFD.
// Otherwise, only the UTF-16 case is tested.
void UnicodeSetTest::testSpanBothUTFs(UnicodeSetWithStrings *sets[4],
                                      const UChar *s16, int32_t length16,
                                      const char *testName, int32_t index) {
    int32_t expectLimits[500];
    int32_t expectCount;

    expectCount=-1;  // Get expectLimits[] from testSpan().
    testSpan(sets, s16, length16, TRUE, expectLimits, expectCount, testName, index);

    if( stringContainsUnpairedSurrogate(s16, length16) &&
            (!(sets[0]->getSet().contains(0xfffd) ?
               sets[0]->getSet().contains(0xd800, 0xdfff) :
               sets[0]->getSet().containsNone(0xd800, 0xdfff)) ||
              sets[0]->hasStringsWithSurrogates())
    ) {
        // contains(U+FFFD) is inconsistent with contains(some surrogates),
        // or the set contains strings with unpaired surrogates which don't translate to valid UTF-8:
        // Skip the UTF-8 part of the test because it is likely to produce a different result.
        return;
    }

    // Convert s16[] and expectLimits[] to UTF-8.
    uint8_t s8[3000];
    int32_t offsets[3000];

    const UChar *s16Limit=s16+length16;
    char *t=(char *)s8;
    char *tLimit=t+sizeof(s8);
    int32_t *o=offsets;
    UErrorCode errorCode=U_ZERO_ERROR;

    // Convert with substitution: Turn unpaired surrogates into U+FFFD.
    ucnv_fromUnicode(openUTF8Converter(), &t, tLimit, &s16, s16Limit, o, TRUE, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("FAIL: %s[0x%lx] ucnv_fromUnicode(to UTF-8) fails with %s",
              testName, (long)index, u_errorName(errorCode));
        ucnv_resetFromUnicode(utf8Cnv);
        return;
    }
    int32_t length8=(int32_t)(t-(char *)s8);

    // Convert expectLimits[].
    int32_t i, j, expect;
    for(i=j=0; i<expectCount; ++i) {
        expect=expectLimits[i];
        if(expect==length16) {
            expectLimits[i]=length8;
        } else {
            while(offsets[j]<expect) {
                ++j;
            }
            expectLimits[i]=j;
        }
    }

    testSpan(sets, s8, length8, FALSE, expectLimits, expectCount, testName, index);
}

static UChar32 nextCodePoint(UChar32 c) {
    // Skip some large and boring ranges.
    switch(c) {
    case 0x3441:
        return 0x4d7f;
    case 0x5100:
        return 0x9f00;
    case 0xb040:
        return 0xd780;
    case 0xe041:
        return 0xf8fe;
    case 0x10100:
        return 0x20000;
    case 0x20041:
        return 0xe0000;
    case 0xe0101:
        return 0x10fffd;
    default:
        return c+1;
    }
}

// Verify that all implementations represent the same set.
void UnicodeSetTest::testSpanContents(UnicodeSetWithStrings *sets[4], const char *testName) {
    UChar s[1000];
    int32_t length=0;

    UChar32 c, first;
    for(first=c=0;; c=nextCodePoint(c)) {
        if(c>0x10ffff || length>(LENGTHOF(s)-U16_MAX_LENGTH)) {
            testSpanBothUTFs(sets, s, length, testName, first);
            if(c>0x10ffff) {
                break;
            }
            length=0;
            first=c;
        }
        U16_APPEND_UNSAFE(s, length, c);
    }
}

// Test with a particular, interesting string.
// Specify length and try NUL-termination.
void UnicodeSetTest::testSpanUTF16String(UnicodeSetWithStrings *sets[4], const char *testName) {
    static const UChar s[]={
        0x61, 0x62, 0x20,                       // Latin, space
        0x3b1, 0x3b2, 0x3b3,                    // Greek
        0xd900,                                 // lead surrogate
        0x3000, 0x30ab, 0x30ad,                 // wide space, Katakana
        0xdc05,                                 // trail surrogate
        0xa0, 0xac00, 0xd7a3,                   // nbsp, Hangul
        0xd900, 0xdc05,                         // unassigned supplementary
        0xd840, 0xdfff, 0xd860, 0xdffe,         // Han supplementary
        0xd7a4, 0xdc05, 0xd900, 0x2028,         // unassigned, surrogates in wrong order, LS
        0                                       // NUL
    };

    testSpan(sets, s, -1, TRUE, testName, 0);
    testSpan(sets, s, LENGTHOF(s)-1, TRUE, testName, 1);
}

void UnicodeSetTest::testSpanUTF8String(UnicodeSetWithStrings *sets[4], const char *testName) {
    static const char s[]={
        "abc"                                   // Latin

        /* trail byte in lead position */
        "\x80"

        " "                                     // space

        /* truncated multi-byte sequences */
        "\xd0"
        "\xe0"
        "\xe1"
        "\xed"
        "\xee"
        "\xf0"
        "\xf1"
        "\xf4"
        "\xf8"
        "\xfc"

        "\xCE\xB1\xCE\xB2\xCE\xB3"              // Greek

        /* trail byte in lead position */
        "\x80"

        "\xe0\x80"
        "\xe0\xa0"
        "\xe1\x80"
        "\xed\x80"
        "\xed\xa0"
        "\xee\x80"
        "\xf0\x80"
        "\xf0\x90"
        "\xf1\x80"
        "\xf4\x80"
        "\xf4\x90"
        "\xf8\x80"
        "\xfc\x80"

        "\xE3\x80\x80\xE3\x82\xAB\xE3\x82\xAD"  // wide space, Katakana

        /* trail byte in lead position */
        "\x80"

        "\xf0\x80\x80"
        "\xf0\x90\x80"
        "\xf1\x80\x80"
        "\xf4\x80\x80"
        "\xf4\x90\x80"
        "\xf8\x80\x80"
        "\xfc\x80\x80"

        "\xC2\xA0\xEA\xB0\x80\xED\x9E\xA3"      // nbsp, Hangul

        /* trail byte in lead position */
        "\x80"

        "\xf8\x80\x80\x80"
        "\xfc\x80\x80\x80"

        "\xF1\x90\x80\x85"                      // unassigned supplementary

        /* trail byte in lead position */
        "\x80"

        "\xfc\x80\x80\x80\x80"

        "\xF0\xA0\x8F\xBF\xF0\xA8\x8F\xBE"      // Han supplementary

        /* trail byte in lead position */
        "\x80"

        /* complete sequences but non-shortest forms or out of range etc. */
        "\xc0\x80"
        "\xe0\x80\x80"
        "\xed\xa0\x80"
        "\xf0\x80\x80\x80"
        "\xf4\x90\x80\x80"
        "\xf8\x80\x80\x80\x80"
        "\xfc\x80\x80\x80\x80\x80"
        "\xfe"
        "\xff"

        /* trail byte in lead position */
        "\x80"

        "\xED\x9E\xA4\xE2\x80\xA8"              // unassigned, LS, NUL-terminated
    };

    testSpan(sets, s, -1, FALSE, testName, 0);
    testSpan(sets, s, LENGTHOF(s)-1, FALSE, testName, 1);
}

void UnicodeSetTest::TestSpan() {
    // "[...]" is a UnicodeSet pattern.
    // "*" performs basic tests.
    // Other strings will be spanned.
    static const char *const testdata[]={
        "[:ID_Continue:]",
        "*",
        "[:White_Space:]",
        "*",
        "[]",
        "*",
        "[\\u0000-\\U0010FFFF]",
        "*",
        "[\\u0000\\u0080\\u0800\\U00010000]",
        "*",
        "[\\u007F\\u07FF\\uFFFF\\U0010FFFF]",
        "*",
        "[[[:ID_Continue:]-[\\u30ab\\u30ad]]{\\u3000\\u30ab}{\\u3000\\u30ab\\u30ad}]",
        "*",
        "[[[:ID_Continue:]-[\\u30ab\\u30ad]]{\\u30ab\\u30ad}{\\u3000\\u30ab\\u30ad}]",
        "*",

        "[x{xy}{xya}{axy}{ax}]",

        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxya"
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxya"
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxy"
        "aaaa",

        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxya"
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxya"
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxy",

        "byayaxya",

        "[a{ab}{abc}{cd}]",
        "acdabcdabccd",

        "[d{cd}{bcd}{ab}]",
        "abbcdabcdabd",

        "[b{bb}]",
        // The following strings are barely doable with the recursive
        // reference implementation.
        // The not-contained character at the end prevents an early exit from the span().
        "bbbbbbbbbbbbbbbbbbbbbbbb-",
        "bbbbbbbbbbbbbbbbbbbbbbbbb-"
    };

    UnicodeSet *sets[SET_COUNT]={ NULL };
    UnicodeSetWithStrings *sets_with_str[SET_COUNT]={ NULL };

    char testName[200];
    char *testNameLimit=testName;

    int32_t i, j;
    for(i=0; i<LENGTHOF(testdata); ++i) {
        if(testdata[i][0]=='[') {
            // Create new test sets from this pattern.
            for(j=0; j<SET_COUNT; ++j) {
                delete sets_with_str[j];
                delete sets[j];
            }
            UErrorCode errorCode=U_ZERO_ERROR;
            sets[SLOW]=new UnicodeSet(UnicodeString(testdata[i], -1, US_INV), errorCode);
            if(U_FAILURE(errorCode)) {
                errln("FAIL: Unable to create UnicodeSet(%s) - %s", testdata[i], u_errorName(errorCode));
                break;
            }
            sets[SLOW_NOT]=new UnicodeSet(*sets[SLOW]);
            sets[SLOW_NOT]->complement();
            sets[FAST]=new UnicodeSet(*sets[SLOW]);
            sets[FAST]->freeze();
            sets[FAST_NOT]=new UnicodeSet(*sets[SLOW_NOT]);
            sets[FAST_NOT]->freeze();

            for(j=0; j<SET_COUNT; ++j) {
                sets_with_str[j]=new UnicodeSetWithStrings(*sets[j]);
            }

            strcpy(testName, testdata[i]);
            testNameLimit=strchr(testName, 0);
            *testNameLimit++=':';
            *testNameLimit=0;
        } else if(0==strcmp(testdata[i], "*")) {
            strcpy(testNameLimit, "bad_string");
            testSpanUTF16String(sets_with_str, testName);
            testSpanUTF8String(sets_with_str, testName);

            strcpy(testNameLimit, "contents");
            testSpanContents(sets_with_str, testName);
        } else {
            UnicodeString string=UnicodeString(testdata[i], -1, US_INV);
            strcpy(testNameLimit, "test_string");
            testSpanBothUTFs(sets_with_str, string.getBuffer(), string.length(), testName, i);
        }
    }
    for(j=0; j<SET_COUNT; ++j) {
        delete sets_with_str[j];
        delete sets[j];
    }
}

void UnicodeSetTest::TestStringSpan() {
    // TODO: Beef up to real test.
    static const char *pattern="[x{xy}{xya}{axy}{ax}]";
    static const char *const string=
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxya"
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxya"
        "xx"
        "xyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxyaxy"
        "aaaa";

    UErrorCode errorCode=U_ZERO_ERROR;
    UnicodeString pattern16=UnicodeString(pattern, -1, US_INV);
    UnicodeSet set(pattern16, errorCode);
    if(U_FAILURE(errorCode)) {
        errln("FAIL: Unable to create UnicodeSet(%s) - %s", pattern, u_errorName(errorCode));
        return;
    }

    UnicodeString string16=UnicodeString(string, -1, US_INV).unescape();

    if(set.containsAll(string16)) {
        errln("FAIL: UnicodeSet(%s).containsAll(%s) should be FALSE", pattern, string);
    }

    // Remove trailing "aaaa".
    string16.truncate(string16.length()-4);
    if(!set.containsAll(string16)) {
        errln("FAIL: UnicodeSet(%s).containsAll(%s[:-4]) should be TRUE", pattern, string);
    }

    string16=UNICODE_STRING_SIMPLE("byayaxya");
    const UChar *s16=string16.getBuffer();
    int32_t length16=string16.length();
    if( set.span(s16, 8, USET_SPAN_WHILE_NOT_CONTAINED)!=4 ||
        set.span(s16, 7, USET_SPAN_WHILE_NOT_CONTAINED)!=4 ||
        set.span(s16, 6, USET_SPAN_WHILE_NOT_CONTAINED)!=4 ||
        set.span(s16, 5, USET_SPAN_WHILE_NOT_CONTAINED)!=5 ||
        set.span(s16, 4, USET_SPAN_WHILE_NOT_CONTAINED)!=4 ||
        set.span(s16, 3, USET_SPAN_WHILE_NOT_CONTAINED)!=3
    ) {
        errln("FAIL: UnicodeSet(%s).span(while not) returns the wrong value", pattern);
    }

    pattern="[a{ab}{abc}{cd}]";
    pattern16=UnicodeString(pattern, -1, US_INV);
    set.applyPattern(pattern16, errorCode);
    if(U_FAILURE(errorCode)) {
        errln("FAIL: Unable to create UnicodeSet(%s) - %s", pattern, u_errorName(errorCode));
        return;
    }
    string16=UNICODE_STRING_SIMPLE("acdabcdabccd");
    s16=string16.getBuffer();
    length16=string16.length();
    if( set.span(s16, 12, USET_SPAN_WHILE_CONTAINED)!=12 ||
        set.span(s16, 12, USET_SPAN_WHILE_LONGEST_MATCH)!=6 ||
        set.span(s16+7, 5, USET_SPAN_WHILE_LONGEST_MATCH)!=5
    ) {
        errln("FAIL: UnicodeSet(%s).span(while longest match) returns the wrong value", pattern);
    }

    pattern="[d{cd}{bcd}{ab}]";
    pattern16=UnicodeString(pattern, -1, US_INV);
    set.applyPattern(pattern16, errorCode).freeze();
    if(U_FAILURE(errorCode)) {
        errln("FAIL: Unable to create UnicodeSet(%s) - %s", pattern, u_errorName(errorCode));
        return;
    }
    string16=UNICODE_STRING_SIMPLE("abbcdabcdabd");
    s16=string16.getBuffer();
    length16=string16.length();
    if( set.spanBack(s16, 12, USET_SPAN_WHILE_CONTAINED)!=0 ||
        set.spanBack(s16, 12, USET_SPAN_WHILE_LONGEST_MATCH)!=6 ||
        set.spanBack(s16, 5, USET_SPAN_WHILE_LONGEST_MATCH)!=0
    ) {
        errln("FAIL: UnicodeSet(%s).spanBack(while longest match) returns the wrong value", pattern);
    }
}
