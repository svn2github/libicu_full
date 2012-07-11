/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatabuilder.h
*
* created on: 2012apr01
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATABUILDER_H__
#define __COLLATIONDATABUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "collation.h"
#include "collationdata.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

class UCharsTrieBuilder;

/**
 * Low-level CollationData builder.
 * Takes (character, CE) pairs and builds them into runtime data structures.
 * Supports characters with context prefixes and contraction suffixes.
 */
class U_I18N_API CollationDataBuilder : public UMemory {
public:
    CollationDataBuilder(UErrorCode &errorCode);

    ~CollationDataBuilder();

    void initBase(UErrorCode &errorCode);

    void initTailoring(const CollationData *b, UErrorCode &errorCode);

    UBool isCompressibleLeadByte(uint32_t b) const {
        return compressibleBytes != NULL ? compressibleBytes[b] : base->isCompressibleLeadByte(b);
    }

    inline UBool isCompressiblePrimary(uint32_t p) const {
        return isCompressibleLeadByte(p >> 24);
    }

    /**
     * @return TRUE if c has CEs in this builder
     */
    UBool isAssigned(UChar32 c) const;

    /**
     * @return the three-byte primary if c maps to a single such CE and has no context data,
     * otherwise returns 0.
     */
    uint32_t getLongPrimaryIfSingleCE(UChar32 c) const;

    /**
     * @return the single CE for c.
     * Sets an error code if c does not have a single CE.
     */
    int64_t getSingleCE(UChar32 c, UErrorCode &errorCode) const;

    void add(const UnicodeString &prefix, const UnicodeString &s,
             const int64_t ces[], int32_t cesLength,
             UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order,
     * if it is worth doing.
     * None of the code points in the range should have complex mappings so far
     * (expansions/contractions/prefixes).
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return TRUE if an OFFSET_TAG range was used for start..end
     */
    UBool setThreeByteOffsetRange(UChar32 start, UChar32 end,
                                  uint32_t primary, int32_t step,
                                  UErrorCode &errorCode);

    CollationData *build(UErrorCode &errorCode);

    int32_t lengthOfCE32s() const { return ce32s.size(); }
    int32_t lengthOfCEs() const { return ce64s.size(); }
    int32_t lengthOfContexts() const { return contexts.length(); }

    int32_t serializeTrie(void *data, int32_t capacity, UErrorCode &errorCode) const;
    int32_t serializeUnsafeBackwardSet(uint16_t *data, int32_t capacity,
                                       UErrorCode &errorCode) const;

private:
    void initHanRanges(UErrorCode &errorCode);
    void initHanCompat(UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order.
     * None of the code points in the range should have complex mappings so far
     * (expansions/contractions/prefixes).
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return the next primary after 'end': start primary incremented by ((end-start)+1)*step
     */
    uint32_t setThreeBytePrimaryRange(UChar32 start, UChar32 end,
                                      uint32_t primary, int32_t step,
                                      UErrorCode &errorCode);

    UBool setJamoCEs(UErrorCode &errorCode);
    void setLeadSurrogates(UErrorCode &errorCode);

    static uint32_t makeLongPrimaryCE32(uint32_t p) { return p + 1; }

    static uint32_t makeSpecialCE32(uint32_t tag, int32_t value) {
        return makeSpecialCE32(tag, (uint32_t)value);
    }
    static uint32_t makeSpecialCE32(uint32_t tag, uint32_t value) {
        return Collation::MIN_SPECIAL_CE32 | (tag << 20) | value;
    }

    static inline UBool isContractionCE32(uint32_t ce32) {
        return Collation::isSpecialCE32(ce32) &&
            Collation::getSpecialCE32Tag(ce32) == Collation::CONTRACTION_TAG;
    }

    uint32_t getCE32FromOffsetCE32(UChar32 c, uint32_t ce32) const;

    int32_t addCE(int64_t ce, UErrorCode &errorCode);
    int32_t addCE32(uint32_t ce32, UErrorCode &errorCode);
    int32_t addConditionalCE32(const UnicodeString &context, uint32_t ce32, UErrorCode &errorCode);

    static uint32_t encodeOneCE(int64_t ce);
    uint32_t encodeCEsAsCE32s(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);
    uint32_t encodeCEs(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);

    void buildContexts(UErrorCode &errorCode);
    void buildContext(UChar32 c, UErrorCode &errorCode);
    int32_t addContextTrie(uint32_t defaultCE32, UCharsTrieBuilder &trieBuilder,
                           UErrorCode &errorCode);

    const Normalizer2Impl &nfcImpl;
    const CollationData *base;
    UTrie2 *trie;
    UVector32 ce32s;
    UVector64 ce64s;
    UVector conditionalCE32s;  // vector of ConditionalCE32
    int64_t jamoCEs[19+21+27];
    // Linear FCD16 data table for U+0000..U+0EFF.
    uint16_t *fcd16_F00;
    // Flags for which primary-weight lead bytes are compressible.
    // NULL in a tailoring builder, consult the base instead.
    UBool *compressibleBytes;
    // Characters that have context (prefixes or contraction suffixes).
    UnicodeSet contextChars;
    // Serialized UCharsTrie structures for finalized contexts.
    UnicodeString contexts;
    UnicodeSet unsafeBackwardSet;
};

#if 0
// TODO: Try v1 approach of building a runtime CollationData instance for canonical closure,
// rather than using the builder and its dynamic data structures for lookups.
// If this is acceptable, then we can revert the BUILDER_CONTEXT_TAG to a RESERVED_TAG.
class CollationDataBuilder : public CollationData {
public:
    virtual uint32_t nextCE32FromBuilderContext(CollationIterator &iter, uint32_t ce32,
                                                UErrorCode &errorCode) {
        // TODO:
        // Build & cache runtime-format prefix/contraction data and return
        // a normal PREFIX_TAG or CONTRACTION_TAG CE32.
        // Then let normal runtime code handle it.
        // This avoids having to prebuild all runtime structures all of the time,
        // and avoids having to provide three versions (get/next/previous)
        // with modified copies of the runtime matching code.
        return 0;
    }
};
#endif

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATABUILDER_H__
