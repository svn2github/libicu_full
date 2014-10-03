/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File SCIFORMATTERTEST.CPP
*
*******************************************************************************
*/
#include "unicode/utypes.h"

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/scientificformatter.h"
#include "unicode/numfmt.h"
#include "unicode/decimfmt.h"
#include "unicode/localpointer.h"

class ScientificFormatterTest : public IntlTest {
public:
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestBasic();
    void TestFarsi();
    void TestPlusSignInExponentMarkup();
    void TestPlusSignInExponentSuperscript();
    void TestFixedDecimalMarkup();
    void TestFixedDecimalSuperscript();
};

void ScientificFormatterTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite ScientificFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasic);
    TESTCASE_AUTO(TestFarsi);
    TESTCASE_AUTO(TestPlusSignInExponentMarkup);
    TESTCASE_AUTO(TestPlusSignInExponentSuperscript);
    TESTCASE_AUTO(TestFixedDecimalMarkup);
    TESTCASE_AUTO(TestFixedDecimalSuperscript);
    TESTCASE_AUTO_END;
}

void ScientificFormatterTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    UnicodeString prefix("String: ");
    UnicodeString appendTo(prefix);
    ScientificFormatter fmt(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::MarkupStyle("<sup>", "</sup>"),
            status);
    fmt.format(1.23456e-78, appendTo, status);
    const char *expected = "String: 1.23456\\u00d710<sup>-78</sup>";
    assertEquals(
            "markup style",
            UnicodeString(expected).unescape(),
            appendTo);

    // Test assignment operator while testing superscript style
    fmt = ScientificFormatter(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::SuperscriptStyle(),
            status);
    appendTo = prefix;
    fmt.format(1.23456e-78, appendTo, status);
    expected = "String: 1.23456\\u00d710\\u207b\\u2077\\u2078";
    assertEquals(
            "superscript style",
            UnicodeString(expected).unescape(),
            appendTo);
  
    // Test copy constructor
    ScientificFormatter fmt2(fmt);
    appendTo = prefix;
    fmt.format(1.23456e-78, appendTo, status);
    expected = "String: 1.23456\\u00d710\\u207b\\u2077\\u2078";
    assertEquals(
            "superscript style",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificFormatterTest::TestFarsi() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("fa", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"fa\", status) - %s", u_errorName(status));
        return;
    }
    UnicodeString prefix("String: ");
    UnicodeString appendTo(prefix);
    ScientificFormatter fmt(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::MarkupStyle("<sup>", "</sup>"),
            status);
    fmt.format(1.23456e-78, appendTo, status);
    const char *expected = "String: \\u06F1\\u066B\\u06F2\\u06F3\\u06F4\\u06F5\\u06F6\\u00d7\\u06F1\\u06F0<sup>\\u200E\\u2212\\u06F7\\u06F8</sup>";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificFormatterTest::TestPlusSignInExponentMarkup() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    decfmt->applyPattern("0.00E+0", status);
    if (!assertSuccess("", status)) {
        return;
    }
    UnicodeString appendTo;
    ScientificFormatter fmt(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::MarkupStyle("<sup>", "</sup>"),
            status);
    fmt.format(6.02e23, appendTo, status);
    const char *expected = "6.02\\u00d710<sup>+23</sup>";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificFormatterTest::TestPlusSignInExponentSuperscript() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    decfmt->applyPattern("0.00E+0", status);
    if (!assertSuccess("", status)) {
        return;
    }
    UnicodeString appendTo;
    ScientificFormatter fmt(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::SuperscriptStyle(),
            status);
    fmt.format(6.02e23, appendTo, status);
    const char *expected = "6.02\\u00d710\\u207a\\u00b2\\u00b3";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificFormatterTest::TestFixedDecimalMarkup() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance("en", status));
    if (assertSuccess("NumberFormat::createInstance", status, TRUE) == FALSE) {
        return;
    }
    ScientificFormatter fmt(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::MarkupStyle("<sup>", "</sup>"),
            status);
    UnicodeString appendTo;
    fmt.format(123456.0, appendTo, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR with fixed decimal number.");
    }
}

void ScientificFormatterTest::TestFixedDecimalSuperscript() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance("en", status));
    if (assertSuccess("NumberFormat::createInstance", status, TRUE) == FALSE) {
        return;
    }
    ScientificFormatter fmt(
            new DecimalFormat(*decfmt),
            new ScientificFormatter::SuperscriptStyle(),
            status);
    UnicodeString appendTo;
    fmt.format(123456.0, appendTo, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR with fixed decimal number.");
    }
}

extern IntlTest *createScientificFormatterTest() {
    return new ScientificFormatterTest();
}

#endif /* !UCONFIG_NO_FORMATTING */
