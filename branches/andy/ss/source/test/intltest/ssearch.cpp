/*
 **********************************************************************
 *   Copyright (C) 2005-2006, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */


#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/putil.h"
#include "unicode/usearch.h"

#include "intltest.h"
#include "ssearch.h"

#include "xmlparser.h"

#include <stdlib.h>
#include <string.h>

char testId[100];

#define TEST_ASSERT(x) {if (!(x)) { \
    errln("Failure in file %s, line %d, test ID = \"%s\"", __FILE__, __LINE__, testId);}}

#define TEST_ASSERT_SUCCESS(errcode) {if (U_FAILURE(errcode)) { \
    errln("Failure in file %s, line %d, test ID \"%s\", status = \"%s\"", \
          __FILE__, __LINE__, testId, u_errorName(errcode));}}


//---------------------------------------------------------------------------
//
//  Test class boilerplate
//
//---------------------------------------------------------------------------
SSearchTest::SSearchTest()
{
}


SSearchTest::~SSearchTest()
{
}



void SSearchTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite CharsetDetectionTest: ");
    switch (index) {
       case 0: name = "searchTest";
            if (exec) searchTest();
            break;

        default: name = "";
            break; //needed to end loop
    }
}



#define PATH_BUFFER_SIZE 2048
const char *SSearchTest::getPath(char buffer[2048], const char *filename) {
    UErrorCode status = U_ZERO_ERROR;
    const char *testDataDirectory = IntlTest::getSourceTestData(status);

    if (U_FAILURE(status) || strlen(testDataDirectory) + strlen(filename) + 1 >= PATH_BUFFER_SIZE) {
        errln("ERROR: getPath() failed - %s", u_errorName(status));
        return NULL;
    }

    strcpy(buffer, testDataDirectory);
    strcat(buffer, filename);
    return buffer;
}


void SSearchTest::searchTest()
{
#if !UCONFIG_NO_REGULAR_EXPRESSIONS
    UErrorCode status = U_ZERO_ERROR;
    char path[PATH_BUFFER_SIZE];
    const char *testFilePath = getPath(path, "ssearch.xml");

    if (testFilePath == NULL) {
        return; /* Couldn't get path: error message already output. */
    }

    UXMLParser  *parser = UXMLParser::createParser(status);
    TEST_ASSERT_SUCCESS(status);
    UXMLElement *root   = parser->parseFile(testFilePath, status);
    TEST_ASSERT_SUCCESS(status);
    if (U_FAILURE(status)) {
        return;
    }

    const UXMLElement *testCase;
    int32_t tc = 0;

    while((testCase = root->nextChildElement(tc)) != NULL) {

        if (testCase->getTagName().compare("test-case") != 0) {
            errln("ssearch, unrecognized XML Element in test file");
            continue;
        }
        const UnicodeString *id       = testCase->getAttribute("id");
        *testId = 0;
        if (id != NULL) {
            id->extract(0, id->length(), testId,  sizeof(testId), US_INV);
        }

        //
        //  Get the requested collation strength.
        //    Default is tertiary if the XML attribute is missing from the test case.
        //
        const UnicodeString *strength = testCase->getAttribute("strength");
        UColAttributeValue collatorStrength;
        if      (strength==NULL)         { collatorStrength = UCOL_TERTIARY;}
        else if (*strength=="PRIMARY")   { collatorStrength = UCOL_PRIMARY;}
        else if (*strength=="SECONDARY") { collatorStrength = UCOL_SECONDARY;}
        else if (*strength=="TERTIARY")  { collatorStrength = UCOL_TERTIARY;}
        else if (*strength=="QUATERNARY") { collatorStrength = UCOL_QUATERNARY;}
        else {
            // Bogus value supplied for strength.  Shouldn't happen, even from
            //  typos, if the  XML source has been validated.
            //  This assert is a little deceiving in that strength can be
            //   any of the allowed values, not just TERTIARY, but it will
            //   do the job of getting the error output.
            TEST_ASSERT(*strength=="TERTIARY")
        }
        
        const UnicodeString defLocale("en");
        char  clocale[100];
        const UnicodeString *locale   = testCase->getAttribute("locale");
        if (locale == NULL || locale->length()==0) {
            locale = &defLocale;
        };
        locale->extract(0, locale->length(), clocale, sizeof(clocale), US_INV);


        UnicodeString  text;
        UnicodeString  target;
        UnicodeString  pattern;
        int32_t        expectedMatchStart = -1;
        int32_t        expectedMatchLimit = -1;
        const UXMLElement  *n;
        int                nodeCount = 0;

        n = testCase->getChildElement("pattern");
        TEST_ASSERT(n != NULL);
        if (n==NULL) {
            continue;
        }
        text = n->getText(FALSE);
        text = text.unescape();
        pattern.append(text);
        nodeCount++;

        n = testCase->getChildElement("pre");
        if (n!=NULL) {
            text = n->getText(FALSE);
            text = text.unescape();
            target.append(text);
            nodeCount++;
        }
        
        n = testCase->getChildElement("m");
        if (n!=NULL) {
            expectedMatchStart = target.length();
            text = n->getText(FALSE);
            text = text.unescape();
            target.append(text);
            expectedMatchLimit = target.length();
            nodeCount++;
        }

        n = testCase->getChildElement("post");
        if (n!=NULL) {
            text = n->getText(FALSE);
            text = text.unescape();
            target.append(text);
            nodeCount++;
        }

        //  Check that there weren't extra things in the XML
        TEST_ASSERT(nodeCount == testCase->countChildren());

        // Open a collotor and StringSearch based on the parameters
        //   obtained from the XML.
        //
        status = U_ZERO_ERROR;       
        UCollator *collator = ucol_open(clocale, &status);
        ucol_setStrength(collator, collatorStrength);
        UStringSearch *uss = usearch_openFromCollator(pattern.getBuffer(), pattern.length(),
                                         target.getBuffer(), target.length(),
                                         collator,
                                         NULL,     // the break iterator
                                         &status);
                                         
        TEST_ASSERT_SUCCESS(status);
        if (U_FAILURE(status)) {
            usearch_close(uss);
            ucol_close(collator);
            continue;
        }

        int32_t foundStart = 0;
        int32_t foundLimit = 0;
        UBool   foundMatch;

        //
        // Do the search, check the match result against the expected results.
        //
        foundMatch= usearch_search(uss, 0, &foundStart, &foundLimit, &status);
        TEST_ASSERT_SUCCESS(status);
        if (foundMatch && expectedMatchStart<0 ||
            foundStart != expectedMatchStart   ||
            foundLimit != expectedMatchLimit) {
                TEST_ASSERT(FALSE);   //  ouput generic error position
                errln("Found, expected match start = %d, %d \n"
                      "Found, expected match limit = %d, %d",
                foundStart, expectedMatchStart, foundLimit, expectedMatchLimit);
        }

        usearch_close(uss);
        ucol_close(collator);
    }

    delete root;
    delete parser;
#endif
}


