/********************************************************************
 * COPYRIGHT:
 * Copyright (C) 2002-2006 IBM, Inc.   All Rights Reserved.
 *
 ********************************************************************/

/** 
 * This program demos string collation
 */

const char gHelpString[] =
    "usage: coll [options*] -source source_string -target target_string\n"
    "-help            Display this message.\n"
    "-locale name     ICU locale to use.  Default is en_US\n"
    "-rules rule      Collation rules file (overrides locale)\n"
    "-french          French accent ordering\n"
    "-norm            Normalizing mode on\n"
    "-shifted         Shifted mode\n"
    "-lower           Lower case first\n"
    "-upper           Upper case first\n"
    "-case            Enable separate case level\n"
    "-level n         Sort level, 1 to 5, for Primary, Secndary, Tertiary, Quaternary, Identical\n"
	"-source string   Source string for comparison\n"
	"-target string   Target string for comparison\n"
    "Example coll -rules \\u0026b\\u003ca -source a -target b\n"
	"The format \\uXXXX is supported for the rules and comparison strings\n"
	;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector.h>

#include <unicode/utypes.h>
#include <unicode/ucol.h>
#include <unicode/coll.h>
#include "unicode/tblcoll.h"
#include "unicode/caniter.h"
#include <unicode/ustring.h>
//#include </usr/local/google/maf/source/i18n/ucol_tok.h>


/** 
 * Command line option variables
 *    These global variables are set according to the options specified
 *    on the command line by the user.
 */
char * opt_locale     = "en_US";
char * opt_rules      = 0;
UBool  opt_help       = FALSE;
UBool  opt_norm       = FALSE;
UBool  opt_french     = FALSE;
UBool  opt_shifted    = FALSE;
UBool  opt_lower      = FALSE;
UBool  opt_upper      = FALSE;
UBool  opt_case       = FALSE;
int    opt_level      = 0;
char * opt_source     = "abc";
char * opt_target     = "abd";
UCollator * collator  = 0;
UCollator * collator2  = 0;

/** 
 * Definitions for the command line options
 */
struct OptSpec {
    const char *name;
    enum {FLAG, NUM, STRING} type;
    void *pVar;
};

OptSpec opts[] = {
    {"-locale",      OptSpec::STRING, &opt_locale},
    {"-rules",       OptSpec::STRING, &opt_rules},
	{"-source",      OptSpec::STRING, &opt_source},
    {"-target",      OptSpec::STRING, &opt_target},
    {"-norm",        OptSpec::FLAG,   &opt_norm},
    {"-french",      OptSpec::FLAG,   &opt_french},
    {"-shifted",     OptSpec::FLAG,   &opt_shifted},
    {"-lower",       OptSpec::FLAG,   &opt_lower},
    {"-upper",       OptSpec::FLAG,   &opt_upper},
    {"-case",        OptSpec::FLAG,   &opt_case},
    {"-level",       OptSpec::NUM,    &opt_level},
    {"-help",        OptSpec::FLAG,   &opt_help},
    {"-?",           OptSpec::FLAG,   &opt_help},
    {0, OptSpec::FLAG, 0}
};

/**  
 * processOptions()  Function to read the command line options.
 */
UBool processOptions(int argc, const char **argv, OptSpec opts[])
{
    for (int argNum = 1; argNum < argc; argNum ++) {
        const char *pArgName = argv[argNum];
        OptSpec *pOpt;
        for (pOpt = opts;  pOpt->name != 0; pOpt ++) {
            if (strcmp(pOpt->name, pArgName) == 0) {
                switch (pOpt->type) {
                case OptSpec::FLAG:
                    *(UBool *)(pOpt->pVar) = TRUE;
                    break;
                case OptSpec::STRING:
                    argNum ++;
                    if (argNum >= argc) {
                        fprintf(stderr, "value expected for \"%s\" option.\n", 
							    pOpt->name);
                        return FALSE;
                    }
                    *(const char **)(pOpt->pVar) = argv[argNum];
                    break;
                case OptSpec::NUM:
                    argNum ++;
                    if (argNum >= argc) {
                        fprintf(stderr, "value expected for \"%s\" option.\n", 
							    pOpt->name);
                        return FALSE;
                    }
                    char *endp;
                    int i = strtol(argv[argNum], &endp, 0);
                    if (endp == argv[argNum]) {
                        fprintf(stderr, 
							    "integer value expected for \"%s\" option.\n", 
								pOpt->name);
                        return FALSE;
                    }
                    *(int *)(pOpt->pVar) = i;
                }
                break;
            }
        }
        if (pOpt->name == 0)
        {
            fprintf(stderr, "Unrecognized option \"%s\"\n", pArgName);
            return FALSE;
        }
    }
	return TRUE;
}

/**
 * ICU string comparison
 */

struct strcmpasd {
bool operator()(std::string s, std::string t) 
{
	UChar source[100];
	UChar target[100];
	u_unescape(s.c_str(), source, 100);
	u_unescape(t.c_str(), target, 100);
    UCollationResult result = ucol_strcoll(collator, source, -1, target, -1);
    if (result == UCOL_LESS) {
		return 1;
	}
    /*    else if (result == UCOL_GREATER) {
		return 1;
                }*/
	return 0;
}};

/**
 * Creates a collator
 */
UBool processCollator()
{
	// Set up an ICU collator
    UErrorCode status = U_ZERO_ERROR;
    int32_t length;
	UChar rules[500]= {0x0026, 0x30a1, 0x003d, 0x0024, 0x3041, 0xff67, 0x0000};;
        char srules[500] = "&N<ñ<<<Ñ&ae<<æ<<<Æ[import de-u-co-phonebk]";
        //char srules[500] = "&ァ=$ぁｧ";
        u_strFromUTF8(rules, 500, &length, srules, strlen(srules), &status);
                collator = ucol_openRules(rules, -1, UCOL_OFF, UCOL_TERTIARY, NULL, &status);
                //collator = ucol_open("th", &status);
                //        collator = Collator::createInstance(Locale("th", "TH", ""), status);
        //    }
        //    else {
               collator2 = ucol_open("es", &status);

               char tt1[20] = "æ";
               char tt2[20] = "Æ";
               UChar t1[20];
               UChar t2[20];
               u_strFromUTF8(t1, 20, &length, tt1, strlen(tt1), &status);
               u_strFromUTF8(t2, 20, &length, tt2, strlen(tt2), &status);

               printf("%d\n", ucol_strcoll(collator, t1, 1, t2, 1));
               printf("%d\n", ucol_strcoll(collator2, t1, 1, t2, 1));
        //    }
	if (U_FAILURE(status)) {
          fprintf(stderr, "Collator creation failed.: %d\n", status);
        return FALSE;
    }
    if (status == U_USING_DEFAULT_WARNING) {
        fprintf(stderr, "Warning, U_USING_DEFAULT_WARNING for %s\n", 
			    opt_locale);
    }
    if (status == U_USING_FALLBACK_WARNING) {
        fprintf(stderr, "Warning, U_USING_FALLBACK_ERROR for %s\n", 
			    opt_locale);
    }
    if (opt_norm) {
        ucol_setAttribute(collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    }
    if (opt_french) {
        ucol_setAttribute(collator, UCOL_FRENCH_COLLATION, UCOL_ON, &status);
    }
    if (opt_lower) {
        ucol_setAttribute(collator, UCOL_CASE_FIRST, UCOL_LOWER_FIRST, 
			              &status);
    }
    if (opt_upper) {
        ucol_setAttribute(collator, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, 
			              &status);
    }
    if (opt_case) {
        ucol_setAttribute(collator, UCOL_CASE_LEVEL, UCOL_ON, &status);
    }
    if (opt_shifted) {
        ucol_setAttribute(collator, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, 
			              &status);
    }
    if (opt_level != 0) {
        switch (opt_level) {
        case 1:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_PRIMARY, &status);
            break;
        case 2:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_SECONDARY, 
				              &status);
            break;
        case 3:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_TERTIARY, &status);
            break;
        case 4:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_QUATERNARY, 
				              &status);
            break;
        case 5:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_IDENTICAL, 
				              &status);
            break;
        default:
            fprintf(stderr, "-level param must be between 1 and 5\n");
            return FALSE;
        }
    }
    if (U_FAILURE(status)) {
        fprintf(stderr, "Collator attribute setting failed.: %d\n", status);
        return FALSE;
    }
	return TRUE;
}

/** 
 * Main   --  process command line, read in and pre-process the test file,
 *            call other functions to do the actual tests.
 */
int main(int argc, const char** argv) 
{
      if (processOptions(argc, argv, opts) != TRUE || opt_help) {
        printf(gHelpString);
        return -1;
    }

    if (processCollator() != TRUE) {
		fprintf(stderr, "Error creating collator for comparison\n");
		return -1;
                }

  /*    UColTokenParser src1;
    UColTokenParser src2;
    UBool startOfRules = TRUE;
    UParseError error1;
    UParseError error2;
    UErrorCode status1 = U_ZERO_ERROR;
    UErrorCode status2 = U_ZERO_ERROR;
    int32_t length1 = 0;
    int32_t length2 = 0;
    //    UCollator *UCA = ucol_initUCA(&status1);

    char srules1[500] = "&a<b[import de]&c<d";
    UChar rules1[500];
    UChar nrules1[500];
    u_strFromUTF8(rules1, 500, &length1, srules1, strlen(srules1), &status1);
    unorm_normalize(rules1, -1, UNORM_NFD, 0, nrules1, 500, &status1);
    char srules2[500] = "";
    UChar rules2[500];
    UChar nrules2[500];
    u_strFromUTF8(rules2, 500, &length2, srules2, strlen(srules2), &status2);
    unorm_normalize(rules2, -1, UNORM_NFD, 0, nrules2, 500, &status2);

    //printf("%d %d %d %d\n", u_strlen(rules1), length1, u_strlen(rules2), length2);

    src1.source = nrules1;
    src1.current = src1.source;
    src1.end = src1.source+u_strlen(nrules1);
    src1.extraCurrent = src1.end+1;
    src1.extraEnd = src1.source+500;
    src1.varTop = NULL;
    src1.UCA = NULL;//UCA;
    src1.invUCA = ucol_initInverseUCA(&status1);
    src1.parsedToken.charsLen = 0;
    src1.parsedToken.charsOffset = 0;
    src1.parsedToken.extensionLen = 0;
    src1.parsedToken.extensionOffset = 0;
    src1.parsedToken.prefixLen = 0;
    src1.parsedToken.prefixOffset = 0;
    src1.parsedToken.flags = 0;
    src1.parsedToken.strength = UCOL_TOK_UNSET;
    src1.buildCCTabFlag = FALSE;
    src1.prevStrength = UCOL_TOK_UNSET;

    src1.opts = (UColOptionSet *)uprv_malloc(sizeof(UColOptionSet));
    // test for NULL /
    if (src1.opts == NULL) {
        status1 = U_MEMORY_ALLOCATION_ERROR;
    }

    //    uprv_memcpy(src1.opts, UCA->options, sizeof(UColOptionSet));


    src2.source = nrules2;
    src2.current = src2.source;
    src2.end = src2.source+u_strlen(nrules2);
    src2.extraCurrent = src2.end+1;
    src2.extraEnd = src2.source+500;
    src2.varTop = NULL;
    src2.UCA = NULL;//UCA;
    src2.invUCA = ucol_initInverseUCA(&status2);
    src2.parsedToken.charsLen = 0;
    src2.parsedToken.charsOffset = 0;
    src2.parsedToken.extensionLen = 0;
    src2.parsedToken.extensionOffset = 0;
    src2.parsedToken.prefixLen = 0;
    src2.parsedToken.prefixOffset = 0;
    src2.parsedToken.flags = 0;
    src2.parsedToken.strength = UCOL_TOK_UNSET;
    src2.buildCCTabFlag = FALSE;
    src2.prevStrength = UCOL_TOK_UNSET;

    src2.opts = (UColOptionSet *)uprv_malloc(sizeof(UColOptionSet));
    //* test for NULL /
    if (src2.opts == NULL) {
        status2 = U_MEMORY_ALLOCATION_ERROR;
    }

    //    uprv_memcpy(src2.opts, UCA->options, sizeof(UColOptionSet));


    //printf("%x %x %x %x %x %x\n", rules1, src1.current, src1.end, rules2, src2.current, src2.end);

    const UChar *tok1, *tok2;

    ucol_tok_initTokenList(&src1, nrules2, u_strlen(nrules1), NULL, &status1);
    ucol_tok_initTokenList(&src2, nrules2, u_strlen(nrules2), NULL, &status2);

    while(((tok1 = ucol_tok_parseNextToken(&src1, startOfRules, &error1, &status1)) != NULL) && ((tok2 = ucol_tok_parseNextToken(&src2, startOfRules, &error2, &status2)) != NULL)){
        startOfRules = FALSE;
        printf("%d %d\n", src1.parsedToken.charsLen, src2.parsedToken.charsLen);
        printf("%d %d\n", src1.parsedToken.extensionLen, src2.parsedToken.extensionLen);
        printf("%d %d\n", src1.parsedToken.prefixLen, src2.parsedToken.prefixLen);
        printf("%d %d\n", src1.parsedToken.flags, src2.parsedToken.flags);
        printf("%d %d\n", src1.parsedToken.strength, src2.parsedToken.strength);
        for(int i = 0; i < src1.parsedToken.charsLen; i++){
          printf("%x\n", *(src1.source+src1.parsedToken.charsOffset+i));
        }
        printf("-\n");
        for(int i = 0; i < src2.parsedToken.charsLen; i++){
          printf("%x\n", *(src2.source+src2.parsedToken.charsOffset+i));
        }
        //printf("%x %x\n", *tok1, *tok2);
        printf("----\n");
    }
  */
  //std::vector<std::string> v;
    /*    v.push_back("Arnold");
    v.push_back("Øystein");
    v.push_back("Ingrid");
    v.push_back("Åke");
    v.push_back("Olof");
    v.push_back("İsmail");
    v.push_back("Örjan");*/

    /*    v.push_back("Apple");
    v.push_back("Banana");
    v.push_back("Carrot");
    v.push_back("Dsomething");
    v.push_back("Xena");
    v.push_back("Zebra");
    v.push_back("Yellow");

    // Sort the vector in alphabetical order.
    sort(v.begin(), v.end(), strcmpasd());

    for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++it) {
      puts(it->c_str());
      }*/

    /*	fprintf(stdout, "Comparing source=%s and target=%s\n", opt_source,
			opt_target);
	int result = strcmp();
	if (result == 0) {
		fprintf(stdout, "source is equals to target\n"); 
	}
	else if (result < 0) {
		fprintf(stdout, "source is less than target\n"); 
	}
	else {
		fprintf(stdout, "source is greater than target\n"); 
	}
    */
    /*ucol_close(collator);*/
	return 0;
}
