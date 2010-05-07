/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uts46test.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010may05
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_IDNA

#include "unicode/bytestream.h"
#include "unicode/idna.h"
#include "unicode/std_string.h"
#include "unicode/stringpiece.h"
#include "unicode/uidna.h"
#include "unicode/unistr.h"
#include "intltest.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

class UTS46Test : public IntlTest {
public:
    UTS46Test() : trans(NULL), nontrans(NULL) {}
    virtual ~UTS46Test();

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);
    void TestAPI();
    void TestSomeCases();
private:
    IDNA *trans, *nontrans;
};

extern IntlTest *createUTS46Test() {
    return new UTS46Test();
}

UTS46Test::~UTS46Test() {
    delete trans;
    delete nontrans;
}

void UTS46Test::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite UTS46Test: ");
        if(trans==NULL) {
            IcuTestErrorCode errorCode(*this, "init/createUTS46Instance()");
            trans=IDNA::createUTS46Instance(
                UIDNA_USE_STD3_RULES|UIDNA_CHECK_BIDI|UIDNA_CHECK_CONTEXTJ,
                errorCode);
            nontrans=IDNA::createUTS46Instance(
                UIDNA_USE_STD3_RULES|UIDNA_CHECK_BIDI|UIDNA_CHECK_CONTEXTJ|
                UIDNA_NONTRANSITIONAL_TO_ASCII|UIDNA_NONTRANSITIONAL_TO_UNICODE,
                errorCode);
            if(errorCode.logDataIfFailureAndReset("createUTS46Instance()")) {
                name="";
                return;
            }
        }
    }
    switch (index) {
        TESTCASE(0, TestAPI);
        TESTCASE(1, TestSomeCases);
        default:
            name="";
            break;  // needed to end the loop
    }
}

void UTS46Test::TestAPI() {
    IcuTestErrorCode errorCode(*this, "TestAPI");
    UnicodeString result;
    IDNAInfo info;
    UnicodeString input=UNICODE_STRING_SIMPLE("www.eXample.cOm");
    UnicodeString expected=UNICODE_STRING_SIMPLE("www.example.com");
    trans->nameToASCII(input, result, info, errorCode);
    if( !errorCode.logIfFailureAndReset("trans->nameToASCII(www.example.com)") &&
        (info.hasErrors() || result!=expected)
    ) {
        errln("trans->nameToASCII(www.example.com) info.errors=%04lx result matches=%d",
              (long)info.getErrors(), result==expected);
    }
}

// TODO: Test various options combinations, e.g., not STD3 passing through non-LDH ASCII.
// TODO: TestToASCII with a few examples, because the following tests it indirectly.

struct TestCase {
    // Input string and options string (Nontransitional/Transitional/Both).
    const char *s, *o;
    // Expected Unicode result string.
    const char *u;
    uint32_t errors;
};

static const TestCase testCases[]={
    { "www.eXample.cOm", "B",  // all ASCII
      "www.example.com", 0 },
    { "B\\u00FCcher.de", "B",  // u-umlaut
      "b\\u00FCcher.de", 0 },
    { "\\u00D6BB", "B",  // O-umlaut
      "\\u00F6bb", 0 },
    { "fa\\u00DF.de", "N",  // sharp s
      "fa\\u00DF.de", 0 },
    { "fa\\u00DF.de", "T",  // sharp s
      "fass.de", 0 },
    { "XN--fA-hia.dE", "B",  // sharp s in Punycode
      "fa\\u00DF.de", 0 },
    { "\\u03B2\\u03CC\\u03BB\\u03BF\\u03C2.com", "N",  // Greek with final sigma
      "\\u03B2\\u03CC\\u03BB\\u03BF\\u03C2.com", 0 },
    { "\\u03B2\\u03CC\\u03BB\\u03BF\\u03C2.com", "T",  // Greek with final sigma
      "\\u03B2\\u03CC\\u03BB\\u03BF\\u03C3.com", 0 },
    { "xn--nxasmm1c", "B",  // Greek with final sigma in Punycode
      "\\u03B2\\u03CC\\u03BB\\u03BF\\u03C2", 0 },
    { "www.\\u0DC1\\u0DCA\\u200D\\u0DBB\\u0DD3.com", "N",  // "Sri" in "Sri Lanka" has a ZWJ
      "www.\\u0DC1\\u0DCA\\u200D\\u0DBB\\u0DD3.com", 0 },
    { "www.\\u0DC1\\u0DCA\\u200D\\u0DBB\\u0DD3.com", "T",  // "Sri" in "Sri Lanka" has a ZWJ
      "www.\\u0DC1\\u0DCA\\u0DBB\\u0DD3.com", 0 },
    { "www.xn--10cl1a0b660p.com", "B",  // "Sri" in Punycode
      "www.\\u0DC1\\u0DCA\\u200D\\u0DBB\\u0DD3.com", 0 },
    { "\\u0646\\u0627\\u0645\\u0647\\u200C\\u0627\\u06CC", "N",  // ZWNJ
      "\\u0646\\u0627\\u0645\\u0647\\u200C\\u0627\\u06CC", 0 },
    { "\\u0646\\u0627\\u0645\\u0647\\u200C\\u0627\\u06CC", "T",  // ZWNJ
      "\\u0646\\u0627\\u0645\\u0647\\u0627\\u06CC", 0 },
    { "xn--mgba3gch31f060k.com", "B",  // ZWNJ in Punycode
      "\\u0646\\u0627\\u0645\\u0647\\u200C\\u0627\\u06CC.com", 0 },
    { "a.b\\uFF0Ec\\u3002d\\uFF61", "B",
      "a.b.c.d.", 0 },
    { "U\\u0308.xn--tda", "B",  // U+umlaut.u-umlaut
      "\\u00FC.\\u00FC", 0 },
    { "xn--u-ccb", "B",  // u+umlaut in Punycode
      "\\u00FC", UIDNA_ERROR_INVALID_ACE_LABEL },
    { "a\\u2488com", "B",  // contains 1-dot
      "a\\uFFFDcom", UIDNA_ERROR_DISALLOWED },
    { "xn--a-ecp.ru", "B",  // contains 1-dot in Punycode
      "a\\uFFFD.ru", UIDNA_ERROR_INVALID_ACE_LABEL|UIDNA_ERROR_DISALLOWED },
    { "xn--0.pt", "B",  // invalid Punycode
      "xn--0\\uFFFD.pt", UIDNA_ERROR_PUNYCODE },
    { "xn--a.pt", "B",  // U+0080
      "\\uFFFD.pt", UIDNA_ERROR_INVALID_ACE_LABEL|UIDNA_ERROR_DISALLOWED },
    { "xn--a-\\u00C4.pt", "B",  // invalid Punycode
      "xn--a-\\u00E4.pt", UIDNA_ERROR_PUNYCODE },
    { "\\u65E5\\u672C\\u8A9E\\u3002\\uFF2A\\uFF30", "B",  // Japanese with fullwidth ".jp"
      "\\u65E5\\u672C\\u8A9E.jp", 0 },
    { "\\u2615", "B", "\\u2615", UIDNA_ERROR_BIDI },  // Unicode 4.0 HOT BEVERAGE
    // many deviation characters, test the special mapping code
    { "1.a\\u00DF\\u200C\\u200Db\\u200C\\u200Dc\\u00DF\\u00DF\\u00DF\\u00DFd"
      "\\u03C2\\u03C3\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFe"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFx"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFy"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u0302\\u00DFz", "N",
      "1.a\\u00DF\\u200C\\u200Db\\u200C\\u200Dc\\u00DF\\u00DF\\u00DF\\u00DFd"
      "\\u03C2\\u03C3\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFe"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFx"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFy"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u0302\\u00DFz",
      UIDNA_ERROR_CONTEXTJ },
    { "1.a\\u00DF\\u200C\\u200Db\\u200C\\u200Dc\\u00DF\\u00DF\\u00DF\\u00DFd"
      "\\u03C2\\u03C3\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFe"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFx"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DFy"
      "\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u00DF\\u0302\\u00DFz", "T",
      "1.assbcssssssssd"
      "\\u03C3\\u03C3sssssssssssssssse"
      "ssssssssssssssssssssx"
      "ssssssssssssssssssssy"
      "sssssssssssssss\\u015Dssz", 0 },
    // "xn--bss" with deviation characters
    { "\\u200Cx\\u200Dn\\u200C-\\u200D-b\\u00DF", "N",
      "\\u200Cx\\u200Dn\\u200C-\\u200D-b\\u00DF", UIDNA_ERROR_BIDI|UIDNA_ERROR_CONTEXTJ },
    { "\\u200Cx\\u200Dn\\u200C-\\u200D-b\\u00DF", "T",
      "\\u5919", 0 },
    // { "", "B",
    //   "", 0 },
};

void UTS46Test::TestSomeCases() {
    IcuTestErrorCode errorCode(*this, "TestSomeCases");
    int32_t i;
    for(i=0; i<LENGTHOF(testCases); ++i) {
        const TestCase &testCase=testCases[i];
        UnicodeString input(ctou(testCase.s));
        UnicodeString expected(ctou(testCase.u));
        // ToASCII/ToUnicode, transitional/nontransitional
        UnicodeString aT, uT, aN, uN;
        IDNAInfo aTInfo, uTInfo, aNInfo, uNInfo;
        //trans->nameToASCII(input, aT, aTInfo, errorCode);
        trans->nameToUnicode(input, uT, uTInfo, errorCode);
        //nontrans->nameToASCII(input, aN, aNInfo, errorCode);
        nontrans->nameToUnicode(input, uN, uNInfo, errorCode);
        if(errorCode.logIfFailureAndReset("first-level processing [%d] %s", (int)i, testCase.s)) {
            continue;
        }
        // ToUnicode does not set length errors.
        uint32_t uniErrors=testCase.errors&~
            (UIDNA_ERROR_EMPTY_LABEL|
             UIDNA_ERROR_LABEL_TOO_LONG|
             UIDNA_ERROR_DOMAIN_NAME_TOO_LONG);
        char mode=testCase.o[0];
        if((mode=='B' || mode=='N') && (uN!=expected || uNInfo.getErrors()!=uniErrors)) {
            char buffer[300];
            prettify(uN).extract(0, 0x7fffffff, buffer, 300);
            errln("N.nameToUnicode([%d] %s) unexpected errors %04lx or string %s",
                  (int)i, testCase.s, (long)uNInfo.getErrors(), buffer);
            continue;
        }
        if((mode=='B' || mode=='T') && (uT!=expected || uTInfo.getErrors()!=uniErrors)) {
            char buffer[300];
            prettify(uT).extract(0, 0x7fffffff, buffer, 300);
            errln("T.nameToUnicode([%d] %s) unexpected errors %04lx or string %s",
                  (int)i, testCase.s, (long)uTInfo.getErrors(), buffer);
            continue;
        }
        // labelToUnicode
        // second-level processing
        // UTF-8 if we have std::string
        // ToASCII is all-ASCII if no errors
    }
}

#endif  // UCONFIG_NO_IDNA
