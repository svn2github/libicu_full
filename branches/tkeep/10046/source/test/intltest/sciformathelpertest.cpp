/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File SCIFORMATHELPERTEST.CPP
*
*******************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>

#include "intltest.h"

// TODO: U_CONFIG_NO_FORMATTING

#include "unicode/sciformathelper.h"
#include "unicode/numfmt.h"
#include "unicode/decimfmt.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

class SciFormatHelperTest : public IntlTest {
public:
    SciFormatHelperTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestBasic();
};

void SciFormatHelperTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite SciFormatHelperTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasic);
    TESTCASE_AUTO_END;
}

void SciFormatHelperTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *decfmt = (DecimalFormat *) NumberFormat::createScientificInstance("en", status);
    UnicodeString appendTo("String: ");
    FieldPositionIterator fpositer;
    decfmt->format(1.23456e-78, appendTo, &fpositer, status);
    UnicodeString appendTo2(appendTo);
    FieldPositionIterator fpositer2(fpositer);
    SciFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    errln(helper.insetMarkup(appendTo, fpositer, "<sup>", "</sup>", status));
    errln(helper.toSuperscriptExponentDigits(appendTo2, fpositer2, status));
}

extern IntlTest *createSciFormatHelperTest() {
    return new SciFormatHelperTest();
}

