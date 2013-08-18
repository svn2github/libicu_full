/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationfastlatin.h
*
* created on: 2013aug09
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONFASTLATIN_H__
#define __COLLATIONFASTLATIN_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

class U_I18N_API CollationFastLatin /* all static */ {
public:
    /**
     * Fast Latin format version (one byte 1..FF).
     * Must be incremented for any runtime-incompatible changes,
     * in particular, for changes to any of the following constants.
     *
     * When the major version number of the main data format changes,
     * we can reset this fast Latin version to 1.
     */
    static const uint16_t VERSION = 1;

    static const int32_t LATIN_MAX = 0x17f;
    static const int32_t LATIN_LIMIT = LATIN_MAX + 1;

    static const int32_t PUNCT_START = 0x2000;
    static const int32_t PUNCT_LIMIT = 0x2040;

    // excludes U+FFFE & U+FFFF
    static const int32_t NUM_FAST_CHARS = LATIN_LIMIT + (PUNCT_LIMIT - PUNCT_START);

    // Note on the supported weight ranges:
    // Analysis of UCA 6.3 and CLDR 23 non-search tailorings shows that
    // the CEs for characters in the above ranges, excluding expansions with length >2,
    // excluding contractions of >2 characters, and other restrictions
    // (see the builder's getCEsFromCE32()),
    // use at most about 150 primary weights (except a lot more for en_US_POSIX),
    // where about 94 primary weights are possibly-variable (space/punct/symbol/currency),
    // at most 4 secondary before-common weights,
    // at most 4 secondary after-common weights,
    // at most 16 secondary high weights (in secondary CEs), and
    // at most 4 tertiary after-common weights.
    // The following ranges are designed to support slightly more weights than that.

    // Digits may use long primaries (preserving more short ones)
    // or short primaries (faster) without changing this data structure.
    // (If we supported numeric collation, then digits would have to have long primaries
    // so that special handling does not affect the fast path.)

    static const uint32_t SHORT_PRIMARY_MASK = 0xfc00;  // bits 15..10
    static const uint32_t INDEX_MASK = 0x3ff;  // bits 9..0 for expansions & contractions
    static const uint32_t SECONDARY_MASK = 0x3e0;  // bits 9..5
    static const uint32_t CASE_MASK = 0x18;  // bits 4..3
    static const uint32_t LONG_PRIMARY_MASK = 0xff80;  // bits 15..3
    static const uint32_t TERTIARY_MASK = 7;  // bits 2..0

    /**
     * Contraction with one fast Latin character.
     * Use INDEX_MASK to find the start of the contraction list after the fixed table.
     * The first entry contains the default mapping.
     * Otherwise use CONTR_CHAR_MASK for the contraction character index
     * (in ascending order).
     * Use CONTR_LENGTH_SHIFT for the length of the entry
     * (1=BAIL_OUT, 2=one CE, 3=two CEs).
     *
     * Also, U+0000 maps to a contraction entry, so that the fast path need not
     * check for NUL termination.
     * It usually maps to a contraction list with only the completely ignorable default value.
     */
    static const uint32_t CONTRACTION = 0x400;
    /**
     * An expansion encodes two CEs.
     * Use INDEX_MASK to find the pair of CEs after the fixed table.
     *
     * The higher a mini CE value, the easier it is to process.
     * For expansions and higher, no context needs to be considered.
     */
    static const uint32_t EXPANSION = 0x800;
    /**
     * Encodes one CE with a long/low mini primary (there are 128).
     * All potentially-variable primaries must be in this range,
     * to make the short-primary path as fast as possible.
     */
    static const uint32_t MIN_LONG = 0xc00;
    static const uint32_t LONG_INC = 8;
    static const uint32_t MAX_LONG = 0xff8;
    /**
     * Encodes one CE with a short/high primary (there are 60),
     * plus a secondary CE if the secondary weight is high.
     * Fast handling: At least all letter primaries should be in this range.
     */
    static const uint32_t MIN_SHORT = 0x1000;
    static const uint32_t SHORT_INC = 0x400;
    static const uint32_t MAX_SHORT = SHORT_PRIMARY_MASK;

    static const uint32_t MIN_SEC_BEFORE = 0;  // must add SEC_OFFSET
    static const uint32_t SEC_INC = 0x20;
    static const uint32_t MAX_SEC_BEFORE = MIN_SEC_BEFORE + 4 * SEC_INC;  // 5 before common
    static const uint32_t COMMON_SEC = MAX_SEC_BEFORE + SEC_INC;
    static const uint32_t MIN_SEC_AFTER = COMMON_SEC + SEC_INC;
    static const uint32_t MAX_SEC_AFTER = MIN_SEC_AFTER + 5 * SEC_INC;  // 6 after common
    static const uint32_t MIN_SEC_HIGH = MAX_SEC_AFTER + SEC_INC;  // 20 high secondaries
    static const uint32_t MAX_SEC_HIGH = SECONDARY_MASK;

    /**
     * Lookup: Add this offset to secondary weights, except for completely ignorable CEs.
     * Must be greater than any special value, e.g., MERGE_WEIGHT.
     * The exact value is not relevant for the format version.
     */
    static const uint32_t SEC_OFFSET = SEC_INC;
    static const uint32_t COMMON_SEC_PLUS_OFFSET = COMMON_SEC + SEC_OFFSET;
    static const uint32_t MIN_SEC_HIGH_PLUS_OFFSET = MIN_SEC_HIGH + SEC_OFFSET;

    static const uint32_t LOWER_CASE = 8;  // case bits include this offset

    static const uint32_t COMMON_TER = 0;  // must add TER_OFFSET
    static const uint32_t MAX_TER_AFTER = 7;  // 7 after common

    /**
     * Lookup: Add this offset to tertiary weights, except for completely ignorable CEs.
     * Must be greater than any special value, e.g., MERGE_WEIGHT.
     * The exact value is not relevant for the format version.
     */
    static const uint32_t TER_OFFSET = 8;

    static const uint32_t MERGE_WEIGHT = 3;
    static const uint32_t EOS = 2;  // end of string
    static const uint32_t BAIL_OUT = 1;

    /**
     * Contraction result first word bits 8..0 contain the
     * second contraction character, as a char index 0..NUM_FAST_CHARS-1.
     * Each contraction list is terminated with a word containing CONTR_CHAR_MASK.
     */
    static const uint32_t CONTR_CHAR_MASK = 0x1ff;
    /**
     * Contraction result first word bits 10..9 contain the result length:
     * 1=bail out, 2=one mini CE, 3=two mini CEs
     */
    static const uint32_t CONTR_LENGTH_SHIFT = 9;

    // The following constants are not relevant for the format version.

    static int32_t getCharIndex(UChar c) {
        if(c <= LATIN_MAX) {
            return c;
        } else if(PUNCT_START <= c && c < PUNCT_LIMIT) {
            return c - (PUNCT_START - LATIN_LIMIT);
        } else {
            // Not a fast Latin character.
            // Note: U+FFFE & U+FFFF are forbidden in tailorings
            // and thus do not occur in any contractions.
            return -1;
        }
    }

private:
    CollationFastLatin();  // no constructor
};
// TODO: document data structure

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONFASTLATIN_H__
