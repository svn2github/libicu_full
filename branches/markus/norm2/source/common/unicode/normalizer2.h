/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#ifndef __NORMALIZER2_H__
#define __NORMALIZER2_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION && defined(XP_CPLUSPLUS)

#include "unicode/unistr.h"
#include "unicode/unorm.h"

// TODO: Move to old unorm.h or new unormalizer2.h.
typedef enum UNormalizationAppendMode {
    UNORM_SIMPLE_APPEND,
    UNORM_MERGE_APPEND,
    UNORM_APPEND_MODE_COUNT
} UNormalizationAppendMode;

U_NAMESPACE_BEGIN

class Normalizer2Impl;

class Normalizer2 : public UObject {
public:
    static Normalizer2 *
    createInstance(const char *packageName,
                   const char *name,
                   UNormalizationMode mode,
                   UErrorCode &errorCode);

    virtual UnicodeString &
    normalizeAndAppend(const UnicodeString &src,
                       UnicodeString &dest,
                       UNormalizationAppendMode appendMode,
                       UErrorCode &errorCode) const = 0;

    UnicodeString
    normalize(const UnicodeString &src, UErrorCode &errorCode) const {
        UnicodeString result;
        normalizeAndAppend(src, result, UNORM_SIMPLE_APPEND, errorCode);
        return result;
    }

protected:
    Normalizer2(const Normalizer2Impl &ni) : impl(ni) {}
    // TODO: no copy, ==, etc.
    const Normalizer2Impl &impl;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION && defined(XP_CPLUSPLUS)
#endif  // __NORMALIZER2_H__
