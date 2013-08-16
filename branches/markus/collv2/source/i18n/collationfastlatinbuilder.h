/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationfastlatinbuilder.h
*
* created on: 2013aug09
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONFASTLATINBUILDER_H__
#define __COLLATIONFASTLATINBUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/unistr.h"
#include "unicode/uobject.h"
// TODO: #include "collationfastlatin.h"
#include "uvectr64.h"

U_NAMESPACE_BEGIN

struct CollationData;

// TODO: move to separate .h
class U_I18N_API CollationFastLatin /* all static */ {
public:
    /**
     * Fast Latin format version.
     * Must be incremented for any runtime-incompatible changes,
     * in particular, for changes to any of the following constants.
     *
     * When the major version number of the main data format changes,
     * we can reset this fast Latin version to 0.
     */
    static const uint16_t VERSION = 0;

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

    static const uint32_t SHORT_PRIMARY_MASK = 0xfc00;  // bits 15..10
    static const uint32_t INDEX_MASK = 0x3ff;  // bits 9..0 for expansions & contractions
    static const uint32_t SECONDARY_MASK = 0x3e0;  // bits 9..5
    static const uint32_t CASE_MASK = 0x18;  // bits 4..3
    static const uint32_t LONG_PRIMARY_MASK = 0xff80;  // bits 15..3
    static const uint32_t TERTIARY_MASK = 7;  // bits 2..0

    static const uint32_t CONTRACTION = 0x400;
    static const uint32_t EXPANSION = 0x800;
    static const uint32_t MIN_LONG = 0xc00;
    static const uint32_t LONG_INC = 8;
    static const uint32_t MAX_LONG = 0xff8;  // 128 long/low primaries
    static const uint32_t MIN_SHORT = 0x1000;  // 60 short/high primaries
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

class U_I18N_API CollationFastLatinBuilder : public UObject {
public:
    CollationFastLatinBuilder(UErrorCode &errorCode);
    ~CollationFastLatinBuilder();

    UBool forData(const CollationData &data, UErrorCode &errorCode);

    const uint16_t *getResult() const {
        return reinterpret_cast<const uint16_t *>(result.getBuffer());
    }
    int32_t resultLength() const { return result.length(); }

private:
    UBool loadGroups(const CollationData &data, UErrorCode &errorCode);
    UBool inSameGroup(uint32_t p, uint32_t q) const;

    UBool getCEsFromCE32(const CollationData &data, UChar32 c, uint32_t ce32,
                         UErrorCode &errorCode);
    UBool getCEsFromContractionCE32(const CollationData &data, uint32_t ce32,
                                    UErrorCode &errorCode);
    static int32_t getSuffixFirstCharIndex(const UnicodeString &suffix);
    void addContractionEntry(int32_t x, int64_t cce0, int64_t cce1, UErrorCode &errorCode);
    void addUniqueCE(int64_t ce, UErrorCode &errorCode);
    uint32_t getMiniCE(int64_t ce) const;
    UBool encodeUniqueCEs(UErrorCode &errorCode);
    UBool encodeCharCEs(UErrorCode &errorCode);
    UBool encodeExpansions(UErrorCode &errorCode);
    UBool encodeContractions(UErrorCode &errorCode);
    uint32_t encodeTwoCEs(int64_t first, int64_t second) const;

    static const uint32_t CONTRACTION_FLAG = 0x80000000;

    // temporary "buffer"
    int64_t ce0, ce1;

    int64_t charCEs[CollationFastLatin::NUM_FAST_CHARS][2];

    UVector64 contractionCEs;
    UVector64 uniqueCEs;

    /** One 16-bit mini CE per unique CE. */
    uint16_t *miniCEs;

    uint32_t firstShortPrimary;
    uint32_t lastLatinPrimary;

    UnicodeString result;
    int32_t headerLength;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONFASTLATINBUILDER_H__
