/*
*******************************************************************************
*   Copyright (C) 1996-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ucol_res.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
* Description:
* This file contains dependencies that the collation run-time doesn't normally
* need. This mainly contains resource bundle usage and collation meta information
*
* Modification history
* Date        Name      Comments
* 1996-1999   various members of ICU team maintained C API for collation framework
* 02/16/2001  synwee    Added internal method getPrevSpecialCE
* 03/01/2001  synwee    Added maxexpansion functionality.
* 03/16/2001  weiv      Collation framework is rewritten in C and made UCA compliant
* 12/08/2004  grhoten   Split part of ucol.cpp into ucol_res.cpp
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION
#include "unicode/uloc.h"
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/caniter.h"
#include "unicode/ustring.h"

#include "ucol_bld.h"
#include "ucol_imp.h"
#include "ucol_tok.h"
#include "ucol_elm.h"
#include "uresimp.h"
#include "ustr_imp.h"
#include "cstring.h"
#include "umutex.h"
#include "ucln_in.h"
#include "ustrenum.h"
#include "putilimp.h"
#include "utracimp.h"
#include "cmemory.h"
#include "uenumimp.h"
#include "ulist.h"

U_NAMESPACE_USE

// static UCA. There is only one. Collators don't use it.
// It is referenced only in ucol_initUCA and ucol_cleanup
static UCollator* _staticUCA = NULL;
// static pointer to udata memory. Inited in ucol_initUCA
// used for cleanup in ucol_cleanup
static UDataMemory* UCA_DATA_MEM = NULL;

U_CDECL_BEGIN
static UBool U_CALLCONV
ucol_res_cleanup(void)
{
    if (UCA_DATA_MEM) {
        udata_close(UCA_DATA_MEM);
        UCA_DATA_MEM = NULL;
    }
    if (_staticUCA) {
        ucol_close(_staticUCA);
        _staticUCA = NULL;
    }
    return TRUE;
}

static UBool U_CALLCONV
isAcceptableUCA(void * /*context*/,
             const char * /*type*/, const char * /*name*/,
             const UDataInfo *pInfo){
  /* context, type & name are intentionally not used */
    if( pInfo->size>=20 &&
        pInfo->isBigEndian==U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily==U_CHARSET_FAMILY &&
        pInfo->dataFormat[0]==UCA_DATA_FORMAT_0 &&   /* dataFormat="UCol" */
        pInfo->dataFormat[1]==UCA_DATA_FORMAT_1 &&
        pInfo->dataFormat[2]==UCA_DATA_FORMAT_2 &&
        pInfo->dataFormat[3]==UCA_DATA_FORMAT_3 &&
        pInfo->formatVersion[0]==UCA_FORMAT_VERSION_0 &&
        pInfo->formatVersion[1]>=UCA_FORMAT_VERSION_1// &&
        //pInfo->formatVersion[1]==UCA_FORMAT_VERSION_1 &&
        //pInfo->formatVersion[2]==UCA_FORMAT_VERSION_2 && // Too harsh
        //pInfo->formatVersion[3]==UCA_FORMAT_VERSION_3 && // Too harsh
        ) {
        UVersionInfo UCDVersion;
        u_getUnicodeVersion(UCDVersion);
        return (UBool)(pInfo->dataVersion[0]==UCDVersion[0]
            && pInfo->dataVersion[1]==UCDVersion[1]);
            //&& pInfo->dataVersion[2]==ucaDataInfo.dataVersion[2]
            //&& pInfo->dataVersion[3]==ucaDataInfo.dataVersion[3]);
    } else {
        return FALSE;
    }
}
U_CDECL_END

/* do not close UCA returned by ucol_initUCA! */
UCollator *
ucol_initUCA(UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return NULL;
    }
    UBool needsInit;
    UMTX_CHECK(NULL, (_staticUCA == NULL), needsInit);

    if(needsInit) {
        UDataMemory *result = udata_openChoice(U_ICUDATA_COLL, UCA_DATA_TYPE, UCA_DATA_NAME, isAcceptableUCA, NULL, status);

        if(U_SUCCESS(*status)){
            UCollator *newUCA = ucol_initCollator((const UCATableHeader *)udata_getMemory(result), NULL, NULL, status);
            if(U_SUCCESS(*status)){
                // Initalize variables for implicit generation
                uprv_uca_initImplicitConstants(status);

                umtx_lock(NULL);
                if(_staticUCA == NULL) {
                    UCA_DATA_MEM = result;
                    _staticUCA = newUCA;
                    newUCA = NULL;
                    result = NULL;
                }
                umtx_unlock(NULL);

                ucln_i18n_registerCleanup(UCLN_I18N_UCOL_RES, ucol_res_cleanup);
                if(newUCA != NULL) {
                    ucol_close(newUCA);
                    udata_close(result);
                }
            }else{
                ucol_close(newUCA);
                udata_close(result);
            }
        }
        else {
            udata_close(result);
        }
    }
    return _staticUCA;
}

U_CAPI void U_EXPORT2
ucol_forgetUCA(void)
{
    _staticUCA = NULL;
    UCA_DATA_MEM = NULL;
}

/****************************************************************************/
/* Following are the open/close functions                                   */
/*                                                                          */
/****************************************************************************/
static UCollator*
tryOpeningFromRules(UResourceBundle *collElem, UErrorCode *status) {
    int32_t rulesLen = 0;
    const UChar *rules = ures_getStringByKey(collElem, "Sequence", &rulesLen, status);
    return ucol_openRules(rules, rulesLen, UCOL_DEFAULT, UCOL_DEFAULT, NULL, status);
}


void ucol_buildScriptReorderTable(UCollator *coll){
    UScriptCode defaultScriptOrder[256] = {
        /* 0x00 */ USCRIPT_INVALID_CODE, /* USCRIPT_INVALID_CODE represents scripts that should not be reordered */
        /* 0x01 */ USCRIPT_INVALID_CODE,
        /* 0x02 */ USCRIPT_INVALID_CODE,
        /* 0x03 */ USCRIPT_INVALID_CODE,
        /* 0x04 */ USCRIPT_INVALID_CODE,
        /* 0x05 */ USCRIPT_COMMON,
        /* 0x06 */ USCRIPT_COMMON,
        /* 0x07 */ USCRIPT_COMMON,
        /* 0x08 */ USCRIPT_COMMON,
        /* 0x09 */ USCRIPT_COMMON,
        /* 0x0A */ USCRIPT_COMMON,
        /* 0x0B */ USCRIPT_COMMON,
        /* 0x0C */ USCRIPT_COMMON,
        /* 0x0D */ USCRIPT_COMMON,
        /* 0x0E */ USCRIPT_COMMON,
        /* 0x0F */ USCRIPT_COMMON,
        /* 0x10 */ USCRIPT_COMMON,
        /* 0x11 */ USCRIPT_COMMON,
        /* 0x12 */ USCRIPT_COMMON,
        /* 0x13 */ USCRIPT_COMMON,
        /* 0x14 */ USCRIPT_COMMON,
        /* 0x15 */ USCRIPT_COMMON,
        /* 0x16 */ USCRIPT_COMMON,
        /* 0x17 */ USCRIPT_COMMON,
        /* 0x18 */ USCRIPT_COMMON,
        /* 0x19 */ USCRIPT_COMMON,
        /* 0x1A */ USCRIPT_COMMON,
        /* 0x1B */ USCRIPT_COMMON,
        /* 0x1C */ USCRIPT_COMMON,
        /* 0x1D */ USCRIPT_COMMON,
        /* 0x1E */ USCRIPT_COMMON,
        /* 0x1F */ USCRIPT_COMMON,
        /* 0x20 */ USCRIPT_COMMON,
        /* 0x21 */ USCRIPT_COMMON,
        /* 0x22 */ USCRIPT_COMMON,
        /* 0x23 */ USCRIPT_COMMON,
        /* 0x24 */ USCRIPT_COMMON,
        /* 0x25 */ USCRIPT_COMMON,
        /* 0x26 */ USCRIPT_COMMON,
        /* 0x27 */ USCRIPT_COMMON,
        /* 0x28 */ USCRIPT_COMMON,
        /* 0x29 */ USCRIPT_COMMON,
        /* 0x2A */ USCRIPT_COMMON,
        /* 0x2B */ USCRIPT_COMMON,
        /* 0x2C */ USCRIPT_LATIN,
        /* 0x2D */ USCRIPT_LATIN,
        /* 0x2E */ USCRIPT_LATIN,
        /* 0x2F */ USCRIPT_LATIN,
        /* 0x30 */ USCRIPT_LATIN,
        /* 0x31 */ USCRIPT_LATIN,
        /* 0x32 */ USCRIPT_LATIN,
        /* 0x33 */ USCRIPT_LATIN,
        /* 0x34 */ USCRIPT_LATIN,
        /* 0x35 */ USCRIPT_LATIN,
        /* 0x36 */ USCRIPT_LATIN,
        /* 0x37 */ USCRIPT_LATIN,
        /* 0x38 */ USCRIPT_LATIN,
        /* 0x39 */ USCRIPT_LATIN,
        /* 0x3A */ USCRIPT_LATIN,
        /* 0x3B */ USCRIPT_LATIN,
        /* 0x3C */ USCRIPT_LATIN,
        /* 0x3D */ USCRIPT_LATIN,
        /* 0x3E */ USCRIPT_LATIN,
        /* 0x3F */ USCRIPT_LATIN,
        /* 0x40 */ USCRIPT_LATIN,
        /* 0x41 */ USCRIPT_LATIN,
        /* 0x42 */ USCRIPT_LATIN,
        /* 0x43 */ USCRIPT_LATIN,
        /* 0x44 */ USCRIPT_LATIN,
        /* 0x45 */ USCRIPT_LATIN,
        /* 0x46 */ USCRIPT_LATIN,
        /* 0x47 */ USCRIPT_LATIN,
        /* 0x48 */ USCRIPT_LATIN,
        /* 0x49 */ USCRIPT_LATIN,
        /* 0x4A */ USCRIPT_LATIN,
        /* 0x4B */ USCRIPT_LATIN,
        /* 0x4C */ USCRIPT_LATIN,
        /* 0x4D */ USCRIPT_LATIN,
        /* 0x4E */ USCRIPT_LATIN,
        /* 0x4F */ USCRIPT_LATIN,
        /* 0x50 */ USCRIPT_LATIN,
        /* 0x51 */ USCRIPT_LATIN,
        /* 0x52 */ USCRIPT_LATIN,
        /* 0x53 */ USCRIPT_LATIN,
        /* 0x54 */ USCRIPT_LATIN,
        /* 0x55 */ USCRIPT_LATIN,
        /* 0x56 */ USCRIPT_LATIN,
        /* 0x57 */ USCRIPT_LATIN,
        /* 0x58 */ USCRIPT_LATIN,
        /* 0x59 */ USCRIPT_LATIN,
        /* 0x5A */ USCRIPT_LATIN,
        /* 0x5B */ USCRIPT_LATIN,
        /* 0x5C */ USCRIPT_LATIN,
        /* 0x5D */ USCRIPT_LATIN,
        /* 0x5E */ USCRIPT_LATIN,
        /* 0x5F */ USCRIPT_LATIN,
        /* 0x60 */ USCRIPT_GREEK,
        /* 0x61 */ USCRIPT_COPTIC,
        /* 0x62 */ USCRIPT_CYRILLIC,
        /* 0x63 */ USCRIPT_CYRILLIC,
        /* 0x64 */ USCRIPT_GLAGOLITIC,
        /* 0x65 */ USCRIPT_GEORGIAN,
        /* 0x66 */ USCRIPT_ARMENIAN,
        /* 0x67 */ USCRIPT_HEBREW,
        /* 0x68 */ USCRIPT_ARABIC,
        /* 0x69 */ USCRIPT_ARABIC,
        /* 0x6A */ USCRIPT_SYRIAC,
        /* 0x6B */ USCRIPT_THAANA,
        /* 0x6C */ USCRIPT_NKO,
        /* 0x6D */ USCRIPT_TIFINAGH,
        /* 0x6E */ USCRIPT_ETHIOPIC,
        /* 0x6F */ USCRIPT_ETHIOPIC,
        /* 0x70 */ USCRIPT_ETHIOPIC,
        /* 0x71 */ USCRIPT_ETHIOPIC,
        /* 0x72 */ USCRIPT_DEVANAGARI,
        /* 0x73 */ USCRIPT_BENGALI,
        /* 0x74 */ USCRIPT_GURMUKHI,
        /* 0x75 */ USCRIPT_GUJARATI,
        /* 0x76 */ USCRIPT_ORIYA,
        /* 0x77 */ USCRIPT_TAMIL,
        /* 0x78 */ USCRIPT_TELUGU,
        /* 0x79 */ USCRIPT_KANNADA,
        /* 0x7A */ USCRIPT_MALAYALAM,
        /* 0x7B */ USCRIPT_SINHALA,
        /* 0x7C */ USCRIPT_SYLOTI_NAGRI,
        /* 0x7D */ USCRIPT_SAURASHTRA,
        /* 0x7E */ USCRIPT_SUNDANESE,
        /* 0x7F */ USCRIPT_THAI,
        /* 0x80 */ USCRIPT_LAO,
        /* 0x81 */ USCRIPT_TIBETAN,
        /* 0x82 */ USCRIPT_LEPCHA,
        /* 0x83 */ USCRIPT_PHAGS_PA,
        /* 0x84 */ USCRIPT_LIMBU,
        /* 0x85 */ USCRIPT_TAGALOG,
        /* 0x86 */ USCRIPT_HANUNOO,
        /* 0x87 */ USCRIPT_BUHID,
        /* 0x88 */ USCRIPT_TAGBANWA,
        /* 0x89 */ USCRIPT_BUGINESE,
        /* 0x8A */ USCRIPT_REJANG,
        /* 0x8B */ USCRIPT_KAYAH_LI,
        /* 0x8C */ USCRIPT_MYANMAR,
        /* 0x8D */ USCRIPT_MYANMAR,
        /* 0x8E */ USCRIPT_KHMER,
        /* 0x8F */ USCRIPT_TAI_LE,
        /* 0x90 */ USCRIPT_NEW_TAI_LUE,
        /* 0x91 */ USCRIPT_CHAM,
        /* 0x92 */ USCRIPT_BALINESE,
        /* 0x93 */ USCRIPT_MONGOLIAN,
        /* 0x94 */ USCRIPT_MONGOLIAN,
        /* 0x95 */ USCRIPT_OL_CHIKI,
        /* 0x96 */ USCRIPT_CHEROKEE,
        /* 0x97 */ USCRIPT_CANADIAN_ABORIGINAL,
        /* 0x98 */ USCRIPT_CANADIAN_ABORIGINAL,
        /* 0x99 */ USCRIPT_CANADIAN_ABORIGINAL,
        /* 0x9A */ USCRIPT_CANADIAN_ABORIGINAL,
        /* 0x9B */ USCRIPT_CANADIAN_ABORIGINAL,
        /* 0x9C */ USCRIPT_CANADIAN_ABORIGINAL,
        /* 0x9D */ USCRIPT_OGHAM,
        /* 0x9E */ USCRIPT_RUNIC,
        /* 0x9F */ USCRIPT_VAI,
        /* 0xA0 */ USCRIPT_VAI,
        /* 0xA1 */ USCRIPT_VAI,
        /* 0xA2 */ USCRIPT_HANGUL,
        /* 0xA3 */ USCRIPT_HIRAGANA,
        /* 0xA4 */ USCRIPT_BOPOMOFO,
        /* 0xA5 */ USCRIPT_YI,
        /* 0xA6 */ USCRIPT_YI,
        /* 0xA7 */ USCRIPT_YI,
        /* 0xA8 */ USCRIPT_YI,
        /* 0xA9 */ USCRIPT_YI,
        /* 0xAA */ USCRIPT_YI,
        /* 0xAB */ USCRIPT_YI,
        /* 0xAC */ USCRIPT_YI,
        /* 0xAD */ USCRIPT_YI,
        /* 0xAE */ USCRIPT_YI,
        /* 0xAF */ USCRIPT_CARIAN,
        /* 0xB0 */ USCRIPT_DESERET,
        /* 0xB1 */ USCRIPT_LINEAR_B,
        /* 0xB2 */ USCRIPT_LINEAR_B,
        /* 0xB3 */ USCRIPT_UGARITIC,
        /* 0xB4 */ USCRIPT_CUNEIFORM,
        /* 0xB5 */ USCRIPT_CUNEIFORM,
        /* 0xB6 */ USCRIPT_CUNEIFORM,
        /* 0xB7 */ USCRIPT_CUNEIFORM,
        /* 0xB8 */ USCRIPT_CUNEIFORM,
        /* 0xB9 */ USCRIPT_CUNEIFORM,
        /* 0xBA */ USCRIPT_CUNEIFORM,
        /* 0xBB */ USCRIPT_INVALID_CODE,
        /* 0xBC */ USCRIPT_INVALID_CODE,
        /* 0xBD */ USCRIPT_INVALID_CODE,
        /* 0xBE */ USCRIPT_INVALID_CODE,
        /* 0xBF */ USCRIPT_INVALID_CODE,
        /* 0xC0 */ USCRIPT_INVALID_CODE,
        /* 0xC1 */ USCRIPT_INVALID_CODE,
        /* 0xC2 */ USCRIPT_INVALID_CODE,
        /* 0xC3 */ USCRIPT_INVALID_CODE,
        /* 0xC4 */ USCRIPT_INVALID_CODE,
        /* 0xC5 */ USCRIPT_INVALID_CODE,
        /* 0xC6 */ USCRIPT_INVALID_CODE,
        /* 0xC7 */ USCRIPT_INVALID_CODE,
        /* 0xC8 */ USCRIPT_INVALID_CODE,
        /* 0xC9 */ USCRIPT_INVALID_CODE,
        /* 0xCA */ USCRIPT_INVALID_CODE,
        /* 0xCB */ USCRIPT_INVALID_CODE,
        /* 0xCC */ USCRIPT_INVALID_CODE,
        /* 0xCD */ USCRIPT_INVALID_CODE,
        /* 0xCE */ USCRIPT_INVALID_CODE,
        /* 0xCF */ USCRIPT_INVALID_CODE,
        /* 0xD0 */ USCRIPT_INVALID_CODE,
        /* 0xD1 */ USCRIPT_INVALID_CODE,
        /* 0xD2 */ USCRIPT_INVALID_CODE,
        /* 0xD3 */ USCRIPT_INVALID_CODE,
        /* 0xD4 */ USCRIPT_INVALID_CODE,
        /* 0xD5 */ USCRIPT_INVALID_CODE,
        /* 0xD6 */ USCRIPT_INVALID_CODE,
        /* 0xD7 */ USCRIPT_INVALID_CODE,
        /* 0xD8 */ USCRIPT_INVALID_CODE,
        /* 0xD9 */ USCRIPT_INVALID_CODE,
        /* 0xDA */ USCRIPT_INVALID_CODE,
        /* 0xDB */ USCRIPT_INVALID_CODE,
        /* 0xDC */ USCRIPT_INVALID_CODE,
        /* 0xDD */ USCRIPT_INVALID_CODE,
        /* 0xDE */ USCRIPT_INVALID_CODE,
        /* 0xDF */ USCRIPT_INVALID_CODE,
        /* 0xE0 */ USCRIPT_HAN,
        /* 0xE1 */ USCRIPT_HAN,
        /* 0xE2 */ USCRIPT_INVALID_CODE,
        /* 0xE3 */ USCRIPT_INVALID_CODE,
        /* 0xE4 */ USCRIPT_INVALID_CODE,
        /* 0xE5 */ USCRIPT_INVALID_CODE,
        /* 0xE6 */ USCRIPT_INVALID_CODE,
        /* 0xE7 */ USCRIPT_INVALID_CODE,
        /* 0xE8 */ USCRIPT_INVALID_CODE,
        /* 0xE9 */ USCRIPT_INVALID_CODE,
        /* 0xEA */ USCRIPT_INVALID_CODE,
        /* 0xEB */ USCRIPT_INVALID_CODE,
        /* 0xEC */ USCRIPT_INVALID_CODE,
        /* 0xED */ USCRIPT_INVALID_CODE,
        /* 0xEE */ USCRIPT_INVALID_CODE,
        /* 0xEF */ USCRIPT_INVALID_CODE,
        /* 0xF0 */ USCRIPT_INVALID_CODE,
        /* 0xF1 */ USCRIPT_INVALID_CODE,
        /* 0xF2 */ USCRIPT_INVALID_CODE,
        /* 0xF3 */ USCRIPT_INVALID_CODE,
        /* 0xF4 */ USCRIPT_INVALID_CODE,
        /* 0xF5 */ USCRIPT_INVALID_CODE,
        /* 0xF6 */ USCRIPT_INVALID_CODE,
        /* 0xF7 */ USCRIPT_INVALID_CODE,
        /* 0xF8 */ USCRIPT_INVALID_CODE,
        /* 0xF9 */ USCRIPT_INVALID_CODE,
        /* 0xFA */ USCRIPT_INVALID_CODE,
        /* 0xFB */ USCRIPT_INVALID_CODE,
        /* 0xFC */ USCRIPT_INVALID_CODE,
        /* 0xFD */ USCRIPT_INVALID_CODE,
        /* 0xFE */ USCRIPT_INVALID_CODE,
        /* 0xFF */ USCRIPT_INVALID_CODE
    };

    int i;
    UScriptCode *next;
    UScriptCode *last;

    // The lowest byte that hasn't been assigned a mapping
    int toBottom = 0;
    // The highest byte that hasn't been assigned a mapping
    int toTop = 255;

    bool filled[256];
    for(i = 0; i < 256; i++){
        filled[i] = false;
    }

    if(coll->scriptOrderLength != 0){
        if(coll->scriptReorderTable == NULL){
            coll->scriptReorderTable = (uint8_t*)uprv_malloc(256*sizeof(uint8_t));
        }
        /* Start from the front of the list and place each script we encounter at the
           earliest possible locatation in the permutation table. If we encounter
           UNKNOWN, start processing from the back, and place each script in the last
           possible location. At each step, we also need to make sure that any scripts
           that need to not be moved are copied to their same location in the final table.*/
        next = coll->scriptOrder;
        while(next < coll->scriptOrder+coll->scriptOrderLength){
            if(*next != USCRIPT_UNKNOWN){
                for (i = 0; i < 256; i++){
                    while(defaultScriptOrder[toBottom] == USCRIPT_INVALID_CODE){
                        filled[toBottom] = true;
                        coll->scriptReorderTable[toBottom] = toBottom++;
                    }
                    if(defaultScriptOrder[i] == *next){
                        filled[i] = true;
                        coll->scriptReorderTable[i] = toBottom++;
                    }
                }
            }else{
                last = coll->scriptOrder+coll->scriptOrderLength-1;
                while(last > next){
                    for (i = 255; i >= 0; i--){
                        while(defaultScriptOrder[toTop] == USCRIPT_INVALID_CODE){
                            filled[toTop] = true;
                            coll->scriptReorderTable[toTop] = toTop--;
                        }
                        if(defaultScriptOrder[i] == *last){
                            filled[i] = true;
                            coll->scriptReorderTable[i] = toTop--;
                        }
                    }
                    --last;
                }
                break;
            }
            ++next;
        }

        /* Copy everything that's left over */
        for (i = 0; i < 256; i++){
            if(!filled[i]){
                coll->scriptReorderTable[i] = toBottom++;
            }
        }
    }else{
        if(coll->scriptReorderTable != NULL){
            uprv_free(coll->scriptReorderTable);
            coll->scriptReorderTable = NULL;
        }
    }
}

// API in ucol_imp.h

U_CFUNC UCollator*
ucol_open_internal(const char *loc,
                   UErrorCode *status)
{
    UErrorCode intStatus = U_ZERO_ERROR;
    const UCollator* UCA = ucol_initUCA(status);

    /* New version */
    if(U_FAILURE(*status)) return 0;



    UCollator *result = NULL;
    UResourceBundle *b = ures_open(U_ICUDATA_COLL, loc, status);

    /* we try to find stuff from keyword */
    UResourceBundle *collations = ures_getByKey(b, "collations", NULL, status);
    UResourceBundle *collElem = NULL;
    char keyBuffer[256];
    // if there is a keyword, we pick it up and try to get elements
    if(!uloc_getKeywordValue(loc, "collation", keyBuffer, 256, status) ||
        !uprv_strcmp(keyBuffer,"default")) { /* Treat 'zz@collation=default' as 'zz'. */
        // no keyword. we try to find the default setting, which will give us the keyword value
        intStatus = U_ZERO_ERROR;
        // finding default value does not affect collation fallback status
        UResourceBundle *defaultColl = ures_getByKeyWithFallback(collations, "default", NULL, &intStatus);
        if(U_SUCCESS(intStatus)) {
            int32_t defaultKeyLen = 0;
            const UChar *defaultKey = ures_getString(defaultColl, &defaultKeyLen, &intStatus);
            u_UCharsToChars(defaultKey, keyBuffer, defaultKeyLen);
            keyBuffer[defaultKeyLen] = 0;
        } else {
            *status = U_INTERNAL_PROGRAM_ERROR;
            return NULL;
        }
        ures_close(defaultColl);
    }
    collElem = ures_getByKeyWithFallback(collations, keyBuffer, collations, status);
    collations = NULL; // We just reused the collations object as collElem.

    UResourceBundle *binary = NULL;

    if(*status == U_MISSING_RESOURCE_ERROR) { /* We didn't find the tailoring data, we fallback to the UCA */
        *status = U_USING_DEFAULT_WARNING;
        result = ucol_initCollator(UCA->image, result, UCA, status);
        if (U_FAILURE(*status)) {
            goto clean;
        }
        // if we use UCA, real locale is root
        ures_close(b);
        b = ures_open(U_ICUDATA_COLL, "", status);
        ures_close(collElem);
        collElem = ures_open(U_ICUDATA_COLL, "", status);
        if(U_FAILURE(*status)) {
            goto clean;
        }
        result->hasRealData = FALSE;
    } else if(U_SUCCESS(*status)) {
        intStatus = U_ZERO_ERROR;

        binary = ures_getByKey(collElem, "%%CollationBin", NULL, &intStatus);

        if(intStatus == U_MISSING_RESOURCE_ERROR) { /* we didn't find the binary image, we should use the rules */
            binary = NULL;
            result = tryOpeningFromRules(collElem, status);
            if(U_FAILURE(*status)) {
                goto clean;
            }
        } else if(U_SUCCESS(intStatus)) { /* otherwise, we'll pick a collation data that exists */
            int32_t len = 0;
            const uint8_t *inData = ures_getBinary(binary, &len, status);
            if(U_FAILURE(*status)) {
                goto clean;
            }
            UCATableHeader *colData = (UCATableHeader *)inData;
            if(uprv_memcmp(colData->UCAVersion, UCA->image->UCAVersion, sizeof(UVersionInfo)) != 0 ||
                uprv_memcmp(colData->UCDVersion, UCA->image->UCDVersion, sizeof(UVersionInfo)) != 0 ||
                colData->version[0] != UCOL_BUILDER_VERSION)
            {
                *status = U_DIFFERENT_UCA_VERSION;
                result = tryOpeningFromRules(collElem, status);
            } else {
                if(U_FAILURE(*status)){
                    goto clean;
                }
                if((uint32_t)len > (paddedsize(sizeof(UCATableHeader)) + paddedsize(sizeof(UColOptionSet)))) {
                    result = ucol_initCollator((const UCATableHeader *)inData, result, UCA, status);
                    if(U_FAILURE(*status)){
                        goto clean;
                    }
                    result->hasRealData = TRUE;
                } else {
                    result = ucol_initCollator(UCA->image, result, UCA, status);
                    ucol_setOptionsFromHeader(result, (UColOptionSet *)(inData+((const UCATableHeader *)inData)->options), status);
                    if(U_FAILURE(*status)){
                        goto clean;
                    }
                    result->hasRealData = FALSE;
                }
                result->freeImageOnClose = FALSE;
            }
        } else { // !U_SUCCESS(binaryStatus)
            if(U_SUCCESS(*status)) {
                *status = intStatus; // propagate underlying error
            }
            goto clean;
        }
        intStatus = U_ZERO_ERROR;
        result->rules = ures_getStringByKey(collElem, "Sequence", &result->rulesLength, &intStatus);
        result->freeRulesOnClose = FALSE;
    } else { /* There is another error, and we're just gonna clean up */
        goto clean;
    }

    intStatus = U_ZERO_ERROR;
    result->ucaRules = ures_getStringByKey(b,"UCARules",NULL,&intStatus);

    if(loc == NULL) {
        loc = ures_getLocaleByType(b, ULOC_ACTUAL_LOCALE, status);
    }
    result->requestedLocale = uprv_strdup(loc);
    /* test for NULL */
    if (result->requestedLocale == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        goto clean;
    }
    loc = ures_getLocaleByType(collElem, ULOC_ACTUAL_LOCALE, status);
    result->actualLocale = uprv_strdup(loc);
    /* test for NULL */
    if (result->actualLocale == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        goto clean;
    }
    loc = ures_getLocaleByType(b, ULOC_ACTUAL_LOCALE, status);
    result->validLocale = uprv_strdup(loc);
    /* test for NULL */
    if (result->validLocale == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        goto clean;
    }

    ures_close(b);
    ures_close(collElem);
    ures_close(binary);
    return result;

clean:
    ures_close(b);
    ures_close(collElem);
    ures_close(binary);
    ucol_close(result);
    return NULL;
}

U_CAPI UCollator*
ucol_open(const char *loc,
          UErrorCode *status)
{
    U_NAMESPACE_USE

    UTRACE_ENTRY_OC(UTRACE_UCOL_OPEN);
    UTRACE_DATA1(UTRACE_INFO, "locale = \"%s\"", loc);
    UCollator *result = NULL;

#if !UCONFIG_NO_SERVICE
    result = Collator::createUCollator(loc, status);
    if (result == NULL)
#endif
    {
        result = ucol_open_internal(loc, status);
    }
    UTRACE_EXIT_PTR_STATUS(result, *status);
    return result;
}


UCollator*
ucol_openRulesForImport( const UChar        *rules,
                         int32_t            rulesLength,
                         UColAttributeValue normalizationMode,
                         UCollationStrength strength,
                         UParseError        *parseError,
                         GetCollationRulesFunction  importFunc,
                         void* context,
                         UErrorCode         *status)
{
    UColTokenParser src;
    UColAttributeValue norm;
    UParseError tErr;

    if(status == NULL || U_FAILURE(*status)){
        return 0;
    }

    if(rules == NULL || rulesLength < -1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if(rulesLength == -1) {
        rulesLength = u_strlen(rules);
    }

    if(parseError == NULL){
        parseError = &tErr;
    }

    switch(normalizationMode) {
    case UCOL_OFF:
    case UCOL_ON:
    case UCOL_DEFAULT:
        norm = normalizationMode;
        break;
    default:
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UCollator *result = NULL;
    UCATableHeader *table = NULL;
    UCollator *UCA = ucol_initUCA(status);

    if(U_FAILURE(*status)){
        return NULL;
    }

    ucol_tok_initTokenList(&src, rules, rulesLength, UCA, importFunc, context, status);
    ucol_tok_assembleTokenList(&src,parseError, status);

    if(U_FAILURE(*status)) {
        /* if status is U_ILLEGAL_ARGUMENT_ERROR, src->current points at the offending option */
        /* if status is U_INVALID_FORMAT_ERROR, src->current points after the problematic part of the rules */
        /* so something might be done here... or on lower level */
#ifdef UCOL_DEBUG
        if(*status == U_ILLEGAL_ARGUMENT_ERROR) {
            fprintf(stderr, "bad option starting at offset %i\n", src.current-src.source);
        } else {
            fprintf(stderr, "invalid rule just before offset %i\n", src.current-src.source);
        }
#endif
        goto cleanup;
    }

    if(src.resultLen > 0 || src.removeSet != NULL) { /* we have a set of rules, let's make something of it */
        /* also, if we wanted to remove some contractions, we should make a tailoring */
        table = ucol_assembleTailoringTable(&src, status);
        if(U_SUCCESS(*status)) {
            // builder version
            table->version[0] = UCOL_BUILDER_VERSION;
            // no tailoring information on this level
            table->version[1] = table->version[2] = table->version[3] = 0;
            // set UCD version
            u_getUnicodeVersion(table->UCDVersion);
            // set UCA version
            uprv_memcpy(table->UCAVersion, UCA->image->UCAVersion, sizeof(UVersionInfo));
            result = ucol_initCollator(table, 0, UCA, status);
            if (U_FAILURE(*status)) {
                goto cleanup;
            }
            result->hasRealData = TRUE;
            result->freeImageOnClose = TRUE;
        }
    } else { /* no rules, but no error either */
        // must be only options
        // We will init the collator from UCA
        result = ucol_initCollator(UCA->image, 0, UCA, status);
        // Check for null result
        if (U_FAILURE(*status)) {
            goto cleanup;
        }
        // And set only the options
        UColOptionSet *opts = (UColOptionSet *)uprv_malloc(sizeof(UColOptionSet));
        /* test for NULL */
        if (opts == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            goto cleanup;
        }
        uprv_memcpy(opts, src.opts, sizeof(UColOptionSet));
        ucol_setOptionsFromHeader(result, opts, status);
        result->freeOptionsOnClose = TRUE;
        result->hasRealData = FALSE;
        result->freeImageOnClose = FALSE;
    }

    if(U_SUCCESS(*status)) {
        UChar *newRules;
        result->dataVersion[0] = UCOL_BUILDER_VERSION;
        if(rulesLength > 0) {
            newRules = (UChar *)uprv_malloc((rulesLength+1)*U_SIZEOF_UCHAR);
            /* test for NULL */
            if (newRules == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                goto cleanup;
            }
            uprv_memcpy(newRules, rules, rulesLength*U_SIZEOF_UCHAR);
            newRules[rulesLength]=0;
            result->rules = newRules;
            result->rulesLength = rulesLength;
            result->freeRulesOnClose = TRUE;
        }
        result->ucaRules = NULL;
        result->actualLocale = NULL;
        result->validLocale = NULL;
        result->requestedLocale = NULL;
        ucol_setAttribute(result, UCOL_STRENGTH, strength, status);
        ucol_setAttribute(result, UCOL_NORMALIZATION_MODE, norm, status);
        ucol_buildScriptReorderTable(result);
    } else {
cleanup:
        if(result != NULL) {
            ucol_close(result);
        } else {
            if(table != NULL) {
                uprv_free(table);
            }
        }
        result = NULL;
    }

    ucol_tok_closeTokenList(&src);

    return result;
}

U_CAPI UCollator* U_EXPORT2
ucol_openRules( const UChar        *rules,
               int32_t            rulesLength,
               UColAttributeValue normalizationMode,
               UCollationStrength strength,
               UParseError        *parseError,
               UErrorCode         *status)
{
    return ucol_openRulesForImport(rules,
                                   rulesLength,
                                   normalizationMode,
                                   strength,
                                   parseError,
                                   ucol_tok_getRulesFromBundle,
                                   NULL,
                                   status);
}

U_CAPI int32_t U_EXPORT2
ucol_getRulesEx(const UCollator *coll, UColRuleOption delta, UChar *buffer, int32_t bufferLen) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = 0;
    int32_t UCAlen = 0;
    const UChar* ucaRules = 0;
    const UChar *rules = ucol_getRules(coll, &len);
    if(delta == UCOL_FULL_RULES) {
        /* take the UCA rules and append real rules at the end */
        /* UCA rules will be probably coming from the root RB */
        ucaRules = coll->ucaRules;
        if (ucaRules) {
            UCAlen = u_strlen(ucaRules);
        }
        /*
        ucaRules = ures_getStringByKey(coll->rb,"UCARules",&UCAlen,&status);
        UResourceBundle* cresb = ures_getByKeyWithFallback(coll->rb, "collations", NULL, &status);
        UResourceBundle*  uca = ures_getByKeyWithFallback(cresb, "UCA", NULL, &status);
        ucaRules = ures_getStringByKey(uca,"Sequence",&UCAlen,&status);
        ures_close(uca);
        ures_close(cresb);
        */
    }
    if(U_FAILURE(status)) {
        return 0;
    }
    if(buffer!=0 && bufferLen>0){
        *buffer=0;
        if(UCAlen > 0) {
            u_memcpy(buffer, ucaRules, uprv_min(UCAlen, bufferLen));
        }
        if(len > 0 && bufferLen > UCAlen) {
            u_memcpy(buffer+UCAlen, rules, uprv_min(len, bufferLen-UCAlen));
        }
    }
    return u_terminateUChars(buffer, bufferLen, len+UCAlen, &status);
}

static const UChar _NUL = 0;

U_CAPI const UChar* U_EXPORT2
ucol_getRules(    const    UCollator       *coll,
              int32_t            *length)
{
    if(coll->rules != NULL) {
        *length = coll->rulesLength;
        return coll->rules;
    }
    else {
        *length = 0;
        return &_NUL;
    }
}

U_CAPI UBool U_EXPORT2
ucol_equals(const UCollator *source, const UCollator *target) {
    UErrorCode status = U_ZERO_ERROR;
    // if pointers are equal, collators are equal
    if(source == target) {
        return TRUE;
    }
    int32_t i = 0, j = 0;
    // if any of attributes are different, collators are not equal
    for(i = 0; i < UCOL_ATTRIBUTE_COUNT; i++) {
        if(ucol_getAttribute(source, (UColAttribute)i, &status) != ucol_getAttribute(target, (UColAttribute)i, &status) || U_FAILURE(status)) {
            return FALSE;
        }
    }
    if(source->scriptOrderLength != target->scriptOrderLength){
        return FALSE;
    }
    for(int i = 0; i < source->scriptOrderLength; i++){
        if(source->scriptOrder[i] != target->scriptOrder[i]){
            return FALSE;
        }
    }

    int32_t sourceRulesLen = 0, targetRulesLen = 0;
    const UChar *sourceRules = ucol_getRules(source, &sourceRulesLen);
    const UChar *targetRules = ucol_getRules(target, &targetRulesLen);

    if(sourceRulesLen == targetRulesLen && u_strncmp(sourceRules, targetRules, sourceRulesLen) == 0) {
        // all the attributes are equal and the rules are equal - collators are equal
        return(TRUE);
    }
    // hard part, need to construct tree from rules and see if they yield the same tailoring
    UBool result = TRUE;
    UParseError parseError;
    UColTokenParser sourceParser, targetParser;
    int32_t sourceListLen = 0, targetListLen = 0;
    ucol_tok_initTokenList(&sourceParser, sourceRules, sourceRulesLen, source->UCA, ucol_tok_getRulesFromBundle, NULL, &status);
    ucol_tok_initTokenList(&targetParser, targetRules, targetRulesLen, target->UCA, ucol_tok_getRulesFromBundle, NULL, &status);
    sourceListLen = ucol_tok_assembleTokenList(&sourceParser, &parseError, &status);
    targetListLen = ucol_tok_assembleTokenList(&targetParser, &parseError, &status);

    if(sourceListLen != targetListLen) {
        // different number of resets
        result = FALSE;
    } else {
        UColToken *sourceReset = NULL, *targetReset = NULL;
        UChar *sourceResetString = NULL, *targetResetString = NULL;
        int32_t sourceStringLen = 0, targetStringLen = 0;
        for(i = 0; i < sourceListLen; i++) {
            sourceReset = sourceParser.lh[i].reset;
            sourceResetString = sourceParser.source+(sourceReset->source & 0xFFFFFF);
            sourceStringLen = sourceReset->source >> 24;
            for(j = 0; j < sourceListLen; j++) {
                targetReset = targetParser.lh[j].reset;
                targetResetString = targetParser.source+(targetReset->source & 0xFFFFFF);
                targetStringLen = targetReset->source >> 24;
                if(sourceStringLen == targetStringLen && (u_strncmp(sourceResetString, targetResetString, sourceStringLen) == 0)) {
                    sourceReset = sourceParser.lh[i].first;
                    targetReset = targetParser.lh[j].first;
                    while(sourceReset != NULL && targetReset != NULL) {
                        sourceResetString = sourceParser.source+(sourceReset->source & 0xFFFFFF);
                        sourceStringLen = sourceReset->source >> 24;
                        targetResetString = targetParser.source+(targetReset->source & 0xFFFFFF);
                        targetStringLen = targetReset->source >> 24;
                        if(sourceStringLen != targetStringLen || (u_strncmp(sourceResetString, targetResetString, sourceStringLen) != 0)) {
                            result = FALSE;
                            goto returnResult;
                        }
                        // probably also need to check the expansions
                        if(sourceReset->expansion) {
                            if(!targetReset->expansion) {
                                result = FALSE;
                                goto returnResult;
                            } else {
                                // compare expansions
                                sourceResetString = sourceParser.source+(sourceReset->expansion& 0xFFFFFF);
                                sourceStringLen = sourceReset->expansion >> 24;
                                targetResetString = targetParser.source+(targetReset->expansion & 0xFFFFFF);
                                targetStringLen = targetReset->expansion >> 24;
                                if(sourceStringLen != targetStringLen || (u_strncmp(sourceResetString, targetResetString, sourceStringLen) != 0)) {
                                    result = FALSE;
                                    goto returnResult;
                                }
                            }
                        } else {
                            if(targetReset->expansion) {
                                result = FALSE;
                                goto returnResult;
                            }
                        }
                        sourceReset = sourceReset->next;
                        targetReset = targetReset->next;
                    }
                    if(sourceReset != targetReset) { // at least one is not NULL
                        // there are more tailored elements in one list
                        result = FALSE;
                        goto returnResult;
                    }


                    break;
                }
            }
            // couldn't find the reset anchor, so the collators are not equal
            if(j == sourceListLen) {
                result = FALSE;
                goto returnResult;
            }
        }
    }

returnResult:
    ucol_tok_closeTokenList(&sourceParser);
    ucol_tok_closeTokenList(&targetParser);
    return result;

}

U_CAPI int32_t U_EXPORT2
ucol_getDisplayName(    const    char        *objLoc,
                    const    char        *dispLoc,
                    UChar             *result,
                    int32_t         resultLength,
                    UErrorCode        *status)
{
    U_NAMESPACE_USE

    if(U_FAILURE(*status)) return -1;
    UnicodeString dst;
    if(!(result==NULL && resultLength==0)) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer
        dst.setTo(result, 0, resultLength);
    }
    Collator::getDisplayName(Locale(objLoc), Locale(dispLoc), dst);
    return dst.extract(result, resultLength, *status);
}

U_CAPI const char* U_EXPORT2
ucol_getAvailable(int32_t index)
{
    int32_t count = 0;
    const Locale *loc = Collator::getAvailableLocales(count);
    if (loc != NULL && index < count) {
        return loc[index].getName();
    }
    return NULL;
}

U_CAPI int32_t U_EXPORT2
ucol_countAvailable()
{
    int32_t count = 0;
    Collator::getAvailableLocales(count);
    return count;
}

#if !UCONFIG_NO_SERVICE
U_CAPI UEnumeration* U_EXPORT2
ucol_openAvailableLocales(UErrorCode *status) {
    U_NAMESPACE_USE

    // This is a wrapper over Collator::getAvailableLocales()
    if (U_FAILURE(*status)) {
        return NULL;
    }
    StringEnumeration *s = Collator::getAvailableLocales();
    if (s == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return uenum_openFromStringEnumeration(s, status);
}
#endif

// Note: KEYWORDS[0] != RESOURCE_NAME - alan

static const char RESOURCE_NAME[] = "collations";

static const char* const KEYWORDS[] = { "collation" };

#define KEYWORD_COUNT (sizeof(KEYWORDS)/sizeof(KEYWORDS[0]))

U_CAPI UEnumeration* U_EXPORT2
ucol_getKeywords(UErrorCode *status) {
    UEnumeration *result = NULL;
    if (U_SUCCESS(*status)) {
        return uenum_openCharStringsEnumeration(KEYWORDS, KEYWORD_COUNT, status);
    }
    return result;
}

U_CAPI UEnumeration* U_EXPORT2
ucol_getKeywordValues(const char *keyword, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    // hard-coded to accept exactly one collation keyword
    // modify if additional collation keyword is added later
    if (keyword==NULL || uprv_strcmp(keyword, KEYWORDS[0])!=0)
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    return ures_getKeywordValues(U_ICUDATA_COLL, RESOURCE_NAME, status);
}

static const UEnumeration defaultKeywordValues = {
    NULL,
    NULL,
    ulist_close_keyword_values_iterator,
    ulist_count_keyword_values,
    uenum_unextDefault,
    ulist_next_keyword_value,
    ulist_reset_keyword_values_iterator
};

U_CAPI UEnumeration* U_EXPORT2
ucol_getKeywordValuesForLocale(const char* /*key*/, const char* locale,
                               UBool /*commonlyUsed*/, UErrorCode* status) {
    /* Get the locale base name. */
    char localeBuffer[ULOC_FULLNAME_CAPACITY] = "";
    uloc_getBaseName(locale, localeBuffer, sizeof(localeBuffer), status);

    /* Create the 2 lists
     * -values is the temp location for the keyword values
     * -results hold the actual list used by the UEnumeration object
     */
    UList *values = ulist_createEmptyList(status);
    UList *results = ulist_createEmptyList(status);
    UEnumeration *en = (UEnumeration *)uprv_malloc(sizeof(UEnumeration));
    if (U_FAILURE(*status) || en == NULL) {
        if (en == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
        } else {
            uprv_free(en);
        }
        ulist_deleteList(values);
        ulist_deleteList(results);
        return NULL;
    }

    memcpy(en, &defaultKeywordValues, sizeof(UEnumeration));
    en->context = results;

    /* Open the resource bundle for collation with the given locale. */
    UResourceBundle bundle, collations, collres, defres;
    ures_initStackObject(&bundle);
    ures_initStackObject(&collations);
    ures_initStackObject(&collres);
    ures_initStackObject(&defres);

    ures_openFillIn(&bundle, U_ICUDATA_COLL, localeBuffer, status);

    while (U_SUCCESS(*status)) {
        ures_getByKey(&bundle, RESOURCE_NAME, &collations, status);
        ures_resetIterator(&collations);
        while (U_SUCCESS(*status) && ures_hasNext(&collations)) {
            ures_getNextResource(&collations, &collres, status);
            const char *key = ures_getKey(&collres);
            /* If the key is default, get the string and store it in results list only
             * if results list is empty.
             */
            if (uprv_strcmp(key, "default") == 0) {
                if (ulist_getListSize(results) == 0) {
                    char *defcoll = (char *)uprv_malloc(sizeof(char) * ULOC_KEYWORDS_CAPACITY);
                    int32_t defcollLength = ULOC_KEYWORDS_CAPACITY;

                    ures_getNextResource(&collres, &defres, status);
                    ures_getUTF8String(&defres, defcoll, &defcollLength, TRUE, status);

                    ulist_addItemBeginList(results, defcoll, TRUE, status);
                }
            } else {
                ulist_addItemEndList(values, key, FALSE, status);
            }
        }

        /* If the locale is "" this is root so exit. */
        if (uprv_strlen(localeBuffer) == 0) {
            break;
        }
        /* Get the parent locale and open a new resource bundle. */
        uloc_getParent(localeBuffer, localeBuffer, sizeof(localeBuffer), status);
        ures_openFillIn(&bundle, U_ICUDATA_COLL, localeBuffer, status);
    }

    ures_close(&defres);
    ures_close(&collres);
    ures_close(&collations);
    ures_close(&bundle);

    if (U_SUCCESS(*status)) {
        char *value = NULL;
        ulist_resetList(values);
        while ((value = (char *)ulist_getNext(values)) != NULL) {
            if (!ulist_containsString(results, value, uprv_strlen(value))) {
                ulist_addItemEndList(results, value, FALSE, status);
                if (U_FAILURE(*status)) {
                    break;
                }
            }
        }
    }

    ulist_deleteList(values);

    if (U_FAILURE(*status)){
        uenum_close(en);
        en = NULL;
    } else {
        ulist_resetList(results);
    }

    return en;
}

U_CAPI int32_t U_EXPORT2
ucol_getFunctionalEquivalent(char* result, int32_t resultCapacity,
                             const char* keyword, const char* locale,
                             UBool* isAvailable, UErrorCode* status)
{
    // N.B.: Resource name is "collations" but keyword is "collation"
    return ures_getFunctionalEquivalent(result, resultCapacity, U_ICUDATA_COLL,
        "collations", keyword, locale,
        isAvailable, TRUE, status);
}

/* returns the locale name the collation data comes from */
U_CAPI const char * U_EXPORT2
ucol_getLocale(const UCollator *coll, ULocDataLocaleType type, UErrorCode *status) {
    return ucol_getLocaleByType(coll, type, status);
}

U_CAPI const char * U_EXPORT2
ucol_getLocaleByType(const UCollator *coll, ULocDataLocaleType type, UErrorCode *status) {
    const char *result = NULL;
    if(status == NULL || U_FAILURE(*status)) {
        return NULL;
    }
    UTRACE_ENTRY(UTRACE_UCOL_GETLOCALE);
    UTRACE_DATA1(UTRACE_INFO, "coll=%p", coll);

    switch(type) {
    case ULOC_ACTUAL_LOCALE:
        result = coll->actualLocale;
        break;
    case ULOC_VALID_LOCALE:
        result = coll->validLocale;
        break;
    case ULOC_REQUESTED_LOCALE:
        result = coll->requestedLocale;
        break;
    default:
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    UTRACE_DATA1(UTRACE_INFO, "result = %s", result);
    UTRACE_EXIT_STATUS(*status);
    return result;
}

U_CFUNC void U_EXPORT2
ucol_setReqValidLocales(UCollator *coll, char *requestedLocaleToAdopt, char *validLocaleToAdopt, char *actualLocaleToAdopt)
{
    if (coll) {
        if (coll->validLocale) {
            uprv_free(coll->validLocale);
        }
        coll->validLocale = validLocaleToAdopt;
        if (coll->requestedLocale) { // should always have
            uprv_free(coll->requestedLocale);
        }
        coll->requestedLocale = requestedLocaleToAdopt;
        if (coll->actualLocale) {
            uprv_free(coll->actualLocale);
        }
        coll->actualLocale = actualLocaleToAdopt;
    }
}

U_CAPI USet * U_EXPORT2
ucol_getTailoredSet(const UCollator *coll, UErrorCode *status)
{
    U_NAMESPACE_USE

    if(status == NULL || U_FAILURE(*status)) {
        return NULL;
    }
    if(coll == NULL || coll->UCA == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    UParseError parseError;
    UColTokenParser src;
    int32_t rulesLen = 0;
    const UChar *rules = ucol_getRules(coll, &rulesLen);
    UBool startOfRules = TRUE;
    // we internally use the C++ class, for the following reasons:
    // 1. we need to utilize canonical iterator, which is a C++ only class
    // 2. canonical iterator returns UnicodeStrings - USet cannot take them
    // 3. USet is internally really UnicodeSet, C is just a wrapper
    UnicodeSet *tailored = new UnicodeSet();
    UnicodeString pattern;
    UnicodeString empty;
    CanonicalIterator it(empty, *status);


    // The idea is to tokenize the rule set. For each non-reset token,
    // we add all the canonicaly equivalent FCD sequences
    ucol_tok_initTokenList(&src, rules, rulesLen, coll->UCA, ucol_tok_getRulesFromBundle, NULL, status);
    while (ucol_tok_parseNextToken(&src, startOfRules, &parseError, status) != NULL) {
        startOfRules = FALSE;
        if(src.parsedToken.strength != UCOL_TOK_RESET) {
            const UChar *stuff = src.source+(src.parsedToken.charsOffset);
            it.setSource(UnicodeString(stuff, src.parsedToken.charsLen), *status);
            pattern = it.next();
            while(!pattern.isBogus()) {
                if(Normalizer::quickCheck(pattern, UNORM_FCD, *status) != UNORM_NO) {
                    tailored->add(pattern);
                }
                pattern = it.next();
            }
        }
    }
    ucol_tok_closeTokenList(&src);
    return (USet *)tailored;
}

#endif /* #if !UCONFIG_NO_COLLATION */
