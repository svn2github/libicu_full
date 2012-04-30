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
    CollationData(const Normalizer2Impl &nfc)
            : trie(NULL), nfcImpl(nfc),
              ce32s(NULL), base(NULL),
              fcd16_F00(NULL), compressibleBytes(NULL) {}

    uint32_t getCE32(UChar32 c) const {
        // TODO: Make this virtual so that we can use utrie2_get32() in the CollationDataBuilder?
        return UTRIE2_GET32(trie, c);
    }

#if 0
    // TODO: Try v1 approach of building a runtime CollationData instance for canonical closure,
    // rather than using the builder and its dynamic data structures for lookups.
    // If this is acceptable, then we can revert the BUILDER_CONTEXT_TAG to a RESERVED_TAG.
    /**
     * Resolves the ce32 with a BUILDER_CONTEXT_TAG into another CE32.
     */
    virtual uint32_t nextCE32FromBuilderContext(CollationIterator &iter, uint32_t ce32,
                                                UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
        return 0;
    }
#endif

    UBool isUnsafeBackward(UChar32 c) const {
        if(U_IS_TRAIL(c)) { return TRUE; }
        return TRUE;  // TODO
        // TODO: Are all cc!=0 marked as unsafe for prevCE() (because of discontiguous contractions)?
        // TODO: Use a frozen UnicodeSet rather than an imprecise bit set, at least initially.
    }

    const int64_t *getCEs(int32_t /*index*/) const {
        return NULL;  // TODO
    }

    const uint32_t *getCE32s(int32_t index) const {
        return ce32s + index;
    }

    /**
     * Returns a pointer to prefix or contraction-suffix matching data.
     */
    const uint16_t *getContext(int32_t /*index*/) const {
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
        return compressibleBytes[b];
    }

    inline UBool isCompressiblePrimary(uint32_t p) const {
        return isCompressibleLeadByte(p >> 24);
    }

    /**
     * Returns the FCD16 value for code point c. c must be >= 0.
     */
    uint16_t getFCD16(UChar32 c) const {
        if(c < 0xf00) { return fcd16_F00[c]; }
        if(c < 0xd800 && !nfcImpl.singleLeadMightHaveNonZeroFCD16(c)) { return 0; }
        return nfcImpl.getFCD16FromNormData(c);
    }

    /** Returns the FCD data for U+0000<=c<U+0F00. */
    uint16_t getFCD16FromBelowF00(UChar32 c) const { return fcd16_F00[c]; }
    // TODO: Change the FCDUTF16CollationIterator to use this.

    const Normalizer2Impl &getNFCImpl() const {
        return nfcImpl;
    }

    /**
     * Collation::DECOMP_HANGUL etc.
     */
    int8_t getFlags() const {
        return 0;  // TODO
    }

protected:  // TODO: private?
    // Main lookup trie.
    const UTrie2 *trie;

    const Normalizer2Impl &nfcImpl;

private:
    friend class CollationDataBuilder;

    // Array of CE32 values.
    // At index 0 there must be CE32(U+0000)
    // which has a special-tag for NUL-termination handling.
    const uint32_t *ce32s;
    const CollationData *base;
    // Linear FCD16 data table for U+0000..U+0EFF.
    const uint16_t *fcd16_F00;
    // Flags for which primary-weight lead bytes are compressible.
    const UBool *compressibleBytes;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATA_H__
