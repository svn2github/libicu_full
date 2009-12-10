/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  filterednormalizer2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009dec10
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"

U_NAMESPACE_BEGIN

// TODO: duplicate from normalizer2impl.h; move to a lower-level header file
/**
 * Check that the string is readable and writable.
 * Sets U_ILLEGAL_ARGUMENT_ERROR if the string isBogus() or has an open getBuffer().
 */
inline void checkCanGetBuffer(const UnicodeString &s, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && s.isBogus()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    }
}

// TODO: use UnicodeSet::span(UnicodeString &s, ...) and UnicodeString::tempSubString()

static int32_t setSpan(const UnicodeSet &set,
                       const UnicodeString &s, int32_t start,
                       USetSpanCondition spanCondition) {
    const UChar *sArray=s.getBuffer();
    int32_t sLength=s.length();
    return start+set.span(sArray+start, sLength-start, spanCondition);
}

UnicodeString &
FilteredNormalizer2::normalize(const UnicodeString &src,
                               UnicodeString &dest,
                               UErrorCode &errorCode) const {
    checkCanGetBuffer(src, errorCode);
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return dest;
    }
    if(&dest==&src) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return dest;
    }
    dest.remove();
    UnicodeString tempDest;  // Don't throw away destination buffer between iterations.
    USetSpanCondition spanCondition=USET_SPAN_SIMPLE;
    for(int32_t prevSpanLimit=0; prevSpanLimit<src.length();) {
        int32_t spanLimit=setSpan(set, src, prevSpanLimit, spanCondition);
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            dest.append(src, prevSpanLimit, prevSpanLimit-spanLimit);
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            // Not norm2.normalizeSecondAndAppend() because we do not want
            // to modify the non-filter part of dest.
            dest.append(norm2.normalize(src.tempSubStringBetween(prevSpanLimit, spanLimit),
                                        tempDest, errorCode));
            if(U_FAILURE(errorCode)) {
                break;
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return dest;
}

UnicodeString &
FilteredNormalizer2::normalizeSecondAndAppend(UnicodeString &first,
                                              const UnicodeString &second,
                                              UErrorCode &errorCode) const {
    return first;  // TODO
}

UnicodeString &
FilteredNormalizer2::append(UnicodeString &first,
                            const UnicodeString &second,
                            UErrorCode &errorCode) const {
    return first;  // TODO
}

UBool
FilteredNormalizer2::isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
    return UNORM_YES==quickCheck(s, errorCode);
}

UNormalizationCheckResult
FilteredNormalizer2::quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
    checkCanGetBuffer(s, errorCode);
    if(U_FAILURE(errorCode)) {
        return UNORM_MAYBE;
    }
    USetSpanCondition spanCondition=USET_SPAN_SIMPLE;
    for(int32_t prevSpanLimit=0; prevSpanLimit<s.length();) {
        int32_t spanLimit=setSpan(set, s, prevSpanLimit, spanCondition);
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            UNormalizationCheckResult qcResult=
                norm2.quickCheck(s.tempSubStringBetween(prevSpanLimit, spanLimit), errorCode);
            if(U_FAILURE(errorCode) || qcResult!=UNORM_YES) {
                return qcResult;
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return UNORM_YES;
}

int32_t
FilteredNormalizer2::spanQuickCheckYes(const UnicodeString &s, UErrorCode &errorCode) const {
    checkCanGetBuffer(s, errorCode);
    if(U_FAILURE(errorCode)) {
        return 0;
    }
    USetSpanCondition spanCondition=USET_SPAN_SIMPLE;
    for(int32_t prevSpanLimit=0; prevSpanLimit<s.length();) {
        int32_t spanLimit=setSpan(set, s, prevSpanLimit, spanCondition);
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            int32_t yesLimit=
                prevSpanLimit+
                norm2.spanQuickCheckYes(
                    s.tempSubStringBetween(prevSpanLimit, spanLimit), errorCode);
            if(U_FAILURE(errorCode) || yesLimit<spanLimit) {
                return yesLimit;
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return s.length();
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FilteredNormalizer2)

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION
