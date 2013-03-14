/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
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
#include "unicode/uversion.h"
#include "collation.h"
#include "collationdata.h"
#include "collationsettings.h"
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
class U_I18N_API CollationDataBuilder : public UObject {
public:
    CollationDataBuilder(UErrorCode &errorCode);

    virtual ~CollationDataBuilder();

    virtual UBool isCompressibleLeadByte(uint32_t b) const;

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

    virtual void add(const UnicodeString &prefix, const UnicodeString &s,
                     const int64_t ces[], int32_t cesLength,
                     UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order,
     * if it is worth doing; otherwise no change is made.
     * None of the code points in the range should have complex mappings so far
     * (expansions/contractions/prefixes).
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return TRUE if an OFFSET_TAG range was used for start..end
     */
    UBool maybeSetPrimaryRange(UChar32 start, UChar32 end,
                               uint32_t primary, int32_t step,
                               UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order.
     * Sets range values if that is worth doing, or else individual values.
     * None of the code points in the range should have complex mappings so far
     * (expansions/contractions/prefixes).
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return the next primary after 'end': start primary incremented by ((end-start)+1)*step
     */
    uint32_t setPrimaryRangeAndReturnNext(UChar32 start, UChar32 end,
                                          uint32_t primary, int32_t step,
                                          UErrorCode &errorCode);

    virtual void build(CollationData &data, UErrorCode &errorCode) = 0;

    int32_t lengthOfCE32s() const { return ce32s.size(); }
    int32_t lengthOfCEs() const { return ce64s.size(); }
    int32_t lengthOfContexts() const { return contexts.length(); }

    int32_t serializeTrie(void *data, int32_t capacity, UErrorCode &errorCode) const;
    int32_t serializeUnsafeBackwardSet(uint16_t *data, int32_t capacity,
                                       UErrorCode &errorCode) const;
    UTrie2 *orphanTrie();

protected:
    UBool setJamoCEs(UErrorCode &errorCode);
    void setLeadSurrogates(UErrorCode &errorCode);

    static inline UBool isContractionCE32(uint32_t ce32) {
        return Collation::hasCE32Tag(ce32, Collation::CONTRACTION_TAG);
    }

    uint32_t getCE32FromOffsetCE32(UChar32 c, uint32_t ce32) const;

    int32_t addCE(int64_t ce, UErrorCode &errorCode);
    int32_t addCE32(uint32_t ce32, UErrorCode &errorCode);
    int32_t addConditionalCE32(const UnicodeString &context, uint32_t ce32, UErrorCode &errorCode);

    static uint32_t encodeOneCE(int64_t ce);
    uint32_t encodeCEsAsCE32s(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);
    uint32_t encodeCEs(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);

    void buildMappings(CollationData &data, UErrorCode &errorCode);

    void buildContexts(UErrorCode &errorCode);
    void buildContext(UChar32 c, UErrorCode &errorCode);
    int32_t addContextTrie(uint32_t defaultCE32, UCharsTrieBuilder &trieBuilder,
                           UErrorCode &errorCode);

    const Normalizer2Impl &nfcImpl;
    const CollationData *base;
    const CollationSettings *baseSettings;
    UTrie2 *trie;
    UVector32 ce32s;
    UVector64 ce64s;
    UVector conditionalCE32s;  // vector of ConditionalCE32
    int64_t jamoCEs[19+21+27];
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
