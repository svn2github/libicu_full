/*
***************************************************************************
* Copyright (C) 2008-2013, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*   file name:  uspoof.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2008Feb13
*   created by: Andy Heninger
*
*   Unicode Spoof Detection
*/
#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/unorm.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "identifier_info.h"
#include "mutex.h"
#include "scriptset.h"
#include "uassert.h"
#include "ucln_in.h"
#include "uspoof_impl.h"
#include "umutex.h"


#if !UCONFIG_NO_NORMALIZATION

U_NAMESPACE_USE


//
// Static Objects used by the spoof impl, their thread safe initialization and their cleanup.
//
static UnicodeSet *gInclusionSet = NULL;
static UnicodeSet *gRecommendedSet = NULL;
static IdentifierInfo *gCachedIdentifierInfo = NULL;
static UMutex gInitMutex = U_MUTEX_INITIALIZER;

static UBool U_CALLCONV
uspoof_cleanup(void) {
    delete gCachedIdentifierInfo;
    gCachedIdentifierInfo = NULL;
    delete gInclusionSet;
    gInclusionSet = NULL;
    delete gRecommendedSet;
    gRecommendedSet = NULL;
    return TRUE;
}

static void initializeStatics() {
    Mutex m(&gInitMutex);
    UErrorCode status = U_ZERO_ERROR;
    if (gInclusionSet == NULL) {
        gInclusionSet = new UnicodeSet(UnicodeString("["
            "\\-.\\u00B7\\u05F3\\u05F4\\u0F0B\\u200C\\u200D\\u2019]"), status);
        gRecommendedSet = new UnicodeSet(UnicodeString("["
            "[0-z\\u00C0-\\u017E\\u01A0\\u01A1\\u01AF\\u01B0\\u01CD-"
            "\\u01DC\\u01DE-\\u01E3\\u01E6-\\u01F5\\u01F8-\\u021B\\u021E"
            "\\u021F\\u0226-\\u0233\\u02BB\\u02BC\\u02EC\\u0300-\\u0304"
            "\\u0306-\\u030C\\u030F-\\u0311\\u0313\\u0314\\u031B\\u0323-"
            "\\u0328\\u032D\\u032E\\u0330\\u0331\\u0335\\u0338\\u0339"
            "\\u0342-\\u0345\\u037B-\\u03CE\\u03FC-\\u045F\\u048A-\\u0525"
            "\\u0531-\\u0586\\u05D0-\\u05F2\\u0621-\\u063F\\u0641-\\u0655"
            "\\u0660-\\u0669\\u0670-\\u068D\\u068F-\\u06D5\\u06E5\\u06E6"
            "\\u06EE-\\u06FF\\u0750-\\u07B1\\u0901-\\u0939\\u093C-\\u094D"
            "\\u0950\\u0960-\\u0972\\u0979-\\u0A4D\\u0A5C-\\u0A74\\u0A81-"
            "\\u0B43\\u0B47-\\u0B61\\u0B66-\\u0C56\\u0C60\\u0C61\\u0C66-"
            "\\u0CD6\\u0CE0-\\u0CEF\\u0D02-\\u0D28\\u0D2A-\\u0D39\\u0D3D-"
            "\\u0D43\\u0D46-\\u0D4D\\u0D57-\\u0D61\\u0D66-\\u0D8E\\u0D91-"
            "\\u0DA5\\u0DA7-\\u0DDE\\u0DF2\\u0E01-\\u0ED9\\u0F00\\u0F20-"
            "\\u0F8B\\u0F90-\\u109D\\u10D0-\\u10F0\\u10F7-\\u10FA\\u1200-"
            "\\u135A\\u135F\\u1380-\\u138F\\u1401-\\u167F\\u1780-\\u17A2"
            "\\u17A5-\\u17A7\\u17A9-\\u17B3\\u17B6-\\u17CA\\u17D2\\u17D7-"
            "\\u17DC\\u17E0-\\u17E9\\u1810-\\u18A8\\u18AA-\\u18F5\\u1E00-"
            "\\u1E99\\u1F00-\\u1FFC\\u2D30-\\u2D65\\u2D80-\\u2DDE\\u3005-"
            "\\u3007\\u3041-\\u31B7\\u3400-\\u9FCB\\uA000-\\uA48C\\uA67F"
            "\\uA717-\\uA71F\\uA788\\uAA60-\\uAA7B\\uAC00-\\uD7A3\\uFA0E-"
            "\\uFA29\\U00020000-"
            "\\U0002B734]-[[:Cn:][:nfkcqc=n:][:XIDC=n:]]]"), status);
    }
    ucln_i18n_registerCleanup(UCLN_I18N_SPOOF, uspoof_cleanup);
    U_ASSERT(U_SUCCESS(status));   // TODO: remove after testing.
    return;
}


U_CAPI USpoofChecker * U_EXPORT2
uspoof_open(UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    SpoofImpl *si = new SpoofImpl(SpoofData::getDefault(*status), *status);
    if (U_FAILURE(*status)) {
        delete si;
        si = NULL;
    }
    return (USpoofChecker *)si;
}


U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSerialized(const void *data, int32_t length, int32_t *pActualLength,
                          UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    SpoofData *sd = new SpoofData(data, length, *status);
    SpoofImpl *si = new SpoofImpl(sd, *status);
    if (U_FAILURE(*status)) {
        delete sd;
        delete si;
        return NULL;
    }
    if (sd == NULL || si == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        delete sd;
        delete si;
        return NULL;
    }
        
    if (pActualLength != NULL) {
        *pActualLength = sd->fRawData->fLength;
    }
    return reinterpret_cast<USpoofChecker *>(si);
}


U_CAPI USpoofChecker * U_EXPORT2
uspoof_clone(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *src = SpoofImpl::validateThis(sc, *status);
    if (src == NULL) {
        return NULL;
    }
    SpoofImpl *result = new SpoofImpl(*src, *status);   // copy constructor
    if (U_FAILURE(*status)) {
        delete result;
        result = NULL;
    }
    return (USpoofChecker *)result;
}


U_CAPI void U_EXPORT2
uspoof_close(USpoofChecker *sc) {
    UErrorCode status = U_ZERO_ERROR;
    SpoofImpl *This = SpoofImpl::validateThis(sc, status);
    delete This;
}


U_CAPI void U_EXPORT2
uspoof_setChecks(USpoofChecker *sc, int32_t checks, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return;
    }

    // Verify that the requested checks are all ones (bits) that 
    //   are acceptable, known values.
    if (checks & ~USPOOF_ALL_CHECKS) {
        *status = U_ILLEGAL_ARGUMENT_ERROR; 
        return;
    }

    This->fChecks = checks;
}


U_CAPI int32_t U_EXPORT2
uspoof_getChecks(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return 0;
    }
    return This->fChecks;
}

U_CAPI void U_EXPORT2
uspoof_setRestrictionLevel(USpoofChecker *sc, URestrictionLevel restrictionLevel) {
    UErrorCode status = U_ZERO_ERROR;
    SpoofImpl *This = SpoofImpl::validateThis(sc, status);
    if (This != NULL) {
        This->fRestrictionLevel = restrictionLevel;
    }
}

U_CAPI URestrictionLevel U_EXPORT2
uspoof_getRestrictionLevel(const USpoofChecker *sc) {
    UErrorCode status = U_ZERO_ERROR;
    const SpoofImpl *This = SpoofImpl::validateThis(sc, status);
    if (This == NULL) {
        return USPOOF_UNRESTRICTIVE;
    }
    return This->fRestrictionLevel;
}

U_CAPI void U_EXPORT2
uspoof_setAllowedLocales(USpoofChecker *sc, const char *localesList, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return;
    }
    This->setAllowedLocales(localesList, *status);
}

U_CAPI const char * U_EXPORT2
uspoof_getAllowedLocales(USpoofChecker *sc, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return NULL;
    }
    return This->getAllowedLocales(*status);
}


U_CAPI const USet * U_EXPORT2
uspoof_getAllowedChars(const USpoofChecker *sc, UErrorCode *status) {
    const UnicodeSet *result = uspoof_getAllowedUnicodeSet(sc, status);
    return reinterpret_cast<const USet *>(result);
}

U_CAPI const UnicodeSet * U_EXPORT2
uspoof_getAllowedUnicodeSet(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return NULL;
    }
    return This->fAllowedCharsSet;
}


U_CAPI void U_EXPORT2
uspoof_setAllowedChars(USpoofChecker *sc, const USet *chars, UErrorCode *status) {
    const UnicodeSet *set = reinterpret_cast<const UnicodeSet *>(chars);
    uspoof_setAllowedUnicodeSet(sc, set, status);
}


U_CAPI void U_EXPORT2
uspoof_setAllowedUnicodeSet(USpoofChecker *sc, const UnicodeSet *chars, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return;
    }
    if (chars->isBogus()) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    UnicodeSet *clonedSet = static_cast<UnicodeSet *>(chars->clone());
    if (clonedSet == NULL || clonedSet->isBogus()) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    clonedSet->freeze();
    delete This->fAllowedCharsSet;
    This->fAllowedCharsSet = clonedSet;
    This->fChecks |= USPOOF_CHAR_LIMIT;
}

// IdentifierInfo Cache. IdentifierInfo objects are somewhat expensive to create.
//                       Maintain a one-element cache, which is sufficient to avoid repeatedly
//                       creating new ones unless we get multi-thread concurrency in spoof
//                       check operations, which should be statistically uncommon.

static IdentifierInfo *getIdentifierInfo(UErrorCode &status) {
    IdentifierInfo *returnIdInfo = NULL;
    if (U_FAILURE(status)) {
        return returnIdInfo;
    }
    {
        Mutex m;
        returnIdInfo = gCachedIdentifierInfo;
        gCachedIdentifierInfo = NULL;
    }
    if (returnIdInfo == NULL) {
        returnIdInfo = new IdentifierInfo(status);
        if (U_SUCCESS(status) && returnIdInfo == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        if (U_FAILURE(status) && returnIdInfo != NULL) {
            delete returnIdInfo;
            returnIdInfo = NULL;
        }
    }
    return returnIdInfo;
}


static void releaseIdentifierInfo(IdentifierInfo *idInfo) {
    if (idInfo != NULL) {
        {
            Mutex m;
            if (gCachedIdentifierInfo == NULL) {
                gCachedIdentifierInfo = idInfo;
                idInfo = NULL;
            }
        }
        delete idInfo;
    }
};

U_CAPI int32_t U_EXPORT2
uspoof_check(const USpoofChecker *sc,
             const UChar *text, int32_t length,
             int32_t *position,
             UErrorCode *status) {
             
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        return 0;
    }
    if (length < -1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (length == -1) {
        // It's not worth the bother to handle nul terminated strings everywhere.
        //   Just get the length and be done with it.
        length = u_strlen(text);
    }

    int32_t result = 0;

    IdentifierInfo *identifierInfo = NULL;
    if ((This->fChecks) & (USPOOF_RESTRICTION_LEVEL | USPOOF_MIXED_NUMBERS)) {
        identifierInfo = getIdentifierInfo(*status);
        if (U_FAILURE(*status)) {
            goto cleanupAndReturn;
        }
        identifierInfo->setIdentifier(UnicodeString(FALSE, text, length), *status);
        identifierInfo->setIdentifierProfile(*This->fAllowedCharsSet);
    }


    if ((This->fChecks) & USPOOF_RESTRICTION_LEVEL) {
        URestrictionLevel textRestrictionLevel = identifierInfo->getRestrictionLevel(*status);
        if (textRestrictionLevel > This->fRestrictionLevel) {
            result |= USPOOF_RESTRICTION_LEVEL;
        }
        if (This->fChecks & USPOOF_AUX_INFO) {
            result |= textRestrictionLevel;
        }
    }

    if ((This->fChecks) & USPOOF_MIXED_NUMBERS) {
        const UnicodeSet *numerics = identifierInfo->getNumerics();
        if (numerics->size() > 1) {
            result |= USPOOF_MIXED_NUMBERS;
        }

        // TODO: ICU4J returns the UnicodeSet of the numerics found in the identifier.
        //       We have no easy way to do the same in C.
        // if (checkResult != null) {
        //     checkResult.numerics = numerics;
        // }
    }


    if (This->fChecks & USPOOF_CHAR_LIMIT) {
        int32_t i;
        UChar32 c;
        for (i=0; i<length ;) {
            U16_NEXT(text, i, length, c);
            if (!This->fAllowedCharsSet->contains(c)) {
                result |= USPOOF_CHAR_LIMIT;
                break;
            }
        }
    }

    if (This->fChecks & 
        (USPOOF_WHOLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE | USPOOF_INVISIBLE)) {
        // These are the checks that need to be done on NFD input
        NFDBuffer   normalizedInput(text, length, *status);
        const UChar  *nfdText = normalizedInput.getBuffer();
        int32_t      nfdLength = normalizedInput.getLength();

        if (This->fChecks & USPOOF_INVISIBLE) {
           
            // scan for more than one occurence of the same non-spacing mark
            // in a sequence of non-spacing marks.
            int32_t     i;
            UChar32     c;
            UChar32     firstNonspacingMark = 0;
            UBool       haveMultipleMarks = FALSE;  
            UnicodeSet  marksSeenSoFar;   // Set of combining marks in a single combining sequence.
            
            for (i=0; i<nfdLength ;) {
                U16_NEXT(nfdText, i, nfdLength, c);
                if (u_charType(c) != U_NON_SPACING_MARK) {
                    firstNonspacingMark = 0;
                    if (haveMultipleMarks) {
                        marksSeenSoFar.clear();
                        haveMultipleMarks = FALSE;
                    }
                    continue;
                }
                if (firstNonspacingMark == 0) {
                    firstNonspacingMark = c;
                    continue;
                }
                if (!haveMultipleMarks) {
                    marksSeenSoFar.add(firstNonspacingMark);
                    haveMultipleMarks = TRUE;
                }
                if (marksSeenSoFar.contains(c)) {
                    // report the error, and stop scanning.
                    // No need to find more than the first failure.
                    result |= USPOOF_INVISIBLE;
                    break;
                }
                marksSeenSoFar.add(c);
            }
        }
       
        
        if (This->fChecks & (USPOOF_WHOLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE)) {
            // The basic test is the same for both whole and mixed script confusables.
            // Compute the set of scripts that every input character has a confusable in.
            // For this computation an input character is always considered to be
            // confusable with itself in its own script.
            //
            // If the number of such scripts is two or more, and the input consisted of
            // characters all from a single script, we have a whole script confusable.
            // (The two scripts will be the original script and the one that is confusable)
            //
            // If the number of such scripts >= one, and the original input contained characters from
            // more than one script, we have a mixed script confusable.  (We can transform
            // some of the characters, and end up with a visually similar string all in
            // one script.)

            if (identifierInfo == NULL) {
                identifierInfo = getIdentifierInfo(*status);
                if (U_FAILURE(*status)) {
                    goto cleanupAndReturn;
                }
                identifierInfo->setIdentifier(UnicodeString(FALSE, text, length), *status);
            }

            int32_t scriptCount = identifierInfo->getScriptCount();
            
            ScriptSet scripts;
            This->wholeScriptCheck(nfdText, nfdLength, &scripts, *status);
            int32_t confusableScriptCount = scripts.countMembers();
            //printf("confusableScriptCount = %d\n", confusableScriptCount);
            
            if ((This->fChecks & USPOOF_WHOLE_SCRIPT_CONFUSABLE) &&
                confusableScriptCount >= 2 &&
                scriptCount == 1) {
                result |= USPOOF_WHOLE_SCRIPT_CONFUSABLE;
            }
        
            if ((This->fChecks & USPOOF_MIXED_SCRIPT_CONFUSABLE) &&
                confusableScriptCount >= 1 &&
                scriptCount > 1) {
                result |= USPOOF_MIXED_SCRIPT_CONFUSABLE;
            }
        }
    }

cleanupAndReturn:
    releaseIdentifierInfo(identifierInfo);
    if (position != NULL) {
        *position = 0;
    }
    return result;
}


U_CAPI int32_t U_EXPORT2
uspoof_checkUTF8(const USpoofChecker *sc,
                 const char *text, int32_t length,
                 int32_t *position,
                 UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return 0;
    }
    UChar stackBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar* text16 = stackBuf;
    int32_t len16;
    
    u_strFromUTF8(text16, USPOOF_STACK_BUFFER_SIZE, &len16, text, length, status);
    if (U_FAILURE(*status) && *status != U_BUFFER_OVERFLOW_ERROR) {
        return 0;
    }
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        text16 = static_cast<UChar *>(uprv_malloc(len16 * sizeof(UChar) + 2));
        if (text16 == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
        *status = U_ZERO_ERROR;
        u_strFromUTF8(text16, len16+1, NULL, text, length, status);
    }

    int32_t result = uspoof_check(sc, text16, len16, position, status);
    if (U_FAILURE(*status)) {
        return 0;
    }

    if (text16 != stackBuf) {
        uprv_free(text16);
    }
    return result;
    
}

/*  A convenience wrapper around the public uspoof_getSkeleton that handles
 *  allocating a larger buffer than provided if the original is too small.
 */
static UChar *getSkeleton(const USpoofChecker *sc, uint32_t type, const UChar *s, int32_t inputLength,
                         UChar *dest, int32_t destCapacity, int32_t *outputLength, UErrorCode *status) {
    int32_t requiredCapacity = 0;
    UChar *buf = dest;

    if (U_FAILURE(*status)) {
        return NULL;
    }
    requiredCapacity = uspoof_getSkeleton(sc, type, s, inputLength, dest, destCapacity, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        buf = static_cast<UChar *>(uprv_malloc(requiredCapacity * sizeof(UChar)));
        if (buf == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        *status = U_ZERO_ERROR;
        uspoof_getSkeleton(sc, type, s, inputLength, buf, requiredCapacity, status);
    }
    *outputLength = requiredCapacity;
    return buf;
}


U_CAPI int32_t U_EXPORT2
uspoof_areConfusable(const USpoofChecker *sc,
                     const UChar *s1, int32_t length1,
                     const UChar *s2, int32_t length2,
                     UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    // 
    // See section 4 of UAX 39 for the algorithm for checking whether two strings are confusable,
    //   and for definitions of the types (single, whole, mixed-script) of confusables.
    
    // We only care about a few of the check flags.  Ignore the others.
    // If no tests relavant to this function have been specified, return an error.
    // TODO:  is this really the right thing to do?  It's probably an error on the caller's part,
    //        but logically we would just return 0 (no error).
    if ((This->fChecks & (USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE | 
                          USPOOF_WHOLE_SCRIPT_CONFUSABLE)) == 0) {
        *status = U_INVALID_STATE_ERROR;
        return 0;
    }
    int32_t  flagsForSkeleton = This->fChecks & USPOOF_ANY_CASE;
    UChar    s1SkeletonBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar   *s1Skeleton;
    int32_t  s1SkeletonLength = 0;

    UChar    s2SkeletonBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar   *s2Skeleton;
    int32_t  s2SkeletonLength = 0;

    int32_t  result = 0;
    IdentifierInfo *identifierInfo = getIdentifierInfo(*status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    identifierInfo->setIdentifier(UnicodeString(FALSE, s1, length1), *status);
    int32_t s1ScriptCount = identifierInfo->getScriptCount();
    identifierInfo->setIdentifier(UnicodeString(FALSE, s2, length2), *status);
    int32_t s2ScriptCount = identifierInfo->getScriptCount();
    releaseIdentifierInfo(identifierInfo);
    identifierInfo = NULL;

    // TODO: Switch the skeleton over to UnicodeString for easier memory management.
    if (This->fChecks & USPOOF_SINGLE_SCRIPT_CONFUSABLE) {
        // Do the Single Script compare.
        if (s1ScriptCount <= 1 && s2ScriptCount <= 1) {
            flagsForSkeleton |= USPOOF_SINGLE_SCRIPT_CONFUSABLE;
            s1Skeleton = getSkeleton(sc, flagsForSkeleton, s1, length1, s1SkeletonBuf, 
                                     sizeof(s1SkeletonBuf)/sizeof(UChar), &s1SkeletonLength, status);
            s2Skeleton = getSkeleton(sc, flagsForSkeleton, s2, length2, s2SkeletonBuf, 
                                     sizeof(s2SkeletonBuf)/sizeof(UChar), &s2SkeletonLength, status);
            if (s1SkeletonLength == s2SkeletonLength && u_strncmp(s1Skeleton, s2Skeleton, s1SkeletonLength) == 0) {
                result |= USPOOF_SINGLE_SCRIPT_CONFUSABLE;
            }
            if (s1Skeleton != s1SkeletonBuf) {
                uprv_free(s1Skeleton);
            }
            if (s2Skeleton != s2SkeletonBuf) {
                uprv_free(s2Skeleton);
            }
        }
    }

    if (result & USPOOF_SINGLE_SCRIPT_CONFUSABLE) {
         // If the two inputs are single script confusable they cannot also be
         // mixed or whole script confusable, according to the UAX39 definitions.
         // So we can skip those tests.
         return result;
    }

    // Optimization for whole script confusables test:  two identifiers are whole script confusable if
    // each is of a single script and they are mixed script confusable.
    UBool possiblyWholeScriptConfusables = 
        s1ScriptCount <= 1 && s2ScriptCount <= 1 && (This->fChecks & USPOOF_WHOLE_SCRIPT_CONFUSABLE);

    //
    // Mixed Script Check
    //
    if ((This->fChecks & USPOOF_MIXED_SCRIPT_CONFUSABLE) || possiblyWholeScriptConfusables ) {
        // For getSkeleton(), resetting the USPOOF_SINGLE_SCRIPT_CONFUSABLE flag will get us
        // the mixed script table skeleton, which is what we want.
        // The Any Case / Lower Case bit in the skelton flags was set at the top of the function.
        flagsForSkeleton &= ~USPOOF_SINGLE_SCRIPT_CONFUSABLE;
        s1Skeleton = getSkeleton(sc, flagsForSkeleton, s1, length1, s1SkeletonBuf, 
                                 sizeof(s1SkeletonBuf)/sizeof(UChar), &s1SkeletonLength, status);
        s2Skeleton = getSkeleton(sc, flagsForSkeleton, s2, length2, s2SkeletonBuf, 
                                 sizeof(s2SkeletonBuf)/sizeof(UChar), &s2SkeletonLength, status);
        if (s1SkeletonLength == s2SkeletonLength && u_strncmp(s1Skeleton, s2Skeleton, s1SkeletonLength) == 0) {
            result |= USPOOF_MIXED_SCRIPT_CONFUSABLE;
            if (possiblyWholeScriptConfusables) {
                result |= USPOOF_WHOLE_SCRIPT_CONFUSABLE;
            }
        }
        if (s1Skeleton != s1SkeletonBuf) {
            uprv_free(s1Skeleton);
        }
        if (s2Skeleton != s2SkeletonBuf) {
            uprv_free(s2Skeleton);
        }
    }

    return result;
}


// Convenience function for converting a UTF-8 input to a UChar * string, including
//          reallocating a buffer when required.  Parameters and their interpretation mostly
//          match u_strFromUTF8.

static UChar * convertFromUTF8(UChar *outBuf, int32_t outBufCapacity, int32_t *outputLength,
                               const char *in, int32_t inLength, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    UChar *dest = outBuf;
    u_strFromUTF8(dest, outBufCapacity, outputLength, in, inLength, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        dest = static_cast<UChar *>(uprv_malloc(*outputLength * sizeof(UChar)));
        if (dest == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        *status = U_ZERO_ERROR;
        u_strFromUTF8(dest, *outputLength, NULL, in, inLength, status);
    }
    return dest;
}

    

U_CAPI int32_t U_EXPORT2
uspoof_areConfusableUTF8(const USpoofChecker *sc,
                         const char *s1, int32_t length1,
                         const char *s2, int32_t length2,
                         UErrorCode *status) {

    SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }

    UChar    s1Buf[USPOOF_STACK_BUFFER_SIZE];
    int32_t  lengthS1U;
    UChar   *s1U = convertFromUTF8(s1Buf, USPOOF_STACK_BUFFER_SIZE, &lengthS1U, s1, length1, status);

    UChar    s2Buf[USPOOF_STACK_BUFFER_SIZE];
    int32_t  lengthS2U;
    UChar   *s2U = convertFromUTF8(s2Buf, USPOOF_STACK_BUFFER_SIZE, &lengthS2U, s2, length2, status);

    int32_t results = uspoof_areConfusable(sc, s1U, lengthS1U, s2U, lengthS2U, status);
    
    if (s1U != s1Buf) {
        uprv_free(s1U);
    }
    if (s2U != s2Buf) {
        uprv_free(s2U);
    }
    return results;
}
 

U_CAPI int32_t U_EXPORT2
uspoof_areConfusableUnicodeString(const USpoofChecker *sc,
                                  const icu::UnicodeString &s1,
                                  const icu::UnicodeString &s2,
                                  UErrorCode *status) {

    const UChar *u1  = s1.getBuffer();
    int32_t  length1 = s1.length();
    const UChar *u2  = s2.getBuffer();
    int32_t  length2 = s2.length();

    int32_t results  = uspoof_areConfusable(sc, u1, length1, u2, length2, status);
    return results;
}




U_CAPI int32_t U_EXPORT2
uspoof_checkUnicodeString(const USpoofChecker *sc,
                          const icu::UnicodeString &text, 
                          int32_t *position,
                          UErrorCode *status) {
    int32_t result = uspoof_check(sc, text.getBuffer(), text.length(), position, status);
    return result;
}


U_CAPI int32_t U_EXPORT2
uspoof_getSkeleton(const USpoofChecker *sc,
                   uint32_t type,
                   const UChar *s,  int32_t length,
                   UChar *dest, int32_t destCapacity,
                   UErrorCode *status) {

    // TODO:  this function could be sped up a bit
    //        Skip the input normalization when not needed, work from callers data.
    //        Put the initial skeleton straight into the caller's destination buffer.
    //        It probably won't need normalization.
    //        But these would make the structure more complicated.  

    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    if (length<-1 || destCapacity<0 || (destCapacity==0 && dest!=NULL) ||
        (type & ~(USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE)) != 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

   int32_t tableMask = 0;
   switch (type) {
      case 0:
        tableMask = USPOOF_ML_TABLE_FLAG;
        break;
      case USPOOF_SINGLE_SCRIPT_CONFUSABLE:
        tableMask = USPOOF_SL_TABLE_FLAG;
        break;
      case USPOOF_ANY_CASE:
        tableMask = USPOOF_MA_TABLE_FLAG;
        break;
      case USPOOF_SINGLE_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE:
        tableMask = USPOOF_SA_TABLE_FLAG;
        break;
      default:
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // NFD transform of the user supplied input
    
    UChar nfdStackBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar *nfdInput = nfdStackBuf;
    int32_t normalizedLen = unorm_normalize(
        s, length, UNORM_NFD, 0, nfdInput, USPOOF_STACK_BUFFER_SIZE, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        nfdInput = (UChar *)uprv_malloc((normalizedLen+1)*sizeof(UChar));
        if (nfdInput == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
        *status = U_ZERO_ERROR;
        normalizedLen = unorm_normalize(s, length, UNORM_NFD, 0,
                                        nfdInput, normalizedLen+1, status);
    }
    if (U_FAILURE(*status)) {
        if (nfdInput != nfdStackBuf) {
            uprv_free(nfdInput);
        }
        return 0;
    }

    // buffer to hold the Unicode defined skeleton mappings for a single code point
    UChar buf[USPOOF_MAX_SKELETON_EXPANSION];

    // Apply the skeleton mapping to the NFD normalized input string
    // Accumulate the skeleton, possibly unnormalized, in a UnicodeString.
    int32_t inputIndex = 0;
    UnicodeString skelStr;
    while (inputIndex < normalizedLen) {
        UChar32 c;
        U16_NEXT(nfdInput, inputIndex, normalizedLen, c);
        int32_t replaceLen = This->confusableLookup(c, tableMask, buf);
        skelStr.append(buf, replaceLen);
    }

    if (nfdInput != nfdStackBuf) {
        uprv_free(nfdInput);
    }
    
    const UChar *result = skelStr.getBuffer();
    int32_t  resultLen  = skelStr.length();
    UChar   *normedResult = NULL;

    // Check the skeleton for NFD, normalize it if needed.
    // Unnormalized results should be very rare.
    if (!unorm_isNormalized(result, resultLen, UNORM_NFD, status)) {
        normalizedLen = unorm_normalize(result, resultLen, UNORM_NFD, 0, NULL, 0, status);
        normedResult = static_cast<UChar *>(uprv_malloc((normalizedLen+1)*sizeof(UChar)));
        if (normedResult == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }
        *status = U_ZERO_ERROR;
        unorm_normalize(result, resultLen, UNORM_NFD, 0, normedResult, normalizedLen+1, status);
        result = normedResult;
        resultLen = normalizedLen;
    }

    // Copy the skeleton to the caller's buffer
    if (U_SUCCESS(*status)) {
        if (destCapacity == 0 || resultLen > destCapacity) {
            *status = resultLen>destCapacity ? U_BUFFER_OVERFLOW_ERROR : U_STRING_NOT_TERMINATED_WARNING;
        } else {
            u_memcpy(dest, result, resultLen);
            if (destCapacity > resultLen) {
                dest[resultLen] = 0;
            } else {
                *status = U_STRING_NOT_TERMINATED_WARNING;
            }
        }
     }       
     uprv_free(normedResult);
     return resultLen;
}



U_I18N_API UnicodeString &  U_EXPORT2
uspoof_getSkeletonUnicodeString(const USpoofChecker *sc,
                                uint32_t type,
                                const UnicodeString &s,
                                UnicodeString &dest,
                                UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return dest;
    }
    dest.remove();
    
    const UChar *str = s.getBuffer();
    int32_t      strLen = s.length();
    UChar        smallBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar       *buf = smallBuf;
    int32_t outputSize = uspoof_getSkeleton(sc, type, str, strLen, smallBuf, USPOOF_STACK_BUFFER_SIZE, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        buf = static_cast<UChar *>(uprv_malloc((outputSize+1)*sizeof(UChar)));
        if (buf == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return dest;
        }
        *status = U_ZERO_ERROR;
        uspoof_getSkeleton(sc, type, str, strLen, buf, outputSize+1, status);
    }
    if (U_SUCCESS(*status)) {
        dest.setTo(buf, outputSize);
    }

    if (buf != smallBuf) {
        uprv_free(buf);
    }
    return dest;
}


U_CAPI int32_t U_EXPORT2
uspoof_getSkeletonUTF8(const USpoofChecker *sc,
                       uint32_t type,
                       const char *s,  int32_t length,
                       char *dest, int32_t destCapacity,
                       UErrorCode *status) {
    // Lacking a UTF-8 normalization API, just converting the input to
    // UTF-16 seems as good an approach as any.  In typical use, input will
    // be an identifier, which is to say not too long for stack buffers.
    if (U_FAILURE(*status)) {
        return 0;
    }
    // Buffers for the UChar form of the input and skeleton strings.
    UChar    smallInBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar   *inBuf = smallInBuf;
    UChar    smallOutBuf[USPOOF_STACK_BUFFER_SIZE];
    UChar   *outBuf = smallOutBuf;

    int32_t  lengthInUChars = 0;
    int32_t  skelLengthInUChars = 0;
    int32_t  skelLengthInUTF8 = 0;
    
    u_strFromUTF8(inBuf, USPOOF_STACK_BUFFER_SIZE, &lengthInUChars,
                  s, length, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        inBuf = static_cast<UChar *>(uprv_malloc((lengthInUChars+1)*sizeof(UChar)));
        if (inBuf == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            goto cleanup;
        }
        *status = U_ZERO_ERROR;
        u_strFromUTF8(inBuf, lengthInUChars+1, &lengthInUChars,
                      s, length, status);
    }
    
    skelLengthInUChars = uspoof_getSkeleton(sc, type, inBuf, lengthInUChars,
                                         outBuf, USPOOF_STACK_BUFFER_SIZE, status);
    if (*status == U_BUFFER_OVERFLOW_ERROR) {
        outBuf = static_cast<UChar *>(uprv_malloc((skelLengthInUChars+1)*sizeof(UChar)));
        if (outBuf == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            goto cleanup;
        }
        *status = U_ZERO_ERROR;
        skelLengthInUChars = uspoof_getSkeleton(sc, type, inBuf, lengthInUChars,
                                         outBuf, skelLengthInUChars+1, status);
    }

    u_strToUTF8(dest, destCapacity, &skelLengthInUTF8,
                outBuf, skelLengthInUChars, status);

  cleanup:
    if (inBuf != smallInBuf) {
        uprv_free(inBuf);
    }
    if (outBuf != smallOutBuf) {
        uprv_free(outBuf);
    }
    return skelLengthInUTF8;
}


U_CAPI int32_t U_EXPORT2
uspoof_serialize(USpoofChecker *sc,void *buf, int32_t capacity, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    if (This == NULL) {
        U_ASSERT(U_FAILURE(*status));
        return 0;
    }
    int32_t dataSize = This->fSpoofData->fRawData->fLength;
    if (capacity < dataSize) {
        *status = U_BUFFER_OVERFLOW_ERROR;
        return dataSize;
    }
    uprv_memcpy(buf, This->fSpoofData->fRawData, dataSize);
    return dataSize;
}

U_CAPI const USet * U_EXPORT2
uspoof_getInclusionSet(UErrorCode *) {
    initializeStatics();
    return gInclusionSet->toUSet();
}

U_CAPI const USet * U_EXPORT2
uspoof_getRecommendedSet(UErrorCode *) {
    initializeStatics();
    return gRecommendedSet->toUSet();
}

U_I18N_API const UnicodeSet * U_EXPORT2
uspoof_getInclusionUnicodeSet(UErrorCode *) {
    initializeStatics();
    return gInclusionSet;
}

U_I18N_API const UnicodeSet * U_EXPORT2
uspoof_getRecommendedUnicodeSet(UErrorCode *) {
    initializeStatics();
    return gRecommendedSet;
}



#endif // !UCONFIG_NO_NORMALIZATION
