/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2006, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "dtptngts.h" 

#include "unicode/calendar.h"
#include "unicode/smpdtfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"

#include "loctest.h"

// TODO remove comment
//#if defined( U_DEBUG_CALSVC ) || defined (U_DEBUG_CAL)
#include <stdio.h>
#include <stdlib.h>
//#endif

static const UnicodeString patternData[] = {
    UnicodeString("yM"),
    UnicodeString("yMMM"),
    UnicodeString("yMd"),
    UnicodeString("yMMMd"),
    UnicodeString("Md"),
    UnicodeString("MMMd"),
    UnicodeString("yQQQ"),
    UnicodeString("hhmm"),
    UnicodeString("HHmm"),
    UnicodeString("mmss"),
    UnicodeString(""),
 };
 
#define MAX_LOCALE   4  
static const char* testLocale[MAX_LOCALE][3] = {
    {"en", "US","\0"},
    {"zh", "Hans", "CN"},
    {"de","DE", "\0"},
    {"fi","\0", "\0"},
 };
 


static const UnicodeString patternResults[] = {
    UnicodeString("1/1999"),  // en_US
    UnicodeString("Jan 1999"),
    UnicodeString("1/13/1999"),
    UnicodeString("Jan/13/1999"),
    UnicodeString("1/13"),
    UnicodeString("Jan 13"),
    UnicodeString("Q1 1999"),
    UnicodeString("11:58 PM"),
    UnicodeString("23:58"),
    UnicodeString("58:59"),
    UnicodeString("1999-1"),  // zh_Hans_CN
    UnicodeString("1999 1"),
    UnicodeString("1999-1-13"),
    UnicodeString("1999 1 13"),
    UnicodeString("1-13"),
    UnicodeString("1 13"),
    UnicodeString("1999 \u7B2C1\u5B63\u5EA6"),
    UnicodeString("23:58"),
    UnicodeString("23:58"),
    UnicodeString("58:59"),
    UnicodeString("1999-1"),  // de_DE
    UnicodeString("1999 Jan"),
    UnicodeString("13.1.1999"),
    UnicodeString("13. Jan 1999"),
    UnicodeString("1-13"),
    UnicodeString("Jan 13"),
    UnicodeString("1999 Q1"),
    UnicodeString("23:58"),
    UnicodeString("23:58"),
    UnicodeString("58:59"),
    UnicodeString("1999-1"),  // fi
    UnicodeString("1999 tammi"),
    UnicodeString("13.1.1999"),
    UnicodeString("13. tammita 1999"),
    UnicodeString("1-13"),
    UnicodeString("tammi 13"),
    UnicodeString("1999 1. nelj."),
    UnicodeString("23.58"),
    UnicodeString("23.58"),
    UnicodeString("58.59"),
    UnicodeString(""),
    
};



// This is an API test, not a unit test.  It doesn't test very many cases, and doesn't
// try to test the full functionality.  It just calls each function in the class and
// verifies that it works on a basic level.

void IntlTestDateTimePatternGeneratorAPI::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite DateTimePatternGeneratorAPI");
    switch (index) {
        case 0: name = "DateTimePatternGenerator API test"; 
                if (exec) {
                    logln("DateTimePatternGenerator API test---"); logln("");
                    UErrorCode status = U_ZERO_ERROR;
                    Locale saveLocale;
                    Locale::setDefault(Locale::getEnglish(), status);
                    if(U_FAILURE(status)) {
                        errln("ERROR: Could not set default locale, test may not give correct results");
                    }
                    testAPI(/*par*/);
                    Locale::setDefault(saveLocale, status);
                }
                break;

        default: name = ""; break;
    }
}

/**
 * Test various generic API methods of DateTimePatternGenerator for API coverage.
 */
void IntlTestDateTimePatternGeneratorAPI::testAPI(/*char *par*/)
{
    UErrorCode status = U_ZERO_ERROR;

    // ======= Test CreateInstance with default locale
    logln("Testing DateTimePatternGenerator createInstance from default locale");
    
    DateTimePatternGenerator *instFromDefaultLocale=DateTimePatternGenerator::createInstance(status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateTimePatternGenerator (default) - exitting");
        return;
    }
    else {
        delete instFromDefaultLocale;
    }

    // ======= Test CreateInstance with given locale    
    logln("Testing DateTimePatternGenerator createInstance from French locale");
    status = U_ZERO_ERROR;
    DateTimePatternGenerator *instFromLocale=DateTimePatternGenerator::createInstance(Locale::getFrench(), status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateTimePatternGenerator (Locale::getFrench()) - exitting");
        return;
    }

    // ======= Test clone DateTimePatternGenerator    
    logln("Testing DateTimePatternGenerator::clone()");
    status = U_ZERO_ERROR;
    
    DateTimePatternGenerator *cloneDTPatternGen=instFromLocale->clone(status);
  
    if (U_FAILURE(status)) {
        delete instFromLocale;
        dataerrln("ERROR: Could not create DateTimePatternGenerator (Locale::getFrench()) - exitting");
        return;
    }
    else {
           delete instFromLocale;
           delete cloneDTPatternGen;
     }
   
    
    // ======= Test various skeleton
    logln("Testing DateTimePatternGenerator with various skeleton");
   
    status = U_ZERO_ERROR;
    int32_t localeIndex=0;
    int32_t resultIndex=0;
    UnicodeString resultDate;
    UDate testDate= LocaleTest::date(99, 0, 13, 23, 58, 59);
    while (localeIndex < MAX_LOCALE )
    {       
        int32_t dataIndex=0;
        UnicodeString bestPattern;
        
        Locale loc(testLocale[localeIndex][0], testLocale[localeIndex][1], testLocale[localeIndex][2], "");
        printf("\n\n Locale: %s_%s_%s", testLocale[localeIndex][0], testLocale[localeIndex][1], testLocale[localeIndex][2]);
        printf("\n    Status:%d", status);
        DateTimePatternGenerator *patGen=DateTimePatternGenerator::createInstance(loc, status);
        if(U_FAILURE(status)) {
            errln("ERROR: Could not create DateTimePatternGenerator with locale index:%d .\n", localeIndex);
        }
        while (patternData[dataIndex].length() > 0) {
            bestPattern = patGen->getBestPattern(patternData[dataIndex++]);
            
            SimpleDateFormat* sdf = new SimpleDateFormat(bestPattern, loc, status);
            resultDate = "";
            resultDate = sdf->format(testDate, resultDate);
            if ( resultDate != patternResults[resultIndex] ) {
                printf("\n\nUnmatched result!\n TestPattern:");
                for (int32_t i=0; i < patternData[dataIndex-1].length(); ++i) {
                     printf("%c", patternData[dataIndex-1].charAt(i));
                }   
                printf("  BestPattern:");
                for (int32_t i=0; i < bestPattern.length(); ++i) {
                   printf("%c", bestPattern.charAt(i));
                } 

                printf("  expected result:");
                for (int32_t i=0; i < patternResults[resultIndex].length(); ++i) {
                   printf("%c", patternResults[resultIndex].charAt(i));
                }
                printf("  expected result in hex:");
                for (int32_t i=0; i < patternResults[resultIndex].length(); ++i) {
                   printf("%x", patternResults[resultIndex].charAt(i));
                }
                printf("  running result:");
                for (int32_t i=0; i < resultDate.length(); ++i) {
                     printf("%c", resultDate.charAt(i));
                }
                printf("  running result in hex:");
                for (int32_t i=0; i < resultDate.length(); ++i) {
                     printf("%x", resultDate.charAt(i));
                }
            }
            resultIndex++;
            delete sdf;
        }
        delete patGen;
        localeIndex++;
    }



    // ======= Test random skeleton 
    const char randomChars[80] = {
     '1','2','3','4','5','6','7','8','9','0','!','@','#','$','%','^','&','*','(',')',
     '`',' ','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r',
     's','t','u','v','w','x','y','z','A','B','C','D','F','G','H','I','J','K','L','M',
     'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',':',';','<','.','?',';','\\'};
    DateTimePatternGenerator *randDTGen= DateTimePatternGenerator::createInstance(status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateTimePatternGenerator (Locale::getFrench()) - exitting");
        return;
    }

    for (int32_t i=0; i<10; ++i) {
        UnicodeString randomSkeleton="";
        int32_t len = rand() % 20;
        for (int32_t j=0; j<len; ++j ) {
           randomSkeleton += rand()%80;
        }
        UnicodeString bestPattern = randDTGen->getBestPattern(randomSkeleton);
    }
    delete randDTGen;
    
    //UnicodeString randomString=Unicode
    // ======= Test getStaticClassID()

    logln("Testing getStaticClassID()");
    status = U_ZERO_ERROR;
    DateTimePatternGenerator *test= DateTimePatternGenerator::createInstance(status);
    
    if(test->getDynamicClassID() != DateTimePatternGenerator::getStaticClassID()) {
        errln("ERROR: getDynamicClassID() didn't return the expected value");
    }
    
}

#endif /* #if !UCONFIG_NO_FORMATTING */
