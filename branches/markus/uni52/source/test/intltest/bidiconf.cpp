/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  bidiconf.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009oct16
*   created by: Markus W. Scherer
*
*   BiDi conformance test, using the Unicode BidiTest.txt file.
*/

#include <stdio.h>
#include <string.h>
#include "unicode/utypes.h"
#include "unicode/ubidi.h"
#include "unicode/errorcode.h"
#include "unicode/putil.h"
#include "intltest.h"
#include "uparse.h"

class BiDiConformanceTest : public IntlTest {
public:
    BiDiConformanceTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);

    void TestBidiTest();
private:
    char *getUnidataPath(char path[]);

    int32_t parseLevels(const char *start, UBiDiLevel levels[]);
    int32_t parseOrdering(const char *start, int32_t ordering[]);
    UnicodeString parseInputStringFromBiDiClasses(const char *&start);

    void checkLevels(const char *line, const UnicodeString &s,
                     const UBiDiLevel expectedLevels[], int32_t expectedCount,
                     const UBiDiLevel actualLevels[], int32_t actualCount,
                     const char *paraLevelName);
};

extern IntlTest *createBiDiConformanceTest() {
    return new BiDiConformanceTest();
}

// TODO: Move to intltest.h.
class IntlTestErrorCode : public ErrorCode {
public:
    IntlTestErrorCode(IntlTest &callingTestSuite, const char *callingTestName) :
        testSuite(callingTestSuite), testName(callingTestName) {}
    virtual ~IntlTestErrorCode() {
        // Safe because our handleFailure() does not throw exceptions.
        if(isFailure()) { handleFailure(); }
    }
    // TODO: Move to ErrorCode base class.
    const char *errorName() const { return u_errorName(errorCode); }
    // Returns TRUE if isFailure().
    UBool logIfFailureAndReset(const char *s) {
        if(isFailure()) {
            testSuite.errln("%s %s failure - %s", testName, s, errorName());
            reset();
            return TRUE;
        } else {
            reset();
            return FALSE;
        }
    }
protected:
    virtual void handleFailure() const {
        testSuite.errln("%s failure - %s", testName, errorName());
    }
private:
    IntlTest &testSuite;
    const char *const testName;
};

void BiDiConformanceTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *par) {
    if(exec) {
        logln("TestSuite BiDiConformanceTest: ");
    }
    switch (index) {
        TESTCASE(0, TestBidiTest);
        default:
            name="";
            break; // needed to end the loop
    }
}

// TODO: Move to a common place (IntlTest?) to avoid duplication with UnicodeTest (ucdtest.cpp).
char *BiDiConformanceTest::getUnidataPath(char path[]) {
    IntlTestErrorCode errorCode(*this, "getUnidataPath");
    const int kUnicodeDataTxtLength=15;  // strlen("UnicodeData.txt")

    // Look inside ICU_DATA first.
    strcpy(path, pathToDataDirectory());
    strcat(path, "unidata" U_FILE_SEP_STRING "UnicodeData.txt");
    FILE *f=fopen(path, "r");
    if(f!=NULL) {
        fclose(f);
        *(strchr(path, 0)-kUnicodeDataTxtLength)=0;  // Remove the basename.
        return path;
    }

    // As a fallback, try to guess where the source data was located
    // at the time ICU was built, and look there.
#   ifdef U_TOPSRCDIR
        strcpy(path, U_TOPSRCDIR  U_FILE_SEP_STRING "data");
#   else
        strcpy(path, loadTestData(errorCode));
        strcat(path, U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".."
                     U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".."
                     U_FILE_SEP_STRING "data");
#   endif
    strcat(path, U_FILE_SEP_STRING);
    strcat(path, "unidata" U_FILE_SEP_STRING "UnicodeData.txt");
    f=fopen(path, "r");
    if(f!=NULL) {
        fclose(f);
        *(strchr(path, 0)-kUnicodeDataTxtLength)=0;  // Remove the basename.
        return path;
    }
    return NULL;
}

U_DEFINE_LOCAL_OPEN_POINTER(LocalStdioFilePointer, FILE, fclose);
U_DEFINE_LOCAL_OPEN_POINTER(LocalUBiDiPointer, UBiDi, ubidi_close);

// TODO: Make "public" in uparse.h.
#define U_IS_INV_WHITESPACE(c) ((c)==' ' || (c)=='\t' || (c)=='\r' || (c)=='\n')

int32_t BiDiConformanceTest::parseLevels(const char *start, UBiDiLevel levels[]) {
    int32_t length=0;
    while(*start!=0 && *(start=u_skipWhitespace(start))!=0) {
        if(*start=='x') {
            levels[length++]=UBIDI_DEFAULT_LTR;
            ++start;
        } else {
            char *end;
            uint32_t value=(uint32_t)strtoul(start, &end, 10);
            if(end<=start || (!U_IS_INV_WHITESPACE(*end) && *end!=0) || value>(UBIDI_MAX_EXPLICIT_LEVEL+1)) {
                errln("@Levels: parse error at %s", start);
                return -1;
            }
            levels[length++]=(UBiDiLevel)value;
            start=end;
        }
    }
    return length;
}

int32_t BiDiConformanceTest::parseOrdering(const char *start, int32_t ordering[]) {
    int32_t length=0;
    while(*start!=0 && *(start=u_skipWhitespace(start))!=0) {
        char *end;
        uint32_t value=(uint32_t)strtoul(start, &end, 10);
        if(end<=start || (!U_IS_INV_WHITESPACE(*end) && *end!=0) || value>=1000) {
            errln("@Reorder: parse error at %s", start);
            return -1;
        }
        ordering[length++]=(int32_t)value;
        start=end;
    }
    return length;
}

static const UChar charFromBiDiClass[U_CHAR_DIRECTION_COUNT]={
    0x6c,   // 'l' for L
    0x52,   // 'R' for R
    0x33,   // '3' for EN
    0x2d,   // '-' for ES
    0x25,   // '%' for ET
    0x39,   // '9' for AN
    0x2c,   // ',' for CS
    0x2f,   // '/' for B
    0x5f,   // '_' for S
    0x20,   // ' ' for WS
    0x3d,   // '=' for ON
    0x65,   // 'e' for LRE
    0x6f,   // 'o' for LRO
    0x41,   // 'A' for AL
    0x45,   // 'E' for RLE
    0x4f,   // 'O' for RLO
    0x2a,   // '*' for PDF
    0x60,   // '`' for NSM
    0x7c    // '|' for BN
};

U_CDECL_BEGIN

static UCharDirection U_CALLCONV
biDiConfUBiDiClassCallback(const void *context, UChar32 c) {
    for(int i=0; i<U_CHAR_DIRECTION_COUNT; ++i) {
        if(c==charFromBiDiClass[i]) {
            return (UCharDirection)i;
        }
    }
    // Character not in our hardcoded table.
    // Should not occur during testing.
    return U_BIDI_CLASS_DEFAULT;
}

U_CDECL_END

UnicodeString BiDiConformanceTest::parseInputStringFromBiDiClasses(const char *&start) {
    UnicodeString s;
    char bidiClassString[20];
    while(*start!=0 && *(start=u_skipWhitespace(start))!=0 && *start!=';') {
        int32_t bidiClassStringLength=0;
        while(*start!=0 && *start!=';' && !U_IS_INV_WHITESPACE(*start)) {
            if(bidiClassStringLength>=(sizeof(bidiClassString)-1)) {
                errln("BiDi class string too long at %s", start-bidiClassStringLength);
                s.setToBogus();
                return s;
            }
            bidiClassString[bidiClassStringLength++]=*start++;
        }
        bidiClassString[bidiClassStringLength]=0;
        int32_t bidiClassInt=u_getPropertyValueEnum(UCHAR_BIDI_CLASS, bidiClassString);
        if(bidiClassInt<0) {
            errln("BiDi class string not recognized at %s", start-bidiClassStringLength);
            s.setToBogus();
            return s;
        }
        s.append(charFromBiDiClass[bidiClassInt]);
    }
    return s;
}

void BiDiConformanceTest::TestBidiTest() {
    char unidataPath[400];
    if(getUnidataPath(unidataPath)==NULL) {
        errln("unable to find the source/data/unidata folder");
    }
    strcat(unidataPath, "BidiTest.txt");
    LocalStdioFilePointer bidiTestFile(fopen(unidataPath, "r"));
    if(bidiTestFile.isNull()) {
        errln("unable to open %s", unidataPath);
    }
    LocalUBiDiPointer ubidi(ubidi_open());
    IntlTestErrorCode errorCode(*this, "TestBidiTest");
    ubidi_setClassCallback(ubidi.getAlias(), biDiConfUBiDiClassCallback, NULL,
                           NULL, NULL, errorCode);
    if(errorCode.logIfFailureAndReset("ubidi_setClassCallback()")) {
        return;
    }
    static char line[10000];
    static UBiDiLevel levels[1000];
    static int32_t ordering[1000];
    int32_t levelsCount=0, orderingCount=0;
    while(fgets(line, (int)sizeof(line), bidiTestFile.getAlias())!=NULL) {
        // Remove trailing comments and whitespace.
        char *commentStart=strchr(line, '#');
        if(commentStart!=NULL) {
            *commentStart=0;
        }
        u_rtrim(line);
        const char *start=u_skipWhitespace(line);
        if(*start==0) {
            continue;  // Skip empty and comment-only lines.
        }
        if(*start=='@') {
            ++start;
            if(0==strncmp(start, "Levels:", 7)) {
                if((levelsCount=parseLevels(start+7, levels))<0) {
                    return;
                }
            } else if(0==strncmp(start, "Reorder:", 8)) {
                if((orderingCount=parseOrdering(start+8, ordering))<0) {
                    return;
                }
            }
            // Skip unknown @Xyz: ...
        } else {
            UnicodeString s=parseInputStringFromBiDiClasses(start);
            if(s.isBogus()) {
                return;
            }
            start=u_skipWhitespace(start);
            if(*start!=';') {
                errln("missing ; separator on input line %s", line);
                return;
            }
            start=u_skipWhitespace(start+1);
            char *end;
            uint32_t bitset=(uint32_t)strtoul(start, &end, 10);
            if(end<=start || (!U_IS_INV_WHITESPACE(*end) && *end!=';' && *end!=0)) {
                errln("input bitset parse error at %s", start);
                return;
            }
            // Loop over the bitset.
            static const UBiDiLevel paraLevels[]={ UBIDI_DEFAULT_LTR, 0, 1 };
            static const char *const paraLevelNames[]={ "auto/LTR", "LTR", "RTL" };
            for(int i=0; i<=2; ++i) {
                if(bitset&(1<<i)) {
                    ubidi_setPara(ubidi.getAlias(), s.getBuffer(), s.length(),
                                  paraLevels[i], NULL, errorCode);
                    const UBiDiLevel *actualLevels=ubidi_getLevels(ubidi.getAlias(), errorCode);
                    if(errorCode.logIfFailureAndReset("ubidi_setPara() or ubidi_getLevels()")) {
                        errln("Input line: %s", line);
                        return;
                    }
                    checkLevels(line, s,
                                levels, levelsCount,
                                actualLevels, ubidi_getProcessedLength(ubidi.getAlias()),
                                paraLevelNames[i]);
                }
            }
        }
    }
}

static UChar printLevel(UBiDiLevel level) {
    if(level<UBIDI_DEFAULT_LTR) {
        return 0x30+level;
    } else {
        return 0x78;  // 'x'
    }
}

void BiDiConformanceTest::checkLevels(const char *line, const UnicodeString &s,
                                      const UBiDiLevel expectedLevels[], int32_t expectedCount,
                                      const UBiDiLevel actualLevels[], int32_t actualCount,
                                      const char *paraLevelName) {
    UBool isError=FALSE;
    if(expectedCount!=actualCount) {
        errln("Wrong number of level values; expected %d actual %d",
              (int)expectedCount, (int)actualCount);
        isError=TRUE;
    } else {
        for(int32_t i=0; i<actualCount; ++i) {
            if(expectedLevels[i]!=actualLevels[i] && expectedLevels[i]<UBIDI_DEFAULT_LTR) {
                errln("Wrong level value at index %d; expected %d actual %d",
                      (int)i, expectedLevels[i], actualLevels[i]);
                isError=TRUE;
                break;
            }
        }
    }
    if(isError) {
        errln("Input line:         %s", line);
        errln(UnicodeString("Input string:       ")+s);
        errln("Para level:         %s", paraLevelName);
        UnicodeString els("Expected levels:   ");
        UnicodeString als("Actual   levels:   ");
        for(int32_t i=0; i<actualCount; ++i) {
            els.append(0x20).append(printLevel(expectedLevels[i]));
            als.append(0x20).append(printLevel(actualLevels[i]));
        }
        errln(els);
        errln(als);
    }
}
