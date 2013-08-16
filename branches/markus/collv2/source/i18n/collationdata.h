/*
*******************************************************************************
* Copyright (C) 2010-2013, International Business Machines
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

#include "unicode/ucol.h"
#include "unicode/uniset.h"
#include "collation.h"
#include "collationsettings.h"
#include "normalizer2impl.h"
#include "utrie2.h"

struct UDataMemory;

U_NAMESPACE_BEGIN

/**
 * Collation data container.
 * Immutable data created by a CollationDataBuilder, or loaded from a file,
 * or deserialized from API-provided binary data.
 *
 * Includes data for the collation base (root/default), aliased if this is not the base.
 */
struct U_I18N_API CollationData : public UMemory {
    CollationData(const Normalizer2Impl &nfc)
            : trie(NULL),
              ce32s(NULL), ces(NULL), contexts(NULL), base(NULL),
              jamoCE32s(NULL),
              nfcImpl(nfc),
              numericPrimary(0x12000000),
              compressibleBytes(NULL),
              unsafeBackwardSet(NULL),
              scripts(NULL), scriptsLength(0),
              rootElements(NULL), rootElementsLength(0) {}

    uint32_t getCE32(UChar32 c) const {
        return UTRIE2_GET32(trie, c);
    }

    uint32_t getCE32FromSupplementary(UChar32 c) const {
        return UTRIE2_GET32_FROM_SUPP(trie, c);
    }

    UBool isUnsafeBackward(UChar32 c) const {
        return unsafeBackwardSet->contains(c);
    }

    UBool isCompressibleLeadByte(uint32_t b) const {
        return compressibleBytes[b];
    }

    inline UBool isCompressiblePrimary(uint32_t p) const {
        return isCompressibleLeadByte(p >> 24);
    }

    /**
     * Returns the CE32 for an indirect special CE32 (e.g., with DIGIT_TAG).
     * Requires that ce32 is special.
     */
    uint32_t getIndirectCE32(uint32_t ce32) const;
    /**
     * Returns the CE32 for an indirect special CE32 (e.g., with DIGIT_TAG),
     * if ce32 is special.
     */
    uint32_t getFinalCE32(uint32_t ce32) const;

    int64_t getCEFromOffsetCE32(UChar32 c, uint32_t ce32) const {
        int64_t dataCE = ces[Collation::indexFromCE32(ce32)];
        return Collation::makeCE(Collation::getThreeBytePrimaryForOffsetData(c, dataCE));
    }

    /**
     * Returns the FCD16 value for code point c. c must be >= 0.
     */
    uint16_t getFCD16(UChar32 c) const {
        return nfcImpl.getFCD16(c);
    }

    /**
     * Finds the variable top primary weight for the maximum variable reordering group.
     * Returns 0 if the maxVariable is not valid.
     */
    uint32_t getVariableTopForMaxVariable(CollationSettings::MaxVariable maxVariable) const;

    /**
     * Returns the first primary for the script's reordering group.
     * @return the primary with only the first primary lead byte of the group
     *         (not necessarily an actual root collator primary weight)
     */
    uint32_t getFirstPrimaryForGroup(int32_t script) const;

    int32_t getEquivalentScripts(int32_t script,
                                 int32_t dest[], int32_t capacity, UErrorCode &errorCode) const;

    /**
     * Writes the permutation table for the given reordering of scripts and groups,
     * mapping from default-order primary-weight lead bytes to reordered lead bytes.
     * The caller checks for illegal arguments and
     * takes care of [DEFAULT] and memory allocation.
     */
    void makeReorderTable(const int32_t *reorder, int32_t length,
                          uint8_t table[256], UErrorCode &errorCode) const;

    /** @see jamoCE32s */
    static const int32_t JAMO_CE32S_LENGTH = 19 + 21 + 27;

    /** Main lookup trie. */
    const UTrie2 *trie;
    /**
     * Array of CE32 values.
     * At index 0 there must be CE32(U+0000)
     * to support U+0000's special-tag for NUL-termination handling.
     */
    const uint32_t *ce32s;
    /** Array of CE values for expansions and OFFSET_TAG. */
    const int64_t *ces;
    /** Array of prefix and contraction-suffix matching data. */
    const UChar *contexts;
    /** Base collation data, or NULL if this data itself is a base. */
    const CollationData *base;
    /**
     * Simple array of JAMO_CE32S_LENGTH=19+21+27 CE32s, one per canonical Jamo L/V/T.
     * They are normally simple CE32s, rarely expansions.
     * For fast handling of HANGUL_TAG.
     */
    const uint32_t *jamoCE32s;
    const Normalizer2Impl &nfcImpl;
    /** The single-byte primary weight (xx000000) for numeric collation. */
    uint32_t numericPrimary;
    /** 256 flags for which primary-weight lead bytes are compressible. */
    const UBool *compressibleBytes;
    /**
     * Set of code points that are unsafe for starting string comparison after an identical prefix,
     * or in backwards CE iteration.
     */
    const UnicodeSet *unsafeBackwardSet;

    /**
     * Data for scripts and reordering groups.
     * Uses include building a reordering permutation table and
     * providing script boundaries to AlphabeticIndex.
     *
     * This data is a sorted list of primary-weight lead byte ranges (reordering groups),
     * each with a list of pairs sorted in base collation order;
     * each pair contains a script/reorder code and the lowest primary weight for that script.
     *
     * Data structure:
     * - Each reordering group is encoded in n+2 16-bit integers.
     *   - First integer:
     *     Bits 15..8: First byte of the reordering group's range.
     *     Bits  7..0: Last byte of the reordering group's range.
     *   - Second integer:
     *     Length n of the list of script/reordering codes.
     *   - Each further integer is a script or reordering code.
     */
    const uint16_t *scripts;
    int32_t scriptsLength;

    /**
     * Collation elements in the root collator.
     * Used by the CollationRootElements class. The data structure is described there.
     * NULL in a tailoring.
     */
    const uint32_t *rootElements;
    int32_t rootElementsLength;

private:
    int32_t findScript(int32_t script) const;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATA_H__
