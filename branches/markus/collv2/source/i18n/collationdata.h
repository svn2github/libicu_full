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

#include "unicode/ucol.h"
#include "collation.h"
#include "normalizer2impl.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

class CollationIterator;

/**
 * Collation data container.
 */
struct U_I18N_API CollationData : public UMemory {
    /**
     * Options bit 0: Perform the FCD check on the input text and deliver normalized text.
     */
    static const int32_t CHECK_FCD = 1;
    /**
     * Options bit 1: COllate Digits As Numbers.
     * Treat digit sequences as numbers with CE sequences in numeric order,
     * rather than returning a normal CE for each digit.
     */
    static const int32_t CODAN = 2;
    /**
     * "Shifted" alternate handling.
     */
    static const int32_t SHIFTED = 4;
    /**
     * Alternate-handling mask. 0 for non-ignorable.
     */
    static const int32_t ALTERNATE_MASK = 4;  // TODO: 0xc;
    /**
     * Options bit 3: On quaternary level, sort Hiragana lower than other characters.
     * ("Shifted" primaries sort even lower.)
     */
    static const int32_t HIRAGANA_QUATERNARY = 8;
    /**
     * Options bit 4: Sort uppercase first if caseLevel or caseFirst is on.
     */
    static const int32_t UPPER_FIRST = 0x10;
    /**
     * Options bit 5: Keep the case bits in the tertiary weight (they trump other tertiary values)
     * unless case level is on (when they are *moved* into the separate case level).
     * By default, the case bits are removed from the tertiary weight (ignored).
     *
     * When CASE_FIRST is off, UPPER_FIRST must be off too, corresponding to
     * the tri-value UCOL_CASE_FIRST attribute: UCOL_OFF vs. UCOL_LOWER_FIRST vs. UCOL_UPPER_FIRST.
     */
    static const int32_t CASE_FIRST = 0x20;
    /**
     * Options bit mask for caseFirst and upperFirst, before shifting.
     * Same value as caseFirst==upperFirst.
     */
    static const int32_t CASE_FIRST_AND_UPPER_MASK = CASE_FIRST | UPPER_FIRST;
    /**
     * Options bit 6: Insert the case level between the secondary and tertiary levels.
     */
    static const int32_t CASE_LEVEL = 0x40;
    /**
     * Options bit 7: Compare secondary weights backwards. ("French secondary")
     */
    static const int32_t BACKWARD_SECONDARY = 0x80;
    /**
     * Options bits 11..8: The 4-bit strength value bit field is shifted by this value.
     * It is the top used bit field in the options. (No need to mask after shifting.)
     */
    static const int32_t STRENGTH_SHIFT = 8;
    /** Strength options bit mask before shifting. */
    static const int32_t STRENGTH_MASK = 0xf00;

    CollationData(const Normalizer2Impl &nfc)
            : trie(NULL),
              ce32s(NULL), ces(NULL), contexts(NULL), base(NULL),
              jamoCEs(NULL),
              fcd16_F00(NULL), nfcImpl(nfc),
              options(UCOL_DEFAULT_STRENGTH << STRENGTH_SHIFT),
              variableTop(0), zeroPrimary(0x12000000),
              compressibleBytes(NULL), reorderTable(NULL) {}

    void setStrength(int32_t value, int32_t defaultOptions, UErrorCode &errorCode);

    static int32_t getStrength(int32_t options) {
        return options >> STRENGTH_SHIFT;
    }

    int32_t getStrength() const {
        return getStrength(options);
    }

    /** Sets the options bit for an on/off attribute. */
    void setFlag(int32_t bit, UColAttributeValue value,
                 int32_t defaultOptions, UErrorCode &errorCode);

    UColAttributeValue getFlag(int32_t bit) const {
        return ((options & bit) != 0) ? UCOL_ON : UCOL_OFF;
    }

    void setCaseFirst(UColAttributeValue value, int32_t defaultOptions, UErrorCode &errorCode);

    void setAlternateHandling(UColAttributeValue value,
                              int32_t defaultOptions, UErrorCode &errorCode);

    static uint32_t getTertiaryMask(int32_t options) {
        // Remove the case bits from the tertiary weight when caseLevel is on or caseFirst is off.
        return ((options & (CASE_LEVEL | CASE_FIRST)) == CASE_FIRST) ? 0xffff : 0x3fff;
    }

    static UBool sortsTertiaryUpperCaseFirst(int32_t options) {
        // On tertiary level, consider case bits and sort uppercase first
        // if caseLevel is off and caseFirst==upperFirst.
        return (options & (CASE_LEVEL | CASE_FIRST_AND_UPPER_MASK)) == CASE_FIRST_AND_UPPER_MASK;
    }

    uint32_t getCE32(UChar32 c) const {
        return UTRIE2_GET32(trie, c);
    }

    uint32_t getCE32FromSupplementary(UChar32 c) const {
        return UTRIE2_GET32_FROM_SUPP(trie, c);
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

    /** Main lookup trie. */
    const UTrie2 *trie;
    /**
     * Array of CE32 values.
     * At index 0 there must be CE32(U+0000)
     * which has a special-tag for NUL-termination handling.
     */
    const uint32_t *ce32s;
    /** Array of CE values for expansions and OFFSET_TAG. */
    const int64_t *ces;
    /** Array of prefix and contraction-suffix matching data. */
    const UChar *contexts;
    /** Base collation data, or NULL if this data itself is a base. */
    const CollationData *base;
    /**
     * Simple array of 19+21+27 CEs, one per canonical Jamo L/V/T.
     * For fast handling of HANGUL_TAG.
     */
    const int64_t *jamoCEs;
        // TODO
        // Build & return a simple array of CEs.
        // Tailoring: Only necessary if Jamos are tailored.
    /** Linear FCD16 data table for U+0000..U+0EFF. */
    const uint16_t *fcd16_F00;
    const Normalizer2Impl &nfcImpl;
    /** Collation::CHECK_FCD etc. */
    int32_t options;
    /** Variable-top primary weight. 0 if "shifted" mode is off. */
    uint32_t variableTop;
    /** The single-byte primary weight (xx000000) for '0' (U+0030). */
    uint32_t zeroPrimary;
    /** 256 flags for which primary-weight lead bytes are compressible. */
    const UBool *compressibleBytes;
    /** 256-byte table for reordering permutation of primary lead bytes; NULL if no reordering. */
    uint8_t *reorderTable;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATA_H__
