/*
*******************************************************************************
*
*   Copyright (C) 2002, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uprops.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002feb24
*   created by: Markus W. Scherer
*
*   Implementations for mostly non-core Unicode character properties
*   stored in uprops.dat.
*/

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/uscript.h"
#include "uprops.h"

/* helper definitions ------------------------------------------------------- */

#define FLAG(n) ((uint32_t)1<<(n))

/* flags for general categories in the order of UCharCategory */
#define _Cn     FLAG(U_GENERAL_OTHER_TYPES)
#define _Lu     FLAG(U_UPPERCASE_LETTER)
#define _Ll     FLAG(U_LOWERCASE_LETTER)
#define _Lt     FLAG(U_TITLECASE_LETTER)
#define _Lm     FLAG(U_MODIFIER_LETTER)
#define _Lo     FLAG(U_OTHER_LETTER)
#define _Mn     FLAG(U_NON_SPACING_MARK)
#define _Me     FLAG(U_ENCLOSING_MARK)
#define _Mc     FLAG(U_COMBINING_SPACING_MARK)
#define _Nd     FLAG(U_DECIMAL_DIGIT_NUMBER)
#define _Nl     FLAG(U_LETTER_NUMBER)
#define _No     FLAG(U_OTHER_NUMBER)
#define _Zs     FLAG(U_SPACE_SEPARATOR)
#define _Zl     FLAG(U_LINE_SEPARATOR)
#define _Zp     FLAG(U_PARAGRAPH_SEPARATOR)
#define _Cc     FLAG(U_CONTROL_CHAR)
#define _Cf     FLAG(U_FORMAT_CHAR)
#define _Co     FLAG(U_PRIVATE_USE_CHAR)
#define _Cs     FLAG(U_SURROGATE)
#define _Pd     FLAG(U_DASH_PUNCTUATION)
#define _Ps     FLAG(U_START_PUNCTUATION)
#define _Pe     FLAG(U_END_PUNCTUATION)
#define _Pc     FLAG(U_CONNECTOR_PUNCTUATION)
#define _Po     FLAG(U_OTHER_PUNCTUATION)
#define _Sm     FLAG(U_MATH_SYMBOL)
#define _Sc     FLAG(U_CURRENCY_SYMBOL)
#define _Sk     FLAG(U_MODIFIER_SYMBOL)
#define _So     FLAG(U_OTHER_SYMBOL)
#define _Pi     FLAG(U_INITIAL_PUNCTUATION)
#define _Pf     FLAG(U_FINAL_PUNCTUATION)

/* API functions ------------------------------------------------------------ */

U_CAPI void U_EXPORT2
u_charAge(UChar32 c, UVersionInfo versionArray) {
    if(versionArray!=NULL) {
        uint32_t version=u_getUnicodeProperties(c, 0)>>UPROPS_AGE_SHIFT;
        versionArray[0]=(uint8_t)(version>>4);
        versionArray[1]=(uint8_t)(version&0xf);
        versionArray[2]=versionArray[3]=0;
    }
}

U_CAPI UScriptCode U_EXPORT2
uscript_getScript(UChar32 c, UErrorCode *pErrorCode) {
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return USCRIPT_INVALID_CODE;
    }
    if((uint32_t)c>0x10ffff) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return USCRIPT_INVALID_CODE;
    }

    return (UScriptCode)(u_getUnicodeProperties(c, 0)&UPROPS_SCRIPT_MASK);
}

U_CAPI UBlockCode U_EXPORT2
ublock_getCode(UChar32 c) {
    uint32_t b;

    if((uint32_t)c>0x10ffff) {
        return UBLOCK_INVALID_CODE;
    }

    b=(u_getUnicodeProperties(c, 0)&UPROPS_BLOCK_MASK)>>UPROPS_BLOCK_SHIFT;
    if(b==0) {
        return UBLOCK_INVALID_CODE;
    } else {
        return (UBlockCode)b;
    }
}

UBool u_hasBinaryProperty(UChar32 c, UProperty which) {
    /* c is range-checked in the functions that are called from here */
    switch(which) {
    case UCHAR_ALPHABETIC:
        /* Lu+Ll+Lt+Lm+Lo+Other_Alphabetic */
        return (FLAG(u_charType(c))&(_Lu|_Ll|_Lt|_Lm|_Lo))!=0 ||
                (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_OTHER_ALPHABETIC))!=0;
    case UCHAR_ASCII_HEX_DIGIT:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_ASCII_HEX_DIGIT))!=0;
    case UCHAR_BIDI_CONTROL:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_BIDI_CONTROL))!=0;
    case UCHAR_BIDI_MIRRORED:
        return u_isMirrored(c);
    case UCHAR_DASH:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_DASH))!=0;
    case UCHAR_DEFAULT_IGNORABLE_CODE_POINT:
        /* Cf+Cc+Cs+Other_Default_Ignorable_Code_Point-White_Space */
        return (FLAG(u_charType(c))&(_Cf|_Cc|_Cs))!=0 ||
                (u_getUnicodeProperties(c, 1)&
                    (FLAG(UPROPS_OTHER_DEFAULT_IGNORABLE_CODE_POINT)|FLAG(UPROPS_WHITE_SPACE))==
                FLAG(UPROPS_OTHER_DEFAULT_IGNORABLE_CODE_POINT));
    case UCHAR_DEPRECATED:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_DEPRECATED))!=0;
    case UCHAR_DIACRITIC:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_DIACRITIC))!=0;
    case UCHAR_EXTENDER:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_EXTENDER))!=0;
    case UCHAR_FULL_COMPOSITION_EXCLUSION:
        return FALSE; /* ### TODO from unorm.dat */
    case UCHAR_GRAPHEME_BASE:
        /*
         * [0..10FFFF]-Cc-Cf-Cs-Co-Cn-Zl-Zp-Grapheme_Link-Grapheme_Extend ==
         * [0..10FFFF]-Cc-Cf-Cs-Co-Cn-Zl-Zp-Grapheme_Link-(Me+Mn+Mc+Other_Grapheme_Extend) ==
         * [0..10FFFF]-Cc-Cf-Cs-Co-Cn-Zl-Zp-Me-Mn-Mc-Grapheme_Link-Other_Grapheme_Extend
         *
         * u_charType(c out of range) returns Cn so we need not check for the range
         */
        return (FLAG(u_charType(c))&(_Cc|_Cf|_Cs|_Co|_Cn|_Zl|_Zp|_Me|_Mn|_Mc))==0 &&
                (u_getUnicodeProperties(c, 1)&
                    (FLAG(UPROPS_GRAPHEME_LINK)|FLAG(UPROPS_OTHER_GRAPHEME_EXTEND))==0);
    case UCHAR_GRAPHEME_EXTEND:
        /* Me+Mn+Mc+Other_Grapheme_Extend-Grapheme_Link */
        return (FLAG(u_charType(c))&(_Me|_Mn|_Mc))!=0 ||
                (u_getUnicodeProperties(c, 1)&
                    (FLAG(UPROPS_OTHER_GRAPHEME_EXTEND)|FLAG(UPROPS_GRAPHEME_LINK))==
                FLAG(UPROPS_OTHER_GRAPHEME_EXTEND));
    case UCHAR_GRAPHEME_LINK:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_GRAPHEME_LINK))!=0;
    case UCHAR_HEX_DIGIT:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_HEX_DIGIT))!=0;
    case UCHAR_HYPHEN:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_HYPHEN))!=0;
    case UCHAR_ID_CONTINUE:
        /* ID_Start+Mn+Mc+Nd+Pc == Lu+Ll+Lt+Lm+Lo+Nl+Mn+Mc+Nd+Pc */
        return (FLAG(u_charType(c))&(_Lu|_Ll|_Lt|_Lm|_Lo|_Nl|_Mn|_Mc|_Nd|_Pc))!=0;
    case UCHAR_ID_START:
        /* Lu+Ll+Lt+Lm+Lo+Nl */
        return (FLAG(u_charType(c))&(_Lu|_Ll|_Lt|_Lm|_Lo|_Nl))!=0;
    case UCHAR_IDEOGRAPHIC:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_IDEOGRAPHIC))!=0;
    case UCHAR_IDS_BINARY_OPERATOR:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_IDS_BINARY_OPERATOR))!=0;
    case UCHAR_IDS_TRIARY_OPERATOR:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_IDS_TRINARY_OPERATOR))!=0;
    case UCHAR_JOIN_CONTROL:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_JOIN_CONTROL))!=0;
    case UCHAR_LOGICAL_ORDER_EXCEPTION:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_LOGICAL_ORDER_EXCEPTION))!=0;
    case UCHAR_LOWERCASE:
        /* Ll+Other_Lowercase */
        return u_charType(c)==U_LOWERCASE_LETTER ||
                (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_OTHER_LOWERCASE))!=0;
    case UCHAR_MATH:
        /* Sm+Other_Math */
        return u_charType(c)==U_MATH_SYMBOL ||
                (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_OTHER_MATH))!=0;
    case UCHAR_NONCHARACTER_CODE_POINT:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_NONCHARACTER_CODE_POINT))!=0;
    case UCHAR_QUOTATION_MARK:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_QUOTATION_MARK))!=0;
    case UCHAR_RADICAL:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_RADICAL))!=0;
    case UCHAR_SOFT_DOTTED:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_SOFT_DOTTED))!=0;
    case UCHAR_TERMINAL_PUNCTUATION:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_TERMINAL_PUNCTUATION))!=0;
    case UCHAR_UNIFIED_IDEOGRAPH:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_UNIFIED_IDEOGRAPH))!=0;
    case UCHAR_UPPERCASE:
        /* Lu+Other_Uppercase */
        return u_charType(c)==U_UPPERCASE_LETTER ||
                (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_OTHER_UPPERCASE))!=0;
    case UCHAR_WHITE_SPACE:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_WHITE_SPACE))!=0;
    case UCHAR_XID_CONTINUE:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_XID_CONTINUE))!=0;
    case UCHAR_XID_START:
        return (u_getUnicodeProperties(c, 1)&FLAG(UPROPS_XID_START))!=0;
    default:
        /* not a known binary property */
        return FALSE;
    };
}

UBool u_isUAlphabetic(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_ALPHABETIC);
}

UBool u_isULowercase(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_LOWERCASE);
}

UBool u_isUUppercase(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_UPPERCASE);
}

UBool u_isUWhiteSpace(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_WHITE_SPACE);
}
