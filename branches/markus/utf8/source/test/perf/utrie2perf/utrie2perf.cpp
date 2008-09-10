/*  
 **********************************************************************
 *   Copyright (C) 2002-2008, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 *  file name:  utrie2perf.cpp
 *  encoding:   US-ASCII
 *  tab size:   8 (not used)
 *  indentation:4
 *
 *  created on: 2008sep07
 *  created by: Markus W. Scherer
 *
 *  Performance test program for UTrie2.
 */

#include <stdio.h>
#include <stdlib.h>
#include "unicode/unorm.h"
#include "unicode/uperf.h"
#include "uoptions.h"

U_CAPI void U_EXPORT2
unorm_initUTrie2(UErrorCode *pErrorCode);

U_CAPI void U_EXPORT2
ubidi_initUTrie2(UErrorCode *pErrorCode);

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

// Test object.
class UTrie2PerfTest : public UPerfTest {
public:
    UTrie2PerfTest(int32_t argc, const char *argv[], UErrorCode &status)
            : UPerfTest(argc, argv, NULL, 0, "", status),
              utf8(NULL), utf8Length(0), countInputCodePoints(0) {
        if (U_SUCCESS(status)) {
            unorm_initUTrie2(&status);
            ubidi_initUTrie2(&status);
            int32_t inputLength;
            UPerfTest::getBuffer(inputLength, status);
            if(U_SUCCESS(status) && inputLength>0) {
                countInputCodePoints = u_countChar32(buffer, bufferLen);

                // Preflight the UTF-8 length and allocate utf8.
                u_strToUTF8(NULL, 0, &utf8Length, buffer, bufferLen, &status);
                if(status==U_BUFFER_OVERFLOW_ERROR) {
                    utf8=(char *)malloc(utf8Length);
                    if(utf8!=NULL) {
                        status=U_ZERO_ERROR;
                        u_strToUTF8(utf8, utf8Length, NULL, buffer, bufferLen, &status);
                    } else {
                        status=U_MEMORY_ALLOCATION_ERROR;
                    }
                }

                if(verbose) {
                    printf("code points:%ld  len16:%ld  len8:%ld  "
                           "B/cp:%.3g\n",
                           (long)countInputCodePoints, (long)bufferLen, (long)utf8Length,
                           (double)utf8Length/countInputCodePoints);
                }
            }
        }
    }

    virtual UPerfFunction* runIndexedTest(int32_t index, UBool exec, const char* &name, char* par = NULL);

    const UChar *getBuffer() const { return buffer; }
    int32_t getBufferLen() const { return bufferLen; }

    char *utf8;
    int32_t utf8Length;

    // Number of code points in the input text.
    int32_t countInputCodePoints;
};

// Performance test function object.
class Command : public UPerfFunction {
protected:
    Command(const UTrie2PerfTest &testcase) : testcase(testcase) {}

public:
    virtual ~Command() {}

    // virtual void call(UErrorCode* pErrorCode) { ... }

    virtual long getOperationsPerIteration() {
        // Number of code points tested.
        return testcase.countInputCodePoints;
    }

    // virtual long getEventsPerIteration();

    const UTrie2PerfTest &testcase;
    UNormalizationCheckResult qcResult;
};

class CheckFCD : public Command {
protected:
    CheckFCD(const UTrie2PerfTest &testcase) : Command(testcase) {}
public:
    static UPerfFunction* get(const UTrie2PerfTest &testcase) {
        return new CheckFCD(testcase);
    }
    virtual void call(UErrorCode* pErrorCode) {
        UErrorCode errorCode=U_ZERO_ERROR;
        qcResult=unorm_quickCheck(testcase.getBuffer(), testcase.getBufferLen(),
                                  UNORM_FCD, &errorCode);
        if(U_FAILURE(errorCode)) {
            fprintf(stderr, "error: unorm_quickCheck(UNORM_FCD) failed: %s\n",
                    u_errorName(errorCode));
        }
    }
};

class CheckFCDAlwaysGet : public Command {
protected:
    CheckFCDAlwaysGet(const UTrie2PerfTest &testcase) : Command(testcase) {}
public:
    static UPerfFunction* get(const UTrie2PerfTest &testcase) {
        return new CheckFCDAlwaysGet(testcase);
    }
    virtual void call(UErrorCode* pErrorCode) {
        UErrorCode errorCode=U_ZERO_ERROR;
        qcResult=unorm_quickCheck(testcase.getBuffer(), testcase.getBufferLen(),
                                  UNORM_FCD_ALWAYS_GET, &errorCode);
        if(U_FAILURE(errorCode)) {
            fprintf(stderr, "error: unorm_quickCheck(UNORM_FCD) failed: %s\n",
                    u_errorName(errorCode));
        }
    }
};

UPerfFunction* UTrie2PerfTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* par) {
    switch (index) {
        case 0: name = "CheckFCD";              if (exec) return CheckFCD::get(*this); break;
        case 1: name = "CheckFCDAlwaysGet";     if (exec) return CheckFCDAlwaysGet::get(*this); break;
        default: name = ""; break;
    }
    return NULL;
}

int main(int argc, const char *argv[]) {
    UErrorCode status = U_ZERO_ERROR;
    UTrie2PerfTest test(argc, argv, status);

	if (U_FAILURE(status)){
        printf("The error is %s\n", u_errorName(status));
        test.usage();
        return status;
    }
        
    if (test.run() == FALSE){
        fprintf(stderr, "FAILED: Tests could not be run please check the "
			            "arguments.\n");
        return -1;
    }

    return 0;
}
