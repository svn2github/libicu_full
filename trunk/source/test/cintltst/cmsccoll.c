/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2001, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File cmsccoll.C
*
*********************************************************************************/
/**
 * These are the tests specific to ICU 1.8 and above, that I didn't know where to 
 * fit.
 */

#include <stdlib.h>
#include <stdio.h>
#include "unicode/utypes.h"
#include "unicode/ucol.h"
#include "unicode/ucoleitr.h"
#include "unicode/uloc.h"
#include "cintltst.h"
#include "cdetst.h"
#include "ccolltst.h"
#include "callcoll.h"
#include "unicode/ustring.h"
#include "string.h"
#include "ucol_imp.h"
#include "ucol_tok.h"
#include "cmemory.h"

static UCollator *myCollation;
const static UChar rules[MAX_TOKEN_LEN] =
/*" & 0 < 1,\u2461<a,A"*/
{ 0x0026, 0x0030, 0x003C, 0x0031, 0x002C, 0x2460, 0x003C, 0x0061, 0x002C, 0x0041, 0x0000 };

const static UChar testCase[][MAX_TOKEN_LEN] =
{
    /*0*/ {0x0031 /*'1'*/, 0x0061/*'a'*/, 0x0000},     
    /*1*/ {0x0031 /*'1'*/, 0x0041/*'A'*/, 0x0000},     
    /*2*/ {0x2460 /*circ'1'*/, 0x0061/*'a'*/, 0x0000},     
    /*3*/ {0x2460 /*circ'1'*/, 0x0041/*'A'*/, 0x0000}
};

const static UCollationResult caseTestResults[][9] =
{
        { UCOL_LESS, UCOL_LESS, UCOL_LESS, 0, UCOL_LESS, UCOL_LESS, 0, 0, UCOL_LESS },        
        { UCOL_GREATER, UCOL_LESS, UCOL_LESS, 0, UCOL_LESS, UCOL_LESS, 0, 0, UCOL_GREATER },
        { UCOL_LESS, UCOL_LESS, UCOL_LESS, 0, UCOL_GREATER, UCOL_LESS, 0, 0, UCOL_LESS },        
        { UCOL_GREATER, UCOL_LESS, UCOL_GREATER, 0, UCOL_LESS, UCOL_LESS, 0, 0, UCOL_GREATER }

};

const static UColAttributeValue caseTestAttributes[][2] =
{
        { UCOL_LOWER_FIRST, UCOL_OFF},        
        { UCOL_UPPER_FIRST, UCOL_OFF},        
        { UCOL_LOWER_FIRST, UCOL_ON},        
        { UCOL_UPPER_FIRST, UCOL_ON}

};

const static char cnt1[][10] = {
  "AA",
  "AC",
  "AZ",
  "AQ",
  "AB",
  "ABZ",
  "ABQ",
  "Z",
  "ABC",
  "Q",
  "B"
};

const static char cnt2[][10] = {
  "DA",
  "DAD",
  "DAZ",
  "MAR",
  "Z",
  "DAVIS",
  "MARK",
  "DAV",
  "DAVI"
};

static void TestCase( )
{
    
    int32_t i,j,k;
    UErrorCode status = U_ZERO_ERROR;
    myCollation = ucol_open("en_US", &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of rule based collator: %s\n", myErrorName(status));
	return;
    }
    log_verbose("Testing different case settings\n");
    ucol_setStrength(myCollation, UCOL_TERTIARY);

    for(k = 0; k<4; k++) {
      ucol_setAttribute(myCollation, UCOL_CASE_FIRST, caseTestAttributes[k][0], &status);
      ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, caseTestAttributes[k][1], &status);
      for (i = 0; i < 3 ; i++) {
        for(j = i+1; j<4; j++) {
          doTest(myCollation, testCase[i], testCase[j], caseTestResults[k][3*i+j-1]);
        }
      }
    }
    ucol_close(myCollation);    
    
    myCollation = ucol_openRules(rules, u_strlen(rules), UNORM_NONE, UCOL_TERTIARY, &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of rule based collator: %s\n", myErrorName(status));
	return;
    }
    log_verbose("Testing different case settings with custom rules\n");
    ucol_setStrength(myCollation, UCOL_TERTIARY);

    for(k = 0; k<4; k++) {
      ucol_setAttribute(myCollation, UCOL_CASE_FIRST, caseTestAttributes[k][0], &status);
      ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, caseTestAttributes[k][1], &status);
      for (i = 0; i < 3 ; i++) {
        for(j = i+1; j<4; j++) {
          doTest(myCollation, testCase[i], testCase[j], caseTestResults[k][3*i+j-1]);
        }
      }
    }
    ucol_close(myCollation);    

}

/**
 * Return an integer array containing all of the collation orders
 * returned by calls to next on the specified iterator
 */
static int32_t* getOrders(UCollationElements *iter, int32_t *orderLength)
{
    UErrorCode status;
    int32_t order;
    int32_t maxSize = 100;
    int32_t size = 0;
    int32_t *temp;
    int32_t *orders =(int32_t*)malloc(sizeof(int32_t) * maxSize);
    status= U_ZERO_ERROR;


    while ((order=ucol_next(iter, &status)) != UCOL_NULLORDER)
    {
        if (size == maxSize)
        {
            maxSize *= 2;
            temp = (int32_t*)malloc(sizeof(int32_t) * maxSize);

            memcpy(temp, orders, size * sizeof(int32_t));
            free(orders);
            orders = temp;
            
        }

        orders[size++] = order;
    }

    if (maxSize > size)
    {
        temp = (int32_t*)malloc(sizeof(int32_t) * size);

        memcpy(temp, orders, size * sizeof(int32_t));
        free(orders);
        orders = temp;


    }

    *orderLength = size;
    return orders;
}

static void backAndForth(UCollationElements *iter)
{
    /* Run through the iterator forwards and stick it into an array */
    int32_t index, o;
    UErrorCode status = U_ZERO_ERROR;
    int32_t orderLength = 0;
    int32_t *orders;
    orders= getOrders(iter, &orderLength);
    
    
    /* Now go through it backwards and make sure we get the same values */
    index = orderLength;
    ucol_reset(iter);
    
    /* synwee : changed */
    while ((o = ucol_previous(iter, &status)) != UCOL_NULLORDER)
    {
      if (o != orders[-- index])
      {
        if (o == 0)
          index ++;
        else
        {
          while (index > 0 && orders[-- index] == 0)
          {
          }
          if (o != orders[index])
          {
            log_err("Mismatch at index : %d\n", index);
            break;
          }
        }
      }
    }

    while (index != 0 && orders[index - 1] == 0) {
      index --;
    }

    if (index != 0)
    {
        log_err("Didn't get back to beginning - index is %d\n", index);

        ucol_reset(iter);
        log_err("\nnext: ");
        while ((o = ucol_next(iter, &status)) != UCOL_NULLORDER)
        {
            log_err("Error at %d\n", o);
        }
        log_err("\nprev: ");
        while ((o = ucol_previous(iter, &status)) != UCOL_NULLORDER)
        {
            log_err("Error at %d\n", o);
        }
        log_verbose("\n");
    }

    free(orders);    
}

static void IncompleteCntTest( )
{
  UErrorCode status = U_ZERO_ERROR;
  UChar *temp=(UChar*)malloc(sizeof(UChar) * 90);
  UChar *t1 =(UChar*)malloc(sizeof(UChar) * 90);
  UChar *t2 =(UChar*)malloc(sizeof(UChar) * 90);

  UCollator *coll =  NULL;
  uint32_t i = 0, j = 0;
  uint32_t size = 0;
  
  u_uastrcpy(temp, " & Z < ABC < Q < B");

  coll = ucol_openRules(temp, u_strlen(temp), UCOL_NO_NORMALIZATION, 
                                                UCOL_DEFAULT_STRENGTH, &status);

  if(U_SUCCESS(status)) {
    size = sizeof(cnt1)/sizeof(cnt1[0]);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        UCollationElements *iter;
        u_uastrcpy(t1, cnt1[i]);
        u_uastrcpy(t2, cnt1[j]);
        doTest(coll, t1, t2, UCOL_LESS);
        /* synwee : added collation element iterator test */
        iter = ucol_openElements(coll, t2, u_strlen(t2), &status);
        if (U_FAILURE(status)) {
          log_err("Creation of iterator failed\n");
          break;
        }
        backAndForth(iter);
        free(iter);
      }
    }
  } 

  ucol_close(coll);


  u_uastrcpy(temp, " & Z < DAVIS < MARK <DAV");
  coll = ucol_openRules(temp, u_strlen(temp), UCOL_NO_NORMALIZATION, 
                                                UCOL_DEFAULT_STRENGTH, &status);

  if(U_SUCCESS(status)) {
    size = sizeof(cnt2)/sizeof(cnt2[0]);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        UCollationElements *iter;
        u_uastrcpy(t1, cnt2[i]);
        u_uastrcpy(t2, cnt2[j]);
        doTest(coll, t1, t2, UCOL_LESS);

        /* synwee : added collation element iterator test */
        iter = ucol_openElements(coll, t2, u_strlen(t2), &status);
        if (U_FAILURE(status)) {
          log_err("Creation of iterator failed\n");
          break;
        }
        backAndForth(iter);
        free(iter);
      }
    }
  } 

  ucol_close(coll);


}

const static char shifted[][20] = {
  "black bird",
  "black-bird",
  "blackbird",
  "black Bird",
  "black-Bird",
  "blackBird",
  "black birds",
  "black-birds",
  "blackbirds"
};

const static UCollationResult shiftedTert[] = {
  0,
  UCOL_EQUAL,
  UCOL_EQUAL,
  UCOL_LESS,
  UCOL_EQUAL,
  UCOL_EQUAL,
  UCOL_LESS,
  UCOL_EQUAL,
  UCOL_EQUAL
};

const static char nonignorable[][20] = {
  "black bird",
  "black Bird",
  "black birds",
  "black-bird",
  "black-Bird",
  "black-birds",
  "blackbird",
  "blackBird",
  "blackbirds"
};

void BlackBirdTest( ) {
  UErrorCode status = U_ZERO_ERROR;
  UChar *t1 =(UChar*)malloc(sizeof(UChar) * 90);
  UChar *t2 =(UChar*)malloc(sizeof(UChar) * 90);

  uint32_t i = 0, j = 0;
  uint32_t size = 0;
  UCollator *coll = ucol_open("en_US", &status);

  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_OFF, &status);
  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);

  if(U_SUCCESS(status)) {
    size = sizeof(nonignorable)/sizeof(nonignorable[0]);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        u_uastrcpy(t1, nonignorable[i]);
        u_uastrcpy(t2, nonignorable[j]);
        doTest(coll, t1, t2, UCOL_LESS);
      }
    }
  } 

  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
  ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_QUATERNARY, &status);

  if(U_SUCCESS(status)) {
    size = sizeof(shifted)/sizeof(shifted[0]);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        u_uastrcpy(t1, shifted[i]);
        u_uastrcpy(t2, shifted[j]);
        doTest(coll, t1, t2, UCOL_LESS);
      }
    }
  } 

  ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_TERTIARY, &status);
  if(U_SUCCESS(status)) {
    size = sizeof(shifted)/sizeof(shifted[0]);
    for(i = 1; i < size; i++) {
      u_uastrcpy(t1, shifted[i-1]);
      u_uastrcpy(t2, shifted[i]);
      doTest(coll, t1, t2, shiftedTert[i]);
    }
  } 

  ucol_close(coll);
}

const static UChar testSourceCases[][MAX_TOKEN_LEN] = {
    {0x0041/*'A'*/, 0x0300, 0x0301, 0x0000},
    {0x0041/*'A'*/, 0x0300, 0x0316, 0x0000},
    {0x0041/*'A'*/, 0x0300, 0x0000},
    {0x00C0, 0x0301, 0x0000},
    /* this would work with forced normalization */
    {0x00C0, 0x0316, 0x0000}
};

const static UChar testTargetCases[][MAX_TOKEN_LEN] = {
    {0x0041/*'A'*/, 0x0301, 0x0300, 0x0000},
    {0x0041/*'A'*/, 0x0316, 0x0300, 0x0000},
    {0x00C0, 0},
    {0x0041/*'A'*/, 0x0301, 0x0300, 0x0000},
    /* this would work with forced normalization */
    {0x0041/*'A'*/, 0x0316, 0x0300, 0x0000}
};

const static UCollationResult results[] = {
    UCOL_GREATER,
    UCOL_EQUAL,
    UCOL_EQUAL,
    UCOL_GREATER,
    UCOL_EQUAL
};

static void FunkyATest( )
{
    
    int32_t i;
    UErrorCode status = U_ZERO_ERROR;
    myCollation = ucol_open("en_US", &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of rule based collator: %s\n", myErrorName(status));
	return;
    }
    log_verbose("Testing some A letters, for some reason\n");
    ucol_setAttribute(myCollation, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    for (i = 0; i < 4 ; i++)
    {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }
    ucol_close(myCollation);
}

UColAttributeValue caseFirst[] = {
    UCOL_OFF,
    UCOL_LOWER_FIRST,
    UCOL_UPPER_FIRST
};


UColAttributeValue alternateHandling[] = {
    UCOL_NON_IGNORABLE,
    UCOL_SHIFTED
};

UColAttributeValue caseLevel[] = {
    UCOL_OFF,
    UCOL_ON
};

UColAttributeValue strengths[] = {
    UCOL_PRIMARY,
    UCOL_SECONDARY,
    UCOL_TERTIARY,
    UCOL_QUATERNARY,
    UCOL_IDENTICAL
};

char * caseFirstC[] = {
    "UCOL_OFF",
    "UCOL_LOWER_FIRST",
    "UCOL_UPPER_FIRST"
};


char * alternateHandlingC[] = {
    "UCOL_NON_IGNORABLE",
    "UCOL_SHIFTED"
};

char * caseLevelC[] = {
    "UCOL_OFF",
    "UCOL_ON"
};

char * strengthsC[] = {
    "UCOL_PRIMARY",
    "UCOL_SECONDARY",
    "UCOL_TERTIARY",
    "UCOL_QUATERNARY",
    "UCOL_IDENTICAL"
};


static void PrintMarkDavis( )
{
  UErrorCode status = U_ZERO_ERROR;
  UChar m[256];
  uint8_t sortkey[256];
  UCollator *coll = ucol_open("en_US", &status);
  uint32_t h,i,j,k, sortkeysize;
  uint32_t sizem = 0;
  char buffer[512];
  uint32_t len = 512;

  u_uastrcpy(m, "Mark Davis");
  sizem = u_strlen(m);


  m[1] = 0xe4;

  for(i = 0; i<sizem; i++) {
    fprintf(stderr, "\\u%04X ", m[i]);
  }
  fprintf(stderr, "\n");

  for(h = 0; h<sizeof(caseFirst)/sizeof(caseFirst[0]); h++) {
    ucol_setAttribute(coll, UCOL_CASE_FIRST, caseFirst[i], &status);
    fprintf(stderr, "caseFirst: %s\n", caseFirstC[h]);

    for(i = 0; i<sizeof(alternateHandling)/sizeof(alternateHandling[0]); i++) {
      ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, alternateHandling[i], &status);
      fprintf(stderr, "  AltHandling: %s\n", alternateHandlingC[i]);

      for(j = 0; j<sizeof(caseLevel)/sizeof(caseLevel[0]); j++) {
        ucol_setAttribute(coll, UCOL_CASE_LEVEL, caseLevel[j], &status);
        fprintf(stderr, "    caseLevel: %s\n", caseLevelC[j]);

        for(k = 0; k<sizeof(strengths)/sizeof(strengths[0]); k++) {
          ucol_setAttribute(coll, UCOL_STRENGTH, strengths[k], &status);
          sortkeysize = ucol_getSortKey(coll, m, sizem, sortkey, 256);
          fprintf(stderr, "      strength: %s\n      Sortkey: ", strengthsC[k]);
          fprintf(stderr, "%s\n", ucol_sortKeyToString(coll, sortkey, buffer, &len));
        }

      }

    }

  }
}

static void CollationLocaleTest( ) {
/*
  UErrorCode status = U_ZERO_ERROR;
  UResourceBundle *rb = ures_open(NULL, "fr_FR_you_ll_never_find_this_locale", &status);
  const char *rbLocale = ures_getLocale(rb, &status);


  UResourceBundle *coll = ures_getByKey(rb, "CollationElements", NULL, &status);
  const char *locale = ures_getLocale(coll, &status);
*/
/*
** check for actual locale via ICU resource bundles
**
** lp points to the original locale ("fr_FR_....")
*/

UResourceBundle *lr,*cr;
UErrorCode              lec = U_ZERO_ERROR;
const char *lp = "fr_FR_you_ll_never_find_this_locale";

if ((lr = ures_open(NULL,lp,&lec))) {
         if ((cr = ures_getByKey(lr,"CollationElements",0,&lec))) {
                 if ((lp = ures_getLocale(cr,&lec))) {
                         if (U_SUCCESS(lec)) {
                         /* copy the fallback locale into the definition */                     
                                 /*cpynbuf(pkdef->kseg_desc,lp,ctKSEGDLEN);*/
                         }
                 }
                 ures_close(cr);
         }
         ures_close(lr);
}

}

void testPrimary(UCollator* col, const UChar* p,const UChar* q){
    UChar source[256] = { '\0'};
    UChar target[256] = { '\0'};
    UChar temp[2] = {'\0'};
    unsigned char utfSource[256] = {'\0'};
    unsigned char utfTarget[256] = {'\0'};
    doTest(col, p, q, UCOL_LESS);
/*
    UCollationResult result = ucol_strcoll(col,p,u_strlen(p),q,u_strlen(q));

    if(result!=UCOL_LESS){
       aescstrdup(p,utfSource,256);
       aescstrdup(q,utfTarget,256);
       fprintf(file,"Primary failed  source: %s target: %s \n", utfSource,utfTarget);
    }
*/
    source[0] = 0x0491;
    u_strcpy(source+1,p);
    target[0] = 0x0413;
    u_strcpy(target+1,q);
    doTest(col, source, target, UCOL_LESS);
/*
    result = ucol_strcoll(col,source,u_strlen(source),target,u_strlen(target));
    if(result!=UCOL_LESS){
       aescstrdup(source,utfSource,256);
       aescstrdup(target,utfTarget,256);
       fprintf(file,"Primary swamps 2nd failed  source: %s target: %s \n", utfSource,utfTarget);
    }
*/
}
   
void testSecondary(UCollator* col, const UChar* p,const UChar* q){
    UChar source[256] = { '\0'};
    UChar target[256] = { '\0'};
    UChar temp[2] = {'\0'};
    unsigned char utfSource[256] = {'\0'};
    unsigned char utfTarget[256] = {'\0'};

    doTest(col, p, q, UCOL_LESS);
/*
    UCollationResult result= ucol_strcoll(col,p,u_strlen(p),q,u_strlen(q));
    
    if(result!=UCOL_LESS){
       aescstrdup(p,utfSource,256);
       aescstrdup(q,utfTarget,256);
       fprintf(file,"secondary failed  source: %s target: %s \n", utfSource,utfTarget);
    }
*/
    source[0] = 0x0053;
    u_strcpy(source+1,p);
    target[0]= 0x0073;
    u_strcpy(target+1,q);

    doTest(col, source, target, UCOL_LESS);
/*
    result = ucol_strcoll(col,source,u_strlen(source),target,u_strlen(target));
    if(result!=UCOL_LESS){
       aescstrdup(source,utfSource,256);
       aescstrdup(target,utfTarget,256);
       fprintf(file,"secondary swamps 3rd failed  source: %s target: %s \n",utfSource,utfTarget);
    }
*/
    /*
    source[0] = '\0';
    u_strcat(source,p);
    source[u_strlen(p)] = 0x62;
    source[u_strlen(p+1)] = 0;

    target[0] = '\0';
    u_strcat(target,q);
    target[u_strlen(q)] = 0x61;
    target[u_strlen(q+1)] = 0;

    doTest(col, source, target, UCOL_GREATER);
    */
/*
    result = ucol_strcoll(col,source,u_strlen(source),target,u_strlen(target));
    if(result!=UCOL_GREATER){
       aescstrdup(source,utfSource,256);
       aescstrdup(target,utfTarget,256);
       fprintf(file,"secondary is swamped by 1  failed  source: %s target: %s \n",utfSource,utfTarget);
    }
*/
}

void testTertiary(UCollator* col, const UChar* p,const UChar* q){
    UChar source[256] = { '\0'};
    UChar target[256] = { '\0'};
    UChar temp[2] = {'\0'};
    unsigned char utfSource[256] = {'\0'};
    unsigned char utfTarget[256] = {'\0'};
    doTest(col, p, q, UCOL_LESS);
/*
    UCollationResult result= ucol_strcoll(col,p,u_strlen(p),q,u_strlen(q));
    if(result!=UCOL_LESS){
       aescstrdup(p,utfSource,256);
       aescstrdup(q,utfTarget,256);
       fprintf(file,"Tertiary failed  source: %s target: %s \n",utfSource,utfTarget);
    }
*/
    source[0] = 0x0020;
    u_strcpy(source+1,p);
    target[0]= 0x002D;
    u_strcpy(target+1,q);

    doTest(col, source, target, UCOL_LESS);
/*
    result = ucol_strcoll(col,source,u_strlen(source),target,u_strlen(target));
    if(result!=UCOL_LESS){
       aescstrdup(source,utfSource,256);
       aescstrdup(target,utfTarget,256);
       fprintf(file,"Tertiary swamps 4th failed  source: %s target: %s \n", utfSource,utfTarget);
    }
*/
    /*
    source[0] = '\0';
    u_strcat(source,p);
    source[u_strlen(p)] = 0xE0;
    source[u_strlen(p+1)] = 0;

    target[0] = '\0';
    u_strcat(target,q);
    target[u_strlen(q)] = 0x61;
    target[u_strlen(q+1)] = 0;

    doTest(col, source, target, UCOL_GREATER);
    */
/*
    result = ucol_strcoll(col,source,u_strlen(source),target,u_strlen(target));
    if(result!=UCOL_GREATER){
       aescstrdup(source,utfSource,256);
       aescstrdup(target,utfTarget,256);
       fprintf(file,"Tertiary is swamped by 3rd failed  source: %s target: %s \n",utfSource,utfTarget);
    }
*/
}
void testEquality(UCollator* col, const UChar* p,const UChar* q){
    UChar source[256] = { '\0'};
    UChar target[256] = { '\0'};
    UChar temp[2] = {'\0'};
    unsigned char utfSource[256] = {'\0'};
    unsigned char utfTarget[256] = {'\0'};
    doTest(col, p, q, UCOL_EQUAL);
/*
    UCollationResult result = ucol_strcoll(col,p,u_strlen(p),q,u_strlen(q));

    if(result!=UCOL_EQUAL){
       aescstrdup(p,utfSource,256);
       aescstrdup(q,utfTarget,256);
       fprintf(file,"Primary failed  source: %s target: %s \n", utfSource,utfTarget);
    }
*/
}

void testCollator(UCollator* col, const UChar* p,const UChar* q, uint32_t strength){
    UChar source[256] = { '\0'};
    UChar target[256] = { '\0'};
    UChar temp[2] = {'\0'};
    unsigned char utfSource[256] = {'\0'};
    unsigned char utfTarget[256] = {'\0'};
    UCollationResult result=0;
    switch(strength){
    case UCOL_IDENTICAL:
        testEquality(col,p,q);
        break;

    case UCOL_PRIMARY:
        testPrimary(col,p,q);
        break;
    case UCOL_SECONDARY:
       testSecondary(col,p,q);
       break;
    case UCOL_TERTIARY:
       testTertiary(col,p,q);
       break;
    default:
        break;
    }
}

static char* localesToTest[] = {
"ar", "bg", "ca", "cs", /*"da", */
"el", "en_BE", "en_US_POSIX", 
"es", "et", "fi", "fr", "hi", 
"hr", "hu", "is", "iw", "ja", 
"ko", "lt", "lv", "mk", "mt", 
"nb", "nn", "nn_NO", "pl", "ro", 
"ru", /*"sh",*/ "sk", "sl", "sq", 
"sr", "sv", "th", "tr", "uk", 
"vi", "zh", "zh_TW"
};

static void RamsRulesTest( ) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t i = 0;
  UCollator *coll = NULL;
  const UChar *rules = NULL, *current = NULL;
  uint32_t ruleLen = 0;
  uint32_t strength = 0;
  uint32_t chOffset = 0; uint32_t chLen = 0;
  uint32_t exOffset = 0; uint32_t exLen = 0;
  uint32_t rExpsLen = 0;
  UBool varT = FALSE; UBool top_ = TRUE;
  UBool startOfRules = TRUE;
  UColTokenParser src;
  UCATableHeader img;

  UChar first[256];
  UChar second[256];
  UChar *rulesCopy = NULL;

  src.image = &img;



  for(i = 0; i<sizeof(localesToTest)/sizeof(localesToTest[0]); i++) {
    coll = ucol_open(localesToTest[i], &status);
    /*fprintf(stderr, "%s\n", localesToTest[i]);*/
    rules = ucol_getRules(coll, &ruleLen);
    if(U_SUCCESS(status) && ruleLen > 0) {
      rulesCopy = (UChar *)uprv_malloc((ruleLen+UCOL_TOK_EXTRA_RULE_SPACE_SIZE)*sizeof(UChar));
      uprv_memcpy(rulesCopy, rules, ruleLen*sizeof(UChar));
      src.source = src.current = rulesCopy;
      src.end = rulesCopy+ruleLen;
      src.extraCurrent = src.end;
      src.extraEnd = src.end+UCOL_TOK_EXTRA_RULE_SPACE_SIZE;
      *first = *second = 0;

      while ((current = ucol_tok_parseNextToken(&src, &strength, 
                        &chOffset, &chLen, &exOffset, &exLen,
                        &varT, &top_, startOfRules, &status)) != NULL) {
        startOfRules = FALSE;

        u_strncpy(second,rulesCopy+chOffset, chLen);
        second[chLen] = 0;

        if(exLen > 0) {
          u_strncat(second+chLen, rulesCopy+exOffset, exLen);
          second[chLen+exLen] = 0;
        } 
/*      
       else if (exLen == 0 && rExpsLen > 0) {
          u_strcat(second+chLen, rExps);
          *rExps = 0;
          rExpsLen = 0;
        } else if (exLen > 0 && rExpsLen > 0) {
          fprintf(stderr, "Bad rule - both reset and explicit expansion\n");
        }
*/
        if(strength != UCOL_TOK_RESET) {
          testCollator(coll,first,second,strength);
        } else {
/*
          if(chLen > 1) {
            rExpsLen = chLen-1;
            second[0] = *(rulesCopy+chOffset);
            second[1] = 0;
            u_strncpy(rExps, rulesCopy+chOffset+1, rExpsLen);
            rExps[rExpsLen] = 0;
          } else {
            *rExps = 0;
            rExpsLen = 0;
          }
*/
        }
        u_strcpy(first, second);

      }
      uprv_free(rulesCopy);
    }
    ucol_close(coll);
  }
}

void addMiscCollTest(TestNode** root)
{ 
    addTest(root, &TestCase, "tscoll/cmsccoll/TestCase");
    addTest(root, &IncompleteCntTest, "tscoll/cmsccoll/IncompleteCntTest");
    addTest(root, &BlackBirdTest, "tscoll/cmsccoll/BlackBirdTest");
    addTest(root, &FunkyATest, "tscoll/cmsccoll/FunkyATest");
    addTest(root, &CollationLocaleTest, "tscoll/cmsccoll/CollationLocaleTest");
    addTest(root, &RamsRulesTest, "tscoll/cmsccoll/RamsRulesTest");
    /*addTest(root, &PrintMarkDavis, "tscoll/cmsccoll/PrintMarkDavis");*/
}

