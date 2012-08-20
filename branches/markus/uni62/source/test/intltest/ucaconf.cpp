/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2002-2012, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

/**
 * UCAConformanceTest performs conformance tests defined in the data
 * files. ICU ships with stub data files, as the whole test are too 
 * long. To do the whole test, download the test files.
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "ucaconf.h"
#include "unicode/ustring.h"
#include "cstring.h"
#include "uparse.h"

UCAConformanceTest::UCAConformanceTest() :
rbUCA(NULL),
testFile(NULL),
status(U_ZERO_ERROR)
{
    UCA = ucol_open("root", &status);
    if(U_FAILURE(status)) {
        errln("ERROR - UCAConformanceTest: Unable to open UCA collator!");
    }

    const char *srcDir = IntlTest::getSourceTestData(status);
    if (U_FAILURE(status)) {
        dataerrln("Could not open test data %s", u_errorName(status));
        return;
    }
    uprv_strcpy(testDataPath, srcDir);
    uprv_strcat(testDataPath, "CollationTest_");

    UVersionInfo uniVersion;
    static const UVersionInfo v62 = { 6, 2, 0, 0 };
    u_getUnicodeVersion(uniVersion);
    isAtLeastUCA62 = uprv_memcmp(uniVersion, v62, 4) >= 0;
}

UCAConformanceTest::~UCAConformanceTest()
{
    ucol_close(UCA);
    if(rbUCA) {
        ucol_close(rbUCA);
    }
    if(testFile) {
        fclose(testFile);
    }
}

void UCAConformanceTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par */)
{
    if (exec) logln("TestSuite UCAConformanceTest: ");
    if(U_SUCCESS(status)) {
      switch (index) {
          case 0: name = "TestTableNonIgnorable"; if (exec)   TestTableNonIgnorable(/* par */); break;
          case 1: name = "TestTableShifted";      if (exec)   TestTableShifted(/* par */);      break;
          case 2: name = "TestRulesNonIgnorable"; if (exec)   TestRulesNonIgnorable(/* par */); break;
          case 3: name = "TestRulesShifted";      if (exec)   TestRulesShifted(/* par */);      break;
          default: name = ""; break;
      }
    } else {
      name = "";
    }
}

void UCAConformanceTest::initRbUCA() 
{
    if(!rbUCA) {
        UParseError parseError;
        UChar      *ucarules;
        // preflight rules
        int32_t size = ucol_getRulesEx(UCA, UCOL_FULL_RULES, NULL, 0);
        ucarules = (UChar *)malloc(size * sizeof(UChar));

        size = ucol_getRulesEx(UCA, UCOL_FULL_RULES, ucarules, size);
        rbUCA = ucol_openRules(ucarules, size, UCOL_DEFAULT, UCOL_TERTIARY, 
            &parseError, &status);
        free(ucarules);
        if (U_FAILURE(status)) {
            errln("Failure creating UCA rule-based collator: %s", u_errorName(status));
            return;
        }
    }
}

void UCAConformanceTest::setCollNonIgnorable(UCollator *coll) 
{
  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
  ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_OFF, &status);
  ucol_setAttribute(coll, UCOL_CASE_LEVEL, UCOL_OFF, &status);
  ucol_setAttribute(coll, UCOL_STRENGTH, isAtLeastUCA62 ? UCOL_IDENTICAL : UCOL_TERTIARY, &status);
  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);
}

void UCAConformanceTest::setCollShifted(UCollator *coll) 
{
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_OFF, &status);
    ucol_setAttribute(coll, UCOL_CASE_LEVEL, UCOL_OFF, &status);
    ucol_setAttribute(coll, UCOL_STRENGTH, isAtLeastUCA62 ? UCOL_IDENTICAL : UCOL_QUATERNARY, &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
}

void UCAConformanceTest::openTestFile(const char *type)
{
    const char *ext = ".txt";
    if(testFile) {
        fclose(testFile);
    }
    char buffer[1024];
    uprv_strcpy(buffer, testDataPath);
    uprv_strcat(buffer, type);
    int32_t bufLen = (int32_t)uprv_strlen(buffer);

    // we try to open 3 files:
    // path/CollationTest_type.txt
    // path/CollationTest_type_SHORT.txt
    // path/CollationTest_type_STUB.txt
    // we are going to test with the first one that we manage to open.

    uprv_strcpy(buffer+bufLen, ext);

    testFile = fopen(buffer, "rb");

    if(testFile == 0) {
        uprv_strcpy(buffer+bufLen, "_SHORT");
        uprv_strcat(buffer, ext);
        testFile = fopen(buffer, "rb");

        if(testFile == 0) {
            uprv_strcpy(buffer+bufLen, "_STUB");
            uprv_strcat(buffer, ext);
            testFile = fopen(buffer, "rb");

            if (testFile == 0) {
                *(buffer+bufLen) = 0;
                dataerrln("Could not open any of the conformance test files, tried opening base %s\n", buffer);
                return;        
            } else {
                infoln(
                    "INFO: Working with the stub file.\n"
                    "If you need the full conformance test, please\n"
                    "download the appropriate data files from:\n"
                    "http://source.icu-project.org/repos/icu/tools/trunk/unicodetools/com/ibm/text/data/");
            }
        }
    }
}

static UBool
skipLineBecauseOfBug(const UChar *s, int32_t length, UBool isShifted) {
    // TODO: Fix ICU ticket #8052
    if(length >= 3 &&
            (s[0] == 0xfb2 || s[0] == 0xfb3) &&
            s[1] == 0x334 &&
            (s[2] == 0xf73 || s[2] == 0xf75 || s[2] == 0xf81)) {
        return TRUE;
    }
    // TODO: Fix ICU ticket #9361
    if(isShifted && length >= 2 && s[0] == 0xfffe) {
        return TRUE;
    }
    return FALSE;
}

static UCollationResult
normalizeResult(int32_t result) {
    return result<0 ? UCOL_LESS : result==0 ? UCOL_EQUAL : UCOL_GREATER;
}

void UCAConformanceTest::testConformance(const UCollator *coll) 
{
    if(testFile == 0) {
        return;
    }
    UBool isShifted = ucol_getAttribute(coll, UCOL_ALTERNATE_HANDLING, &status) == UCOL_SHIFTED;

    int32_t line = 0;

    UChar b1[1024], b2[1024];
    UChar *buffer = b1, *oldB = NULL;

    char lineB1[1024], lineB2[1024];
    char *lineB = lineB1, *oldLineB = lineB2;

    uint8_t sk1[1024], sk2[1024];
    uint8_t *oldSk = NULL, *newSk = sk1;

    int32_t oldLen = 0;
    int32_t oldBlen = 0;
    uint32_t first = 0;


    while (fgets(lineB, 1024, testFile) != NULL) {
        // remove trailing whitespace
        u_rtrim(lineB);

        line++;
        if(*lineB == 0 || lineB[0] == '#') {
            continue;
        }
        int32_t buflen = u_parseString(lineB, buffer, 1024, &first, &status);
        if(U_FAILURE(status)) {
            errln("Error parsing line %ld (%s): %s\n",
                  (long)line, u_errorName(status), lineB);
            status = U_ZERO_ERROR;
        }
        buffer[buflen] = 0;

        if(skipLineBecauseOfBug(buffer, buflen, isShifted)) {
            logln("Skipping line %i because of a known bug", line);
            continue;
        }

        int32_t resLen = ucol_getSortKey(coll, buffer, buflen, newSk, 1024);

        if(oldSk != NULL) {
            int32_t skres = strcmp((char *)oldSk, (char *)newSk);
            int32_t cmpres = ucol_strcoll(coll, oldB, oldBlen, buffer, buflen);
            int32_t cmpres2 = ucol_strcoll(coll, buffer, buflen, oldB, oldBlen);

            if(cmpres != -cmpres2) {
                errln("Compare result not symmetrical on line %i", line);
            }

            if(cmpres != normalizeResult(skres)) {
                errln("Difference between ucol_strcoll (%d) and sortkey compare (%d) on line %i",
                      cmpres, skres, line);
                errln("  Previous data line %s", oldLineB);
                errln("  Current data line  %s", lineB);
            }

            int32_t res = cmpres;
            if(res == 0 && !isAtLeastUCA62) {
                // Up to UCA 6.1, the collation test files use a custom tie-breaker,
                // comparing the raw input strings.
                res = u_strcmpCodePointOrder(oldB, buffer);
                // Starting with UCA 6.2, the collation test files use the standard UCA tie-breaker,
                // comparing the NFD versions of the input strings,
                // which we do via setting strength=identical.
            }
            if(res > 0) {
                errln("Line %i is not greater or equal than previous line", line);
                errln("  Previous data line %s", oldLineB);
                errln("  Current data line  %s", lineB);
                UnicodeString oldS, newS;
                prettify(CollationKey(oldSk, oldLen), oldS);
                prettify(CollationKey(newSk, resLen), newS);
                errln("  Previous key: "+oldS);
                errln("  Current key:  "+newS);
            }
        }

        // swap buffers
        oldLineB = lineB;
        oldB = buffer;
        oldSk = newSk;
        if(lineB == lineB1) {
            lineB = lineB2;
            buffer = b2;
            newSk = sk2;
        } else {
            lineB = lineB1;
            buffer = b1;
            newSk = sk1;
        }
        oldLen = resLen;
        oldBlen = buflen;
    }
}

void UCAConformanceTest::TestTableNonIgnorable(/* par */) {
    setCollNonIgnorable(UCA);
    openTestFile("NON_IGNORABLE");
    testConformance(UCA);
}

void UCAConformanceTest::TestTableShifted(/* par */) {
    setCollShifted(UCA);
    openTestFile("SHIFTED");
    testConformance(UCA);
}

void UCAConformanceTest::TestRulesNonIgnorable(/* par */) {
    initRbUCA();

    if(U_SUCCESS(status)) {
        setCollNonIgnorable(rbUCA);
        openTestFile("NON_IGNORABLE");
        testConformance(rbUCA);
    }
}

void UCAConformanceTest::TestRulesShifted(/* par */) {
    logln("This test is currently disabled, as it is impossible to "
        "wholly represent fractional UCA using tailoring rules.");
    return;

    initRbUCA();

    if(U_SUCCESS(status)) {
        setCollShifted(rbUCA);
        openTestFile("SHIFTED");
        testConformance(rbUCA);
    }
}

#endif /* #if !UCONFIG_NO_COLLATION */
