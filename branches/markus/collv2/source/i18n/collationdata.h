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
#include "normalizer2impl.h"
#include "utrie2.h"

struct UDataMemory;

U_NAMESPACE_BEGIN

/**
 * Collation data container.
 * Includes data for the collation base (root/default), aliased if this is not the base.
 */
struct U_I18N_API CollationData : public UMemory {
    /**
     * Options bit 0: Perform the FCD check on the input text and deliver normalized text.
     */
    static const int32_t CHECK_FCD = 1;
    /**
     * Options bit 1: Numeric collation.
     * Also known as CODAN = COllate Digits As Numbers.
     *
     * Treat digit sequences as numbers with CE sequences in numeric order,
     * rather than returning a normal CE for each digit.
     */
    static const int32_t NUMERIC = 2;
    /**
     * "Shifted" alternate handling, see ALTERNATE_MASK.
     */
    static const int32_t SHIFTED = 4;
    /**
     * Options bits 3..2: Alternate-handling mask. 0 for non-ignorable.
     * Reserve values 8 and 0xc for shift-trimmed and blanked.
     */
    static const int32_t ALTERNATE_MASK = 0xc;
    /**
     * Options bits 7..4: The 4-bit maxVariable value bit field is shifted by this value.
     */
    static const int32_t MAX_VARIABLE_SHIFT = 4;
    /** maxVariable options bit mask before shifting. */
    static const int32_t MAX_VARIABLE_MASK = 0xf0;
    /**
     * Options bit 8: Sort uppercase first if caseLevel or caseFirst is on.
     */
    static const int32_t UPPER_FIRST = 0x100;
    /**
     * Options bit 9: Keep the case bits in the tertiary weight (they trump other tertiary values)
     * unless case level is on (when they are *moved* into the separate case level).
     * By default, the case bits are removed from the tertiary weight (ignored).
     *
     * When CASE_FIRST is off, UPPER_FIRST must be off too, corresponding to
     * the tri-value UCOL_CASE_FIRST attribute: UCOL_OFF vs. UCOL_LOWER_FIRST vs. UCOL_UPPER_FIRST.
     */
    static const int32_t CASE_FIRST = 0x200;
    /**
     * Options bit mask for caseFirst and upperFirst, before shifting.
     * Same value as caseFirst==upperFirst.
     */
    static const int32_t CASE_FIRST_AND_UPPER_MASK = CASE_FIRST | UPPER_FIRST;
    /**
     * Options bit 10: Insert the case level between the secondary and tertiary levels.
     */
    static const int32_t CASE_LEVEL = 0x400;
    /**
     * Options bit 11: Compare secondary weights backwards. ("French secondary")
     */
    static const int32_t BACKWARD_SECONDARY = 0x800;
    /**
     * Options bits 15..12: The 4-bit strength value bit field is shifted by this value.
     * It is the top used bit field in the options. (No need to mask after shifting.)
     */
    static const int32_t STRENGTH_SHIFT = 12;
    /** Strength options bit mask before shifting. */
    static const int32_t STRENGTH_MASK = 0xf000;

    /** maxVariable values */
    enum MaxVariable {
        MAX_VAR_SPACE,
        MAX_VAR_PUNCT,
        MAX_VAR_SYMBOL,
        MAX_VAR_CURRENCY
    };

    CollationData(const Normalizer2Impl &nfc)
            : trie(NULL),
              ce32s(NULL), ces(NULL), contexts(NULL), base(NULL),
              jamoCEs(NULL),
              nfcImpl(nfc),
              options((UCOL_DEFAULT_STRENGTH << STRENGTH_SHIFT) |
                      (MAX_VAR_PUNCT << MAX_VARIABLE_SHIFT)),
              variableTop(0), numericPrimary(0x12000000),
              compressibleBytes(NULL), reorderTable(NULL),
              reorderCodes(NULL), reorderCodesLength(0),
              unsafeBackwardSet(NULL),
              scripts(NULL), scriptsLength(0),
              memory(NULL),
              ownedTrie(NULL), ownedUnsafeBackwardSet(NULL) {}

    CollationData *clone() const;

    ~CollationData();

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

    UColAttributeValue getCaseFirst() const {
        int32_t option = options & CASE_FIRST_AND_UPPER_MASK;
        return (option == 0) ? UCOL_OFF :
                (option == CASE_FIRST) ? UCOL_LOWER_FIRST : UCOL_UPPER_FIRST;
    }

    void setAlternateHandling(UColAttributeValue value,
                              int32_t defaultOptions, UErrorCode &errorCode);

    UColAttributeValue getAlternateHandling() const {
        return ((options & CollationData::ALTERNATE_MASK) == 0) ? UCOL_NON_IGNORABLE : UCOL_SHIFTED;
    }

    static uint32_t getTertiaryMask(int32_t options) {
        // Remove the case bits from the tertiary weight when caseLevel is on or caseFirst is off.
        return ((options & (CASE_LEVEL | CASE_FIRST)) == CASE_FIRST) ?
                Collation::CASE_AND_TERTIARY_MASK : Collation::ONLY_TERTIARY_MASK;
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
        return unsafeBackwardSet->contains(c);
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
        return nfcImpl.getFCD16(c);
    }

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

    enum {
        /**
         * Number of int32_t indexes.
         * Must be a multiple of 2 for padding,
         * should be a multiple of 4 for optimal alignment.
         */
        IX_INDEXES_LENGTH,
        /**
         * Bits 31..24: numericPrimary, for numeric collation
         *      23.. 0: options bit set
         */
        IX_OPTIONS,
        IX_RESERVED2,
        IX_RESERVED3,

        // Byte offsets from the start of the data, after the generic header.
        // The indexes[] are at byte offset 0, other data follows.
        // The data items should be in descending order of unit size,
        // to minimize the need for padding.
        /** Byte offset to the collation trie. Its length is a multiple of 8 bytes. */
        IX_TRIE_OFFSET,
        IX_RESERVED5_OFFSET,
        /** Byte offset to int64_t ces[] */
        IX_CES_OFFSET,
        IX_RESERVED7_OFFSET,

        /** Byte offset to uint32_t ce32s[] */
        IX_CE32S_OFFSET,
        IX_RESERVED9_OFFSET,
        /** Byte offset to int32_t reorderCodes[] */
        IX_REORDER_CODES_OFFSET,
        IX_RESERVED11_OFFSET,

        /** Byte offset to UChar *contexts[] */
        IX_CONTEXTS_OFFSET,
        /** Byte offset to uint16_t [] with serialized unsafeBackwardSet. */
        IX_UNSAFE_BWD_OFFSET,
        /** Byte offset to uint16_t scripts[] */
        IX_SCRIPTS_OFFSET,
        IX_RESERVED15_OFFSET,

        /** Byte offset to UBool compressibleBytes[] */
        IX_COMPRESSIBLE_BYTES_OFFSET,
        /** Byte offset to uint8_t reorderTable[] */
        IX_REORDER_TABLE_OFFSET,
        IX_RESERVED18_OFFSET,
        IX_TOTAL_SIZE,

        /** Array offset to Jamo CEs in ces[], or <0 if none. */
        IX_JAMO_CES_START,
        IX_RESERVED21,
        IX_RESERVED22,
        IX_RESERVED23,

        IX_COUNT
    };

    void setData(const CollationData *baseData, const uint8_t *inBytes, UErrorCode &errorCode);

    static UBool U_CALLCONV
    isAcceptable(void *context, const char *type, const char *name, const UDataInfo *pInfo);

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
     * Simple array of 19+21+27 CEs, one per canonical Jamo L/V/T.
     * For fast handling of HANGUL_TAG.
     */
    const int64_t *jamoCEs;
        // TODO
        // Build & return a simple array of CEs.
        // Tailoring: Only necessary if Jamos are tailored.
    const Normalizer2Impl &nfcImpl;
    /** Collation::CHECK_FCD etc. */
    int32_t options;
    /** Variable-top primary weight. 0 if "shifted" mode is off. */
    uint32_t variableTop;
    /** The single-byte primary weight (xx000000) for '0' (U+0030). */
    uint32_t numericPrimary;
    /** 256 flags for which primary-weight lead bytes are compressible. */
    const UBool *compressibleBytes;
    /** 256-byte table for reordering permutation of primary lead bytes; NULL if no reordering. */
    const uint8_t *reorderTable;
    /** Array of reorder codes; NULL if no reordering. */
    const int32_t *reorderCodes;
    int32_t reorderCodesLength;
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

    UDataMemory *memory;
    /** Same as trie if this data object owns it, otherwise NULL. */
    UTrie2 *ownedTrie;
    /** Same as unsafeBackwardSet if this data object owns it, otherwise NULL. */
    UnicodeSet *ownedUnsafeBackwardSet;

private:
    int32_t findScript(int32_t script) const;
    /**
     * Finds the variable top primary weight for the maximum variable reordering group.
     * Returns 0 if the maxVariable is not valid.
     */
    uint32_t getVariableTopForMaxVariable(MaxVariable maxVariable) const;
};

/**
 * Format of collation data (ucadata.icu, binary data in coll/ *.res files).
 * Format version 4.0.
 *
 * TODO: document format, see normalizer2impl.h
 */

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATA_H__
