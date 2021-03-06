/*
*******************************************************************************
*   Copyright (C) 1997-2000, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   Date        Name        Description
*   06/23/00    aliu        Creation.
*******************************************************************************
*/

#include "unicode/utrans.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "cintltst.h"
#include "unicode/ustring.h"
#include <stdio.h>

#define TEST(x) addTest(root, &x, "utrans/" # x)

static void TestAPI();
static void TestSimpleRules();
static void TestFilter();
static void TestOpenInverse();
static void TestClone();
static void TestRegisterUnregister();

static void _expectRules(const char*, const char*, const char*);
static void _expect(const UTransliterator* trans, const char* cfrom, const char* cto);

void
addUTransTest(TestNode** root) {
    TEST(TestAPI);
    TEST(TestSimpleRules);
    TEST(TestFilter);
    TEST(TestOpenInverse);
    TEST(TestClone);
    TEST(TestRegisterUnregister);
}

static void TestAPI() {
    enum { BUF_CAP = 128 };
    char buf[BUF_CAP], buf2[BUF_CAP];
    UErrorCode status = U_ZERO_ERROR;
    UTransliterator* trans = NULL;
    int32_t i, n;
    
    /* Test getAvailableIDs */
    n = utrans_countAvailableIDs();
    if (n < 1) {
        log_err("FAIL: utrans_countAvailableIDs() returned %d\n", n);
    } else {
        log_verbose("System ID count: %d\n", n);
    }
    for (i=0; i<n; ++i) {
        utrans_getAvailableID(i, buf, BUF_CAP);
        if (*buf == 0) {
            log_err("FAIL: System transliterator %d: \"\"\n", i);
        } else {
            log_verbose("System transliterator %d: \"%s\"\n", i, buf);
        }
    }

    /* Test open */
    utrans_getAvailableID(0, buf, BUF_CAP);
    trans = utrans_open(buf, UTRANS_FORWARD, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_open(%s) failed, error=%s\n",
                buf, u_errorName(status));
    }
    
    /* Test getID */
    utrans_getID(trans, buf2, BUF_CAP);
    if (0 != uprv_strcmp(buf, buf2)) {
        log_err("FAIL: utrans_getID(%s) returned %s\n",
                buf, buf2);
    }
    utrans_close(trans);
}

static void TestOpenInverse(){
    UErrorCode status=U_ZERO_ERROR;
    UTransliterator* t1=NULL;
    UTransliterator* inverse1=NULL;
    enum { BUF_CAP = 128 };
    char buf1[BUF_CAP];
    int32_t i=0;

    const char TransID[][25]={
           "Halfwidth-Fullwidth",
           "Fullwidth-Halfwidth",
           "Greek-Latin" ,
           "Latin-Greek", 
           "Arabic-Latin",
           "Latin-Arabic",
           "Kana-Latin",
           "Latin-Kana",
           "Hebrew-Latin",
           "Latin-Hebrew",
           "Cyrillic-Latin", 
           "Latin-Cyrillic", 
           "Devanagari-Latin", 
           "Latin-Devanagari", 
           "Unicode-Hex",
           "Hex-Unicode"
         };
     
    for(i=0; i<sizeof(TransID)/sizeof(TransID[0]); i=i+2){
        t1=utrans_open(TransID[i], UTRANS_FORWARD, &status);
        if(t1 == NULL || U_FAILURE(status)){
            log_err("FAIL: in instantiation for id=%s\n", TransID[i]);
            continue;
        }
        inverse1=utrans_openInverse(t1, &status);
        if(U_FAILURE(status)){
            log_err("FAIL: utrans_openInverse() failed for id=%s. Error=%s\n", TransID[i], myErrorName(status));
            continue;
        }
        utrans_getID(inverse1, buf1, BUF_CAP);
        if(uprv_strcmp(buf1, TransID[i+1]) != 0){
            log_err("FAIL :openInverse() for %s returned %s instead of %s\n", TransID[i], buf1, TransID[i+1]);
        }
        utrans_close(t1);
        utrans_close(inverse1);
   }
}

static void TestClone(){
    UErrorCode status=U_ZERO_ERROR;
    UTransliterator* t1=NULL;
    UTransliterator* t2=NULL;
    UTransliterator* t3=NULL;
    UTransliterator* t4=NULL;
    enum { BUF_CAP = 128 };
    char buf1[BUF_CAP], buf2[BUF_CAP], buf3[BUF_CAP];
   
    t1=utrans_open("Latin-Devanagari", UTRANS_FORWARD, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: construction\n");
        return;
    }
    t2=utrans_open("Latin-Hebrew", UTRANS_FORWARD, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: construction\n");
        utrans_close(t1);
        return;
    }

    t3=utrans_clone(t1, &status);
    t4=utrans_clone(t2, &status);

    utrans_getID(t1, buf1, BUF_CAP);
    utrans_getID(t2, buf2, BUF_CAP);
    utrans_getID(t3, buf3, BUF_CAP);

    if(uprv_strcmp(buf1, buf3) != 0 ||
        uprv_strcmp(buf1, buf2) == 0) {
        log_err("FAIL: utrans_clone() failed\n");
    }

    utrans_getID(t4, buf3, BUF_CAP);

    if(uprv_strcmp(buf2, buf3) != 0 ||
        uprv_strcmp(buf1, buf3) == 0) {
        log_err("FAIL: utrans_clone() failed\n");
    }

    utrans_close(t1);
    utrans_close(t2);
    utrans_close(t3);
    utrans_close(t4);

}

static void TestRegisterUnregister(){
    UErrorCode status=U_ZERO_ERROR;
    UTransliterator* t1=NULL;
    UTransliterator* rules=NULL;
    UTransliterator* inverse1=NULL;
    UChar rule[]={ 0x0061, 0x003c, 0x003e, 0x0063}; /*a<>b*/

    /* Make sure it doesn't exist */
    t1=utrans_open("TestA-TestB", UTRANS_FORWARD, &status);
    if(t1 != NULL || U_SUCCESS(status)) {
        log_err("FAIL: TestA-TestB already registered\n");
        return;
    }
    status=U_ZERO_ERROR;
    /* Check inverse too */
    inverse1=utrans_open("TestA-TestB", UTRANS_REVERSE, &status);
    if(inverse1 != NULL || U_SUCCESS(status)) {
        log_err("FAIL: TestA-TestB already registered\n");
        return;
    }
    status=U_ZERO_ERROR;
    /* Create it */
    rules=utrans_openRules("TestA-TestB", rule, 4, UTRANS_FORWARD, NULL, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: utrans_openRules(a<>B) failed with error=%s\n", myErrorName(status));
        return;
    }
    status=U_ZERO_ERROR;
    /* Register it */
    utrans_register(rules, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: utrans_register failed with error=%s\n", myErrorName(status));
        return;
    }
    status=U_ZERO_ERROR;
    /* Now check again -- should exist now*/
    t1= utrans_open("TestA-TestB", UTRANS_FORWARD, &status);
    if(U_FAILURE(status) || t1 == NULL){
        log_err("FAIL: TestA-TestB not registered\n");
        return;
    }
    utrans_close(t1);
   
    /*unregister the instance*/
    status=U_ZERO_ERROR;
    utrans_unregister("TestA-TestB");
    /* now Make sure it doesn't exist */
    t1=utrans_open("TestA-TestB", UTRANS_FORWARD, &status);
    if(U_SUCCESS(status) || t1 != NULL) {
        log_err("FAIL: TestA-TestB isn't unregistered\n");
        return;
    }
    
    utrans_close(t1);
    utrans_close(inverse1);
}

static void TestSimpleRules() {
    /* Test rules */
    /* Example: rules 1. ab>x|y
     *                2. yc>z
     *
     * []|eabcd  start - no match, copy e to tranlated buffer
     * [e]|abcd  match rule 1 - copy output & adjust cursor
     * [ex|y]cd  match rule 2 - copy output & adjust cursor
     * [exz]|d   no match, copy d to transliterated buffer
     * [exzd]|   done
     */
    _expectRules("ab>x|y;"
                 "yc>z",
                 "eabcd", "exzd");

    /* Another set of rules:
     *    1. ab>x|yzacw
     *    2. za>q
     *    3. qc>r
     *    4. cw>n
     *
     * []|ab       Rule 1
     * [x|yzacw]   No match
     * [xy|zacw]   Rule 2
     * [xyq|cw]    Rule 4
     * [xyqn]|     Done
     */
    _expectRules("ab>x|yzacw;"
                 "za>q;"
                 "qc>r;"
                 "cw>n",
                 "ab", "xyqn");

    /* Test categories
     */
    _expectRules("$dummy=" "\\uE100" ";" /* careful here with E100 */
                 "$vowel=[aeiouAEIOU];"
                 "$lu=[:Lu:];"
                 "$vowel } $lu > '!';"
                 "$vowel > '&';"
                 "'!' { $lu > '^';"
                 "$lu > '*';"
                 "a > ERROR",
                 "abcdefgABCDEFGU", "&bcd&fg!^**!^*&");
}

static void TestFilter() {
    UErrorCode status = U_ZERO_ERROR;
    UChar filt[128];
    UChar buf[128];
    UChar exp[128];
    char cbuf[128];
    int32_t limit;
    const char* DATA[] = {
        "[^c]", /* Filter out 'c' */
        "abcde",
        "\\u0061\\u0062c\\u0064\\u0065",

        "", /* No filter */
        "abcde",
        "\\u0061\\u0062\\u0063\\u0064\\u0065"
    };
    int32_t DATA_length = sizeof(DATA) / sizeof(DATA[0]);
    int32_t i;

    UTransliterator* hex = utrans_open("Unicode-Hex", UTRANS_FORWARD, &status);

    if (hex == 0 || U_FAILURE(status)) {
        log_err("FAIL: utrans_open(Unicode-Hex) failed, error=%s\n",
                u_errorName(status)); 
        goto exit;
    }

    for (i=0; i<DATA_length; i+=3) {
        /*u_uastrcpy(filt, DATA[i]);*/
        u_charsToUChars(DATA[i], filt, uprv_strlen(DATA[i])+1);
        utrans_setFilter(hex, filt, -1, &status);

        if (U_FAILURE(status)) {
            log_err("FAIL: utrans_setFilter() failed, error=%s\n",
                    u_errorName(status));
            goto exit;
        }
        
        /*u_uastrcpy(buf, DATA[i+1]);*/
        u_charsToUChars(DATA[i+1], buf, uprv_strlen(DATA[i+1])+1);
        limit = 5;
        utrans_transUChars(hex, buf, NULL, 128, 0, &limit, &status);
        
        if (U_FAILURE(status)) {
            log_err("FAIL: utrans_transUChars() failed, error=%s\n",
                    u_errorName(status));
            goto exit;
        }
        
        /*u_austrcpy(cbuf, buf);*/
        u_UCharsToChars(buf, cbuf, u_strlen(buf)+1);
        /*u_uastrcpy(exp, DATA[i+2]);*/
        u_charsToUChars(DATA[i+2], exp, uprv_strlen(DATA[i+2])+1);
        if (0 == u_strcmp(buf, exp)) {
            log_verbose("Ok: %s | %s -> %s\n", DATA[i+1], DATA[i], cbuf);
        } else {
            log_err("FAIL: %s | %s -> %s, expected %s\n", DATA[i+1], DATA[i], cbuf, DATA[i+2]);
        }
    }

 exit:
    utrans_close(hex);
}

static void _expectRules(const char* crules,
                  const char* cfrom,
                  const char* cto) {
    /* u_uastrcpy has no capacity param for the buffer -- so just
     * make all buffers way too big */
    enum { CAP = 256 };
    UChar rules[CAP];
    UTransliterator *trans;
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseErr;

    u_uastrcpy(rules, crules);

    trans = utrans_openRules(crules /*use rules as ID*/, rules, -1, UTRANS_FORWARD,
                             &parseErr, &status);
    if (U_FAILURE(status)) {
        utrans_close(trans);
        log_err("FAIL: utrans_openRules(%s) failed, error=%s\n",
                crules, u_errorName(status));
        return;
    }

    _expect(trans, cfrom, cto);

    utrans_close(trans);
}

static void _expect(const UTransliterator* trans,
             const char* cfrom,
             const char* cto) {
    /* u_uastrcpy has no capacity param for the buffer -- so just
     * make all buffers way too big */
    enum { CAP = 256 };
    UChar from[CAP];
    UChar to[CAP];
    UChar buf[CAP];
    char id[CAP];
    UErrorCode status = U_ZERO_ERROR;
    int32_t limit;
    UTransPosition pos;

    u_uastrcpy(from, cfrom);
    u_uastrcpy(to, cto);

    utrans_getID(trans, id, CAP);

    /* utrans_transUChars() */
    u_strcpy(buf, from);
    limit = u_strlen(buf);
    utrans_transUChars(trans, buf, NULL, CAP, 0, &limit, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_transUChars() failed, error=%s\n",
                u_errorName(status));
        return;
    }

    if (0 == u_strcmp(buf, to)) {
        log_verbose("Ok: utrans_transUChars(%s) x %s -> %s\n",
                    id, cfrom, cto);
    } else {
        char actual[CAP];
        u_austrcpy(actual, buf);
        log_err("FAIL: utrans_transUChars(%s) x %s -> %s, expected %s\n",
                id, cfrom, actual, cto);
    }

    /* utrans_transIncrementalUChars() */
    u_strcpy(buf, from);
    pos.start = pos.contextStart = 0;
    pos.limit = pos.contextLimit = u_strlen(buf);
    utrans_transIncrementalUChars(trans, buf, NULL, CAP, &pos, &status);
    utrans_transUChars(trans, buf, NULL, CAP, pos.start, &pos.limit, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_transIncrementalUChars() failed, error=%s\n",
                u_errorName(status));
        return;
    }

    if (0 == u_strcmp(buf, to)) {
        log_verbose("Ok: utrans_transIncrementalUChars(%s) x %s -> %s\n",
                    id, cfrom, cto);
    } else {
        char actual[CAP];
        u_austrcpy(actual, buf);
        log_err("FAIL: utrans_transIncrementalUChars(%s) x %s -> %s, expected %s\n",
                id, cfrom, actual, cto);
    }
}
