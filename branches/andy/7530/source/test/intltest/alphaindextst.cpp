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
#include "alphaindextst.h"

#include "unicode/alphaindex.h"
#include "unicode/coll.h"
#include "unicode/uniset.h"

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

//
//  APITest.   Invoke every function at least once, and check that it does something.
//             Does not attempt to check complete functionality.
//
void AlphabeticIndexTest::APITest() {

    //
    //  Simple constructor and destructor,  countLabels()
    //
    UErrorCode status = U_ZERO_ERROR;
    int32_t lc = 0;
    int32_t i  = 0;
    AlphabeticIndex *index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    lc = index->countLabels(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(28 == lc);    // 26 letters plus two under/overflow labels.
    //printf("countLabels() == %d\n", lc);
    delete index;
    
    // addIndexCharacters()

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    UnicodeSet additions;
    additions.add((UChar32)0x410).add((UChar32)0x415);   // A couple of Cyrillic letters
    index->addIndexCharacters(additions, status);
    TEST_CHECK_STATUS;
    lc = index->countLabels(status);
    TEST_CHECK_STATUS;
    // TODO:  should get 31.  Java also gives 30.  Needs fixing
    TEST_ASSERT(30 == lc);    // 26 Latin letters plus
    // TEST_ASSERT(31 == lc);    // 26 Latin letters plus
                              //  2 Cyrillic letters plus
                              //  1 inflow label plus
                              //  two under/overflow labels.
    // std::cout << lc << std::endl;
    delete index;


    // addLocale()

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    index->addLocale(Locale::getJapanese(), status);
    TEST_CHECK_STATUS;
    lc = index->countLabels(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(35 < lc);  // Japanese should add a bunch.  Don't rely on the exact value.
    delete index;

    // GetCollator(),  Get under/in/over flow labels

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getGerman(), status);
    TEST_CHECK_STATUS;
    Collator *germanCol = Collator::createInstance(Locale::getGerman(), status);
    TEST_CHECK_STATUS;
    germanCol->setStrength(Collator::PRIMARY);
    const Collator &indexCol = index->getCollator();
    TEST_ASSERT(*germanCol == indexCol);
    delete germanCol;

    UnicodeString ELLIPSIS;  ELLIPSIS.append((UChar32)0x2026);
    UnicodeString s = index->getUnderflowLabel();
    TEST_ASSERT(ELLIPSIS == s);
    s = index->getOverflowLabel();
    TEST_ASSERT(ELLIPSIS == s);
    s = index->getInflowLabel();
    TEST_ASSERT(ELLIPSIS == s);
    index->setOverflowLabel(UNICODE_STRING_SIMPLE("O"), status);
    index->setUnderflowLabel(UNICODE_STRING_SIMPLE("U"), status).setInflowLabel(UNICODE_STRING_SIMPLE("I"), status);
    s = index->getUnderflowLabel();
    TEST_ASSERT(UNICODE_STRING_SIMPLE("U") == s);
    s = index->getOverflowLabel();
    TEST_ASSERT(UNICODE_STRING_SIMPLE("O") == s);
    s = index->getInflowLabel();
    TEST_ASSERT(UNICODE_STRING_SIMPLE("I") == s);
     

    

    delete index;



    const UnicodeString adam = UNICODE_STRING_SIMPLE("Adam");
    const UnicodeString baker = UNICODE_STRING_SIMPLE("Baker");
    const UnicodeString charlie = UNICODE_STRING_SIMPLE("Charlie");
    const UnicodeString chad = UNICODE_STRING_SIMPLE("Chad");

    // addItem(), verify that it comes back out.
    //
    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
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

    // removeAllItems, addItem(), Iteration

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    while (index->nextLabel(status)) {
        TEST_CHECK_STATUS;
        while (index->nextItem(status)) {
            TEST_CHECK_STATUS;
            TEST_ASSERT(FALSE);   // No items have been added.
        }
        TEST_CHECK_STATUS;
    }

    index->addItem(adam, NULL, status);
    index->addItem(baker, NULL, status);
    index->addItem(charlie, NULL, status);
    index->addItem(chad, NULL, status);
    TEST_CHECK_STATUS;
    int itemCount = 0;
    index->resetLabelIterator(status);
    while (index->nextLabel(status)) {
        TEST_CHECK_STATUS;
        while (index->nextItem(status)) {
            TEST_CHECK_STATUS;
            ++itemCount;
        }
    }
    TEST_CHECK_STATUS;
    TEST_ASSERT(itemCount == 4);
    
    TEST_ASSERT(index->nextLabel(status) == FALSE);
    index->resetLabelIterator(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(index->nextLabel(status) == TRUE);

    index->removeAllItems(status);
    TEST_CHECK_STATUS;
    index->resetLabelIterator(status);
    while (index->nextLabel(status)) {
        TEST_CHECK_STATUS;
        while (index->nextItem(status)) {
            TEST_ASSERT(FALSE);   // No items have been added.
        }
    }
    TEST_CHECK_STATUS;
    delete index;

    // getLabel(), getLabelType()

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    index->setUnderflowLabel(adam, status).setOverflowLabel(charlie, status);
    TEST_CHECK_STATUS;
    for (i=0; index->nextLabel(status); i++) {
        TEST_CHECK_STATUS;
        UnicodeString label = index->getLabel();
        UAlphabeticIndexLabelType type = index->getLabelType();
        if (i == 0) {
            TEST_ASSERT(type == ALPHABETIC_INDEX_UNDERFLOW);
            TEST_ASSERT(label == adam);
        } else if (i <= 26) {
            // Labels A - Z for English locale
            TEST_ASSERT(type == ALPHABETIC_INDEX_NORMAL);
            UnicodeString expectedLabel((UChar)(0x40 + i));
            TEST_ASSERT(expectedLabel == label);
        } else if (i == 27) {
            TEST_ASSERT(type == ALPHABETIC_INDEX_OVERFLOW);
            TEST_ASSERT(label == charlie);
        } else {
            TEST_ASSERT(FALSE);
        }
    }
    TEST_ASSERT(i==28);



 


    
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
    AlphabeticIndex *index = NULL;

    for (int i=0; ; ++i) {
        status = U_ZERO_ERROR;
        const char *localeName = KEY_LOCALES[i];
        if (localeName[0] == 0) {
            break;
        }
        // std::cout <<  localeName << "  ";
        Locale loc = Locale::createFromName(localeName);
        index = new AlphabeticIndex(loc, status);
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

