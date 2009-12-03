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

// TODO: add to list of C++ header files

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/unistr.h"
#include "unicode/unorm.h"

// TODO: Copy \file comments from unorm.h and/or normlzr.h.
// TODO: Move UNORM2_ enums to new unormalizer2.h.

/**
 * Constants for normalization modes.
 * @draft ICU 4.4
 */
typedef enum {
    /**
     * Decomposition followed by composition.
     * @draft ICU 4.4
     */
    UNORM2_COMPOSE,
    /**
     * Compose only contiguously.
     * The result will often but not always be in NFC.
     * The result will conform to FCD which is useful for processing.
     * Not a standard Unicode normalization form.
     * @draft ICU 4.4
     */
    UNORM2_FCC,
    /**
     * Map and canonically order.
     * @draft ICU 4.4
     */
    UNORM2_DECOMPOSE,
    /**
     * "Fast C or D" form.
     * Further decomposition without reordering would yield NFD.
     * Not a standard Unicode normalization form.
     * Not a unique form: Different FCD strings can be canonically equivalent.
     * @draft ICU 4.4
     */
    UNORM2_FCD,
    /** One more than the highest normalization mode constant. @draft ICU 4.4 */
    // TODO: needed? error checking?? UNORM2_MODE_COUNT
} UNormalization2Mode;

U_NAMESPACE_BEGIN

class Normalizer2Impl;

class Normalizer2 : public UObject {
public:
    // Returns unmodifiable singleton.
    static Normalizer2 *
    getInstance(const char *packageName,
                const char *name,
                UNormalization2Mode mode,
                UErrorCode &errorCode);

    /**
     * Returns the normalized form of the source string.
     * @param src source string
     * @return normalized src
     * @draft ICU 4.4
     */
    UnicodeString
    normalize(const UnicodeString &src, UErrorCode &errorCode) const {
        UnicodeString result;
        normalize(src, result, errorCode);
        return result;
    }
    /**
     * Writes the normalized form of the source string to the destination string
     * (replacing its contents) and returns the destination string.
     * @param src source string
     * @param dest destination string; its contents is replaced with normalized src
     * @return dest
     * @draft ICU 4.4
     */
    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const = 0;
    /**
     * Appends the normalized form of the second string to the first string
     * (merging them at the boundary) and returns the first string.
     * The result is normalized if the first string was normalized.
     * @param first string, should be normalized
     * @param second string, will be normalized
     * @return first
     * @draft ICU 4.4
     */
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const = 0;
    /**
     * Appends the second string to the first string
     * (merging them at the boundary) and returns the first string.
     * The result is normalized if both the strings were normalized.
     * @param first string, should be normalized
     * @param second string, should be normalized
     * @return first
     * @draft ICU 4.4
     */
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const = 0;

protected:
    Normalizer2(const Normalizer2Impl &ni) : impl(ni) {}
    // TODO: no copy, ==, etc.
    const Normalizer2Impl &impl;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION
#endif  // __NORMALIZER2_H__
