/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdata.h
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATA_H__
#define __COLLATIONDATA_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "collation.h"
#include "normalizer2impl.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

class CollationIterator;

/**
 * Data container with access functions which walk the data structures.
 */
class U_I18N_API CollationData : public UMemory {
public:
    uint32_t getCE32(UChar32 c) const {
        // TODO: Make this virtual so that we can use utrie2_get32() in the CollationDataBuilder?
        return UTRIE2_GET32(trie, c);
    }

    /**
     * Resolves the ce32 with a BUILDER_CONTEXT_TAG into another CE32.
     */
    virtual uint32_t nextCE32FromBuilderContext(CollationIterator &iter, uint32_t ce32,
                                                UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
        return 0;
    }

    UBool isUnsafeBackward(UChar32 c) const {
        if(U_IS_TRAIL(c)) {
            return TRUE;
        }
        return TRUE;  // TODO
        // TODO: Are all cc!=0 marked as unsafe for prevCE() (because of discontiguous contractions)?
        // TODO: Use a frozen UnicodeSet rather than an imprecise bit set, at least initially.
    }

    const int64_t *getCEs(int32_t index) const {
        return NULL;  // TODO
    }

    const uint32_t *getCE32s(int32_t index) const {
        return NULL;  // TODO
    }

    /**
     * Returns a pointer to prefix or contraction-suffix matching data.
     */
    const uint16_t *getContext(int32_t index) const {
        return NULL;  // TODO
    }

    /**
     * Returns this data object's main UTrie2.
     * To be used only by the CollationIterator constructor.
     */
    const UTrie2 *getTrie() const { return trie; }

    const CollationData *getBase() const { return base; }

    /**
     * Returns a simple array of 19+21+27 CEs, one per canonical Jamo L/V/T.
     * For fast handling of HANGUL_TAG.
     *
     * Returns NULL if Jamos require more complicated handling.
     * In this case, the DECOMP_HANGUL flag is set.
     */
    const int64_t *getJamoCEs() const {
        // TODO
        // Build & return a simple array of CE32s.
        // Tailoring: Only necessary if Jamos are tailored.
        // If any Jamos have special CE32s, then set DECOMP_HANGUL instead.
        return NULL;
    }

    /**
     * Returns the single-byte primary weight (xx000000) for '0' (U+0030).
     */
    uint32_t getZeroPrimary() const {
        return 0x12000000;  // TODO
    }

    UBool isCompressibleLeadByte(uint32_t b) const {
        return FALSE;  // TODO
    }

    inline UBool isCompressiblePrimary(uint32_t p) const {
        return isCompressibleLeadByte(p >> 24);
    }

    uint16_t getFCD16(UChar32 c) const {
        return nfcImpl.getFCD16(c);
    }

    const Normalizer2Impl &getNFCImpl() const {
        return nfcImpl;
    }

    /**
     * Collation::DECOMP_HANGUL etc.
     */
    int8_t getFlags() const {
        return 0;  // TODO
    }

protected:
    // Main lookup trie.
    const UTrie2 *trie;

    const Normalizer2Impl &nfcImpl;

private:
    UBool isFinalData;  // TODO: needed?
    const CollationData *base;  // TODO: probably needed?
};

/**
 * Low-level CollationData builder.
 * Takes (character, CE) pairs and builds them into runtime data structures.
 * Supports characters with context prefixes and contraction suffixes.
 */
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

// TODO: In CollationWeights allocator,
// try to treat secondary & tertiary weights as 3/4-byte weights with bytes 1 & 2 == 0.
// Natural secondary limit of 0x10000.

// TODO: Copy Latin-1 into each tailoring, but not 0..ff, rather 0..7f && c0..ff.

// TODO: For U+0000, move its normal ce32 into CE32s[0] and set IMPLICIT_TAG with 0 data bits.

// TODO: Not compressible:
// - digits
// - Latin
// - Hani
// - trail weights

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATA_H__
