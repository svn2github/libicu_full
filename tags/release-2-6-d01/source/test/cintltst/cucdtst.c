/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2003, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CUCDTST.C
*
* Modification History:
*        Name                     Description
*     Madhu Katragadda            Ported for C API, added tests for string functions
*********************************************************************************
*/

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/putil.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"

#include "cintltst.h"
#include "uparse.h"
#include "uprops.h"
#include "usc_impl.h"
#include "unormimp.h"

#define LENGTHOF(array) (sizeof(array)/sizeof((array)[0]))

/* prototypes --------------------------------------------------------------- */

static void setUpDataTable(void);
static void cleanUpDataTable(void);

static void TestUpperLower(void);
static void TestLetterNumber(void);
static void TestMisc(void);
static void TestPOSIX(void);
static void TestControlPrint(void);
static void TestIdentifier(void);
static void TestUnicodeData(void);
static void TestCodeUnit(void);
static void TestCodePoint(void);
static void TestCharLength(void);
static void TestCharNames(void);
static void TestMirroring(void);
       void TestUScriptCodeAPI(void);	/* defined in cucdapi.c */
static void TestUScriptRunAPI(void);
static void TestAdditionalProperties(void);
static void TestNumericProperties(void);
static void TestPropertyNames(void);
static void TestPropertyValues(void);
static void TestConsistency(void);

/* internal methods used */
static int32_t MakeProp(char* str);
static int32_t MakeDir(char* str);

#define LENGTHOF(array) (sizeof(array)/sizeof((array)[0]))

/* test data ---------------------------------------------------------------- */

const UChar  LAST_CHAR_CODE_IN_FILE = 0xFFFD;
const char tagStrings[] = "MnMcMeNdNlNoZsZlZpCcCfCsCoCnLuLlLtLmLoPcPdPsPePoSmScSkSoPiPf";
const int32_t tagValues[] =
    {
    /* Mn */ U_NON_SPACING_MARK,
    /* Mc */ U_COMBINING_SPACING_MARK,
    /* Me */ U_ENCLOSING_MARK,
    /* Nd */ U_DECIMAL_DIGIT_NUMBER,
    /* Nl */ U_LETTER_NUMBER,
    /* No */ U_OTHER_NUMBER,
    /* Zs */ U_SPACE_SEPARATOR,
    /* Zl */ U_LINE_SEPARATOR,
    /* Zp */ U_PARAGRAPH_SEPARATOR,
    /* Cc */ U_CONTROL_CHAR,
    /* Cf */ U_FORMAT_CHAR,
    /* Cs */ U_SURROGATE,
    /* Co */ U_PRIVATE_USE_CHAR,
    /* Cn */ U_UNASSIGNED,
    /* Lu */ U_UPPERCASE_LETTER,
    /* Ll */ U_LOWERCASE_LETTER,
    /* Lt */ U_TITLECASE_LETTER,
    /* Lm */ U_MODIFIER_LETTER,
    /* Lo */ U_OTHER_LETTER,
    /* Pc */ U_CONNECTOR_PUNCTUATION,
    /* Pd */ U_DASH_PUNCTUATION,
    /* Ps */ U_START_PUNCTUATION,
    /* Pe */ U_END_PUNCTUATION,
    /* Po */ U_OTHER_PUNCTUATION,
    /* Sm */ U_MATH_SYMBOL,
    /* Sc */ U_CURRENCY_SYMBOL,
    /* Sk */ U_MODIFIER_SYMBOL,
    /* So */ U_OTHER_SYMBOL,
    /* Pi */ U_INITIAL_PUNCTUATION,
    /* Pf */ U_FINAL_PUNCTUATION
    };

const char dirStrings[][5] = {
    "L",
    "R",
    "EN",
    "ES",
    "ET",
    "AN",
    "CS",
    "B",
    "S",
    "WS",
    "ON",
    "LRE",
    "LRO",
    "AL",
    "RLE",
    "RLO",
    "PDF",
    "NSM",
    "BN"
};

void addUnicodeTest(TestNode** root)
{
    addTest(root, &TestUnicodeData, "tsutil/cucdtst/TestUnicodeData");
    addTest(root, &TestCodeUnit, "tsutil/cucdtst/TestCodeUnit");
    addTest(root, &TestCodePoint, "tsutil/cucdtst/TestCodePoint");
    addTest(root, &TestCharLength, "tsutil/cucdtst/TestCharLength");
    addTest(root, &TestAdditionalProperties, "tsutil/cucdtst/TestAdditionalProperties");
    addTest(root, &TestNumericProperties, "tsutil/cucdtst/TestNumericProperties");
    addTest(root, &TestUpperLower, "tsutil/cucdtst/TestUpperLower");
    addTest(root, &TestLetterNumber, "tsutil/cucdtst/TestLetterNumber");
    addTest(root, &TestMisc, "tsutil/cucdtst/TestMisc");
    addTest(root, &TestPOSIX, "tsutil/cucdtst/TestPOSIX");
    addTest(root, &TestControlPrint, "tsutil/cucdtst/TestControlPrint");
    addTest(root, &TestIdentifier, "tsutil/cucdtst/TestIdentifier");
    addTest(root, &TestCharNames, "tsutil/cucdtst/TestCharNames");
    addTest(root, &TestMirroring, "tsutil/cucdtst/TestMirroring");
    addTest(root, &TestUScriptCodeAPI, "tsutil/cucdtst/TestUScriptCodeAPI");
    addTest(root, &TestUScriptRunAPI, "tsutil/cucdtst/TestUScriptRunAPI");
    addTest(root, &TestPropertyNames, "tsutil/cucdtst/TestPropertyNames");
    addTest(root, &TestPropertyValues, "tsutil/cucdtst/TestPropertyValues");
    addTest(root, &TestConsistency, "tsutil/cucdtst/TestConsistency");
}

/*==================================================== */
/* test u_toupper() and u_tolower()                    */
/*==================================================== */
static void TestUpperLower()
{
    const UChar upper[] = {0x41, 0x42, 0x00b2, 0x01c4, 0x01c6, 0x01c9, 0x01c8, 0x01c9, 0x000c, 0x0000};
    const UChar lower[] = {0x61, 0x62, 0x00b2, 0x01c6, 0x01c6, 0x01c9, 0x01c9, 0x01c9, 0x000c, 0x0000};
    U_STRING_DECL(upperTest, "abcdefg123hij.?:klmno", 21);
    U_STRING_DECL(lowerTest, "ABCDEFG123HIJ.?:KLMNO", 21);
    int i;

    U_STRING_INIT(upperTest, "abcdefg123hij.?:klmno", 21);
    U_STRING_INIT(lowerTest, "ABCDEFG123HIJ.?:KLMNO", 21);

/*
Checks LetterLike Symbols which were previously a source of confusion
[Bertrand A. D. 02/04/98]
*/
    for (i=0x2100;i<0x2138;i++)
    {
        if(i!=0x2126 && i!=0x212a && i!=0x212b)
        {
            if (i != (int)u_tolower(i)) /* itself */
                log_err("Failed case conversion with itself: U+%04x\n", i);
            if (i != (int)u_toupper(i))
                log_err("Failed case conversion with itself: U+%04x\n", i);
        }
    }

    for(i=0; i < u_strlen(upper); i++){
        if(u_tolower(upper[i]) != lower[i]){
            log_err("FAILED u_tolower() for %lx Expected %lx Got %lx\n", upper[i], lower[i], u_tolower(upper[i]));
        }
    }

    log_verbose("testing upper lower\n");
    for (i = 0; i < 21; i++) {

        if (u_isalpha(upperTest[i]) && !u_islower(upperTest[i]))
        {
            log_err("Failed isLowerCase test at  %c\n", upperTest[i]);
        }
        else if (u_isalpha(lowerTest[i]) && !u_isupper(lowerTest[i]))
         {
            log_err("Failed isUpperCase test at %c\n", lowerTest[i]);
        }
        else if (upperTest[i] != u_tolower(lowerTest[i]))
        {
            log_err("Failed case conversion from %c  To %c :\n", lowerTest[i], upperTest[i]);
        }
        else if (lowerTest[i] != u_toupper(upperTest[i]))
         {
            log_err("Failed case conversion : %c To %c \n", upperTest[i], lowerTest[i]);
        }
        else if (upperTest[i] != u_tolower(upperTest[i]))
        {
            log_err("Failed case conversion with itself: %c\n", upperTest[i]);
        }
        else if (lowerTest[i] != u_toupper(lowerTest[i]))
        {
            log_err("Failed case conversion with itself: %c\n", lowerTest[i]);
        }
    }
    log_verbose("done testing upper lower\n");
    
    log_verbose("testing u_istitle\n");
    {
    	UChar expected[] = {
    					0x1F88,
    					0x1F89,
    					0x1F8A,
    					0x1F8B,
    					0x1F8C,
    					0x1F8D,
    					0x1F8E,
    					0x1F8F,
    					0x1F88,
    					0x1F89,
    					0x1F8A,
    					0x1F8B,
    					0x1F8C,
    					0x1F8D,
    					0x1F8E,
    					0x1F8F,
    					0x1F98,
    					0x1F99,
    					0x1F9A,
    					0x1F9B,
    					0x1F9C,
    					0x1F9D,
    					0x1F9E,
    					0x1F9F,
    					0x1F98,
    					0x1F99,
    					0x1F9A,
    					0x1F9B,
    					0x1F9C,
    					0x1F9D,
    					0x1F9E,
    					0x1F9F,
    					0x1FA8,
    					0x1FA9,
    					0x1FAA,
    					0x1FAB,
    					0x1FAC,
    					0x1FAD,
    					0x1FAE,
    					0x1FAF,
    					0x1FA8,
    					0x1FA9,
    					0x1FAA,
    					0x1FAB,
    					0x1FAC,
    					0x1FAD,
    					0x1FAE,
    					0x1FAF,
    					0x1FBC,
    					0x1FBC,
    					0x1FCC,
    					0x1FCC,
    					0x1FFC,
    					0x1FFC,
    			};
    	int32_t num = sizeof(expected)/sizeof(expected[0]);
    	int32_t i=0;
    	for(;i<num;i++){
    		if(!u_istitle(expected[i])){
    			log_err("u_istitle failed for 0x%4X. Expected TRUE, got FALSE\n",expected[i]);
    		}
    	}

    }
}

/* compare two sets, which is not easy with the current (ICU 2.4) C API... */

static UBool
showADiffB(const USet *a, const USet *b,
           const char *a_name, const char *b_name,
           UBool expect, UBool diffIsError) {
    int32_t i, start, end, length;
    UBool equal;
    UErrorCode errorCode;

    errorCode=U_ZERO_ERROR;
    equal=TRUE;
    i=0;
    for(;;) {
        length=uset_getItem(a, i, &start, &end, NULL, 0, &errorCode);
        if(errorCode==U_INDEX_OUTOFBOUNDS_ERROR) {
            return equal; /* done */
        }
        if(U_FAILURE(errorCode)) {
            log_err("error comparing %s with %s at item %d: %s\n",
                a_name, b_name, i, u_errorName(errorCode));
            return FALSE;
        }
        if(length!=0) {
            return equal; /* done with code points, got a string or -1 */
        }

        if(expect!=uset_containsRange(b, start, end)) {
            equal=FALSE;
            while(start<=end) {
                if(expect!=uset_contains(b, start)) {
                    if(diffIsError) {
                        if(expect) {
                            log_err("error: %s contains U+%04x but %s does not\n", a_name, start, b_name);
                        } else {
                            log_err("error: %s and %s both contain U+%04x but should not intersect\n", a_name, b_name, start);
                        }
                    } else {
                        if(expect) {
                            log_verbose("info: %s contains U+%04x but %s does not\n", a_name, start, b_name);
                        } else {
                            log_verbose("info: %s and %s both contain U+%04x but should not intersect\n", a_name, b_name, start);
                        }
                    }
                }
                ++start;
            }
        }

        ++i;
    }
}

static UBool
showAMinusB(const USet *a, const USet *b,
            const char *a_name, const char *b_name,
            UBool diffIsError) {
    return showADiffB(a, b, a_name, b_name, TRUE, diffIsError);
}

static UBool
showAIntersectB(const USet *a, const USet *b,
                const char *a_name, const char *b_name,
                UBool diffIsError) {
    return showADiffB(a, b, a_name, b_name, FALSE, diffIsError);
}

static UBool
compareUSets(const USet *a, const USet *b,
             const char *a_name, const char *b_name,
             UBool diffIsError) {
    return
        showAMinusB(a, b, a_name, b_name, diffIsError) &&
        showAMinusB(b, a, b_name, a_name, diffIsError);
}

/* test isLetter(u_isapha()) and isDigit(u_isdigit()) */
static void TestLetterNumber()
{
    UChar i = 0x0000;

    log_verbose("Testing for isalpha\n");
    for (i = 0x0041; i < 0x005B; i++) {
        if (!u_isalpha(i))
        {
            log_err("Failed isLetter test at  %.4X\n", i);
        }
    }
    for (i = 0x0660; i < 0x066A; i++) {
        if (u_isalpha(i))
        {
            log_err("Failed isLetter test with numbers at %.4X\n", i);
        }
    }

    log_verbose("Testing for isdigit\n");
    for (i = 0x0660; i < 0x066A; i++) {
        if (!u_isdigit(i))
        {
            log_verbose("Failed isNumber test at %.4X\n", i);
        }
    }

    log_verbose("Testing for isalnum\n");
    for (i = 0x0041; i < 0x005B; i++) {
        if (!u_isalnum(i))
        {
            log_err("Failed isAlNum test at  %.4X\n", i);
        }
    }
    for (i = 0x0660; i < 0x066A; i++) {
        if (!u_isalnum(i))
        {
            log_err("Failed isAlNum test at  %.4X\n", i);
        }
    }

    {
        /*
         * The following checks work only starting from Unicode 4.0.
         * Check the version number here.
         */
        UVersionInfo version;
        u_getUnicodeVersion(version);
        if(version[0]<4) {
            return;
        }
    }

    {
        /*
         * Sanity check:
         * Verify that exactly the digit characters have decimal digit values.
         * This assumption is used in the implementation of u_digit()
         * (which checks nt=de)
         * compared with the parallel java.lang.Character.digit()
         * (which checks Nd).
         *
         * This was not true in Unicode 3.2 and earlier.
         * The following characters had decimal digit values but were No not Nd.
         * (from DerivedNumericType-3.2.0.txt)
00B2..00B3    ; decimal # No   [2] SUPERSCRIPT TWO..SUPERSCRIPT THREE
00B9          ; decimal # No       SUPERSCRIPT ONE
2070          ; decimal # No       SUPERSCRIPT ZERO
2074..2079    ; decimal # No   [6] SUPERSCRIPT FOUR..SUPERSCRIPT NINE
2080..2089    ; decimal # No  [10] SUBSCRIPT ZERO..SUBSCRIPT NINE
         */
        U_STRING_DECL(digitsPattern, "[:Nd:]", 6);
        U_STRING_DECL(decimalValuesPattern, "[:Numeric_Type=Decimal:]", 24);

        USet *digits, *decimalValues;
        UErrorCode errorCode;

        U_STRING_INIT(digitsPattern, "[:Nd:]", 6);
        U_STRING_INIT(decimalValuesPattern, "[:Numeric_Type=Decimal:]", 24);
        errorCode=U_ZERO_ERROR;
        digits=uset_openPattern(digitsPattern, 6, &errorCode);
        decimalValues=uset_openPattern(decimalValuesPattern, 24, &errorCode);

        if(U_SUCCESS(errorCode)) {
            compareUSets(digits, decimalValues, "[:Nd:]", "[:Numeric_Type=Decimal:]", TRUE);
        }

        uset_close(digits);
        uset_close(decimalValues);
    }
}

/* Tests for isDefined(u_isdefined)(, isBaseForm(u_isbase()), isSpaceChar(u_isspace()), isWhiteSpace(), u_CharDigitValue() */
static void TestMisc()
{
    const UChar sampleSpaces[] = {0x0020, 0x00a0, 0x2000, 0x2001, 0x2005};
    const UChar sampleNonSpaces[] = {0x61, 0x62, 0x63, 0x64, 0x74};
    const UChar sampleUndefined[] = {0xfff1, 0xfff7, 0xfa6b };
    const UChar sampleDefined[] = {0x523E, 0x4f88, 0xfffd};
    const UChar sampleBase[] = {0x0061, 0x0031, 0x03d2};
    const UChar sampleNonBase[] = {0x002B, 0x0020, 0x203B};
    const UChar sampleChars[] = {0x000a, 0x0045, 0x4e00, 0xDC00, 0xFFE8, 0xFFF0};
    const UChar sampleDigits[]= {0x0030, 0x0662, 0x0F23, 0x0ED5};
    const UChar sampleNonDigits[] = {0x0010, 0x0041, 0x0122, 0x68FE};
    const UChar sampleWhiteSpaces[] = {0x2008, 0x2009, 0x200a, 0x001c, 0x000c};
    const UChar sampleNonWhiteSpaces[] = {0x61, 0x62, 0x3c, 0x28, 0x3f};


    const int32_t sampleDigitValues[] = {0, 2, 3, 5};
    const int32_t sample2DigitValues[]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; /*special characters not in the properties table*/

    uint32_t mask;

    int i;
    char icuVersion[U_MAX_VERSION_STRING_LENGTH];
    UVersionInfo realVersion;

    memset(icuVersion, 0, U_MAX_VERSION_STRING_LENGTH);

    log_verbose("Testing for isspace and nonspaces\n");
    for (i = 0; i < 5; i++) {
        if (!(u_isspace(sampleSpaces[i])) ||
                (u_isspace(sampleNonSpaces[i])))
        {
            log_err("Space char test error : %d or %d \n", (int32_t)sampleSpaces[i], (int32_t)sampleNonSpaces[i]);
        }
        if (!(u_isJavaSpaceChar(sampleSpaces[i])) ||
                (u_isJavaSpaceChar(sampleNonSpaces[i])))
        {
            log_err("u_isJavaSpaceChar() test error : %d or %d \n", (int32_t)sampleSpaces[i], (int32_t)sampleNonSpaces[i]);
        }
    }

    log_verbose("Testing for isspace and nonspaces\n");
    for (i = 0; i < 5; i++) {
        if (!(u_isWhitespace(sampleWhiteSpaces[i])) ||
                (u_isWhitespace(sampleNonWhiteSpaces[i])))
        {
            log_err("White Space char test error : %lx or %lx \n", sampleWhiteSpaces[i], sampleNonWhiteSpaces[i]);
        }
    }

    log_verbose("Testing for isdefined\n");
    for (i = 0; i < 3; i++) {
        if ((u_isdefined(sampleUndefined[i])) ||
                !(u_isdefined(sampleDefined[i])))
        {
            log_err("Undefined char test error : U+%04x or U+%04x\n", (int32_t)sampleUndefined[i], (int32_t)sampleDefined[i]);
        }
    }

    log_verbose("Testing for isbase\n");
    for (i = 0; i < 3; i++) {
        if ((u_isbase(sampleNonBase[i])) ||
                !(u_isbase(sampleBase[i])))
        {
            log_err("Non-baseform char test error : U+%04x or U+%04x",(int32_t)sampleNonBase[i], (int32_t)sampleBase[i]);
        }
    }

    log_verbose("Testing for isdigit \n");
    for (i = 0; i < 4; i++) {
        if ((u_isdigit(sampleDigits[i]) && 
            (u_charDigitValue(sampleDigits[i])!= sampleDigitValues[i])) ||
            (u_isdigit(sampleNonDigits[i]))) {
            log_err("Digit char test error : %lx   or   %lx\n", sampleDigits[i], sampleNonDigits[i]);
        }
    }

    /* Tests the ICU version #*/
    u_getVersion(realVersion);
    u_versionToString(realVersion, icuVersion);
    if (strncmp(icuVersion, U_ICU_VERSION, uprv_min(strlen(icuVersion), strlen(U_ICU_VERSION))) != 0)
    {
        log_err("ICU version test failed. Header says=%s, got=%s \n", U_ICU_VERSION, icuVersion);
    }
#if defined(ICU_VERSION)
    /* test only happens where we have configure.in with VERSION - sanity check. */
    if(strcmp(U_ICU_VERSION, ICU_VERSION))
    {
        log_err("ICU version mismatch: Header says %s, build environment says %s.\n",  U_ICU_VERSION, ICU_VERSION);
    }
#endif

    /* test U_GC_... */
    if(
        U_GET_GC_MASK(0x41)!=U_GC_LU_MASK ||
        U_GET_GC_MASK(0x662)!=U_GC_ND_MASK ||
        U_GET_GC_MASK(0xa0)!=U_GC_ZS_MASK ||
        U_GET_GC_MASK(0x28)!=U_GC_PS_MASK ||
        U_GET_GC_MASK(0x2044)!=U_GC_SM_MASK ||
        U_GET_GC_MASK(0xe0063)!=U_GC_CF_MASK
    ) {
        log_err("error: U_GET_GC_MASK does not work properly\n");
    }

    mask=0;
    mask=(mask&~U_GC_CN_MASK)|U_GC_CN_MASK;

    mask=(mask&~U_GC_LU_MASK)|U_GC_LU_MASK;
    mask=(mask&~U_GC_LL_MASK)|U_GC_LL_MASK;
    mask=(mask&~U_GC_LT_MASK)|U_GC_LT_MASK;
    mask=(mask&~U_GC_LM_MASK)|U_GC_LM_MASK;
    mask=(mask&~U_GC_LO_MASK)|U_GC_LO_MASK;

    mask=(mask&~U_GC_MN_MASK)|U_GC_MN_MASK;
    mask=(mask&~U_GC_ME_MASK)|U_GC_ME_MASK;
    mask=(mask&~U_GC_MC_MASK)|U_GC_MC_MASK;

    mask=(mask&~U_GC_ND_MASK)|U_GC_ND_MASK;
    mask=(mask&~U_GC_NL_MASK)|U_GC_NL_MASK;
    mask=(mask&~U_GC_NO_MASK)|U_GC_NO_MASK;

    mask=(mask&~U_GC_ZS_MASK)|U_GC_ZS_MASK;
    mask=(mask&~U_GC_ZL_MASK)|U_GC_ZL_MASK;
    mask=(mask&~U_GC_ZP_MASK)|U_GC_ZP_MASK;

    mask=(mask&~U_GC_CC_MASK)|U_GC_CC_MASK;
    mask=(mask&~U_GC_CF_MASK)|U_GC_CF_MASK;
    mask=(mask&~U_GC_CO_MASK)|U_GC_CO_MASK;
    mask=(mask&~U_GC_CS_MASK)|U_GC_CS_MASK;

    mask=(mask&~U_GC_PD_MASK)|U_GC_PD_MASK;
    mask=(mask&~U_GC_PS_MASK)|U_GC_PS_MASK;
    mask=(mask&~U_GC_PE_MASK)|U_GC_PE_MASK;
    mask=(mask&~U_GC_PC_MASK)|U_GC_PC_MASK;
    mask=(mask&~U_GC_PO_MASK)|U_GC_PO_MASK;

    mask=(mask&~U_GC_SM_MASK)|U_GC_SM_MASK;
    mask=(mask&~U_GC_SC_MASK)|U_GC_SC_MASK;
    mask=(mask&~U_GC_SK_MASK)|U_GC_SK_MASK;
    mask=(mask&~U_GC_SO_MASK)|U_GC_SO_MASK;

    mask=(mask&~U_GC_PI_MASK)|U_GC_PI_MASK;
    mask=(mask&~U_GC_PF_MASK)|U_GC_PF_MASK;

    if(mask!=(U_CHAR_CATEGORY_COUNT<32 ? U_MASK(U_CHAR_CATEGORY_COUNT)-1: 0xffffffff)) {
        log_err("error: problems with U_GC_XX_MASK constants\n");
    }

    mask=0;
    mask=(mask&~U_GC_C_MASK)|U_GC_C_MASK;
    mask=(mask&~U_GC_L_MASK)|U_GC_L_MASK;
    mask=(mask&~U_GC_M_MASK)|U_GC_M_MASK;
    mask=(mask&~U_GC_N_MASK)|U_GC_N_MASK;
    mask=(mask&~U_GC_Z_MASK)|U_GC_Z_MASK;
    mask=(mask&~U_GC_P_MASK)|U_GC_P_MASK;
    mask=(mask&~U_GC_S_MASK)|U_GC_S_MASK;

    if(mask!=(U_CHAR_CATEGORY_COUNT<32 ? U_MASK(U_CHAR_CATEGORY_COUNT)-1: 0xffffffff)) {
        log_err("error: problems with U_GC_Y_MASK constants\n");
    }
    {
    	UChar32 digit[10]={ 0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039 };
    	int32_t i=0;
    	for(i=0;i<10;i++){
    		if(digit[i]!=u_forDigit(i,10)){
    			log_err("u_forDigit failed for %i. Expected: 0x%4X Got: 0x%4X\n",i,digit[i],u_forDigit(i,10));
    		}
    	}
    }

    /* test u_digit() */
    {
        static const struct {
            UChar32 c;
            int8_t radix, value;
        } data[]={
            /* base 16 */
            { 0x0031, 16, 1 },
            { 0x0038, 16, 8 },
            { 0x0043, 16, 12 },
            { 0x0066, 16, 15 },
            { 0x00e4, 16, -1 },
            { 0x0662, 16, 2 },
            { 0x06f5, 16, 5 },
            { 0xff13, 16, 3 },
            { 0xff41, 16, 10 },

            /* base 8 */
            { 0x0031, 8, 1 },
            { 0x0038, 8, -1 },
            { 0x0043, 8, -1 },
            { 0x0066, 8, -1 },
            { 0x00e4, 8, -1 },
            { 0x0662, 8, 2 },
            { 0x06f5, 8, 5 },
            { 0xff13, 8, 3 },
            { 0xff41, 8, -1 },

            /* base 36 */
            { 0x5a, 36, 35 },
            { 0x7a, 36, 35 },
            { 0xff3a, 36, 35 },
            { 0xff5a, 36, 35 },

            /* wrong radix values */
            { 0x0031, 1, -1 },
            { 0xff3a, 37, -1 }
        };

        int32_t i;

        for(i=0; i<LENGTHOF(data); ++i) {
            if(u_digit(data[i].c, data[i].radix)!=data[i].value) {
                log_err("u_digit(U+%04x, %d)=%d expected %d\n",
                        data[i].c,
                        data[i].radix,
                        u_digit(data[i].c, data[i].radix),
                        data[i].value);
            }
        }
    }
}

/* test C/POSIX-style functions --------------------------------------------- */

/* bit flags */
#define ISAL     1
#define ISLO     2
#define ISUP     4

#define ISDI     8
#define ISXD  0x10

#define ISAN  0x20

#define ISPU  0x40
#define ISGR  0x80
#define ISPR 0x100

#define ISSP 0x200
#define ISBL 0x400
#define ISCN 0x800

/* C/POSIX-style functions, in the same order as the bit flags */
typedef UBool IsPOSIXClass(UChar32 c);

static const struct {
    IsPOSIXClass *fn;
    const char *name;
} posixClasses[]={
    { u_isalpha, "isalpha" },
    { u_islower, "islower" },
    { u_isupper, "isupper" },
    { u_isdigit, "isdigit" },
    { u_isxdigit, "isxdigit" },
    { u_isalnum, "isalnum" },
    { u_ispunct, "ispunct" },
    { u_isgraph, "isgraph" },
    { u_isprint, "isprint" },
    { u_isspace, "isspace" },
    { u_isblank, "isblank" },
    { u_iscntrl, "iscntrl" }
};

static const struct {
    UChar32 c;
    uint32_t posixResults;
} posixData[]={
    { 0x0008,                                                        ISCN },    /* backspace */
    { 0x0009,                                              ISSP|ISBL|ISCN },    /* TAB */
    { 0x000a,                                              ISSP|     ISCN },    /* LF */
    { 0x000c,                                              ISSP|     ISCN },    /* FF */
    { 0x000d,                                              ISSP|     ISCN },    /* CR */
    { 0x0020,                                         ISPR|ISSP|ISBL      },    /* space */
    { 0x0021,                               ISPU|ISGR|ISPR                },    /* ! */
    { 0x0033,                ISDI|ISXD|ISAN|     ISGR|ISPR                },    /* 3 */
    { 0x0040,                               ISPU|ISGR|ISPR                },    /* @ */
    { 0x0041, ISAL|     ISUP|     ISXD|ISAN|     ISGR|ISPR                },    /* A */
    { 0x007a, ISAL|ISLO|               ISAN|     ISGR|ISPR                },    /* z */
    { 0x007b,                               ISPU|ISGR|ISPR                },    /* { */
    { 0x0085,                                              ISSP|     ISCN },    /* NEL */
    { 0x00a0,                                         ISPR|ISSP|ISBL      },    /* NBSP */
    { 0x00a4,                                    ISGR|ISPR                },    /* currency sign */
    { 0x00e4, ISAL|ISLO|               ISAN|     ISGR|ISPR                },    /* a-umlaut */
    { 0x0300,                                    ISGR|ISPR                },    /* combining grave */
    { 0x0600,                                                        ISCN },    /* arabic number sign */
    { 0x0627, ISAL|                    ISAN|     ISGR|ISPR                },    /* alef */
    { 0x0663,                ISDI|ISXD|ISAN|     ISGR|ISPR                },    /* arabic 3 */
    { 0x2002,                                         ISPR|ISSP|ISBL      },    /* en space */
    { 0x2007,                                         ISPR|ISSP|ISBL      },    /* figure space */
    { 0x2009,                                         ISPR|ISSP|ISBL      },    /* thin space */
    { 0x200b,                                         ISPR|ISSP           },    /* ZWSP */
    { 0x200e,                                                        ISCN },    /* LRM */
    { 0x2028,                                         ISPR|ISSP|     ISCN },    /* LS */
    { 0x2029,                                         ISPR|ISSP|     ISCN },    /* PS */
    { 0x20ac,                                    ISGR|ISPR                },    /* Euro */
    { 0xff15,                ISDI|ISXD|ISAN|     ISGR|ISPR                },    /* fullwidth 5 */
    { 0xff25, ISAL|     ISUP|     ISXD|ISAN|     ISGR|ISPR                },    /* fullwidth E */
    { 0xff35, ISAL|     ISUP|          ISAN|     ISGR|ISPR                },    /* fullwidth U */
    { 0xff45, ISAL|ISLO|          ISXD|ISAN|     ISGR|ISPR                },    /* fullwidth e */
    { 0xff55, ISAL|ISLO|               ISAN|     ISGR|ISPR                }     /* fullwidth u */
};

static void
TestPOSIX() {
    uint32_t mask;
    int32_t cl, i;
    UBool expect;

    mask=1;
    for(cl=0; cl<12; ++cl) {
        for(i=0; i<LENGTHOF(posixData); ++i) {
            expect=(UBool)((posixData[i].posixResults&mask)!=0);
            if(posixClasses[cl].fn(posixData[i].c)!=expect) {
                log_err("u_%s(U+%04x)=%s is wrong\n",
                    posixClasses[cl].name, posixData[i].c, expect ? "FALSE" : "TRUE");
            }
        }
        mask<<=1;
    }
}

/* Tests for isControl(u_iscntrl()) and isPrintable(u_isprint()) */
static void TestControlPrint()
{
    const UChar sampleControl[] = {0x1b, 0x97, 0x82, 0x2028, 0x2029, 0x200c, 0x202b};
    const UChar sampleNonControl[] = {0x61, 0x0031, 0x00e2};
    const UChar samplePrintable[] = {0x0042, 0x005f, 0x2014};
    const UChar sampleNonPrintable[] = {0x200c, 0x009f, 0x001b};
    UChar32 c;
    int i;

    log_verbose("Testing for iscontrol\n");
    for (i = 0; i < LENGTHOF(sampleControl); i++) {
        if (!u_iscntrl(sampleControl[i]))
        {
            log_err("Control char test error : U+%04x should be control but is not\n", (int32_t)sampleControl[i]);
        }
    }

    log_verbose("Testing for !iscontrol\n");
    for (i = 0; i < LENGTHOF(sampleNonControl); i++) {
        if (u_iscntrl(sampleNonControl[i]))
        {
            log_err("Control char test error : U+%04x should not be control but is\n", (int32_t)sampleNonControl[i]);
        }
    }

    log_verbose("testing for isprintable\n");
    for (i = 0; i < 3; i++) {
        if (!u_isprint(samplePrintable[i]))
        {
            log_err("Printable char test error : U+%04x should be printable but is not\n", (int32_t)samplePrintable[i]);
        }
        if (u_isprint(sampleNonPrintable[i]))
        {
            log_err("Printable char test error : U+%04x should not be printable but is\n", (int32_t)sampleNonPrintable[i]);
        }
    }

    /* test all ISO 8 controls */
    for(c=0; c<=0x9f; ++c) {
        if(c==0x20) {
            /* skip ASCII graphic characters and continue with DEL */
            c=0x7f;
        }
        if(!u_iscntrl(c)) {
            log_err("error: u_iscntrl(ISO 8 control U+%04x)=FALSE\n", c);
        }
        if(!u_isISOControl(c)) {
            log_err("error: u_isISOControl(ISO 8 control U+%04x)=FALSE\n", c);
        }
        if(u_isprint(c)) {
            log_err("error: u_isprint(ISO 8 control U+%04x)=TRUE\n", c);
        }
    }

    /* test all Latin-1 graphic characters */
    for(c=0x20; c<=0xff; ++c) {
        if(c==0x7f) {
            c=0xa0;
        } else if(c==0xad) {
            /* Unicode 4 changes 00AD Soft Hyphen to Cf (and it is in fact not printable) */
            ++c;
        }
        if(!u_isprint(c)) {
            log_err("error: u_isprint(Latin-1 graphic character U+%04x)=FALSE\n", c);
        }
    }
}

/* u_isJavaIDStart, u_isJavaIDPart, u_isIDStart(), u_isIDPart(), u_isIDIgnorable()*/
static void TestIdentifier()
{
    const UChar sampleJavaIDStart[] = {0x0071, 0x00e4, 0x005f};
    const UChar sampleNonJavaIDStart[] = {0x0020, 0x2030, 0x0082};
    const UChar sampleJavaIDPart[] = {0x005f, 0x0032, 0x0045};
    const UChar sampleNonJavaIDPart[] = {0x2030, 0x2020, 0x0020};
    const UChar sampleUnicodeIDStart[] = {0x0250, 0x00e2, 0x0061};
    const UChar sampleNonUnicodeIDStart[] = {0x2000, 0x000a, 0x2019};
    const UChar sampleUnicodeIDPart[] = {0x005f, 0x0032, 0x0045};
    const UChar sampleNonUnicodeIDPart[] = {0x2030, 0x00a3, 0x0020};
    const UChar sampleIDIgnore[] = {0x0006, 0x0010, 0x206b};
    const UChar sampleNonIDIgnore[] = {0x0075, 0x00a3, 0x0061};

    int i;

    log_verbose("Testing sampleJavaID start \n");
    for (i = 0; i < 3; i++) {
        if (!(u_isJavaIDStart(sampleJavaIDStart[i])) ||
                (u_isJavaIDStart(sampleNonJavaIDStart[i])))
            log_err("Java ID Start char test error : %lx or %lx\n",
            sampleJavaIDStart[i], sampleNonJavaIDStart[i]);
    }

    log_verbose("Testing sampleJavaID part \n");
    for (i = 0; i < 3; i++) {
        if (!(u_isJavaIDPart(sampleJavaIDPart[i])) ||
                (u_isJavaIDPart(sampleNonJavaIDPart[i])))
            log_err("Java ID Part char test error : %lx or %lx\n",
             sampleJavaIDPart[i], sampleNonJavaIDPart[i]);
    }

    log_verbose("Testing sampleUnicodeID start \n");
    for (i = 0; i < 3; i++) {
        /* T_test_logln_ustr((int32_t)i); */
        if (!(u_isIDStart(sampleUnicodeIDStart[i])) ||
                (u_isIDStart(sampleNonUnicodeIDStart[i])))
        {
            log_err("Unicode ID Start char test error : %lx  or  %lx\n", sampleUnicodeIDStart[i],
                                    sampleNonUnicodeIDStart[i]);
        }
    }

    log_verbose("Testing sample unicode ID part \n");
    for (i = 2; i < 3; i++) {   /* nos *** starts with 2 instead of 0, until clarified */
        /* T_test_logln_ustr((int32_t)i); */
        if (!(u_isIDPart(sampleUnicodeIDPart[i])) ||
                (u_isIDPart(sampleNonUnicodeIDPart[i])))
           {
            log_err("Unicode ID Part char test error : %lx  or  %lx", sampleUnicodeIDPart[i], sampleNonUnicodeIDPart[i]);
            }
    }

    log_verbose("Testing  sampleId ignore\n");
    for (i = 0; i < 3; i++) {
        /*T_test_logln_ustr((int32_t)i); */
        if (!(u_isIDIgnorable(sampleIDIgnore[i])) ||
                (u_isIDIgnorable(sampleNonIDIgnore[i])))
        {
            log_err("ID ignorable char test error : U+%04x  or  U+%04x\n", sampleIDIgnore[i], sampleNonIDIgnore[i]);
        }
    }
}

/* for each line of UnicodeData.txt, check some of the properties */
/*
 * ### TODO
 * This test fails incorrectly if the First or Last code point of a repetitive area
 * is overridden, which is allowed and is encouraged for the PUAs.
 * Currently, this means that both area First/Last and override lines are
 * tested against the properties from the API,
 * and the area boundary will not match and cause an error.
 *
 * This function should detect area boundaries and skip them for the test of individual
 * code points' properties.
 * Then it should check that the areas contain all the same properties except where overridden.
 * For this, it would have had to set a flag for which code points were listed explicitly.
 */
static void U_CALLCONV
unicodeDataLineFn(void *context,
                  char *fields[][2], int32_t fieldCount,
                  UErrorCode *pErrorCode)
{
    char buffer[100];
    char *end;
    uint32_t value;
    UChar32 c;
    int32_t i;
    int8_t type;

    /* get the character code, field 0 */
    c=strtoul(fields[0][0], &end, 16);
    if(end<=fields[0][0] || end!=fields[0][1]) {
        log_err("error: syntax error in field 0 at %s\n", fields[0][0]);
        return;
    }
    if((uint32_t)c>=UCHAR_MAX_VALUE + 1) {
        log_err("error in UnicodeData.txt: code point %lu out of range\n", c);
        return;
    }

    /* get general category, field 2 */
    *fields[2][1]=0;
    type = (int8_t)tagValues[MakeProp(fields[2][0])];
    if(u_charType(c)!=type) {
        log_err("error: u_charType(U+%04lx)==%u instead of %u\n", c, u_charType(c), type);
    }
    if((uint32_t)u_getIntPropertyValue(c, UCHAR_GENERAL_CATEGORY_MASK)!=U_MASK(type)) {
        log_err("error: (uint32_t)u_getIntPropertyValue(U+%04lx, UCHAR_GENERAL_CATEGORY_MASK)!=U_MASK(u_charType())\n", c);
    }

    /* get canonical combining class, field 3 */
    value=strtoul(fields[3][0], &end, 10);
    if(end<=fields[3][0] || end!=fields[3][1]) {
        log_err("error: syntax error in field 3 at code 0x%lx\n", c);
        return;
    }
    if(value>255) {
        log_err("error in UnicodeData.txt: combining class %lu out of range\n", value);
        return;
    }
#if !UCONFIG_NO_NORMALIZATION
    if(value!=u_getCombiningClass(c) || value!=(uint32_t)u_getIntPropertyValue(c, UCHAR_CANONICAL_COMBINING_CLASS)) {
        log_err("error: u_getCombiningClass(U+%04lx)==%hu instead of %lu\n", c, u_getCombiningClass(c), value);
    }
#endif

    /* get BiDi category, field 4 */
    *fields[4][1]=0;
    i=MakeDir(fields[4][0]);
    if(i!=u_charDirection(c) || i!=u_getIntPropertyValue(c, UCHAR_BIDI_CLASS)) {
        log_err("error: u_charDirection(U+%04lx)==%u instead of %u (%s)\n", c, u_charDirection(c), MakeDir(fields[4][0]), fields[4][0]);
    }

    /* get ISO Comment, field 11 */
    *fields[11][1]=0;
    i=u_getISOComment(c, buffer, sizeof(buffer), pErrorCode);
    if(U_FAILURE(*pErrorCode) || 0!=strcmp(fields[11][0], buffer)) {
        log_err("error: u_getISOComment(U+%04lx) wrong (%s): \"%s\" should be \"%s\"\n",
            c, u_errorName(*pErrorCode),
            U_FAILURE(*pErrorCode) ? buffer : "[error]",
            fields[11][0]);
    }

    /* get uppercase mapping, field 12 */
    if(fields[12][0]!=fields[12][1]) {
        value=strtoul(fields[12][0], &end, 16);
        if(end!=fields[12][1]) {
            log_err("error: syntax error in field 12 at code 0x%lx\n", c);
            return;
        }
        if((UChar32)value!=u_toupper(c)) {
            log_err("error: u_toupper(U+%04lx)==U+%04lx instead of U+%04lx\n", c, u_toupper(c), value);
        }
    } else {
        /* no case mapping: the API must map the code point to itself */
        if(c!=u_toupper(c)) {
            log_err("error: U+%04lx does not have an uppercase mapping but u_toupper()==U+%04lx\n", c, u_toupper(c));
        }
    }

    /* get lowercase mapping, field 13 */
    if(fields[13][0]!=fields[13][1]) {
        value=strtoul(fields[13][0], &end, 16);
        if(end!=fields[13][1]) {
            log_err("error: syntax error in field 13 at code 0x%lx\n", c);
            return;
        }
        if((UChar32)value!=u_tolower(c)) {
            log_err("error: u_tolower(U+%04lx)==U+%04lx instead of U+%04lx\n", c, u_tolower(c), value);
        }
    } else {
        /* no case mapping: the API must map the code point to itself */
        if(c!=u_tolower(c)) {
            log_err("error: U+%04lx does not have a lowercase mapping but u_tolower()==U+%04lx\n", c, u_tolower(c));
        }
    }

    /* get titlecase mapping, field 14 */
    if(fields[14][0]!=fields[14][1]) {
        value=strtoul(fields[14][0], &end, 16);
        if(end!=fields[14][1]) {
            log_err("error: syntax error in field 14 at code 0x%lx\n", c);
            return;
        }
        if((UChar32)value!=u_totitle(c)) {
            log_err("error: u_totitle(U+%04lx)==U+%04lx instead of U+%04lx\n", c, u_totitle(c), value);
        }
    } else {
        /* no case mapping: the API must map the code point to itself */
        if(c!=u_totitle(c)) {
            log_err("error: U+%04lx does not have a titlecase mapping but u_totitle()==U+%04lx\n", c, u_totitle(c));
        }
    }
}

static UBool U_CALLCONV
enumTypeRange(const void *context, UChar32 start, UChar32 limit, UCharCategory type) {
    static const UChar32 test[][2]={
        {0x41, U_UPPERCASE_LETTER},
        {0x308, U_NON_SPACING_MARK},
        {0xfffe, U_GENERAL_OTHER_TYPES},
        {0xe0041, U_FORMAT_CHAR},
        {0xeffff, U_UNASSIGNED}
    };

    /* default Bidi classes for unassigned code points */
    static const int32_t defaultBidi[][2]={ /* { limit, class } */
        { 0x0590, U_LEFT_TO_RIGHT },
        { 0x0600, U_RIGHT_TO_LEFT },
        { 0x07C0, U_RIGHT_TO_LEFT_ARABIC },
        { 0x0900, U_RIGHT_TO_LEFT },
        { 0xFB1D, U_LEFT_TO_RIGHT },
        { 0xFB50, U_RIGHT_TO_LEFT },
        { 0xFE00, U_RIGHT_TO_LEFT_ARABIC },
        { 0xFE70, U_LEFT_TO_RIGHT },
        { 0xFF00, U_RIGHT_TO_LEFT_ARABIC },
        { 0x10800, U_LEFT_TO_RIGHT },
        { 0x11000, U_RIGHT_TO_LEFT },
        { 0x110000, U_LEFT_TO_RIGHT }
    };

    UChar32 c;
    int i, count;

    if(0!=strcmp((const char *)context, "a1")) {
        log_err("error: u_enumCharTypes() passes on an incorrect context pointer\n");
        return FALSE;
    }

    count=sizeof(test)/sizeof(test[0]);
    for(i=0; i<count; ++i) {
        if(start<=test[i][0] && test[i][0]<limit) {
            if(type!=(UCharCategory)test[i][1]) {
                log_err("error: u_enumCharTypes() has range [U+%04lx, U+%04lx[ with %ld instead of U+%04lx with %ld\n",
                        start, limit, (long)type, test[i][0], test[i][1]);
            }
            /* stop at the range that includes the last test code point */
            return i==(count-1) ? FALSE : TRUE;
        }
    }

    if(start>test[count-1][0]) {
        log_err("error: u_enumCharTypes() has range [U+%04lx, U+%04lx[ with %ld after it should have stopped\n",
                start, limit, (long)type);
        return FALSE;
    }

    /*
     * LineBreak.txt specifies:
     *   #  - Assigned characters that are not listed explicitly are given the value
     *   #    "AL".
     *   #  - Unassigned characters are given the value "XX".
     *
     * PUA characters are listed explicitly with "XX".
     * Verify that no assigned character has "XX".
     */
    if(type!=U_UNASSIGNED && type!=U_PRIVATE_USE_CHAR) {
        c=start;
        while(c<limit) {
            if(0==u_getIntPropertyValue(c, UCHAR_LINE_BREAK)) {
                log_err("error UCHAR_LINE_BREAK(assigned U+%04lx)=XX\n", c);
            }
            ++c;
        }
    }

    /*
     * Verify default Bidi classes.
     * See table 3-7 "Bidirectional Character Types" in UAX #9.
     * http://www.unicode.org/reports/tr9/
     *
     * See also DerivedBidiClass.txt for Cn code points!
     */
    if(type==U_UNASSIGNED || type==U_PRIVATE_USE_CHAR) {
        /* enumerate the intersections of defaultBidi ranges with [start..limit[ */
        c=start;
        for(i=0; i<LENGTHOF(defaultBidi) && c<limit; ++i) {
            if((int32_t)c<defaultBidi[i][0]) {
                while(c<limit && (int32_t)c<defaultBidi[i][0]) {
                    if( u_charDirection(c)!=(UCharDirection)defaultBidi[i][1] ||
                        u_getIntPropertyValue(c, UCHAR_BIDI_CLASS)!=defaultBidi[i][1]
                    ) {
                        log_err("error: u_charDirection(unassigned/PUA U+%04lx)=%s should be %s\n",
                            c, dirStrings[u_charDirection(c)], dirStrings[defaultBidi[i][1]]);
                    }
                    ++c;
                }
            }
        }
    }

    return TRUE;
}

/* tests for several properties */
static void TestUnicodeData()
{
    char newPath[256];
    char backupPath[256];
    UVersionInfo expectVersionArray;
    UVersionInfo versionArray;
    char *fields[15][2];
    UErrorCode errorCode;
    UChar32 c;
    int8_t type;

    /* Look inside ICU_DATA first */
    strcpy(newPath, u_getDataDirectory());
    strcat(newPath, ".." U_FILE_SEP_STRING "unidata" U_FILE_SEP_STRING "UnicodeData.txt");

    /* As a fallback, try to guess where the source data was located
     *    at the time ICU was built, and look there.
     */
#if defined (U_TOPSRCDIR)
    strcpy(backupPath, U_TOPSRCDIR  U_FILE_SEP_STRING "data");
#else
    strcpy(backupPath, __FILE__);
    strrchr(backupPath, U_FILE_SEP_CHAR)[0] = 0; /* Remove the file name */
    strrchr(backupPath, U_FILE_SEP_CHAR)[0] = 0; /* Previous directory */
    strrchr(backupPath, U_FILE_SEP_CHAR)[0] = 0; /* Previous directory */
    strcat(backupPath, U_FILE_SEP_STRING "data");
#endif
    strcat(backupPath, U_FILE_SEP_STRING);
    strcat(backupPath, "unidata" U_FILE_SEP_STRING "UnicodeData.txt");

    u_versionFromString(expectVersionArray, U_UNICODE_VERSION);
    u_getUnicodeVersion(versionArray);
    if(memcmp(versionArray, expectVersionArray, U_MAX_VERSION_LENGTH) != 0)
    {
        log_err("Testing u_getUnicodeVersion() - expected " U_UNICODE_VERSION " got %d.%d.%d.%d\n",
        versionArray[0], versionArray[1], versionArray[2], versionArray[3]);
    }

#if defined(ICU_UNICODE_VERSION)
    /* test only happens where we have configure.in with UNICODE_VERSION - sanity check. */
    if(strcmp(U_UNICODE_VERSION, ICU_UNICODE_VERSION))
    {
         log_err("Testing configure.in's ICU_UNICODE_VERSION - expected " U_UNICODE_VERSION " got " ICU_UNICODE_VERSION "\n");
    }
#endif

    if (ublock_getCode((UChar)0x0041) != UBLOCK_BASIC_LATIN || u_getIntPropertyValue(0x41, UCHAR_BLOCK)!=(int32_t)UBLOCK_BASIC_LATIN) {
        log_err("ublock_getCode(U+0041) property failed! Expected : %i Got: %i \n", UBLOCK_BASIC_LATIN,ublock_getCode((UChar)0x0041));
    }

    errorCode=U_ZERO_ERROR;
    u_parseDelimitedFile(newPath, ';', fields, 15, unicodeDataLineFn, NULL, &errorCode);
    if(errorCode==U_FILE_ACCESS_ERROR) {
        errorCode=U_ZERO_ERROR;
        u_parseDelimitedFile(backupPath, ';', fields, 15, unicodeDataLineFn, NULL, &errorCode);
    }
    if(U_FAILURE(errorCode)) {
        log_err("error parsing UnicodeData.txt: %s\n", u_errorName(errorCode));
    }

    /* sanity check on repeated properties */
    for(c=0xfffe; c<=0x10ffff;) {
        type=u_charType(c);
        if((uint32_t)u_getIntPropertyValue(c, UCHAR_GENERAL_CATEGORY_MASK)!=U_MASK(type)) {
            log_err("error: (uint32_t)u_getIntPropertyValue(U+%04lx, UCHAR_GENERAL_CATEGORY_MASK)!=U_MASK(u_charType())\n", c);
        }
        if(type!=U_UNASSIGNED) {
            log_err("error: u_charType(U+%04lx)!=U_UNASSIGNED (returns %d)\n", c, u_charType(c));
        }
        if((c&0xffff)==0xfffe) {
            ++c;
        } else {
            c+=0xffff;
        }
    }

    /* test that PUA is not "unassigned" */
    for(c=0xe000; c<=0x10fffd;) {
        type=u_charType(c);
        if((uint32_t)u_getIntPropertyValue(c, UCHAR_GENERAL_CATEGORY_MASK)!=U_MASK(type)) {
            log_err("error: (uint32_t)u_getIntPropertyValue(U+%04lx, UCHAR_GENERAL_CATEGORY_MASK)!=U_MASK(u_charType())\n", c);
        }
        if(type==U_UNASSIGNED) {
            log_err("error: u_charType(U+%04lx)==U_UNASSIGNED\n", c);
        } else if(type!=U_PRIVATE_USE_CHAR) {
            log_verbose("PUA override: u_charType(U+%04lx)=%d\n", c, type);
        }
        if(c==0xf8ff) {
            c=0xf0000;
        } else if(c==0xffffd) {
            c=0x100000;
        } else {
            ++c;
        }
    }

    /* test u_enumCharTypes() */
    u_enumCharTypes(enumTypeRange, "a1");
}

static void TestCodeUnit(){
    const UChar codeunit[]={0x0000,0xe065,0x20ac,0xd7ff,0xd800,0xd841,0xd905,0xdbff,0xdc00,0xdc02,0xddee,0xdfff,0};

    int32_t i;

    for(i=0; i<(int32_t)(sizeof(codeunit)/sizeof(codeunit[0])); i++){
        UChar c=codeunit[i];
        if(i<4){
            if(!(UTF_IS_SINGLE(c)) || (UTF_IS_LEAD(c)) || (UTF_IS_TRAIL(c)) ||(UTF_IS_SURROGATE(c))){
                log_err("ERROR: U+%04x is a single", c);
            }

        }
        if(i >= 4 && i< 8){
            if(!(UTF_IS_LEAD(c)) || UTF_IS_SINGLE(c) || UTF_IS_TRAIL(c) || !(UTF_IS_SURROGATE(c))){
                log_err("ERROR: U+%04x is a first surrogate", c);
            }
        }
        if(i >= 8 && i< 12){
            if(!(UTF_IS_TRAIL(c)) || UTF_IS_SINGLE(c) || UTF_IS_LEAD(c) || !(UTF_IS_SURROGATE(c))){
                log_err("ERROR: U+%04x is a second surrogate", c);
            }
        }
    }

}

static void TestCodePoint(){
    const UChar32 codePoint[]={
        /*surrogate, notvalid(codepoint), not a UnicodeChar, not Error */
        0xd800,
        0xdbff,
        0xdc00,
        0xdfff,
        0xdc04,
        0xd821,
        /*not a surrogate, valid, isUnicodeChar , not Error*/
        0x20ac,
        0xd7ff,
        0xe000,
        0xe123,
        0x0061,
        0xe065, 
        0x20402,
        0x24506,
        0x23456,
        0x20402,
        0x10402,
        0x23456,
        /*not a surrogate, not valid, isUnicodeChar, isError */
        0x0015,
        0x009f,
        /*not a surrogate, not valid, not isUnicodeChar, isError */
        0xffff,
        0xfffe,
    };
    int32_t i;
    for(i=0; i<(int32_t)(sizeof(codePoint)/sizeof(codePoint[0])); i++){
        UChar32 c=codePoint[i];
        if(i<6){
            if(!UTF_IS_SURROGATE(c) || !U_IS_SURROGATE(c) || !U16_IS_SURROGATE(c)){
                log_err("ERROR: isSurrogate() failed for U+%04x\n", c);
            }
            if(UTF_IS_VALID(c)){
                log_err("ERROR: isValid() failed for U+%04x\n", c);
            }
            if(UTF_IS_UNICODE_CHAR(c) || U_IS_UNICODE_CHAR(c)){
                log_err("ERROR: isUnicodeChar() failed for U+%04x\n", c);
            }
            if(UTF_IS_ERROR(c)){
                log_err("ERROR: isError() failed for U+%04x\n", c);
            }
        }else if(i >=6 && i<18){
            if(UTF_IS_SURROGATE(c) || U_IS_SURROGATE(c) || U16_IS_SURROGATE(c)){
                log_err("ERROR: isSurrogate() failed for U+%04x\n", c);
            }
            if(!UTF_IS_VALID(c)){
                log_err("ERROR: isValid() failed for U+%04x\n", c);
            }
            if(!UTF_IS_UNICODE_CHAR(c) || !U_IS_UNICODE_CHAR(c)){
                log_err("ERROR: isUnicodeChar() failed for U+%04x\n", c);
            }
            if(UTF_IS_ERROR(c)){
                log_err("ERROR: isError() failed for U+%04x\n", c);
            }
        }else if(i >=18 && i<20){
            if(UTF_IS_SURROGATE(c) || U_IS_SURROGATE(c) || U16_IS_SURROGATE(c)){
                log_err("ERROR: isSurrogate() failed for U+%04x\n", c);
            }
            if(UTF_IS_VALID(c)){
                log_err("ERROR: isValid() failed for U+%04x\n", c);
            }
            if(!UTF_IS_UNICODE_CHAR(c) || !U_IS_UNICODE_CHAR(c)){
                log_err("ERROR: isUnicodeChar() failed for U+%04x\n", c);
            }
            if(!UTF_IS_ERROR(c)){
                log_err("ERROR: isError() failed for U+%04x\n", c);
            }
        }
        else if(i >=18 && i<(int32_t)(sizeof(codePoint)/sizeof(codePoint[0]))){
            if(UTF_IS_SURROGATE(c) || U_IS_SURROGATE(c) || U16_IS_SURROGATE(c)){
                log_err("ERROR: isSurrogate() failed for U+%04x\n", c);
            }
            if(UTF_IS_VALID(c)){
                log_err("ERROR: isValid() failed for U+%04x\n", c);
            }
            if(UTF_IS_UNICODE_CHAR(c) || U_IS_UNICODE_CHAR(c)){
                log_err("ERROR: isUnicodeChar() failed for U+%04x\n", c);
            }
            if(!UTF_IS_ERROR(c)){
                log_err("ERROR: isError() failed for U+%04x\n", c);
            }
        }
    }

}

static void TestCharLength()
{
    const int32_t codepoint[]={
        1, 0x0061,
        1, 0xe065,
        1, 0x20ac,
        2, 0x20402,
        2, 0x23456,
        2, 0x24506,
        2, 0x20402,
        2, 0x10402,
        1, 0xd7ff,
        1, 0xe000
    };

    int32_t i;
    UBool multiple;
    for(i=0; i<(int32_t)(sizeof(codepoint)/sizeof(codepoint[0])); i=(int16_t)(i+2)){
        UChar32 c=codepoint[i+1];
        if(UTF_CHAR_LENGTH(c) != codepoint[i] || U16_LENGTH(c) != codepoint[i]){
            log_err("The no: of code units for U+%04x:- Expected: %d Got: %d\n", c, codepoint[i], UTF_CHAR_LENGTH(c));
        }
        multiple=(UBool)(codepoint[i] == 1 ? FALSE : TRUE);
        if(UTF_NEED_MULTIPLE_UCHAR(c) != multiple){
            log_err("ERROR: Unicode::needMultipleUChar() failed for U+%04x\n", c);
        }
    }
}

/*internal functions ----*/
static int32_t MakeProp(char* str) 
{
    int32_t result = 0;
    char* matchPosition =0;

    matchPosition = strstr(tagStrings, str);
    if (matchPosition == 0) 
    {
        log_err("unrecognized type letter ");
        log_err(str);
    }
    else result = ((matchPosition - tagStrings) / 2);
    return result;
}

static int32_t MakeDir(char* str) 
{
    int32_t pos = 0;
    for (pos = 0; pos < 19; pos++) {
        if (strcmp(str, dirStrings[pos]) == 0) {
            return pos;
        }
    }
    return -1;
}

/* test u_charName() -------------------------------------------------------- */

static const struct {
    uint32_t code;
    const char *name, *oldName, *extName;
} names[]={
    {0x0061, "LATIN SMALL LETTER A", "", "LATIN SMALL LETTER A"},
    {0x0284, "LATIN SMALL LETTER DOTLESS J WITH STROKE AND HOOK", "LATIN SMALL LETTER DOTLESS J BAR HOOK", "LATIN SMALL LETTER DOTLESS J WITH STROKE AND HOOK" },
    {0x3401, "CJK UNIFIED IDEOGRAPH-3401", "", "CJK UNIFIED IDEOGRAPH-3401" },
    {0x7fed, "CJK UNIFIED IDEOGRAPH-7FED", "", "CJK UNIFIED IDEOGRAPH-7FED" },
    {0xac00, "HANGUL SYLLABLE GA", "", "HANGUL SYLLABLE GA" },
    {0xd7a3, "HANGUL SYLLABLE HIH", "", "HANGUL SYLLABLE HIH" },
    {0xd800, "", "", "<lead surrogate-D800>" },
    {0xdc00, "", "", "<trail surrogate-DC00>" },
    {0xff08, "FULLWIDTH LEFT PARENTHESIS", "FULLWIDTH OPENING PARENTHESIS", "FULLWIDTH LEFT PARENTHESIS" },
    {0xffe5, "FULLWIDTH YEN SIGN", "", "FULLWIDTH YEN SIGN" },
    {0xffff, "", "", "<noncharacter-FFFF>" },
    {0x23456, "CJK UNIFIED IDEOGRAPH-23456", "", "CJK UNIFIED IDEOGRAPH-23456" }
};

static UBool
enumCharNamesFn(void *context,
                UChar32 code, UCharNameChoice nameChoice,
                const char *name, int32_t length) {
    int32_t *pCount=(int32_t *)context;
    int i;

    if(length<=0 || length!=(int32_t)strlen(name)) {
        /* should not be called with an empty string or invalid length */
        log_err("u_enumCharName(0x%lx)=%s but length=%ld\n", name, length);
        return TRUE;
    }

    ++*pCount;
    for(i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        if(code==(UChar32)names[i].code) {
            switch (nameChoice) {
                case U_EXTENDED_CHAR_NAME:
                    if(0!=strcmp(name, names[i].extName)) {
                        log_err("u_enumCharName(0x%lx - Extended)=%s instead of %s\n", code, name, names[i].extName);
                    }
                    break;
                case U_UNICODE_CHAR_NAME:
                    if(0!=strcmp(name, names[i].name)) {
                        log_err("u_enumCharName(0x%lx)=%s instead of %s\n", code, name, names[i].name);
                    }
                    break;
                case U_UNICODE_10_CHAR_NAME:
                    if(names[i].oldName[0]==0 || 0!=strcmp(name, names[i].oldName)) {
                        log_err("u_enumCharName(0x%lx - 1.0)=%s instead of %s\n", code, name, names[i].oldName);
                    }
                    break;
                case U_CHAR_NAME_CHOICE_COUNT:
                    break;
            }
            break;
        }
    }
    return TRUE;
}

struct enumExtCharNamesContext {
    uint32_t length;
    int32_t last;
};

static UBool
enumExtCharNamesFn(void *context,
                UChar32 code, UCharNameChoice nameChoice,
                const char *name, int32_t length) {
    struct enumExtCharNamesContext *ecncp = (struct enumExtCharNamesContext *) context;

    if (ecncp->last != (int32_t) code - 1) {
        if (ecncp->last < 0) {
            log_err("u_enumCharName(0x%lx - Ext) after u_enumCharName(0x%lx - Ext) instead of u_enumCharName(0x%lx - Ext)\n", code, ecncp->last, ecncp->last + 1);
        } else {
            log_err("u_enumCharName(0x%lx - Ext) instead of u_enumCharName(0x0 - Ext)\n", code);
        }
    }
    ecncp->last = (int32_t) code;

    if (!*name) {
        log_err("u_enumCharName(0x%lx - Ext) should not be an empty string\n", code);
    }

    return enumCharNamesFn(&ecncp->length, code, nameChoice, name, length);
}

/**
 * This can be made more efficient by moving it into putil.c and having
 * it directly access the ebcdic translation tables.
 * TODO: If we get this method in putil.c, then delete it from here.
 */
static UChar
u_charToUChar(char c) {
    UChar uc;
    u_charsToUChars(&c, &uc, 1);
    return uc;
}

static void
TestCharNames() {
    static char name[80];
    UErrorCode errorCode=U_ZERO_ERROR;
    struct enumExtCharNamesContext extContext;
    int32_t length;
    UChar32 c;
    int i;

    log_verbose("Testing uprv_getMaxCharNameLength()\n");
    length=uprv_getMaxCharNameLength();
    if(length==0) {
        /* no names data available */
        return;
    }
    if(length<83) { /* Unicode 3.2 max char name length */
        log_err("uprv_getMaxCharNameLength()=%d is too short");
    }
    /* ### TODO same tests for max ISO comment length as for max name length */

    log_verbose("Testing u_charName()\n");
    for(i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        /* modern Unicode character name */
        length=u_charName(names[i].code, U_UNICODE_CHAR_NAME, name, sizeof(name), &errorCode);
        if(U_FAILURE(errorCode)) {
            log_err("u_charName(0x%lx) error %s\n", names[i].code, u_errorName(errorCode));
            return;
        }
        if(length<0 || 0!=strcmp(name, names[i].name) || length!=(uint16_t)strlen(name)) {
            log_err("u_charName(0x%lx) gets: %s (length %ld) instead of: %s\n", names[i].code, name, length, names[i].name);
        }

        /* find the modern name */
        if (*names[i].name) {
            c=u_charFromName(U_UNICODE_CHAR_NAME, names[i].name, &errorCode);
            if(U_FAILURE(errorCode)) {
                log_err("u_charFromName(%s) error %s\n", names[i].name, u_errorName(errorCode));
                return;
            }
            if(c!=(UChar32)names[i].code) {
                log_err("u_charFromName(%s) gets 0x%lx instead of 0x%lx\n", names[i].name, c, names[i].code);
            }
        }

        /* Unicode 1.0 character name */
        length=u_charName(names[i].code, U_UNICODE_10_CHAR_NAME, name, sizeof(name), &errorCode);
        if(U_FAILURE(errorCode)) {
            log_err("u_charName(0x%lx - 1.0) error %s\n", names[i].code, u_errorName(errorCode));
            return;
        }
        if(length<0 || (length>0 && 0!=strcmp(name, names[i].oldName)) || length!=(uint16_t)strlen(name)) {
            log_err("u_charName(0x%lx - 1.0) gets %s length %ld instead of nothing or %s\n", names[i].code, name, length, names[i].oldName);
        }

        /* find the Unicode 1.0 name if it is stored (length>0 means that we could read it) */
        if(names[i].oldName[0]!=0 /* && length>0 */) {
            c=u_charFromName(U_UNICODE_10_CHAR_NAME, names[i].oldName, &errorCode);
            if(U_FAILURE(errorCode)) {
                log_err("u_charFromName(%s - 1.0) error %s\n", names[i].oldName, u_errorName(errorCode));
                return;
            }
            if(c!=(UChar32)names[i].code) {
                log_err("u_charFromName(%s - 1.0) gets 0x%lx instead of 0x%lx\n", names[i].oldName, c, names[i].code);
            }
        }
    }

    /* test u_enumCharNames() */
    length=0;
    errorCode=U_ZERO_ERROR;
    u_enumCharNames(UCHAR_MIN_VALUE, UCHAR_MAX_VALUE + 1, enumCharNamesFn, &length, U_UNICODE_CHAR_NAME, &errorCode);
    if(U_FAILURE(errorCode) || length<94140) {
        log_err("u_enumCharNames(%ld..%lx) error %s names count=%ld\n", UCHAR_MIN_VALUE, UCHAR_MAX_VALUE, u_errorName(errorCode), length);
    }

    extContext.length = 0;
    extContext.last = -1;
    errorCode=U_ZERO_ERROR;
    u_enumCharNames(UCHAR_MIN_VALUE, UCHAR_MAX_VALUE + 1, enumExtCharNamesFn, &extContext, U_EXTENDED_CHAR_NAME, &errorCode);
    if(U_FAILURE(errorCode) || extContext.length<UCHAR_MAX_VALUE + 1) {
        log_err("u_enumCharNames(%ld..0x%lx - Extended) error %s names count=%ld\n", UCHAR_MIN_VALUE, UCHAR_MAX_VALUE + 1, u_errorName(errorCode), extContext.length);
    }

    /* test that u_charFromName() uppercases the input name, i.e., works with mixed-case names (new in 2.0) */
    if(0x61!=u_charFromName(U_UNICODE_CHAR_NAME, "LATin smALl letTER A", &errorCode)) {
        log_err("u_charFromName(U_UNICODE_CHAR_NAME, \"LATin smALl letTER A\") did not find U+0061 (%s)\n", u_errorName(errorCode));
    }

    /* Test getCharNameCharacters */
    if(!QUICK) {
        enum { BUFSIZE = 256 };
        UErrorCode ec = U_ZERO_ERROR;
        char buf[BUFSIZE];
        int32_t i, maxLength;
        UChar32 cp;
        UChar pat[BUFSIZE], dumbPat[BUFSIZE];
        int32_t l1, l2;
        UBool map[256];
        UBool ok;

        USet* set = uset_open(1, 0); /* empty set */
        USet* dumb = uset_open(1, 0); /* empty set */

        /*
         * uprv_getCharNameCharacters() will likely return more lowercase
         * letters than actual character names contain because
         * it includes all the characters in lowercased names of
         * general categories, for the full possible set of extended names.
         */
        uprv_getCharNameCharacters(set);

        /* build set the dumb (but sure-fire) way */
        for (i=0; i<256; ++i) map[i] = FALSE;

        maxLength=0;
        for (cp=0; cp<0x110000; ++cp) {
            int32_t len = u_charName(cp, U_EXTENDED_CHAR_NAME,
                                     buf, BUFSIZE, &ec);
            if (U_FAILURE(ec)) {
                log_err("FAIL: u_charName failed when it shouldn't\n");
                uset_close(set);
                uset_close(dumb);
                return;
            }
            if(len>maxLength) {
                maxLength=len;
            }

            for (i=0; i<len; ++i) {
                if (!map[(uint8_t) buf[i]]) {
                    uset_add(dumb, (UChar32)u_charToUChar(buf[i]));
                    map[(uint8_t) buf[i]] = TRUE;
                }
            }
        }

        length=uprv_getMaxCharNameLength();
        if(length!=maxLength) {
            log_err("uprv_getMaxCharNameLength()=%d differs from the maximum length %d of all extended names\n",
                    length, maxLength);
        }

        /* compare the sets.  Where is my uset_equals?!! */
        ok=TRUE;
        for(i=0; i<256; ++i) {
            if(uset_contains(set, i)!=uset_contains(dumb, i)) {
                if(0x61<=i && i<=0x7a /* a-z */ && uset_contains(set, i) && !uset_contains(dumb, i)) {
                    /* ignore lowercase a-z that are in set but not in dumb */
                    ok=TRUE;
                } else {
                    ok=FALSE;
                    break;
                }
            }
        }

        l1 = uset_toPattern(set, pat, BUFSIZE, TRUE, &ec);
        l2 = uset_toPattern(dumb, dumbPat, BUFSIZE, TRUE, &ec);
        if (U_FAILURE(ec)) {
            log_err("FAIL: uset_toPattern failed when it shouldn't\n");
            uset_close(set);
            uset_close(dumb);
            return;
        }

        if (l1 >= BUFSIZE) {
            l1 = BUFSIZE-1;
            pat[l1] = 0;
        }
        if (l2 >= BUFSIZE) {
            l2 = BUFSIZE-1;
            dumbPat[l2] = 0;
        }

        if (!ok) {
            char c1[256], c2[256];
            u_UCharsToChars(pat, c1, l1);
            u_UCharsToChars(dumbPat, c2, l2);
            c1[l1] = c2[l2] = 0;
            log_err("FAIL: uprv_getCharNameCharacters() returned %s, expected %s (too many lowercase a-z are ok)\n",
                    c1, c2);
        } else {
            char c1[256];
            u_UCharsToChars(pat, c1, l1);
            c1[l1] = 0;
            log_verbose("Ok: uprv_getCharNameCharacters() returned %s\n", c1);
        }

        uset_close(set);
        uset_close(dumb);
    }

    /* ### TODO: test error cases and other interesting things */
}

/* test u_isMirrored() and u_charMirror() ----------------------------------- */

static void
TestMirroring() {
    log_verbose("Testing u_isMirrored()\n");
    if(!(u_isMirrored(0x28) && u_isMirrored(0xbb) && u_isMirrored(0x2045) && u_isMirrored(0x232a) &&
         !u_isMirrored(0x27) && !u_isMirrored(0x61) && !u_isMirrored(0x284) && !u_isMirrored(0x3400)
        )
    ) {
        log_err("u_isMirrored() does not work correctly\n");
    }

    log_verbose("Testing u_charMirror()\n");
    if(!(u_charMirror(0x3c)==0x3e && u_charMirror(0x5d)==0x5b && u_charMirror(0x208d)==0x208e && u_charMirror(0x3017)==0x3016 &&
         u_charMirror(0x2e)==0x2e && u_charMirror(0x6f3)==0x6f3 && u_charMirror(0x301c)==0x301c && u_charMirror(0xa4ab)==0xa4ab 
         )
    ) {
        log_err("u_charMirror() does not work correctly\n");
    }
}


struct RunTestData
{
    const char *runText;
    UScriptCode runCode;
};

typedef struct RunTestData RunTestData;

static void
CheckScriptRuns(UScriptRun *scriptRun, int32_t *runStarts, const RunTestData *testData, int32_t nRuns,
                const char *prefix)
{
    int32_t run, runStart, runLimit;
    UScriptCode runCode;

    /* iterate over all the runs */
    run = 0;
    while (uscript_nextRun(scriptRun, &runStart, &runLimit, &runCode)) {
        if (runStart != runStarts[run]) {
            log_err("%s: incorrect start offset for run %d: expected %d, got %d\n",
                prefix, run, runStarts[run], runStart);
        }

        if (runLimit != runStarts[run + 1]) {
            log_err("%s: incorrect limit offset for run %d: expected %d, got %d\n",
                prefix, run, runStarts[run + 1], runLimit);
        }

        if (runCode != testData[run].runCode) {
            log_err("%s: incorrect script for run %d: expected \"%s\", got \"%s\"\n",
                prefix, run, uscript_getName(testData[run].runCode), uscript_getName(runCode));
        }
        
        run += 1;

        /* stop when we've seen all the runs we expect to see */
        if (run >= nRuns) {
            break;
        }
    }

    /* Complain if we didn't see then number of runs we expected */
    if (run != nRuns) {
        log_err("%s: incorrect number of runs: expected %d, got %d\n", prefix, run, nRuns);
    }
}

static void
TestUScriptRunAPI()
{
    static const RunTestData testData[] = {
        {"\\u0020\\u0946\\u0939\\u093F\\u0928\\u094D\\u0926\\u0940\\u0020", USCRIPT_DEVANAGARI},
        {"\\u0627\\u0644\\u0639\\u0631\\u0628\\u064A\\u0629\\u0020", USCRIPT_ARABIC},
        {"\\u0420\\u0443\\u0441\\u0441\\u043A\\u0438\\u0439\\u0020", USCRIPT_CYRILLIC},
        {"English (", USCRIPT_LATIN},
        {"\\u0E44\\u0E17\\u0E22", USCRIPT_THAI},
        {") ", USCRIPT_LATIN},
        {"\\u6F22\\u5B75", USCRIPT_HAN},
        {"\\u3068\\u3072\\u3089\\u304C\\u306A\\u3068", USCRIPT_HIRAGANA},
        {"\\u30AB\\u30BF\\u30AB\\u30CA", USCRIPT_KATAKANA},
        {"\\U00010400\\U00010401\\U00010402\\U00010403", USCRIPT_DESERET}
    };

    int32_t nTestRuns = sizeof testData / sizeof testData[0];

    UChar testString[1024];
    int32_t runStarts[256];

    int32_t run, stringLimit;
    UScriptRun *scriptRun = NULL;
    UErrorCode err;

    /*
     * Fill in the test string and the runStarts array.
     */
    stringLimit = 0;
    for (run = 0; run < nTestRuns; run += 1) {
        runStarts[run] = stringLimit;
        stringLimit += u_unescape(testData[run].runText, &testString[stringLimit], 1024 - stringLimit);
        /*stringLimit -= 1;*/
    }

    /* The limit of the last run */ 
    runStarts[nTestRuns] = stringLimit;

    /*
     * Make sure that calling uscript_OpenRun with a NULL text pointer
     * and a non-zero text length returns the correct error.
     */
    err = U_ZERO_ERROR;
    scriptRun = uscript_openRun(NULL, stringLimit, &err);

    if (err != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("uscript_openRun(NULL, stringLimit, &err) returned %s instead of U_ILLEGAL_ARGUMENT_ERROR.\n", u_errorName(err));
    }

    if (scriptRun != NULL) {
        log_err("uscript_openRun(NULL, stringLimit, &err) returned a non-NULL result.\n");
        uscript_closeRun(scriptRun);
    }

    /*
     * Make sure that calling uscript_OpenRun with a non-NULL text pointer
     * and a zero text length returns the correct error.
     */
    err = U_ZERO_ERROR;
    scriptRun = uscript_openRun(testString, 0, &err);

    if (err != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("uscript_openRun(testString, 0, &err) returned %s instead of U_ILLEGAL_ARGUMENT_ERROR.\n", u_errorName(err));
    }

    if (scriptRun != NULL) {
        log_err("uscript_openRun(testString, 0, &err) returned a non-NULL result.\n");
        uscript_closeRun(scriptRun);
    }

    /*
     * Make sure that calling uscript_openRun with a NULL text pointer
     * and a zero text length doesn't return an error.
     */
    err = U_ZERO_ERROR;
    scriptRun = uscript_openRun(NULL, 0, &err);

    if (U_FAILURE(err)) {
        log_err("Got error %s from uscript_openRun(NULL, 0, &err)\n", u_errorName(err));
    }

    /* Make sure that the empty iterator doesn't find any runs */
    if (uscript_nextRun(scriptRun, NULL, NULL, NULL)) {
        log_err("uscript_nextRun(...) returned TRUE for an empty iterator.\n");
    }

    /*
     * Make sure that calling uscript_setRunText with a NULL text pointer
     * and a non-zero text length returns the correct error.
     */
    err = U_ZERO_ERROR;
    uscript_setRunText(scriptRun, NULL, stringLimit, &err);

    if (err != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("uscript_setRunText(scriptRun, NULL, stringLimit, &err) returned %s instead of U_ILLEGAL_ARGUMENT_ERROR.\n", u_errorName(err));
    }

    /*
     * Make sure that calling uscript_OpenRun with a non-NULL text pointer
     * and a zero text length returns the correct error.
     */
    err = U_ZERO_ERROR;
    uscript_setRunText(scriptRun, testString, 0, &err);

    if (err != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("uscript_setRunText(scriptRun, testString, 0, &err) returned %s instead of U_ILLEGAL_ARGUMENT_ERROR.\n", u_errorName(err));
    }

    /*
     * Now call uscript_setRunText on the empty iterator
     * and make sure that it works.
     */
    err = U_ZERO_ERROR;
    uscript_setRunText(scriptRun, testString, stringLimit, &err);

    if (U_FAILURE(err)) {
        log_err("Got error %s from uscript_setRunText(...)\n", u_errorName(err));
    } else {
        CheckScriptRuns(scriptRun, runStarts, testData, nTestRuns, "uscript_setRunText");
    }

    uscript_closeRun(scriptRun);

    /* 
     * Now open an interator over the testString
     * using uscript_openRun and make sure that it works
     */
    scriptRun = uscript_openRun(testString, stringLimit, &err);

    if (U_FAILURE(err)) {
        log_err("Got error %s from uscript_openRun(...)\n", u_errorName(err));
    } else {
        CheckScriptRuns(scriptRun, runStarts, testData, nTestRuns, "uscript_openRun");
    }

    /* Now reset the iterator, and make sure
     * that it still works.
     */
    uscript_resetRun(scriptRun);

    CheckScriptRuns(scriptRun, runStarts, testData, nTestRuns, "uscript_resetRun");

    /* Close the iterator */
    uscript_closeRun(scriptRun);
}

/* test additional, non-core properties */
static void
TestAdditionalProperties() {
    /* test data for u_charAge() */
    static const struct {
        UChar32 c;
        UVersionInfo version;
    } charAges[]={
        {0x41,    { 1, 1, 0, 0 }},
        {0xffff,  { 1, 1, 0, 0 }},
        {0x20ab,  { 2, 0, 0, 0 }},
        {0x2fffe, { 2, 0, 0, 0 }},
        {0x20ac,  { 2, 1, 0, 0 }},
        {0xfb1d,  { 3, 0, 0, 0 }},
        {0x3f4,   { 3, 1, 0, 0 }},
        {0x10300, { 3, 1, 0, 0 }},
        {0x220,   { 3, 2, 0, 0 }},
        {0xff60,  { 3, 2, 0, 0 }}
    };

    /* test data for u_hasBinaryProperty() */
    static int32_t
    props[][3]={ /* code point, property, value */
        { 0x0627, UCHAR_ALPHABETIC, TRUE },
        { 0x1034a, UCHAR_ALPHABETIC, TRUE },
        { 0x2028, UCHAR_ALPHABETIC, FALSE },

        { 0x0066, UCHAR_ASCII_HEX_DIGIT, TRUE },
        { 0x0067, UCHAR_ASCII_HEX_DIGIT, FALSE },

        { 0x202c, UCHAR_BIDI_CONTROL, TRUE },
        { 0x202f, UCHAR_BIDI_CONTROL, FALSE },

        { 0x003c, UCHAR_BIDI_MIRRORED, TRUE },
        { 0x003d, UCHAR_BIDI_MIRRORED, FALSE },

        { 0x058a, UCHAR_DASH, TRUE },
        { 0x007e, UCHAR_DASH, FALSE },

        { 0x0c4d, UCHAR_DIACRITIC, TRUE },
        { 0x3000, UCHAR_DIACRITIC, FALSE },

        { 0x0e46, UCHAR_EXTENDER, TRUE },
        { 0x0020, UCHAR_EXTENDER, FALSE },

#if !UCONFIG_NO_NORMALIZATION
        { 0xfb1d, UCHAR_FULL_COMPOSITION_EXCLUSION, TRUE },
        { 0x1d15f, UCHAR_FULL_COMPOSITION_EXCLUSION, TRUE },
        { 0xfb1e, UCHAR_FULL_COMPOSITION_EXCLUSION, FALSE },
#endif

        { 0x0044, UCHAR_HEX_DIGIT, TRUE },
        { 0xff46, UCHAR_HEX_DIGIT, TRUE },
        { 0x0047, UCHAR_HEX_DIGIT, FALSE },

        { 0x30fb, UCHAR_HYPHEN, TRUE },
        { 0xfe58, UCHAR_HYPHEN, FALSE },

        { 0x2172, UCHAR_ID_CONTINUE, TRUE },
        { 0x0307, UCHAR_ID_CONTINUE, TRUE },
        { 0x005c, UCHAR_ID_CONTINUE, FALSE },

        { 0x2172, UCHAR_ID_START, TRUE },
        { 0x007a, UCHAR_ID_START, TRUE },
        { 0x0039, UCHAR_ID_START, FALSE },

        { 0x4db5, UCHAR_IDEOGRAPHIC, TRUE },
        { 0x2f999, UCHAR_IDEOGRAPHIC, TRUE },
        { 0x2f99, UCHAR_IDEOGRAPHIC, FALSE },

        { 0x200c, UCHAR_JOIN_CONTROL, TRUE },
        { 0x2029, UCHAR_JOIN_CONTROL, FALSE },

        { 0x1d7bc, UCHAR_LOWERCASE, TRUE },
        { 0x0345, UCHAR_LOWERCASE, TRUE },
        { 0x0030, UCHAR_LOWERCASE, FALSE },

        { 0x1d7a9, UCHAR_MATH, TRUE },
        { 0x2135, UCHAR_MATH, TRUE },
        { 0x0062, UCHAR_MATH, FALSE },

        { 0xfde1, UCHAR_NONCHARACTER_CODE_POINT, TRUE },
        { 0x10ffff, UCHAR_NONCHARACTER_CODE_POINT, TRUE },
        { 0x10fffd, UCHAR_NONCHARACTER_CODE_POINT, FALSE },

        { 0x0022, UCHAR_QUOTATION_MARK, TRUE },
        { 0xff62, UCHAR_QUOTATION_MARK, TRUE },
        { 0xd840, UCHAR_QUOTATION_MARK, FALSE },

        { 0x061f, UCHAR_TERMINAL_PUNCTUATION, TRUE },
        { 0xe003f, UCHAR_TERMINAL_PUNCTUATION, FALSE },

        { 0x1d44a, UCHAR_UPPERCASE, TRUE },
        { 0x2162, UCHAR_UPPERCASE, TRUE },
        { 0x0345, UCHAR_UPPERCASE, FALSE },

        { 0x0020, UCHAR_WHITE_SPACE, TRUE },
        { 0x202f, UCHAR_WHITE_SPACE, TRUE },
        { 0x3001, UCHAR_WHITE_SPACE, FALSE },

        { 0x0711, UCHAR_XID_CONTINUE, TRUE },
        { 0x1d1aa, UCHAR_XID_CONTINUE, TRUE },
        { 0x007c, UCHAR_XID_CONTINUE, FALSE },

        { 0x16ee, UCHAR_XID_START, TRUE },
        { 0x23456, UCHAR_XID_START, TRUE },
        { 0x1d1aa, UCHAR_XID_START, FALSE },

        /*
         * Version break:
         * The following properties are only supported starting with the
         * Unicode version indicated in the second field.
         */
        { -1, 0x32, 0 },

        { 0x180c, UCHAR_DEFAULT_IGNORABLE_CODE_POINT, TRUE },
        { 0xfe02, UCHAR_DEFAULT_IGNORABLE_CODE_POINT, TRUE },
        { 0x1801, UCHAR_DEFAULT_IGNORABLE_CODE_POINT, FALSE },

        { 0x0341, UCHAR_DEPRECATED, TRUE },
        { 0xe0041, UCHAR_DEPRECATED, FALSE },

        { 0x00a0, UCHAR_GRAPHEME_BASE, TRUE },
        { 0x0a4d, UCHAR_GRAPHEME_BASE, FALSE },
        { 0xff9f, UCHAR_GRAPHEME_BASE, TRUE },      /* changed from Unicode 3.2 to 4 */

        { 0x0300, UCHAR_GRAPHEME_EXTEND, TRUE },
        { 0xff9f, UCHAR_GRAPHEME_EXTEND, FALSE },   /* changed from Unicode 3.2 to 4 */
        { 0x0603, UCHAR_GRAPHEME_EXTEND, FALSE },

        { 0x0a4d, UCHAR_GRAPHEME_LINK, TRUE },
        { 0xff9f, UCHAR_GRAPHEME_LINK, FALSE },

        { 0x2ff7, UCHAR_IDS_BINARY_OPERATOR, TRUE },
        { 0x2ff3, UCHAR_IDS_BINARY_OPERATOR, FALSE },

        { 0x2ff3, UCHAR_IDS_TRINARY_OPERATOR, TRUE },
        { 0x2f03, UCHAR_IDS_TRINARY_OPERATOR, FALSE },

        { 0x0ec1, UCHAR_LOGICAL_ORDER_EXCEPTION, TRUE },
        { 0xdcba, UCHAR_LOGICAL_ORDER_EXCEPTION, FALSE },

        { 0x2e9b, UCHAR_RADICAL, TRUE },
        { 0x4e00, UCHAR_RADICAL, FALSE },

        { 0x012f, UCHAR_SOFT_DOTTED, TRUE },
        { 0x0049, UCHAR_SOFT_DOTTED, FALSE },

        { 0xfa11, UCHAR_UNIFIED_IDEOGRAPH, TRUE },
        { 0xfa12, UCHAR_UNIFIED_IDEOGRAPH, FALSE },

        /* enum/integer type properties */

        /* UCHAR_BIDI_CLASS tested for assigned characters in TestUnicodeData() */
        /* test default Bidi classes for unassigned code points */
        { 0x0590, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x05a2, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x05ed, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x07f2, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x08ba, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0xfb37, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0xfb42, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x10806, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x10909, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },
        { 0x10fe4, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT },

        { 0x0606, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0x061c, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0x063f, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0x070e, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0x0775, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0xfbc2, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0xfd90, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },
        { 0xfefe, UCHAR_BIDI_CLASS, U_RIGHT_TO_LEFT_ARABIC },

        { 0x02AF, UCHAR_BLOCK, UBLOCK_IPA_EXTENSIONS },
        { 0x0C4E, UCHAR_BLOCK, UBLOCK_TELUGU },
        { 0x155A, UCHAR_BLOCK, UBLOCK_UNIFIED_CANADIAN_ABORIGINAL_SYLLABICS },
        { 0x1717, UCHAR_BLOCK, UBLOCK_TAGALOG },
        { 0x1AFF, UCHAR_BLOCK, UBLOCK_NO_BLOCK },
        { 0x3040, UCHAR_BLOCK, UBLOCK_HIRAGANA },
        { 0x1D0FF, UCHAR_BLOCK, UBLOCK_BYZANTINE_MUSICAL_SYMBOLS },
        { 0x10D0FF, UCHAR_BLOCK, UBLOCK_SUPPLEMENTARY_PRIVATE_USE_AREA_B },
        { 0xEFFFF, UCHAR_BLOCK, UBLOCK_NO_BLOCK },

        /* UCHAR_CANONICAL_COMBINING_CLASS tested for assigned characters in TestUnicodeData() */
        { 0xd7d7, UCHAR_CANONICAL_COMBINING_CLASS, 0 },

        { 0x00A0, UCHAR_DECOMPOSITION_TYPE, U_DT_NOBREAK },
        { 0x00A8, UCHAR_DECOMPOSITION_TYPE, U_DT_COMPAT },
        { 0x00bf, UCHAR_DECOMPOSITION_TYPE, U_DT_NONE },
        { 0x00c0, UCHAR_DECOMPOSITION_TYPE, U_DT_CANONICAL },
        { 0x1E9B, UCHAR_DECOMPOSITION_TYPE, U_DT_CANONICAL },
        { 0xBCDE, UCHAR_DECOMPOSITION_TYPE, U_DT_CANONICAL },
        { 0xFB5D, UCHAR_DECOMPOSITION_TYPE, U_DT_MEDIAL },
        { 0x1D736, UCHAR_DECOMPOSITION_TYPE, U_DT_FONT },
        { 0xe0033, UCHAR_DECOMPOSITION_TYPE, U_DT_NONE },

        { 0x0009, UCHAR_EAST_ASIAN_WIDTH, U_EA_NEUTRAL },
        { 0x0020, UCHAR_EAST_ASIAN_WIDTH, U_EA_NARROW },
        { 0x00B1, UCHAR_EAST_ASIAN_WIDTH, U_EA_AMBIGUOUS },
        { 0x20A9, UCHAR_EAST_ASIAN_WIDTH, U_EA_HALFWIDTH },
        { 0x2FFB, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE },
        { 0x3000, UCHAR_EAST_ASIAN_WIDTH, U_EA_FULLWIDTH },
        { 0x35bb, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE },
        { 0x58bd, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE },
        { 0xD7A3, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE },
        { 0xEEEE, UCHAR_EAST_ASIAN_WIDTH, U_EA_AMBIGUOUS },
        { 0x1D198, UCHAR_EAST_ASIAN_WIDTH, U_EA_NEUTRAL },
        { 0x20000, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE },
        { 0x2F8C7, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE },
        { 0x3a5bd, UCHAR_EAST_ASIAN_WIDTH, U_EA_WIDE }, /* plane 3 got default W values in Unicode 4 */
        { 0x5a5bd, UCHAR_EAST_ASIAN_WIDTH, U_EA_NEUTRAL },
        { 0xFEEEE, UCHAR_EAST_ASIAN_WIDTH, U_EA_AMBIGUOUS },
        { 0x10EEEE, UCHAR_EAST_ASIAN_WIDTH, U_EA_AMBIGUOUS },

        /* UCHAR_GENERAL_CATEGORY tested for assigned characters in TestUnicodeData() */
        { 0xd7d7, UCHAR_GENERAL_CATEGORY, 0 },

        { 0x0444, UCHAR_JOINING_GROUP, U_JG_NO_JOINING_GROUP },
        { 0x0639, UCHAR_JOINING_GROUP, U_JG_AIN },
        { 0x072A, UCHAR_JOINING_GROUP, U_JG_DALATH_RISH },
        { 0x0647, UCHAR_JOINING_GROUP, U_JG_HEH },
        { 0x06C1, UCHAR_JOINING_GROUP, U_JG_HEH_GOAL },
        { 0x06C3, UCHAR_JOINING_GROUP, U_JG_HAMZA_ON_HEH_GOAL },

        { 0x200C, UCHAR_JOINING_TYPE, U_JT_NON_JOINING },
        { 0x200D, UCHAR_JOINING_TYPE, U_JT_JOIN_CAUSING },
        { 0x0639, UCHAR_JOINING_TYPE, U_JT_DUAL_JOINING },
        { 0x0640, UCHAR_JOINING_TYPE, U_JT_JOIN_CAUSING },
        { 0x06C3, UCHAR_JOINING_TYPE, U_JT_RIGHT_JOINING },
        { 0x0300, UCHAR_JOINING_TYPE, U_JT_TRANSPARENT },
        { 0x070F, UCHAR_JOINING_TYPE, U_JT_TRANSPARENT },
        { 0xe0033, UCHAR_JOINING_TYPE, U_JT_TRANSPARENT },

        /* TestUnicodeData() verifies that no assigned character has "XX" (unknown) */
        { 0xe7e7, UCHAR_LINE_BREAK, U_LB_UNKNOWN },
        { 0x10fffd, UCHAR_LINE_BREAK, U_LB_UNKNOWN },
        { 0x0028, UCHAR_LINE_BREAK, U_LB_OPEN_PUNCTUATION },
        { 0x232A, UCHAR_LINE_BREAK, U_LB_CLOSE_PUNCTUATION },
        { 0x3401, UCHAR_LINE_BREAK, U_LB_IDEOGRAPHIC },
        { 0x4e02, UCHAR_LINE_BREAK, U_LB_IDEOGRAPHIC },
        { 0xac03, UCHAR_LINE_BREAK, U_LB_IDEOGRAPHIC },
        { 0x20004, UCHAR_LINE_BREAK, U_LB_IDEOGRAPHIC },
        { 0xf905, UCHAR_LINE_BREAK, U_LB_IDEOGRAPHIC },
        { 0xdb7e, UCHAR_LINE_BREAK, U_LB_SURROGATE },
        { 0xdbfd, UCHAR_LINE_BREAK, U_LB_SURROGATE },
        { 0xdffc, UCHAR_LINE_BREAK, U_LB_SURROGATE },
        { 0x2762, UCHAR_LINE_BREAK, U_LB_EXCLAMATION },
        { 0x002F, UCHAR_LINE_BREAK, U_LB_BREAK_SYMBOLS },
        { 0x1D49C, UCHAR_LINE_BREAK, U_LB_ALPHABETIC },
        { 0x1731, UCHAR_LINE_BREAK, U_LB_ALPHABETIC },

        /* UCHAR_NUMERIC_TYPE tested in TestNumericProperties() */

        /* UCHAR_SCRIPT tested in TestUScriptCodeAPI() */

        { 0x1100, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LEADING_JAMO },
        { 0x1111, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LEADING_JAMO },
        { 0x1159, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LEADING_JAMO },
        { 0x115f, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LEADING_JAMO },

        { 0x1160, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_VOWEL_JAMO },
        { 0x1161, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_VOWEL_JAMO },
        { 0x1172, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_VOWEL_JAMO },
        { 0x11a2, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_VOWEL_JAMO },

        { 0x11a8, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_TRAILING_JAMO },
        { 0x11b8, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_TRAILING_JAMO },
        { 0x11c8, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_TRAILING_JAMO },
        { 0x11f9, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_TRAILING_JAMO },

        { 0x115a, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },
        { 0x115e, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },
        { 0x11a3, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },
        { 0x11a7, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },
        { 0x11fa, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },
        { 0x11ff, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },

        { 0xac00, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LV_SYLLABLE },
        { 0xac1c, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LV_SYLLABLE },
        { 0xc5ec, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LV_SYLLABLE },
        { 0xd788, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LV_SYLLABLE },

        { 0xac01, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LVT_SYLLABLE },
        { 0xac1b, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LVT_SYLLABLE },
        { 0xac1d, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LVT_SYLLABLE },
        { 0xc5ee, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LVT_SYLLABLE },
        { 0xd7a3, UCHAR_HANGUL_SYLLABLE_TYPE, U_HST_LVT_SYLLABLE },

        { 0xd7a4, UCHAR_HANGUL_SYLLABLE_TYPE, 0 },

        /* undefined UProperty values */
        { 0x61, 0x4a7, 0 },
        { 0x234bc, 0x15ed, 0 }
    };

    UVersionInfo version;
    UChar32 c;
    int32_t i, result, uVersion;
    UProperty which;

    /* what is our Unicode version? */
    u_getUnicodeVersion(version);
    uVersion=(version[0]<<4)|version[1]; /* major/minor version numbers */

    u_charAge(0x20, version);
    if(version[0]==0) {
        /* no additional properties available */
        log_err("TestAdditionalProperties: no additional properties available, not tested\n");
        return;
    }

    /* test u_charAge() */
    for(i=0; i<sizeof(charAges)/sizeof(charAges[0]); ++i) {
        u_charAge(charAges[i].c, version);
        if(0!=memcmp(version, charAges[i].version, sizeof(UVersionInfo))) {
            log_err("error: u_charAge(U+%04lx)={ %u, %u, %u, %u } instead of { %u, %u, %u, %u }\n",
                charAges[i].c,
                version[0], version[1], version[2], version[3],
                charAges[i].version[0], charAges[i].version[1], charAges[i].version[2], charAges[i].version[3]);
        }
    }

    if( u_getIntPropertyMinValue(UCHAR_DASH)!=0 ||
        u_getIntPropertyMinValue(UCHAR_BIDI_CLASS)!=0 ||
        u_getIntPropertyMinValue(UCHAR_BLOCK)!=0 ||   /* j2478 */
        u_getIntPropertyMinValue(UCHAR_SCRIPT)!=0 || /*JB#2410*/
        u_getIntPropertyMinValue(0x2345)!=0
    ) {
        log_err("error: u_getIntPropertyMinValue() wrong\n");
    }

    if( u_getIntPropertyMaxValue(UCHAR_DASH)!=1 ||
        u_getIntPropertyMaxValue(UCHAR_ID_CONTINUE)!=1 ||
        u_getIntPropertyMaxValue(UCHAR_BINARY_LIMIT-1)!=1 ||
        u_getIntPropertyMaxValue(UCHAR_BIDI_CLASS)!=(int32_t)U_CHAR_DIRECTION_COUNT-1 ||
        u_getIntPropertyMaxValue(UCHAR_BLOCK)!=(int32_t)UBLOCK_COUNT-1 ||
        u_getIntPropertyMaxValue(UCHAR_LINE_BREAK)!=(int32_t)U_LB_COUNT-1 ||
        u_getIntPropertyMaxValue(UCHAR_SCRIPT)!=(int32_t)USCRIPT_CODE_LIMIT-1 ||
        u_getIntPropertyMaxValue(0x2345)!=-1 /*JB#2410*/ ||
        u_getIntPropertyMaxValue(UCHAR_DECOMPOSITION_TYPE) != (int32_t) (U_DT_COUNT - 1) ||
        u_getIntPropertyMaxValue(UCHAR_JOINING_GROUP) !=  (int32_t) (U_JG_COUNT -1) ||
        u_getIntPropertyMaxValue(UCHAR_JOINING_TYPE) != (int32_t) (U_JT_COUNT -1) ||
        u_getIntPropertyMaxValue(UCHAR_EAST_ASIAN_WIDTH) != (int32_t) (U_EA_COUNT -1)
    ) {
        log_err("error: u_getIntPropertyMaxValue() wrong\n");
    }

    /* test u_hasBinaryProperty() and u_getIntPropertyValue() */
    for(i=0; i<sizeof(props)/sizeof(props[0]); ++i) {
        if(props[i][0]<0) {
            /* Unicode version break */
            if(uVersion<props[i][1]) {
                break; /* do not test properties that are not yet supported */
            } else {
                continue; /* skip this row */
            }
        }

        c=(UChar32)props[i][0];
        which=(UProperty)props[i][1];

        if(which<UCHAR_INT_START) {
            result=u_hasBinaryProperty(c, which);
            if(result!=props[i][2]) {
                log_err("error: u_hasBinaryProperty(U+%04lx, %d)=%d is wrong (props[%d])\n",
                        c, which, result, i);
            }
        }

        result=u_getIntPropertyValue(c, which);
        if(result!=props[i][2]) {
            log_err("error: u_getIntPropertyValue(U+%04lx, 0x1000+%d)=%d is wrong, should be %d (props[%d])\n",
                    c, (int32_t)which-0x1000, result, props[i][2], i);
        }

        /* test separate functions, too */
        switch((UProperty)props[i][1]) {
        case UCHAR_ALPHABETIC:
            if(u_isUAlphabetic((UChar32)props[i][0])!=(UBool)props[i][2]) {
                log_err("error: u_isUAlphabetic(U+%04lx)=%d is wrong (props[%d])\n",
                        props[i][0], result, i);
            }
            break;
        case UCHAR_LOWERCASE:
            if(u_isULowercase((UChar32)props[i][0])!=(UBool)props[i][2]) {
                log_err("error: u_isULowercase(U+%04lx)=%d is wrong (props[%d])\n",
                        props[i][0], result, i);
            }
            break;
        case UCHAR_UPPERCASE:
            if(u_isUUppercase((UChar32)props[i][0])!=(UBool)props[i][2]) {
                log_err("error: u_isUUppercase(U+%04lx)=%d is wrong (props[%d])\n",
                        props[i][0], result, i);
            }
            break;
        case UCHAR_WHITE_SPACE:
            if(u_isUWhiteSpace((UChar32)props[i][0])!=(UBool)props[i][2]) {
                log_err("error: u_isUWhiteSpace(U+%04lx)=%d is wrong (props[%d])\n",
                        props[i][0], result, i);
            }
            break;
        default:
            break;
        }
    }
}

static void
TestNumericProperties(void) {
    /* see UnicodeData.txt, DerivedNumericValues.txt */
    static const struct {
        UChar32 c;
        int32_t type;
        double numValue;
    } values[]={
        { 0x0F33, U_NT_NUMERIC, -1./2. },
        { 0x0C66, U_NT_DECIMAL, 0 },
        { 0x96f6, U_NT_NUMERIC, 0 },
        { 0x2159, U_NT_NUMERIC, 1./6. },
        { 0x00BD, U_NT_NUMERIC, 1./2. },
        { 0x0031, U_NT_DECIMAL, 1. },
        { 0x4e00, U_NT_NUMERIC, 1. },
        { 0x58f1, U_NT_NUMERIC, 1. },
        { 0x10320, U_NT_NUMERIC, 1. },
        { 0x0F2B, U_NT_NUMERIC, 3./2. },
        { 0x00B2, U_NT_DIGIT, 2. },
        { 0x5f10, U_NT_NUMERIC, 2. },
        { 0x1813, U_NT_DECIMAL, 3. },
        { 0x5f0e, U_NT_NUMERIC, 3. },
        { 0x2173, U_NT_NUMERIC, 4. },
        { 0x8086, U_NT_NUMERIC, 4. },
        { 0x278E, U_NT_DIGIT, 5. },
        { 0x1D7F2, U_NT_DECIMAL, 6. },
        { 0x247A, U_NT_DIGIT, 7. },
        { 0x7396, U_NT_NUMERIC, 9. },
        { 0x1372, U_NT_NUMERIC, 10. },
        { 0x216B, U_NT_NUMERIC, 12. },
        { 0x16EE, U_NT_NUMERIC, 17. },
        { 0x249A, U_NT_NUMERIC, 19. },
        { 0x303A, U_NT_NUMERIC, 30. },
        { 0x5345, U_NT_NUMERIC, 30. },
        { 0x32B2, U_NT_NUMERIC, 37. },
        { 0x1375, U_NT_NUMERIC, 40. },
        { 0x10323, U_NT_NUMERIC, 50. },
        { 0x0BF1, U_NT_NUMERIC, 100. },
        { 0x964c, U_NT_NUMERIC, 100. },
        { 0x217E, U_NT_NUMERIC, 500. },
        { 0x2180, U_NT_NUMERIC, 1000. },
        { 0x4edf, U_NT_NUMERIC, 1000. },
        { 0x2181, U_NT_NUMERIC, 5000. },
        { 0x137C, U_NT_NUMERIC, 10000. },
        { 0x4e07, U_NT_NUMERIC, 10000. },
        { 0x4ebf, U_NT_NUMERIC, 100000000. },
        { 0x5146, U_NT_NUMERIC, 1000000000000. },
        { 0x61, U_NT_NONE, U_NO_NUMERIC_VALUE },
        { 0x3000, U_NT_NONE, U_NO_NUMERIC_VALUE },
        { 0xfffe, U_NT_NONE, U_NO_NUMERIC_VALUE },
        { 0x10301, U_NT_NONE, U_NO_NUMERIC_VALUE },
        { 0xe0033, U_NT_NONE, U_NO_NUMERIC_VALUE },
        { 0x10ffff, U_NT_NONE, U_NO_NUMERIC_VALUE }
    };

    double nv;
    UChar32 c;
    int32_t i, type;

    for(i=0; i<LENGTHOF(values); ++i) {
        c=values[i].c;
        type=u_getIntPropertyValue(c, UCHAR_NUMERIC_TYPE);
        nv=u_getNumericValue(c);

        if(type!=values[i].type) {
            log_err("UCHAR_NUMERIC_TYPE(U+%04lx)=%d should be %d\n", c, type, values[i].type);
        }
        if(0.000001 <= fabs(nv - values[i].numValue)) {
            log_err("u_getNumericValue(U+%04lx)=%g should be %g\n", c, nv, values[i].numValue);
        }
    }
}

/**
 * Test the property names and property value names API.
 */
static void
TestPropertyNames(void) {
    int32_t p, v, choice, rev;

    for (p=0; ; ++p) {
        UBool sawProp = FALSE;
        for (choice=0; ; ++choice) {
            const char* name = u_getPropertyName(p, choice);
            if (name) {
                if (!sawProp) log_verbose("prop 0x%04x+%2d:", p&~0xfff, p&0xfff);
                log_verbose("%d=\"%s\"", choice, name);
                sawProp = TRUE;

                /* test reverse mapping */
                rev = u_getPropertyEnum(name);
                if (rev != p) {
                    log_err("Property round-trip failure: %d -> %s -> %d\n",
                            p, name, rev);
                }
            }
            if (!name && choice>0) break;
        }
        if (sawProp) {
            /* looks like a valid property; check the values */
            const char* pname = u_getPropertyName(p, U_LONG_PROPERTY_NAME);
            int32_t max = 0;
            if (p == UCHAR_CANONICAL_COMBINING_CLASS) {
                max = 255;
            } else if (p == UCHAR_GENERAL_CATEGORY_MASK) {
                /* it's far too slow to iterate all the way up to
                   the real max, U_GC_P_MASK */
                max = U_GC_NL_MASK;
            } else if (p == UCHAR_BLOCK) {
                /* UBlockCodes, unlike other values, start at 1 */
                max = 1;
            }
            log_verbose("\n");
            for (v=-1; ; ++v) {
                UBool sawValue = FALSE;
                for (choice=0; ; ++choice) {
                    const char* vname = u_getPropertyValueName(p, v, choice);
                    if (vname) {
                        if (!sawValue) log_verbose(" %s, value %d:", pname, v);
                        log_verbose("%d=\"%s\"", choice, vname);
                        sawValue = TRUE;

                        /* test reverse mapping */
                        rev = u_getPropertyValueEnum(p, vname);
                        if (rev != v) {
                            log_err("Value round-trip failure (%s): %d -> %s -> %d\n",
                                    pname, v, vname, rev);
                        }
                    }
                    if (!vname && choice>0) break;
                }
                if (sawValue) {
                    log_verbose("\n");
                }
                if (!sawValue && v>=max) break;
            }
        }
        if (!sawProp) {
            if (p>=UCHAR_STRING_LIMIT) {
                break;
            } else if (p>=UCHAR_DOUBLE_LIMIT) {
                p = UCHAR_STRING_START - 1;
            } else if (p>=UCHAR_MASK_LIMIT) {
                p = UCHAR_DOUBLE_START - 1;
            } else if (p>=UCHAR_INT_LIMIT) {
                p = UCHAR_MASK_START - 1;
            } else if (p>=UCHAR_BINARY_LIMIT) {
                p = UCHAR_INT_START - 1;
            }
        }
    }
}

/**
 * Test the property values API.  See JB#2410.
 */
static void
TestPropertyValues(void) {
    int32_t i, p, min, max;
    UErrorCode ec;

    /* Min should be 0 for everything. */
    /* Until JB#2478 is fixed, the one exception is UCHAR_BLOCK. */
    for (p=UCHAR_INT_START; p<UCHAR_INT_LIMIT; ++p) {
        min = u_getIntPropertyMinValue(p);
        if (min != 0) {
            if (p == UCHAR_BLOCK) {
                /* This is okay...for now.  See JB#2487.
                   TODO Update this for JB#2487. */
            } else {
                const char* name;
                name = u_getPropertyName(p, U_LONG_PROPERTY_NAME);
                if (name == NULL) name = "<ERROR>";
                log_err("FAIL: u_getIntPropertyMinValue(%s) = %d, exp. 0\n",
                        name, min);
            }
        }
    }

    if( u_getIntPropertyMinValue(UCHAR_GENERAL_CATEGORY_MASK)!=0 ||
        u_getIntPropertyMaxValue(UCHAR_GENERAL_CATEGORY_MASK)!=-1) {
        log_err("error: u_getIntPropertyMin/MaxValue(UCHAR_GENERAL_CATEGORY_MASK) is wrong\n");
    }

    /* Max should be -1 for invalid properties. */
    max = u_getIntPropertyMaxValue(-1);
    if (max != -1) {
        log_err("FAIL: u_getIntPropertyMaxValue(-1) = %d, exp. -1\n",
                max);
    }

    /* Script should return 0 for an invalid code point. */
    for (i=0; i<2; ++i) {
        int32_t script;
        const char* desc;
        ec = U_ZERO_ERROR;
        switch (i) {
        case 0:
            script = uscript_getScript(-1, &ec);
            desc = "uscript_getScript(-1)";
            break;
        case 1:
            script = u_getIntPropertyValue(-1, UCHAR_SCRIPT);
            desc = "u_getIntPropertyValue(-1, UCHAR_SCRIPT)";
            break;
        }
        /* We don't explicitly test ec.  It should be U_FAILURE but it
           isn't documented as such. */
        if (script != 0) {
            log_err("FAIL: %s = %d, exp. 0\n",
                    desc, script);
        }
    }
}

/* add characters from a serialized set to a normal one */
static void
_setAddSerialized(USet *set, const USerializedSet *sset) {
    UChar32 start, end;
    int32_t i, count;

    count=uset_getSerializedRangeCount(sset);
    for(i=0; i<count; ++i) {
        uset_getSerializedRange(sset, i, &start, &end);
        uset_addRange(set, start, end);
    }
}

/* various tests for consistency of UCD data and API behavior */
static void
TestConsistency() {
#if !UCONFIG_NO_NORMALIZATION
    UChar buffer16[300];
#endif
    char buffer[300];
    USet *set1, *set2, *set3, *set4;
    UErrorCode errorCode;

#if !UCONFIG_NO_NORMALIZATION
    USerializedSet sset;
#endif
    UChar32 start, end;
    int32_t i, length;

    U_STRING_DECL(hyphenPattern, "[:Hyphen:]", 10);
    U_STRING_DECL(dashPattern, "[:Dash:]", 8);
    U_STRING_DECL(lowerPattern, "[:Lowercase:]", 13);
    U_STRING_DECL(formatPattern, "[:Cf:]", 6);
    U_STRING_DECL(alphaPattern, "[:Alphabetic:]", 14);

    U_STRING_INIT(hyphenPattern, "[:Hyphen:]", 10);
    U_STRING_INIT(dashPattern, "[:Dash:]", 8);
    U_STRING_INIT(lowerPattern, "[:Lowercase:]", 13);
    U_STRING_INIT(formatPattern, "[:Cf:]", 6);
    U_STRING_INIT(alphaPattern, "[:Alphabetic:]", 14);

    /*
     * It used to be that UCD.html and its precursors said
     * "Those dashes used to mark connections between pieces of words,
     *  plus the Katakana middle dot."
     *
     * Unicode 4 changed 00AD Soft Hyphen to Cf and removed it from Dash
     * but not from Hyphen.
     * UTC 94 (2003mar) decided to leave it that way and to changed UCD.html.
     * Therefore, do not show errors when testing the Hyphen property.
     */
    log_verbose("Starting with Unicode 4, inconsistencies with [:Hyphen:] are\n"
                "known to the UTC and not considered errors.\n");

    errorCode=U_ZERO_ERROR;
    set1=uset_openPattern(hyphenPattern, 10, &errorCode);
    set2=uset_openPattern(dashPattern, 8, &errorCode);
    if(U_SUCCESS(errorCode)) {
        /* remove the Katakana middle dot(s) from set1 */
        uset_remove(set1, 0x30fb);
        uset_remove(set1, 0xff65); /* halfwidth variant */
        showAMinusB(set1, set2, "[:Hyphen:]", "[:Dash:]", FALSE);
    } else {
        log_err("error opening [:Hyphen:] or [:Dash:] - %s\n", u_errorName(errorCode));
    }

    /* check that Cf is neither Hyphen nor Dash nor Alphabetic */
    set3=uset_openPattern(formatPattern, 6, &errorCode);
    set4=uset_openPattern(alphaPattern, 14, &errorCode);
    if(U_SUCCESS(errorCode)) {
        showAIntersectB(set3, set1, "[:Cf:]", "[:Hyphen:]", FALSE);
        showAIntersectB(set3, set2, "[:Cf:]", "[:Dash:]", TRUE);
        showAIntersectB(set3, set4, "[:Cf:]", "[:Alphabetic:]", TRUE);
    } else {
        log_err("error opening [:Cf:] or [:Alpbabetic:] - %s\n", u_errorName(errorCode));
    }

    uset_close(set1);
    uset_close(set2);
    uset_close(set3);
    uset_close(set4);

    /*
     * Check that each lowercase character has "small" in its name
     * and not "capital".
     * There are some such characters, some of which seem odd.
     * Use the verbose flag to see these notices.
     */
    errorCode=U_ZERO_ERROR;
    set1=uset_openPattern(lowerPattern, 13, &errorCode);
    if(U_SUCCESS(errorCode)) {
        for(i=0;; ++i) {
            length=uset_getItem(set1, i, &start, &end, NULL, 0, &errorCode);
            if(errorCode==U_INDEX_OUTOFBOUNDS_ERROR) {
                break; /* done */
            }
            if(U_FAILURE(errorCode)) {
                log_err("error iterating over [:Lowercase:] at item %d: %s\n",
                        i, u_errorName(errorCode));
                break;
            }
            if(length!=0) {
                break; /* done with code points, got a string or -1 */
            }

            while(start<=end) {
                length=u_charName(start, U_UNICODE_CHAR_NAME, buffer, sizeof(buffer), &errorCode);
                if(U_FAILURE(errorCode)) {
                    log_err("error getting the name of U+%04x - %s\n", start, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
                if( (strstr(buffer, "SMALL")==NULL || strstr(buffer, "CAPITAL")!=NULL) &&
                    strstr(buffer, "SMALL CAPITAL")==NULL
                ) {
                    log_verbose("info: [:Lowercase:] contains U+%04x whose name does not suggest lowercase: %s\n", start, buffer);
                }
                ++start;
            }
        }
    } else {
        log_err("error opening [:Lowercase:] - %s\n", u_errorName(errorCode));
    }
    uset_close(set1);

#if !UCONFIG_NO_NORMALIZATION

    /*
     * Test for an example that unorm_getCanonStartSet() delivers
     * all characters that compose from the input one,
     * even in multiple steps.
     * For example, the set for "I" (0049) should contain both
     * I-diaeresis (00CF) and I-diaeresis-acute (1E2E).
     * In general, the set for the middle such character should be a subset
     * of the set for the first.
     */
    set1=uset_open(1, 0);
    set2=uset_open(1, 0);

    unorm_getCanonStartSet(0x49, &sset);
    _setAddSerialized(set1, &sset);

    /* enumerate all characters that are plausible to be latin letters */
    for(start=0xa0; start<0x2000; ++start) {
        if(unorm_getDecomposition(start, FALSE, buffer16, LENGTHOF(buffer16))>1 && buffer16[0]==0x49) {
            uset_add(set2, start);
        }
    }

    compareUSets(set1, set2,
                 "[canon start set of 0049]", "[all c with canon decomp with 0049]",
                 TRUE);
    uset_close(set1);
    uset_close(set2);

#endif
}
