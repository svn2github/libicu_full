/*
 **********************************************************************
 *   Copyright (C) 2005-2008, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */


#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/putil.h"
#include "unicode/usearch.h"

#include "cmemory.h"
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/coleitr.h"

#include "intltest.h"
#include "ssearch.h"

#include "xmlparser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char testId[100];

#define TEST_ASSERT(x) {if (!(x)) { \
    errln("Failure in file %s, line %d, test ID = \"%s\"", __FILE__, __LINE__, testId);}}

#define TEST_ASSERT_M(x, m) {if (!(x)) { \
    errln("Failure in file %s, line %d.   \"%s\"", __FILE__, __LINE__, m);return;}}

#define TEST_ASSERT_SUCCESS(errcode) {if (U_FAILURE(errcode)) { \
    errln("Failure in file %s, line %d, test ID \"%s\", status = \"%s\"", \
          __FILE__, __LINE__, testId, u_errorName(errcode));}}

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

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
            
        case 1: name = "searchTime";
            if (exec) searchTime();
            break;

        case 2: name = "offsetTest";
            if (exec) offsetTest();
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

    const UnicodeString *debugTestCase = root->getAttribute("debug");
    if (debugTestCase != NULL) {
//       setenv("USEARCH_DEBUG", "1", 1);
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

        // If debugging test case has been specified and this is not it, skip to next.
        if (id!=NULL && debugTestCase!=NULL && *id != *debugTestCase) {
            continue;
        }
        //
        //  Get the requested collation strength.
        //    Default is tertiary if the XML attribute is missing from the test case.
        //
        const UnicodeString *strength = testCase->getAttribute("strength");
        UColAttributeValue collatorStrength;
        if      (strength==NULL)          { collatorStrength = UCOL_TERTIARY;}
        else if (*strength=="PRIMARY")    { collatorStrength = UCOL_PRIMARY;}
        else if (*strength=="SECONDARY")  { collatorStrength = UCOL_SECONDARY;}
        else if (*strength=="TERTIARY")   { collatorStrength = UCOL_TERTIARY;}
        else if (*strength=="QUATERNARY") { collatorStrength = UCOL_QUATERNARY;}
        else if (*strength=="IDENTICAL")  { collatorStrength = UCOL_IDENTICAL;}
        else {
            // Bogus value supplied for strength.  Shouldn't happen, even from
            //  typos, if the  XML source has been validated.
            //  This assert is a little deceiving in that strength can be
            //   any of the allowed values, not just TERTIARY, but it will
            //   do the job of getting the error output.
            TEST_ASSERT(*strength=="TERTIARY")
        }

        //
        // Get the collator normalization flag.  Default is UCOL_OFF.
        //
        UColAttributeValue normalize = UCOL_OFF;
        const UnicodeString *norm = testCase->getAttribute("norm");
        TEST_ASSERT (norm==NULL || *norm=="ON" || *norm=="OFF");
        if (norm!=NULL && *norm=="ON") {
            normalize = UCOL_ON;
        }

        const UnicodeString defLocale("en");
        char  clocale[100];
        const UnicodeString *locale   = testCase->getAttribute("locale");
        if (locale == NULL || locale->length()==0) {
            locale = &defLocale;
        };
        locale->extract(0, locale->length(), clocale, sizeof(clocale), NULL);


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
        ucol_setAttribute(collator, UCOL_NORMALIZATION_MODE, normalize, &status);
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
                infoln("Found, expected match start = %d, %d \n"
                       "Found, expected match limit = %d, %d",
                foundStart, expectedMatchStart, foundLimit, expectedMatchLimit);
        }

        // In case there are other matches...
        // (should we only do this if the test case passed?)
        while (foundMatch) {
            expectedMatchStart = foundStart;
            expectedMatchLimit = foundLimit;

            foundMatch = usearch_search(uss, foundLimit, &foundStart, &foundLimit, &status);
        }

        usearch_close(uss);
        usearch_openFromCollator(pattern.getBuffer(), pattern.length(),
            target.getBuffer(), target.length(),
            collator,
            NULL,
            &status);

        //
        // Do the backwards search, check the match result against the expected results.
        //
        foundMatch= usearch_searchBackwards(uss, target.length(), &foundStart, &foundLimit, &status);
        TEST_ASSERT_SUCCESS(status);
        if (foundMatch && expectedMatchStart<0 ||
            foundStart != expectedMatchStart   ||
            foundLimit != expectedMatchLimit) {
                TEST_ASSERT(FALSE);   //  ouput generic error position
                infoln("Found, expected backwards match start = %d, %d \n"
                       "Found, expected backwards match limit = %d, %d",
                foundStart, expectedMatchStart, foundLimit, expectedMatchLimit);
        }

        usearch_close(uss);
        ucol_close(collator);
    }

    delete root;
    delete parser;
#endif
}

//
//  searchTime()    A quick and dirty performance test for string search.
//                  Probably  doesn't really belong as part of intltest, but it
//                  does check that the search succeeds, and gets the right result,
//                  so it serves as a functionality test also.
//
//                  To run as a perf test, up the loop count, select by commenting
//                  and uncommenting in the code the operation to be measured,
//                  rebuild, and measure the running time of this test alone.
//
//                     time LD_LIBRARY_PATH=whatever  ./intltest  collate/SSearchTest/searchTime
//
void SSearchTest::searchTime() {
    static const char *longishText =
"Whylom, as olde stories tellen us,\n"
"Ther was a duk that highte Theseus:\n"
"Of Athenes he was lord and governour,\n"
"And in his tyme swich a conquerour,\n"
"That gretter was ther noon under the sonne.\n"
"Ful many a riche contree hadde he wonne;\n"
"What with his wisdom and his chivalrye,\n"
"He conquered al the regne of Femenye,\n"
"That whylom was y-cleped Scithia;\n"
"And weddede the quene Ipolita,\n"
"And broghte hir hoom with him in his contree\n"
"With muchel glorie and greet solempnitee,\n"
"And eek hir yonge suster Emelye.\n"
"And thus with victorie and with melodye\n"
"Lete I this noble duk to Athenes ryde,\n"
"And al his hoost, in armes, him bisyde.\n"
"And certes, if it nere to long to here,\n"
"I wolde han told yow fully the manere,\n"
"How wonnen was the regne of Femenye\n"
"By Theseus, and by his chivalrye;\n"
"And of the grete bataille for the nones\n"
"Bitwixen Athen's and Amazones;\n"
"And how asseged was Ipolita,\n"
"The faire hardy quene of Scithia;\n"
"And of the feste that was at hir weddinge,\n"
"And of the tempest at hir hoom-cominge;\n"
"But al that thing I moot as now forbere.\n"
"I have, God woot, a large feeld to ere,\n"
"And wayke been the oxen in my plough.\n"
"The remenant of the tale is long y-nough.\n"
"I wol nat letten eek noon of this route;\n"
"Lat every felawe telle his tale aboute,\n"
"And lat see now who shal the soper winne;\n"
"And ther I lefte, I wol ageyn biginne.\n"
"This duk, of whom I make mencioun,\n"
"When he was come almost unto the toun,\n"
"In al his wele and in his moste pryde,\n"
"He was war, as he caste his eye asyde,\n"
"Wher that ther kneled in the hye weye\n"
"A companye of ladies, tweye and tweye,\n"
"Ech after other, clad in clothes blake; \n"
"But swich a cry and swich a wo they make,\n"
"That in this world nis creature livinge,\n"
"That herde swich another weymentinge;\n"
"And of this cry they nolde never stenten,\n"
"Til they the reynes of his brydel henten.\n"
"'What folk ben ye, that at myn hoomcominge\n"
"Perturben so my feste with cryinge'?\n"
"Quod Theseus, 'have ye so greet envye\n"
"Of myn honour, that thus compleyne and crye? \n"
"Or who hath yow misboden, or offended?\n"
"And telleth me if it may been amended;\n"
"And why that ye ben clothed thus in blak'?\n"
"The eldest lady of hem alle spak,\n"
"When she hadde swowned with a deedly chere,\n"
"That it was routhe for to seen and here,\n"
"And seyde: 'Lord, to whom Fortune hath yiven\n"
"Victorie, and as a conquerour to liven,\n"
"Noght greveth us your glorie and your honour;\n"
"But we biseken mercy and socour.\n"
"Have mercy on our wo and our distresse.\n"
"Som drope of pitee, thurgh thy gentilesse,\n"
"Up-on us wrecched wommen lat thou falle.\n"
"For certes, lord, ther nis noon of us alle,\n"
"That she nath been a duchesse or a quene;\n"
"Now be we caitifs, as it is wel sene:\n"
"Thanked be Fortune, and hir false wheel,\n"
"That noon estat assureth to be weel.\n"
"And certes, lord, t'abyden your presence,\n"
"Here in the temple of the goddesse Clemence\n"
"We han ben waytinge al this fourtenight;\n"
"Now help us, lord, sith it is in thy might.\n"
"I wrecche, which that wepe and waille thus,\n"
"Was whylom wyf to king Capaneus,\n"
"That starf at Thebes, cursed be that day!\n"
"And alle we, that been in this array,\n"
"And maken al this lamentacioun,\n"
"We losten alle our housbondes at that toun,\n"
"Whyl that the sege ther-aboute lay.\n"
"And yet now th'olde Creon, weylaway!\n"
"The lord is now of Thebes the citee, \n"
"Fulfild of ire and of iniquitee,\n"
"He, for despyt, and for his tirannye,\n"
"To do the dede bodyes vileinye,\n"
"Of alle our lordes, whiche that ben slawe,\n"
"Hath alle the bodyes on an heep y-drawe,\n"
"And wol nat suffren hem, by noon assent,\n"
"Neither to been y-buried nor y-brent,\n"
"But maketh houndes ete hem in despyt. zet'\n";

const char *cPattern = "maketh houndes ete hem";
//const char *cPattern = "Whylom";
//const char *cPattern = "zet";
    const char *testId = "searchTime()";   // for error macros.
    UnicodeString target = longishText;
    UErrorCode status = U_ZERO_ERROR;


    UCollator *collator = ucol_open("en", &status);
    TEST_ASSERT_SUCCESS(status);
    //ucol_setStrength(collator, collatorStrength);
    //ucol_setAttribute(collator, UCOL_NORMALIZATION_MODE, normalize, &status);
    UnicodeString pattern = cPattern;
    UStringSearch *uss = usearch_openFromCollator(pattern.getBuffer(), pattern.length(),
                                        target.getBuffer(), target.length(),
                                        collator,
                                        NULL,     // the break iterator
                                        &status);
    TEST_ASSERT_SUCCESS(status);
    
//  int32_t foundStart;
//  int32_t foundEnd;
    UBool   found;
    
    // Find the match position usgin strstr
    const char *pm = strstr(longishText, cPattern);
    TEST_ASSERT_M(pm!=NULL, "No pattern match with strstr");
    int  refMatchPos = (int)(pm - longishText);
    int  icuMatchPos;
    int  icuMatchEnd;
    usearch_search(uss, 0, &icuMatchPos, &icuMatchEnd, &status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT_M(refMatchPos == icuMatchPos, "strstr and icu give different match positions.");

    int i;
    int j=0;

    // Try loopcounts around 100000 to some millions, depending on the operation,
    //   to get runtimes of at least several seconds.
    for (i=0; i<1; i++) {
        found = usearch_search(uss, 0, &icuMatchPos, &icuMatchEnd, &status);
        //TEST_ASSERT_SUCCESS(status);
        //TEST_ASSERT(found);

        // usearch_setOffset(uss, 0, &status);
        // icuMatchPos = usearch_next(uss, &status);

         // The i+j stuff is to confuse the optimizer and get it to actually leave the
         //   call to strstr in place.
         //pm = strstr(longishText+j, cPattern);
         //j = (j + i)%5;
    }

    printf("%d\n", pm-longishText, j);
    usearch_close(uss);
    ucol_close(collator);
}

struct Order
{
    int32_t order;
    int32_t offset;
};

class OrderList
{
public:
    OrderList();
    ~OrderList();

    int32_t size(void);
    void add(int32_t order, int32_t offset);
    const Order *get(int32_t index);
    void reverse(void);
    UBool compare(const OrderList &other);

private:
    Order *list;
    int32_t listMax;
    int32_t listSize;
};

OrderList::OrderList()
  : list(NULL), listSize(0), listMax(16)
{
    list = new Order[listMax];
}

OrderList::~OrderList()
{
    delete[] list;
}

void OrderList::add(int32_t order, int32_t offset)
{
    if (listSize >= listMax) {
        listMax *= 2;

        Order *newList = new Order[listMax];

        uprv_memcpy(newList, list, listSize * sizeof(Order));
        delete[] list;
        list = newList;
    }

    list[listSize].order  = order;
    list[listSize].offset = offset;

    listSize += 1;
}

const Order *OrderList::get(int32_t index)
{
    if (index >= listSize) {
        return NULL;
    }

    return &list[index];
}

int32_t OrderList::size()
{
    return listSize;
}

void OrderList::reverse()
{
    for(int32_t f = 0, b = listSize - 1; f < b; f += 1, b -= 1) {
        Order swap = list[b];

        list[b] = list[f];
        list[f] = swap;
    }
}

UBool OrderList::compare(const OrderList &other)
{
    if (listSize != other.listSize) {
        return FALSE;
    }

    for(int32_t i = 0; i < listSize; i += 1) {
        if (list[i].order  != other.list[i].order ||
            list[i].offset != other.list[i].offset) {
                return FALSE;
        }
    }

    return TRUE;
}

static char *printOffsets(char *buffer, OrderList &list)
{
    int32_t size = list.size();
    char *s = buffer;

    for(int32_t i = 0; i < size; i += 1) {
        const Order *order = list.get(i);

        if (i != 0) {
            s += sprintf(s, ", ");
        }

        s += sprintf(s, "%d", order->offset);
    }

    return buffer;
}

static char *printOrders(char *buffer, OrderList &list)
{
    int32_t size = list.size();
    char *s = buffer;

    for(int32_t i = 0; i < size; i += 1) {
        const Order *order = list.get(i);

        if (i != 0) {
            s += sprintf(s, ", ");
        }

        s += sprintf(s, "%8.8X", order->order);
    }

    return buffer;
}

void SSearchTest::offsetTest()
{
    UnicodeString test[] = {
        "A\\u0300\\u0323B",
        "A\\u0301\\u0323B",
        "A\\u0302\\u0301\\u0323B",
        "abc",
        "ab\\u0300c",
        "ab\\u0300\\u0323c",
        " \\uD800\\uDC00\\uDC00",
        "a\\uD800\\uDC00\\uDC00",
        "A\\u0301\\u0301",
        "A\\u0301\\u0323",
        "A\\u0301\\u0323B",
        "B\\u0301\\u0323C",
        "A\\u0300\\u0323B",
        "\\u0301A\\u0301\\u0301",
        "abcd\\r\\u0301",
        "p\\u00EAche",
        "pe\\u0302che",
    };

    int32_t testCount = ARRAY_SIZE(test);
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedCollator *col = (RuleBasedCollator *) Collator::createInstance(Locale::getEnglish(), status);
    char buffer[256];  // A bit of a hack... just happens to be long enough for all the test cases...
                       // We could allocate one that's the right size by (CE_count * 10) + 2
                       // 10 chars is enough room for 8 hex digits plus ", ". 2 extra chars for "[" and "]"

    for(int32_t i = 0; i < testCount; i += 1) {
        UnicodeString ts = test[i].unescape();
        CollationElementIterator *iter = col->createCollationElementIterator(ts);
        OrderList forwardList;
        OrderList backwardList;
        int32_t order, offset;

        do {
            offset = iter->getOffset();
            order  = iter->next(status);

            forwardList.add(order, offset);
        } while (order != CollationElementIterator::NULLORDER);

        iter->reset();
        iter->setOffset(ts.length(), status);

        backwardList.add(CollationElementIterator::NULLORDER, iter->getOffset());

        do {
            order  = iter->previous(status);
            offset = iter->getOffset();

            if (order == CollationElementIterator::NULLORDER) {
                break;
            }

            backwardList.add(order, offset);
        } while (TRUE);

        backwardList.reverse();

        if (forwardList.compare(backwardList)) {
            logln("Works with \"%S\"", test[i].getTerminatedBuffer());
            logln("Forward offsets:  [%s]", printOffsets(buffer, forwardList));
//          logln("Backward offsets: [%s]", printOffsets(buffer, backwardList));

            logln("Forward CEs:  [%s]", printOrders(buffer, forwardList));
//          logln("Backward CEs: [%s]", printOrders(buffer, backwardList));

            logln();
        } else {
            errln("Fails with \"%S\"", test[i].getTerminatedBuffer());
            infoln("Forward offsets:  [%s]", printOffsets(buffer, forwardList));
            infoln("Backward offsets: [%s]", printOffsets(buffer, backwardList));

            infoln("Forward CEs:  [%s]", printOrders(buffer, forwardList));
            infoln("Backward CEs: [%s]", printOrders(buffer, backwardList));

            infoln();
        }
    }
}
        
        
