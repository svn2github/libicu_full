/*
 **********************************************************************
 *   Copyright (C) 2005-2013, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucsdet.h"

#include "csdetect.h"
#include "csmatch.h"
#include "uenumimp.h"

#include "cmemory.h"
#include "cstring.h"
#include "umutex.h"
#include "ucln_in.h"
#include "uarrsort.h"
#include "inputext.h"
#include "csrsbcs.h"
#include "csrmbcs.h"
#include "csrutf8.h"
#include "csrucode.h"
#include "csr2022.h"

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

#define NEW_ARRAY(type,count) (type *) uprv_malloc((count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *) (array))

static icu::CharsetRecognizer **fCSRecognizers = NULL;
static UInitOnce gCSRecognizersInitOnce;
static int32_t fCSRecognizers_size = 0;


typedef struct CharsetRecogFlag {
    const char *encoding;
    UBool enabled;
	} CharsetRecogFlag;
CharsetRecogFlag **CharsetRecogStatus = NULL;
UBool curCharsetRecogFlag = FALSE;
int32_t curfCSRecognizers_size = 0;
char **curEncodingList = NULL;

U_CDECL_BEGIN
static UBool U_CALLCONV csdet_cleanup(void)
{
    U_NAMESPACE_USE
    if (fCSRecognizers != NULL) {
        for(int32_t r = 0; r < fCSRecognizers_size; r += 1) {
            delete fCSRecognizers[r];
            fCSRecognizers[r] = NULL;
        }

        DELETE_ARRAY(fCSRecognizers);
        fCSRecognizers = NULL;
        fCSRecognizers_size = 0;
    }
    gCSRecognizersInitOnce.reset();

    return TRUE;
}

static int32_t U_CALLCONV
charsetMatchComparator(const void * /*context*/, const void *left, const void *right)
{
    U_NAMESPACE_USE

    const CharsetMatch **csm_l = (const CharsetMatch **) left;
    const CharsetMatch **csm_r = (const CharsetMatch **) right;

    // NOTE: compare is backwards to sort from highest to lowest.
    return (*csm_r)->getConfidence() - (*csm_l)->getConfidence();
}

static void U_CALLCONV initRecognizers(UErrorCode &status) {
    U_NAMESPACE_USE
    ucln_i18n_registerCleanup(UCLN_I18N_CSDET, csdet_cleanup);
    CharsetRecognizer *tempArray[] = {
        new CharsetRecog_UTF8(),

        new CharsetRecog_UTF_16_BE(),
        new CharsetRecog_UTF_16_LE(),
        new CharsetRecog_UTF_32_BE(),
        new CharsetRecog_UTF_32_LE(),

        new CharsetRecog_8859_1(),
        new CharsetRecog_8859_2(),
        new CharsetRecog_8859_5_ru(),
        new CharsetRecog_8859_6_ar(),
        new CharsetRecog_8859_7_el(),
        new CharsetRecog_8859_8_I_he(),
        new CharsetRecog_8859_8_he(),
        new CharsetRecog_windows_1251(),
        new CharsetRecog_windows_1256(),
        new CharsetRecog_KOI8_R(),
        new CharsetRecog_8859_9_tr(),
        new CharsetRecog_sjis(),
        new CharsetRecog_gb_18030(),
        new CharsetRecog_euc_jp(),
        new CharsetRecog_euc_kr(),
        new CharsetRecog_big5(),

        new CharsetRecog_2022JP(),
        new CharsetRecog_2022KR(),
        new CharsetRecog_2022CN(),

        new CharsetRecog_IBM424_he_rtl(),
        new CharsetRecog_IBM424_he_ltr(),
        new CharsetRecog_IBM420_ar_rtl(),
        new CharsetRecog_IBM420_ar_ltr()
    };
    int32_t rCount = ARRAY_SIZE(tempArray);

    fCSRecognizers = NEW_ARRAY(CharsetRecognizer *, rCount);
	
    if (fCSRecognizers == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } 
	else {
        fCSRecognizers_size = rCount;
        for (int32_t r = 0; r < rCount; r += 1) {
            fCSRecognizers[r] = tempArray[r];
			if (fCSRecognizers[r] == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
            }
        }
    }
}

U_CDECL_END

U_NAMESPACE_BEGIN

void CharsetDetector::setRecognizers(UErrorCode &status)
{
    umtx_initOnce(gCSRecognizersInitOnce, &initRecognizers, status);
}

CharsetDetector::CharsetDetector(UErrorCode &status)
  : textIn(new InputText(status)), resultArray(NULL),
    resultCount(0), fStripTags(FALSE), fFreshTextSet(FALSE)
{
    if (U_FAILURE(status)) {
        return;
    }

    setRecognizers(status);

    if (U_FAILURE(status)) {
        return;
    }

    resultArray = (CharsetMatch **)uprv_malloc(sizeof(CharsetMatch *)*fCSRecognizers_size);

    if (resultArray == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    for(int32_t i = 0; i < fCSRecognizers_size; i += 1) {
        resultArray[i] = new CharsetMatch();

        if (resultArray[i] == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            break;
        }
    }

	CharsetRecogStatus = NEW_ARRAY(CharsetRecogFlag *,fCSRecognizers_size);
	if (CharsetRecogStatus == NULL)
		{ status = U_MEMORY_ALLOCATION_ERROR;
			return;
		}
	else {
		for (int32_t r=0;r<fCSRecognizers_size;r++)
		{
		CharsetRecogStatus[r] = NEW_ARRAY(CharsetRecogFlag,1);
		if (CharsetRecogStatus[r] == NULL)
			{	status = U_MEMORY_ALLOCATION_ERROR;
				break;
			}
		CharsetRecogStatus[r]->encoding = fCSRecognizers[r]->getName();
		if (uprv_strcmp(fCSRecognizers[r]->getName(),"IBM424_rtl") == 0 || uprv_strcmp(fCSRecognizers[r]->getName(),"IBM424_ltr") == 0
			|| uprv_strcmp(fCSRecognizers[r]->getName(),"IBM420_rtl")== 0 || uprv_strcmp(fCSRecognizers[r]->getName(),"IBM420_ltr") == 0)
			{ 
				CharsetRecogStatus[r]->enabled = FALSE;
			}
		else 
			{
				CharsetRecogStatus[r]->enabled = TRUE;
			}
		}
	}
	curfCSRecognizers_size =0;
	for (int32_t r=0; r<fCSRecognizers_size;r++)
	{
		 if (CharsetRecogStatus[r]->enabled)
			curfCSRecognizers_size+=1;
	}		
	curEncodingList = NEW_ARRAY(char *, curfCSRecognizers_size);
	if (curEncodingList == NULL)
		{ status = U_MEMORY_ALLOCATION_ERROR;
		}
	else {
	int32_t i = 0;
	for (int32_t r=0; r<fCSRecognizers_size; r++)
	 {
		 if ((CharsetRecogStatus[r]->enabled))
		 {
			 curEncodingList[i] = (char*) CharsetRecogStatus[r]->encoding;
			 i++;
		 }
	 } 
	}
}

CharsetDetector::~CharsetDetector()
{
    delete textIn;

    for(int32_t i = 0; i < fCSRecognizers_size; i += 1) {
        delete resultArray[i];
    }

    uprv_free(resultArray);

	 if (curEncodingList != NULL) {
        for(int32_t r = 0; r < curfCSRecognizers_size; r += 1) {
          curEncodingList[r] = NULL;
        }

        DELETE_ARRAY(curEncodingList);
        curEncodingList = NULL;
	}
	if (CharsetRecogStatus != NULL) {
        for(int32_t r = 0; r < fCSRecognizers_size; r += 1) {
            DELETE_ARRAY(CharsetRecogStatus[r]);
            CharsetRecogStatus[r] = NULL;
        }

        DELETE_ARRAY(CharsetRecogStatus);
        CharsetRecogStatus = NULL;
	}
}

void CharsetDetector::setText(const char *in, int32_t len)
{
    textIn->setText(in, len);
    fFreshTextSet = TRUE;
}

UBool CharsetDetector::setStripTagsFlag(UBool flag)
{
    UBool temp = fStripTags;
    fStripTags = flag;
    fFreshTextSet = TRUE;
    return temp;
}

UBool CharsetDetector::getStripTagsFlag() const
{
    return fStripTags;
}

void CharsetDetector::setDeclaredEncoding(const char *encoding, int32_t len) const
{
    textIn->setDeclaredEncoding(encoding,len);
}

int32_t CharsetDetector::getDetectableCount()
{
    UErrorCode status = U_ZERO_ERROR;

    setRecognizers(status);

    return fCSRecognizers_size; 
}

const CharsetMatch *CharsetDetector::detect(UErrorCode &status)
{
    int32_t maxMatchesFound = 0;

    detectAll(maxMatchesFound, status);

    if(maxMatchesFound > 0) {
        return resultArray[0];
    } else {
        return NULL;
    }
}

const CharsetMatch * const *CharsetDetector::detectAll(int32_t &maxMatchesFound, UErrorCode &status)
{
    if(!textIn->isSet()) {
        status = U_MISSING_RESOURCE_ERROR;// TODO:  Need to set proper status code for input text not set

        return NULL;
    } else if (fFreshTextSet) {
        CharsetRecognizer *csr;
        int32_t            i;

        textIn->MungeInput(fStripTags);

        // Iterate over all possible charsets, remember all that
        // give a match quality > 0.
        resultCount = 0;
        for (i = 0; i < fCSRecognizers_size; i += 1) {
			if (CharsetRecogStatus[i]->enabled) {
				csr = fCSRecognizers[i];
				if (csr->match(textIn, resultArray[resultCount])) {
					resultCount++;
				}
			}
        }

        if (resultCount > 1) {
            uprv_sortArray(resultArray, resultCount, sizeof resultArray[0], charsetMatchComparator, NULL, TRUE, &status);
        }
        fFreshTextSet = FALSE;
    }

    maxMatchesFound = resultCount;

    return resultArray;
}

void CharsetDetector::setDetectableCharset(const char *encoding, UBool enabled, UErrorCode &status)
{
		// to check if argument "encoding" is a valid encoding. flag status to U_ILLEGAL_ARGUMENT_ERROR
        UBool ValidEncodingFlag = FALSE;
        if (!ValidEncodingFlag) 
		{
            for (int32_t r =0; r<fCSRecognizers_size;r++)
			{
   				if (uprv_strcmp(fCSRecognizers[r]->getName(), encoding) == 0)
				{ 
                    ValidEncodingFlag = true;
                    break;
                }      
            }
        }
        if (!ValidEncodingFlag)
          { status = U_ILLEGAL_ARGUMENT_ERROR;
			return;
		  }

         // set the value of enabled
        for (int32_t r=0;r<fCSRecognizers_size;r++)
		{
            if(uprv_strcmp(CharsetRecogStatus[r]->encoding, encoding) == 0) 
				{
                       //  set value of enabled
					 CharsetRecogStatus[r]->enabled = enabled;
                     break;
                }
        }

	 if (curEncodingList != NULL) {
        for(int32_t r = 0; r < curfCSRecognizers_size; r += 1) {
            curEncodingList[r] = NULL;
        }

        DELETE_ARRAY(curEncodingList);
        curEncodingList = NULL;
	}
	 	curfCSRecognizers_size =0;
	 for (int32_t r=0; r<fCSRecognizers_size;r++)
	 {
		 if (CharsetRecogStatus[r]->enabled) {
			curfCSRecognizers_size+=1;
		 }
	 }
	 int32_t i = 0;
	 curEncodingList = NEW_ARRAY(char *, curfCSRecognizers_size);
	 if (curEncodingList == NULL)
		{ status = U_MEMORY_ALLOCATION_ERROR;
		}
	 else 
	 {
		for (int32_t r=0; r<fCSRecognizers_size; r++)
		{
				 if ((CharsetRecogStatus[r]->enabled))
			 {
				 curEncodingList[i] = (char *) CharsetRecogStatus[r]->encoding;
				 i++;
			}
		}
	 }
	
}
/*const char *CharsetDetector::getCharsetName(int32_t index, UErrorCode &status) const
{
    if( index > fCSRecognizers_size-1 || index < 0) {
        status = U_INDEX_OUTOFBOUNDS_ERROR;

        return 0;
    } else {
        return fCSRecognizers[index]->getName();
    }
}*/

U_NAMESPACE_END

U_CDECL_BEGIN
typedef struct {
    int32_t currIndex;
} Context;



static void U_CALLCONV
enumClose(UEnumeration *en) {
    if(en->context != NULL) {
        DELETE_ARRAY(en->context);
    }

    DELETE_ARRAY(en);
}

static int32_t U_CALLCONV
enumCount(UEnumeration *, UErrorCode *) {
	if (curCharsetRecogFlag)
		return curfCSRecognizers_size;
	else
		return fCSRecognizers_size;
}

static const char* U_CALLCONV
enumNext(UEnumeration *en, int32_t *resultLength, UErrorCode * /*status*/) {
	const char *currName;
	if (curCharsetRecogFlag)
	{
		if(((Context *)en->context)->currIndex >= curfCSRecognizers_size) {
		    if(resultLength != NULL) {
            *resultLength = 0;
		   }
        return NULL;
		}
		currName = (const char *)curEncodingList[((Context *)en->context)->currIndex];

	}
	else 
	{
		if(((Context *)en->context)->currIndex >= fCSRecognizers_size) {
		    if(resultLength != NULL) {
            *resultLength = 0;
		   }
        return NULL;
		}
		currName = fCSRecognizers[((Context *)en->context)->currIndex]->getName();
	}
    if(resultLength != NULL) {
        *resultLength = (int32_t)uprv_strlen(currName);
    }
    ((Context *)en->context)->currIndex++;

    return currName;
}


static void U_CALLCONV
enumReset(UEnumeration *en, UErrorCode *) {
    ((Context *)en->context)->currIndex = 0;
}

static const UEnumeration gCSDetEnumeration = {
    NULL,
    NULL,
    enumClose,
    enumCount,
    uenum_unextDefault,
    enumNext,
    enumReset
};


U_CAPI  UEnumeration * U_EXPORT2
ucsdet_getAllDetectableCharsets(const UCharsetDetector * /*ucsd*/, UErrorCode *status)
{
    U_NAMESPACE_USE
	curCharsetRecogFlag = FALSE;
    if(U_FAILURE(*status)) {
        return 0;
    }
	
	/* Initialize recognized charsets. */
    CharsetDetector::getDetectableCount();

    UEnumeration *en = NEW_ARRAY(UEnumeration, 1);
    memcpy(en, &gCSDetEnumeration, sizeof(UEnumeration));
    en->context = (void*)NEW_ARRAY(Context, 1);
    uprv_memset(en->context, 0, sizeof(Context));
    return en;
}

U_DRAFT UEnumeration * U_EXPORT2
ucsdet_getDetectableCharsets(const UCharsetDetector *ucsd,  UErrorCode *status)
{
	U_NAMESPACE_USE
	curCharsetRecogFlag = TRUE;

    if(U_FAILURE(*status)) {
        return 0;
    }

    /* Initialize recognized charsets. */
    CharsetDetector::getDetectableCount();

    UEnumeration *en = NEW_ARRAY(UEnumeration, 1);
    memcpy(en, &gCSDetEnumeration, sizeof(UEnumeration));
    en->context = (void*)NEW_ARRAY(Context, 1);
    uprv_memset(en->context, 0, sizeof(Context));
    return en;
}


U_CDECL_END

#endif