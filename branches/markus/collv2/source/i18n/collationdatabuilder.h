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

U_NAMESPACE_BEGIN

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

    void add(const UnicodeString &prefix, const UnicodeString &s,
             const int64_t ces[], int32_t cesLength,
             UErrorCode &errorCode);

    CollationData *build(UErrorCode &errorCode);

private:
    void initHanRanges(UErrorCode &errorCode);
    void initHanCompat(UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order.
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return the next primary after 'end': start primary incremented by ((end-start)+1)*step
     */
    uint32_t setThreeByteOffsetRange(UChar32 start, UChar32 end,
                                     uint32_t primary, int32_t step,
                                     UErrorCode &errorCode);

    static uint32_t makeLongPrimaryCE32(uint32_t p) { return p + 1; }

    static uint32_t makeSpecialCE32(uint32_t tag, uint32_t value) {
        return Collation::MIN_SPECIAL_CE32 | (tag << 20) | value;
    }

    uint32_t getCE32FromOffsetCE32(UChar32 c, uint32_t ce32) const;

    UBool isCompressibleLeadByte(uint32_t b) const {
        return base != NULL ? base->isCompressibleLeadByte(b) : compressibleBytes[b];
    }

    inline UBool isCompressiblePrimary(uint32_t p) const {
        return isCompressibleLeadByte(p >> 24);
    }

    const Normalizer2Impl &nfcImpl;
    const CollationData *base;
    UTrie2 *trie;
    UVector32 ce32s;
    UVector64 ce64s;
    // Flags for which primary-weight lead bytes are compressible.
    // NULL in a tailoring builder, consult the base instead.
    UBool *compressibleBytes;
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
