/*
 *******************************************************************************
 *
 *   Copyright (C) 2003, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 *******************************************************************************
 *   file name:  spreptst.c
 *   encoding:   US-ASCII
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2003jul11
 *   created by: Ram Viswanadha
 */
#if !UCONFIG_NO_IDNA

#include <stdlib.h>
#include <string.h>
#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/usprep.h"
#include "cintltst.h"
#include "nfsprep.h"


#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

void addUStringPrepTest(TestNode** root);
void doStringPrepTest(const char* binFileName, const char* txtFileName, 
                 int32_t options, UErrorCode* errorCode);

static void Test_nfs4_cs_prep_data(void);
static void Test_nfs4_cis_prep_data(void);
static void Test_nfs4_mixed_prep_data(void);
static void Test_nfs4_cs_prep(void);
static void Test_nfs4_cis_prep(void);
static void Test_nfs4_mixed_prep(void);

void 
addUStringPrepTest(TestNode** root)
{
   addTest(root, &Test_nfs4_cs_prep_data,    "spreptst/Test_nfs4_cs_prep_data");
   addTest(root, &Test_nfs4_cis_prep_data,   "spreptst/Test_nfs4_cis_prep_data");
   addTest(root, &Test_nfs4_mixed_prep_data, "spreptst/Test_nfs4_mixed_prep_data");
   addTest(root, &Test_nfs4_cs_prep,         "spreptst/Test_nfs4_cs_prep");
   addTest(root, &Test_nfs4_cis_prep,        "spreptst/Test_nfs4_cis_prep");
   addTest(root, &Test_nfs4_mixed_prep,      "spreptst/Test_nfs4_mixed_prep");
}

static void 
Test_nfs4_cs_prep_data(void){
    UErrorCode errorCode = U_ZERO_ERROR;
    log_verbose("Testing nfs4_cs_prep_ci.txt\n");
    doStringPrepTest("nfscsi","nfs4_cs_prep_ci.txt", USPREP_DEFAULT, &errorCode);

    log_verbose("Testing nfs4_cs_prep_cs.txt\n");
    errorCode = U_ZERO_ERROR;
    doStringPrepTest("nfscss","nfs4_cs_prep_cs.txt", USPREP_DEFAULT, &errorCode);
    

}
static void 
Test_nfs4_cis_prep_data(void){
    UErrorCode errorCode = U_ZERO_ERROR;
    log_verbose("Testing nfs4_cis_prep.txt\n");
    doStringPrepTest("nfscis","nfs4_cis_prep.txt", USPREP_DEFAULT, &errorCode);
}
static void 
Test_nfs4_mixed_prep_data(void){
    UErrorCode errorCode = U_ZERO_ERROR;
    log_verbose("Testing nfs4_mixed_prep_s.txt\n");
    doStringPrepTest("nfsmxs","nfs4_mixed_prep_s.txt", USPREP_DEFAULT, &errorCode);

    errorCode = U_ZERO_ERROR;
    log_verbose("Testing nfs4_mixed_prep_p.txt\n");
    doStringPrepTest("nfsmxp","nfs4_mixed_prep_p.txt", USPREP_DEFAULT, &errorCode);

}

static struct ConformanceTestCases
   {
     const char *comment;
     const char *in;
     const char *out;
     const char *profile;
     UErrorCode expectedStatus;
   }
   conformanceTestCases[] =
   {
  
     {
       "Case folding ASCII U+0043 U+0041 U+0046 U+0045",
       "\x43\x41\x46\x45", "\x63\x61\x66\x65",
       "nfs4_cis_prep", 
       U_ZERO_ERROR

     },
     {
       "Case folding 8bit U+00DF (german sharp s)",
       "\xC3\x9F", "\x73\x73", 
       "nfs4_cis_prep", 
       U_ZERO_ERROR  
     },
     {
       "Non-ASCII multibyte space character U+1680",
       "\xE1\x9A\x80", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Non-ASCII 8bit control character U+0085",
       "\xC2\x85", NULL, 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Non-ASCII multibyte control character U+180E",
       "\xE1\xA0\x8E", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Non-ASCII control character U+1D175",
       "\xF0\x9D\x85\xB5", NULL, 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Plane 0 private use character U+F123",
       "\xEF\x84\xA3", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Plane 15 private use character U+F1234",
       "\xF3\xB1\x88\xB4", NULL, 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Plane 16 private use character U+10F234",
       "\xF4\x8F\x88\xB4", NULL, 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Non-character code point U+8FFFE",
       "\xF2\x8F\xBF\xBE", NULL, 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Non-character code point U+10FFFF",
       "\xF4\x8F\xBF\xBF", NULL,
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR 
     },
 /* 
     {
       "Surrogate code U+DF42",
       "\xED\xBD\x82", NULL, "nfs4_cis_prep", UIDNA_DEFAULT,
       U_STRINGPREP_PROHIBITED_ERROR
     },
*/
     {
       "Non-plain text character U+FFFD",
       "\xEF\xBF\xBD", NULL, 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Ideographic description character U+2FF5",
       "\xE2\xBF\xB5", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Display property character U+0341",
       "\xCD\x81", "\xCC\x81",
       "nfs4_cis_prep", U_ZERO_ERROR

     },

     {
       "Left-to-right mark U+200E",
       "\xE2\x80\x8E", "\xCC\x81", 
       "nfs4_cis_prep",
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {

       "Deprecated U+202A",
       "\xE2\x80\xAA", "\xCC\x81", 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Language tagging character U+E0001",
       "\xF3\xA0\x80\x81", "\xCC\x81", 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Language tagging character U+E0042",
       "\xF3\xA0\x81\x82", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_PROHIBITED_ERROR
     },
     {
       "Bidi: RandALCat character U+05BE and LCat characters",
       "\x66\x6F\x6F\xD6\xBE\x62\x61\x72", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_CHECK_BIDI_ERROR
     },
     {
       "Bidi: RandALCat character U+FD50 and LCat characters",
       "\x66\x6F\x6F\xEF\xB5\x90\x62\x61\x72", NULL,
       "nfs4_cis_prep",
       U_STRINGPREP_CHECK_BIDI_ERROR
     },
     {
       "Bidi: RandALCat character U+FB38 and LCat characters",
       "\x66\x6F\x6F\xEF\xB9\xB6\x62\x61\x72", "\x66\x6F\x6F \xd9\x8e\x62\x61\x72",
       "nfs4_cis_prep", 
       U_ZERO_ERROR
     },
     { "Bidi: RandALCat without trailing RandALCat U+0627 U+0031",
       "\xD8\xA7\x31", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_CHECK_BIDI_ERROR
     },
     {
       "Bidi: RandALCat character U+0627 U+0031 U+0628",
       "\xD8\xA7\x31\xD8\xA8", "\xD8\xA7\x31\xD8\xA8",
       "nfs4_cis_prep", 
       U_ZERO_ERROR
     },
     {
       "Unassigned code point U+E0002",
       "\xF3\xA0\x80\x82", NULL, 
       "nfs4_cis_prep", 
       U_STRINGPREP_UNASSIGNED_ERROR
     },

/*  // Invalid UTF-8
     {
       "Larger test (shrinking)",
       "X\xC2\xAD\xC3\xDF\xC4\xB0\xE2\x84\xA1\x6a\xcc\x8c\xc2\xa0\xc2"
       "\xaa\xce\xb0\xe2\x80\x80", "xssi\xcc\x87""tel\xc7\xb0 a\xce\xb0 ",
       "nfs4_cis_prep",
        U_ZERO_ERROR
     },
    {

       "Larger test (expanding)",
       "X\xC3\xDF\xe3\x8c\x96\xC4\xB0\xE2\x84\xA1\xE2\x92\x9F\xE3\x8c\x80",
       "xss\xe3\x82\xad\xe3\x83\xad\xe3\x83\xa1\xe3\x83\xbc\xe3\x83\x88"
       "\xe3\x83\xab""i\xcc\x87""tel\x28""d\x29\xe3\x82\xa2\xe3\x83\x91"
       "\xe3\x83\xbc\xe3\x83\x88"
       "nfs4_cis_prep",
        U_ZERO_ERROR
     },
  */
};
static void Test_nfs4_cis_prep(void){
    int32_t i=0;
    for(i=0;i< (int32_t)(sizeof(conformanceTestCases)/sizeof(conformanceTestCases[0]));i++){
        const char* src = conformanceTestCases[i].in;
        UErrorCode status = U_ZERO_ERROR;
        UParseError parseError;
        UErrorCode expectedStatus = conformanceTestCases[i].expectedStatus;
        const char* expectedDest = conformanceTestCases[i].out;
        char* dest = NULL;
        int32_t destLen = 0;
        destLen = nfs4_cis_prepare(src , strlen(src), dest, destLen, &parseError, &status); 
        if(status == U_BUFFER_OVERFLOW_ERROR){
            status = U_ZERO_ERROR;
            dest = (char*) malloc(++destLen);
            destLen = nfs4_cis_prepare( src , strlen(src), dest, destLen, &parseError, &status); 
        }
        if(expectedStatus != status){
            log_err("Did not get the expected status for nfs4_cis_prep at index %i. Expected: %s Got: %s\n",i, u_errorName(expectedStatus), u_errorName(status));
        }
        if(U_SUCCESS(status) && (strcmp(expectedDest,dest) !=0)){
              log_err("Did not get the expected output for nfs4_cis_prep at index %i.\n", i);
        }
        free(dest);
    }
}



/*
   There are several special identifiers ("who") which need to be
   understood universally, rather than in the context of a particular
   DNS domain.  Some of these identifiers cannot be understood when an
   NFS client accesses the server, but have meaning when a local process
   accesses the file.  The ability to display and modify these
   permissions is permitted over NFS, even if none of the access methods
   on the server understands the identifiers.

    Who                    Description
   _______________________________________________________________

   "OWNER"                The owner of the file.
   "GROUP"                The group associated with the file.
   "EVERYONE"             The world.
   "INTERACTIVE"          Accessed from an interactive terminal.
   "NETWORK"              Accessed via the network.
   "DIALUP"               Accessed as a dialup user to the server.
   "BATCH"                Accessed from a batch job.
   "ANONYMOUS"            Accessed without any authentication.
   "AUTHENTICATED"        Any authenticated user (opposite of
                          ANONYMOUS)
   "SERVICE"              Access from a system service.

   To avoid conflict, these special identifiers are distinguish by an
   appended "@" and should appear in the form "xxxx@" (note: no domain
   name after the "@").  For example: ANONYMOUS@.
*/
static const char* mixed_prep_data[] ={
    "OWNER@",
    "GROUP@",        
    "EVERYONE@",     
    "INTERACTIVE@",  
    "NETWORK@",      
    "DIALUP@",       
    "BATCH@",        
    "ANONYMOUS@",    
    "AUTHENTICATED@",
    "\\u0930\\u094D\\u092E\\u094D\\u0915\\u094D\\u0937\\u0947\\u0924\\u094D@slip129-37-118-146.nc.us.ibm.net",
    "\\u0936\\u094d\\u0930\\u0940\\u092e\\u0926\\u094d@saratoga.pe.utexas.edu",
    "\\u092d\\u0917\\u0935\\u0926\\u094d\\u0917\\u0940\\u0924\\u093e@dial-120-45.ots.utexas.edu",
    "\\u0905\\u0927\\u094d\\u092f\\u093e\\u092f@woo-085.dorms.waller.net",
    "\\u0905\\u0930\\u094d\\u091c\\u0941\\u0928@hd30-049.hil.compuserve.com",
    "\\u0935\\u093f\\u0937\\u093e\\u0926@pem203-31.pe.ttu.edu",
    "\\u092f\\u094b\\u0917@56K-227.MaxTNT3.pdq.net",
    "\\u0927\\u0943\\u0924\\u0930\\u093e\\u0937\\u094d\\u091f\\u094d\\u0930@dial-36-2.ots.utexas.edu",
    "\\u0909\\u0935\\u093E\\u091A\\u0943@slip129-37-23-152.ga.us.ibm.net",
    "\\u0927\\u0930\\u094d\\u092e\\u0915\\u094d\\u0937\\u0947\\u0924\\u094d\\u0930\\u0947@ts45ip119.cadvision.com",
    "\\u0915\\u0941\\u0930\\u0941\\u0915\\u094d\\u0937\\u0947\\u0924\\u094d\\u0930\\u0947@sdn-ts-004txaustP05.dialsprint.net",
    "\\u0938\\u092e\\u0935\\u0947\\u0924\\u093e@bar-tnt1s66.erols.com",
    "\\u092f\\u0941\\u092f\\u0941\\u0924\\u094d\\u0938\\u0935\\u0903@101.st-louis-15.mo.dial-access.att.net",
    "\\u092e\\u093e\\u092e\\u0915\\u093e\\u0903@h92-245.Arco.COM",
    "\\u092a\\u093e\\u0923\\u094d\\u0921\\u0935\\u093e\\u0936\\u094d\\u091a\\u0948\\u0935@dial-13-2.ots.utexas.edu",
    "\\u0915\\u093f\\u092e\\u0915\\u0941\\u0930\\u094d\\u0935\\u0924@net-redynet29.datamarkets.com.ar",
    "\\u0938\\u0902\\u091c\\u0935@ccs-shiva28.reacciun.net.ve",
    "\\u0c30\\u0c18\\u0c41\\u0c30\\u0c3e\\u0c2e\\u0c4d@7.houston-11.tx.dial-access.att.net",
    "\\u0c35\\u0c3f\\u0c36\\u0c4d\\u0c35\\u0c28\\u0c3e\\u0c27@ingw129-37-120-26.mo.us.ibm.net",
    "\\u0c06\\u0c28\\u0c02\\u0c26\\u0c4d@dialup6.austintx.com",
    "\\u0C35\\u0C26\\u0C4D\\u0C26\\u0C3F\\u0C30\\u0C3E\\u0C1C\\u0C41@dns2.tpao.gov.tr",
    "\\u0c30\\u0c3e\\u0c1c\\u0c40\\u0c35\\u0c4d@slip129-37-119-194.nc.us.ibm.net",
    "\\u0c15\\u0c36\\u0c30\\u0c2c\\u0c3e\\u0c26@cs7.dillons.co.uk.203.119.193.in-addr.arpa",
    "\\u0c38\\u0c02\\u0c1c\\u0c40\\u0c35\\u0c4d@swprd1.innovplace.saskatoon.sk.ca",
    "\\u0c15\\u0c36\\u0c30\\u0c2c\\u0c3e\\u0c26@bikini.bologna.maraut.it",
    "\\u0c38\\u0c02\\u0c1c\\u0c40\\u0c2c\\u0c4d@node91.subnet159-198-79.baxter.com",
    "\\u0c38\\u0c46\\u0c28\\u0c4d\\u0c17\\u0c41\\u0c2a\\u0c4d\\u0c24@cust19.max5.new-york.ny.ms.uu.net",
    "\\u0c05\\u0c2e\\u0c30\\u0c47\\u0c02\\u0c26\\u0c4d\\u0c30@balexander.slip.andrew.cmu.edu",
    "\\u0c39\\u0c28\\u0c41\\u0c2e\\u0c3e\\u0c28\\u0c41\\u0c32@pool029.max2.denver.co.dynip.alter.net",
    "\\u0c30\\u0c35\\u0c3f@cust49.max9.new-york.ny.ms.uu.net",
    "\\u0c15\\u0c41\\u0c2e\\u0c3e\\u0c30\\u0c4d@s61.abq-dialin2.hollyberry.com",
    "\\u0c35\\u0c3f\\u0c36\\u0c4d\\u0c35\\u0c28\\u0c3e\\u0c27@\\u0917\\u0928\\u0947\\u0936.sanjose.ibm.com",
    "\\u0c06\\u0c26\\u0c3f\\u0c24\\u0c4d\\u0c2f@www.\\u00E0\\u00B3\\u00AF.com",
    "\\u0C15\\u0C02\\u0C26\\u0C4D\\u0C30\\u0C47\\u0C17\\u0C41\\u0c32@www.\\u00C2\\u00A4.com",
    "\\u0c36\\u0c4d\\u0c30\\u0c40\\u0C27\\u0C30\\u0C4D@www.\\u00C2\\u00A3.com",
    "\\u0c15\\u0c02\\u0c1f\\u0c2e\\u0c36\\u0c46\\u0c1f\\u0c4d\\u0c1f\\u0c3f@\\u0025",
    "\\u0c2e\\u0c3e\\u0c27\\u0c35\\u0c4d@\\u005C\\u005C",
    "\\u0c26\\u0c46\\u0c36\\u0c46\\u0c1f\\u0c4d\\u0c1f\\u0c3f@www.\\u0021.com",
    "test@www.\\u0024.com",
    "help@\\u00C3\\u00BC.com",

};

#define MAX_BUFFER_SIZE  1000

static int32_t 
unescapeData(const char* src, int32_t srcLen, 
             char* dest, int32_t destCapacity, 
             UErrorCode* status){

    UChar b1Stack[MAX_BUFFER_SIZE];
    char b2Stack[MAX_BUFFER_SIZE];
    int32_t b1Capacity = MAX_BUFFER_SIZE,
            b2Capacity = MAX_BUFFER_SIZE,
            b1Len      = 0,
            b2Len      = 0;

    UChar* b1 = b1Stack;
    char*  b2 = b2Stack;

    b1Len = u_unescape(src,b1,b1Capacity);
    u_strToUTF8(b2, b2Capacity, &b2Len, b1, b1Len, status);
    if(U_SUCCESS(*status) &&  b2Len <= destCapacity){
        memmove(dest, b2, b2Len);
    }
    return b2Len;
}
static void 
Test_nfs4_mixed_prep(void){
    int32_t i=0;
    char src[MAX_BUFFER_SIZE];
    int32_t srcLen;

    for(i=0; i< LENGTHOF(mixed_prep_data); i++){
        int32_t destLen=0;
        char* dest = NULL;
        UErrorCode status = U_ZERO_ERROR;
        UParseError parseError;
        srcLen = unescapeData(mixed_prep_data[i], strlen(mixed_prep_data[i]), src, MAX_BUFFER_SIZE, &status);
        if(U_FAILURE(status)){
            log_err("Conversion of data at index %i failed. Error: %s\n", i, u_errorName(status));
            continue;
        }
        destLen = nfs4_mixed_prepare(src, srcLen, NULL, 0, &parseError, &status);
        if(status == U_BUFFER_OVERFLOW_ERROR){
            status = U_ZERO_ERROR;
            dest = (char*)malloc(++destLen);
            destLen = nfs4_mixed_prepare(src, srcLen, dest, destLen, &parseError, &status);
        }
        free(dest);
        if(U_FAILURE(status)){
            log_err("Preparation of string at index %i failed. Error: %s\n", i, u_errorName(status));
            continue;
        }
    } 
    /* test the error condition */
    {
        const char* src = "OWNER@oss.software.ibm.com";
        char dest[MAX_BUFFER_SIZE];
        UErrorCode status = U_ZERO_ERROR;
        UParseError parseError;
        int32_t destLen = nfs4_mixed_prepare(src, srcLen, dest, MAX_BUFFER_SIZE, &parseError, &status);
        if(status != U_PARSE_ERROR){
            log_err("Did not get the expected error.Expected: %s Got: %s\n", u_errorName(U_PARSE_ERROR), u_errorName(status));
        }
    }


}

static void 
Test_nfs4_cs_prep(void){
    {
        /* BiDi checking is turned off */
        const char *source = "\\uC138\\uACC4\\uC758\\uBAA8\\uB4E0\\uC0AC\\uB78C\\uB4E4\\uC774\\u0644\\u064A\\u0647\\uD55C\\uAD6D\\uC5B4\\uB97C\\uC774\\uD574\\uD55C\\uB2E4\\uBA74";
        UErrorCode status = U_ZERO_ERROR;
        char src[MAX_BUFFER_SIZE]={'\0'};
        UParseError parseError;
        int32_t srcLen = unescapeData(source, strlen(source), src, MAX_BUFFER_SIZE, &status);
        if(U_SUCCESS(status)){
            char dest[MAX_BUFFER_SIZE] = {'\0'};
            int32_t destLen = nfs4_cs_prepare(src, srcLen, dest, MAX_BUFFER_SIZE, FALSE, &parseError, &status);
            if(U_FAILURE(status)){
                log_err("StringPrep failed with error: %s\n", u_errorName(status));
            }
            if(strcmp(dest,src)!=0){
                log_err("Did not get the expected output!");
            }
        }else{
            log_err("Conversion failed with error: %s\n", u_errorName(status));
        }
    }
    {
        /* Normalization turned off */
        const char *source = "www.\\u00E0\\u00B3\\u00AF.com";
        UErrorCode status = U_ZERO_ERROR;
        char src[MAX_BUFFER_SIZE]={'\0'};
        UParseError parseError;
        int32_t srcLen = unescapeData(source, strlen(source), src, MAX_BUFFER_SIZE, &status);
        if(U_SUCCESS(status)){
            char dest[MAX_BUFFER_SIZE] = {'\0'};
            int32_t destLen = nfs4_cs_prepare(src, srcLen, dest, MAX_BUFFER_SIZE, FALSE, &parseError, &status);
            if(U_FAILURE(status)){
                log_err("StringPrep failed with error: %s\n", u_errorName(status));
            }
            if(strcmp(dest,src)!=0){
                log_err("Did not get the expected output!");
            }
        }else{
            log_err("Conversion failed with error: %s\n", u_errorName(status));
        }
    }
    {
        /* case mapping turned off */
        const char *source = "THISISATEST";
        UErrorCode status = U_ZERO_ERROR;
        char src[MAX_BUFFER_SIZE]={'\0'};
        UParseError parseError;
        int32_t srcLen = unescapeData(source, strlen(source), src, MAX_BUFFER_SIZE, &status);
        if(U_SUCCESS(status)){
            char dest[MAX_BUFFER_SIZE] = {'\0'};
            int32_t destLen = nfs4_cs_prepare(src, srcLen, dest, MAX_BUFFER_SIZE, TRUE, &parseError, &status);
            if(U_FAILURE(status)){
                log_err("StringPrep failed with error: %s\n", u_errorName(status));
            }
            if(strcmp(dest,src)!=0){
                log_err("Did not get the expected output!");
            }
        }else{
            log_err("Conversion failed with error: %s\n", u_errorName(status));
        }
    }
    {
        /* case mapping turned on */
        const char *source = "THISISATEST";
        const char *expected = "thisisatest";
        UErrorCode status = U_ZERO_ERROR;
        char src[MAX_BUFFER_SIZE]={'\0'};
        UParseError parseError;
        int32_t srcLen = unescapeData(source, strlen(source), src, MAX_BUFFER_SIZE, &status);
        if(U_SUCCESS(status)){
            char dest[MAX_BUFFER_SIZE] = {'\0'};
            int32_t destLen = nfs4_cs_prepare(src, srcLen, dest, MAX_BUFFER_SIZE, FALSE, &parseError, &status);
            if(U_FAILURE(status)){
                log_err("StringPrep failed with error: %s\n", u_errorName(status));
            }
            if(strcmp(expected, dest)!=0){
                log_err("Did not get the expected output!");
            }
        }else{
            log_err("Conversion failed with error: %s\n", u_errorName(status));
        }
    }
}

#endif

/*
 * Hey, Emacs, please set the following:
 *
 * Local Variables:
 * indent-tabs-mode: nil
 * End:
 *
 */
