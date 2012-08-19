/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationfcd.h
*
* created on: 2012aug18
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONFCD_H__
#define __COLLATIONFCD_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

/**
 * Data and functions for the FCD check fast path.
 *
 * The fast path looks at a pair of 16-bit code units and checks
 * whether there is an FCD boundary between them;
 * there is if the first unit has a trailing ccc=0 (!hasTccc(first))
 * or the second unit has a leading ccc=0 (!hasLccc(second)),
 * or both.
 * When the fast path finds a possible non-boundary,
 * then the FCD check slow path looks at the actual sequence of FCD values.
 *
 * This is a pure optimization.
 * The fast path must at least find all possible non-boundaries.
 * If the fast path is too pessimistic, it costs performance.
 *
 * For a pair of BMP characters, the fast path tests are precise (1 bit per character).
 *
 * For a supplementary code point, the two units are its lead and trail surrogates.
 * We set hasTccc(lead)=true if any of its 1024 associated supplementary code points
 * has lccc!=0 or tccc!=0.
 * We set hasLccc(trail)=true for all trail surrogates.
 * As a result, we leave the fast path if the lead surrogate might start a
 * supplementary code point that is not FCD-inert.
 * (So the fast path need not detect that there is a surrogate pair,
 * nor look ahead to the next full code point.)
 *
 * hasLccc(lead)=true if any of its 1024 associated supplementary code points
 * has lccc!=0, for fast boundary checking between BMP & supplementary.
 *
 * hasTccc(trail)=false:
 * It should only be tested for unpaired trail surrogates which are FCD-inert.
 */
class U_I18N_API CollationFCD {
public:
    // TODO: add unit tests to make sure this agrees with other properties APIs
    // TODO: Do all of these tests actually improve performance?
    static inline UBool hasLccc(UChar c) {
        int32_t i;
        return
            // U+0300 is the first character with lccc!=0.
            c >= 0x300 &&
            (i = lcccIndex[c >> 5]) != 0 &&
            (lcccBits[i] & ((uint32_t)1 << (c & 0x1f))) != 0;
    }

    static inline UBool hasTccc(UChar c) {
        int32_t i;
        return
            // U+00C0 is the first character with tccc!=0.
            c >= 0xc0 &&
            (i = tcccIndex[c >> 5]) != 0 &&
            (tcccBits[i] & ((uint32_t)1 << (c & 0x1f))) != 0;
    }

private:
    CollationFCD();  // No instantiation.

    static const uint8_t lcccIndex[2048];
    static const uint8_t tcccIndex[2048];
    static const uint32_t lcccBits[65];
    static const uint32_t tcccBits[114];
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONFCD_H__
