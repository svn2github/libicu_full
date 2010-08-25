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

#include <string>
#include <iostream>

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

        case 1: name = "ManyLocales";
            if (exec) ManyLocalesTest();
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
    int32_t lc = 0;
    IndexCharacters *index = new IndexCharacters(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    lc = index->countLabels(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(28 == lc);    // 26 letters plus two under/overflow labels.
    //printf("countLabels() == %d\n", lc);

    // Add an item, verify that it comes back out.
    //
    const UnicodeString adam = UNICODE_STRING_SIMPLE("Adam");
    index->addItem(UnicodeString("Adam"), this, status);
    UBool   b;
    TEST_CHECK_STATUS;
    index->resetLabelIterator(status);
    TEST_CHECK_STATUS;
    index->nextLabel(status);  // Move to underflow label
    index->nextLabel(status);  // Move to "A"
    TEST_CHECK_STATUS;
    const UnicodeString &label2 = index->getLabel();
    UnicodeString A_STR = UNICODE_STRING_SIMPLE("A");
    TEST_ASSERT(A_STR == label2);

    b = index->nextItem(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(b);
    const UnicodeString &itemName = index->getItemName();
    TEST_ASSERT(adam == itemName);

    const void *itemContext = index->getItemContext();
    TEST_ASSERT(itemContext == this);

    
    delete index;
}


static const char * KEY_LOCALES[] = {
            "en", "es", "de", "fr", "ja", "it", "tr", "pt", "zh", "nl", 
            "pl", "ar", "ru", "zh_Hant", "ko", "th", "sv", "fi", "da", 
            "he", "nb", "el", "hr", "bg", "sk", "lt", "vi", "lv", "sr", 
            "pt_PT", "ro", "hu", "cs", "id", "sl", "fil", "fa", "uk", 
            "ca", "hi", "et", "eu", "is", "sw", "ms", "bn", "am", "ta", 
            "te", "mr", "ur", "ml", "kn", "gu", "or", ""};


void AlphabeticIndexTest::ManyLocalesTest() {
    UErrorCode status = U_ZERO_ERROR;
    int32_t  lc = 0;
    IndexCharacters *index = NULL;

    for (int i=0; ; ++i) {
        status = U_ZERO_ERROR;
        const char *localeName = KEY_LOCALES[i];
        if (localeName[0] == 0) {
            break;
        }
        // std::cout <<  localeName << "  ";
        Locale loc = Locale::createFromName(localeName);
        index = new IndexCharacters(loc, status);
        TEST_CHECK_STATUS;
        lc = index->countLabels(status);
        TEST_CHECK_STATUS;
        // std::cout << "countLabels() == " << lc << std::endl;

        while (index->nextLabel(status)) {
            TEST_CHECK_STATUS;
            const UnicodeString &label = index->getLabel();
            // std::string ss;
            // std::cout << ":" << label.toUTF8String(ss);
        }
        // std::cout << ":" << std::endl;


        delete index;
    }
}

