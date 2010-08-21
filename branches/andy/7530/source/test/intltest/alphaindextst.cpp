/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2010, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
//
//   file:  alphaindex.cpp
//          Alphabetic Index Tests.
//
#include "intltest.h"

#include "unicode/indexchars.h"
#include "alphaindextst.h"

AlphabeticIndexTest::AlphabeticIndexTest() {
}

AlphabeticIndexTest::~AlphabeticIndexTest() {
}

void AlphabeticIndexTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite RegexTest: ");
    switch (index) {

        case 0: name = "APITest";
            if (exec) APITest();
            break;

         default: name = "";
            break; //needed to end loop
    }
}

#define TEST_CHECK_STATUS {if (U_FAILURE(status)) {dataerrln("%s:%d: Test failure.  status=%s", \
                                                              __FILE__, __LINE__, u_errorName(status)); return;}}

#define TEST_ASSERT(expr) {if ((expr)==FALSE) {errln("%s:%d: Test failure \n", __FILE__, __LINE__);};}

void AlphabeticIndexTest::APITest() {
    UErrorCode status = U_ZERO_ERROR;
    IndexCharacters *index = new IndexCharacters(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    int32_t lc = index->countLabels(status);
    TEST_CHECK_STATUS;

    printf("countLabels() == %d", lc);

    delete index;
}

