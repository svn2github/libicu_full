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
        errln("trans->nameToASCII(www.example.com) info.errors=%08lx result matches=%d",
              (long)info.getErrors(), result==expected);
    }
}

void UTS46Test::TestSomeCases() {
}

#endif  // UCONFIG_NO_IDNA
