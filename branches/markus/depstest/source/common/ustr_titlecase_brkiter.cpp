/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ustr_titlecase_brkiter.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011may30
*   created by: Markus W. Scherer
*
*   Titlecasing functions that are based on BreakIterator
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/ubrk.h"
#include "unicode/ucasemap.h"
#include "cmemory.h"
#include "ucase.h"
#include "ustr_imp.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

/*
 * Internal titlecasing function.
 */
static int32_t
_toTitle(UCaseMap *csm,
         UChar *dest, int32_t destCapacity,
         const UChar *src, UCaseContext *csc,
         int32_t srcLength,
         UErrorCode *pErrorCode) {
    const UChar *s;
    UChar32 c;
    int32_t prev, titleStart, titleLimit, idx, destIndex, length;
    UBool isFirstIndex;

    if(csm->iter!=NULL) {
        ubrk_setText(csm->iter, src, srcLength, pErrorCode);
    } else {
        csm->iter=ubrk_open(UBRK_WORD, csm->locale,
                            src, srcLength,
                            pErrorCode);
    }
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }

    /* set up local variables */
    destIndex=0;
    prev=0;
    isFirstIndex=TRUE;

    /* titlecasing loop */
    while(prev<srcLength) {
        /* find next index where to titlecase */
        if(isFirstIndex) {
            isFirstIndex=FALSE;
            idx=ubrk_first(csm->iter);
        } else {
            idx=ubrk_next(csm->iter);
        }
        if(idx==UBRK_DONE || idx>srcLength) {
            idx=srcLength;
        }

        /*
         * Unicode 4 & 5 section 3.13 Default Case Operations:
         *
         * R3  toTitlecase(X): Find the word boundaries based on Unicode Standard Annex
         * #29, "Text Boundaries." Between each pair of word boundaries, find the first
         * cased character F. If F exists, map F to default_title(F); then map each
         * subsequent character C to default_lower(C).
         *
         * In this implementation, segment [prev..index[ into 3 parts:
         * a) uncased characters (copy as-is) [prev..titleStart[
         * b) first case letter (titlecase)         [titleStart..titleLimit[
         * c) subsequent characters (lowercase)                 [titleLimit..index[
         */
        if(prev<idx) {
            /* find and copy uncased characters [prev..titleStart[ */
            titleStart=titleLimit=prev;
            U16_NEXT(src, titleLimit, idx, c);
            if((csm->options&U_TITLECASE_NO_BREAK_ADJUSTMENT)==0 && UCASE_NONE==ucase_getType(csm->csp, c)) {
                /* Adjust the titlecasing index (titleStart) to the next cased character. */
                for(;;) {
                    titleStart=titleLimit;
                    if(titleLimit==idx) {
                        /*
                         * only uncased characters in [prev..index[
                         * stop with titleStart==titleLimit==index
                         */
                        break;
                    }
                    U16_NEXT(src, titleLimit, idx, c);
                    if(UCASE_NONE!=ucase_getType(csm->csp, c)) {
                        break; /* cased letter at [titleStart..titleLimit[ */
                    }
                }
                length=titleStart-prev;
                if(length>0) {
                    if((destIndex+length)<=destCapacity) {
                        uprv_memcpy(dest+destIndex, src+prev, length*U_SIZEOF_UCHAR);
                    }
                    destIndex+=length;
                }
            }

            if(titleStart<titleLimit) {
                /* titlecase c which is from [titleStart..titleLimit[ */
                csc->cpStart=titleStart;
                csc->cpLimit=titleLimit;
                c=ucase_toFullTitle(csm->csp, c, utf16_caseContextIterator, csc, &s, csm->locale, &csm->locCache);
                destIndex=ustrcase_appendResultU16(dest, destIndex, destCapacity, c, s); 

                /* Special case Dutch IJ titlecasing */
                if ( titleStart+1 < idx && 
                     ucase_getCaseLocale(csm->locale,&csm->locCache) == UCASE_LOC_DUTCH &&
                     ( src[titleStart] == (UChar32) 0x0049 || src[titleStart] == (UChar32) 0x0069 ) &&
                     ( src[titleStart+1] == (UChar32) 0x004A || src[titleStart+1] == (UChar32) 0x006A )) { 
                            c=(UChar32) 0x004A;
                            destIndex=ustrcase_appendResultU16(dest, destIndex, destCapacity, c, s);
                            titleLimit++;
                }

                /* lowercase [titleLimit..index[ */
                if(titleLimit<idx) {
                    if((csm->options&U_TITLECASE_NO_LOWERCASE)==0) {
                        /* Normal operation: Lowercase the rest of the word. */
                        destIndex+=
                            ustrcase_map(
                                csm, ucase_toFullLower,
                                dest+destIndex, destCapacity-destIndex,
                                src, csc,
                                titleLimit, idx,
                                pErrorCode);
                    } else {
                        /* Optionally just copy the rest of the word unchanged. */
                        length=idx-titleLimit;
                        if((destIndex+length)<=destCapacity) {
                            uprv_memcpy(dest+destIndex, src+titleLimit, length*U_SIZEOF_UCHAR);
                        }
                        destIndex+=length;
                    }
                }
            }
        }

        prev=idx;
    }

    if(destIndex>destCapacity) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    return destIndex;
}

// Duplicate of ustrcase.cpp/caseMap() except this version only handles titlecasing
// while the other one handles all other case mappings.
// Duplicated and separated for simpler dependencies.
static int32_t
caseMap(UCaseMap *csm,
        UChar *dest, int32_t destCapacity,
        const UChar *src, int32_t srcLength,
        UErrorCode *pErrorCode) {
    UChar buffer[300];
    UChar *temp;

    int32_t destLength;

    /* check argument values */
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if( destCapacity<0 ||
        (dest==NULL && destCapacity>0) ||
        src==NULL ||
        srcLength<-1
    ) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    /* get the string length */
    if(srcLength==-1) {
        srcLength=u_strlen(src);
    }

    /* check for overlapping source and destination */
    if( dest!=NULL &&
        ((src>=dest && src<(dest+destCapacity)) ||
         (dest>=src && dest<(src+srcLength)))
    ) {
        /* overlap: provide a temporary destination buffer and later copy the result */
        if(destCapacity<=LENGTHOF(buffer)) {
            /* the stack buffer is large enough */
            temp=buffer;
        } else {
            /* allocate a buffer */
            temp=(UChar *)uprv_malloc(destCapacity*U_SIZEOF_UCHAR);
            if(temp==NULL) {
                *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
                return 0;
            }
        }
    } else {
        temp=dest;
    }

    destLength=0;

    UCaseContext csc=UCASECONTEXT_INITIALIZER;

    csc.p=(void *)src;
    csc.limit=srcLength;

    destLength=_toTitle(csm, temp, destCapacity,
                        src, &csc, srcLength,
                        pErrorCode);
    if(temp!=dest) {
        /* copy the result string to the destination buffer */
        if(destLength>0) {
            int32_t copyLength= destLength<=destCapacity ? destLength : destCapacity;
            if(copyLength>0) {
                uprv_memmove(dest, temp, copyLength*U_SIZEOF_UCHAR);
            }
        }
        if(temp!=buffer) {
            uprv_free(temp);
        }
    }

    return u_terminateUChars(dest, destCapacity, destLength, pErrorCode);
}

/* functions available in the common library (for unistr_case.cpp) */

/*
 * Set parameters on an empty UCaseMap, for UCaseMap-less API functions.
 * Do this fast because it is called with every function call.
 * Duplicate of the same function in ustrcase.cpp, to keep it inline.
 */
static inline void
setTempCaseMap(UCaseMap *csm, const char *locale) {
    if(csm->csp==NULL) {
        csm->csp=ucase_getSingleton();
    }
    if(locale!=NULL && locale[0]==0) {
        csm->locale[0]=0;
    } else {
        ustrcase_setTempCaseMapLocale(csm, locale);
    }
}

U_CFUNC int32_t
ustr_toTitle(const UCaseProps *csp,
             UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             UBreakIterator *titleIter,
             const char *locale, uint32_t options,
             UErrorCode *pErrorCode) {
    UCaseMap csm=UCASEMAP_INITIALIZER;
    UCaseContext csc=UCASECONTEXT_INITIALIZER;
    int32_t length;

    csm.csp=csp;
    csm.iter=titleIter;
    csm.options=options;
    setTempCaseMap(&csm, locale);
    csc.p=(void *)src;
    csc.limit=srcLength;

    length=_toTitle(&csm,
                    dest, destCapacity,
                    src, &csc, srcLength,
                    pErrorCode);
    if(titleIter==NULL && csm.iter!=NULL) {
        ubrk_close(csm.iter);
    }
    return length;
}

/* public API functions */

U_CAPI int32_t U_EXPORT2
u_strToTitle(UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             UBreakIterator *titleIter,
             const char *locale,
             UErrorCode *pErrorCode) {
    UCaseMap csm=UCASEMAP_INITIALIZER;
    int32_t length;

    csm.iter=titleIter;
    setTempCaseMap(&csm, locale);
    length=caseMap(&csm,
                   dest, destCapacity,
                   src, srcLength,
                   pErrorCode);
    if(titleIter==NULL && csm.iter!=NULL) {
        ubrk_close(csm.iter);
    }
    return length;
}

U_CAPI int32_t U_EXPORT2
ucasemap_toTitle(UCaseMap *csm,
                 UChar *dest, int32_t destCapacity,
                 const UChar *src, int32_t srcLength,
                 UErrorCode *pErrorCode) {
    return caseMap(csm,
                   dest, destCapacity,
                   src, srcLength,
                   pErrorCode);
}

#endif  // !UCONFIG_NO_BREAK_ITERATION
