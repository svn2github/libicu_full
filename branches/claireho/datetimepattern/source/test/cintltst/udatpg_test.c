/*
*******************************************************************************
*
*   Copyright (C) 2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udatpg_test.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007aug01
*   created by: Markus W. Scherer
*
*   Test of the C wrapper for the DateTimePatternGenerator.
*   Calls each C API function and exercises code paths in the wrapper,
*   but the full functionality is tested in the C++ intltest.
*
*   One item to note: C API functions which return a const UChar *
*   should return a NUL-terminated string.
*   (The C++ implementation needs to use getTerminatedBuffer()
*   on UnicodeString objects which end up being returned this way.)
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udatpg.h"
#include "unicode/ustring.h"
#include "cintltst.h"

#define TESTCASE(x) addTest(root, &x, "tsformat/udatpg_test/" #x)

static void TestOpenClose(void);
static void TestUsage(void);
static void TestBuilder(void);

void addDateTimePatternGeneratorTest(TestNode** root) {
    TESTCASE(TestOpenClose);
    TESTCASE(TestUsage);
    TESTCASE(TestBuilder);
}

/*
 * Pipe symbol '|'. We pass only the first UChar without NUL-termination.
 * The second UChar is just to verify that the API does not pick that up.
 */
static const UChar pipeString[]={ 0x7c, 0x0a };

static void TestOpenClose() {
    UErrorCode errorCode=U_ZERO_ERROR;
    UDateTimePatternGenerator *dtpg, *dtpg2;
    const UChar *s;
    int32_t length;

    /* Open a DateTimePatternGenerator for the default locale. */
    dtpg=udatpg_open(NULL, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_open(NULL) failed - %s\n", u_errorName(errorCode));
        return;
    }
    udatpg_close(dtpg);

    /* Now one for German. */
    dtpg=udatpg_open("de", &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_open(de) failed - %s\n", u_errorName(errorCode));
        return;
    }

    /* Make some modification which we verify gets passed on to the clone. */
    udatpg_setDecimal(dtpg, pipeString, 1);

    /* Clone the generator. */
    dtpg2=udatpg_clone(dtpg, &errorCode);
    if(U_FAILURE(errorCode) || dtpg2==NULL) {
        log_err("udatpg_clone() failed - %s\n", u_errorName(errorCode));
    }

    /* Verify that the clone has the custom decimal symbol. */
    s=udatpg_getDecimal(dtpg2, &length);
    if(s==pipeString || length!=1 || 0!=u_memcmp(s, pipeString, length) || s[length]!=0) {
        log_err("udatpg_getDecimal(cloned object) did not return the expected string\n");
    }

    udatpg_close(dtpg);
    udatpg_close(dtpg2);
}

static void TestUsage() {
}

static void TestBuilder() {
}

#endif
