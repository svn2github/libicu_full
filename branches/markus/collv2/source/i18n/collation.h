/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collation.h
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#ifndef __COLLATION_H__
#define __COLLATION_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

/**
 * Collation v2 basic definitions and static helper functions.
 *
 * Data structures except for expansion tables store 32-bit CEs which are
 * either specials (see tags below) or are compact forms of 64-bit CEs.
 */
class U_I18N_API Collation {
    // Special sort key bytes for all levels.
    static const uint8_t TERMINATOR_BYTE = 0;
    static const uint8_t LEVEL_SEPARATOR_BYTE = 1;
    static const uint8_t MERGE_SEPARATOR_BYTE = 2;
    static const uint32_t MERGE_SEPARATOR_PRIMARY = 0x02000000;  // U+FFFE

    /**
     * Primary compression low terminator.
     * Reserved value in primary second byte if the lead byte is compressible.
     * Otherwise usable in all CE weight bytes.
     */
    static const uint8_t PRIMARY_COMPRESSION_LOW_BYTE = 3;
    /**
     * Primary compression high terminator.
     * Reserved value in primary second byte if the lead byte is compressible.
     * Otherwise usable in all CE weight bytes.
     */
    static const uint8_t PRIMARY_COMPRESSION_HIGH_BYTE = 0xff;

    /** Default secondary/tertiary weight lead byte. */
    static const uint8_t COMMON_BYTE = 5;
    static const uint32_t COMMON_WEIGHT = 0x0500;
    /** Middle 16 bits of a CE with a common secondary weight. */
    static const uint32_t COMMON_SECONDARY_CE = 0x05000000;
    /** Lower 16 bits of a CE with a common tertiary weight. */
    static const uint32_t COMMON_TERTIARY_CE = 0x0500;
    /** Lower 32 bits of a CE with common secondary and tertiary weights. */
    static const uint32_t COMMON_SEC_AND_TER_CE = 0x05000500;

    static const uint8_t UNASSIGNED_IMPLICIT_BYTE = 0xfd;  // compressible

    static const uint8_t TRAIL_WEIGHT_BYTE = 0xfe;  // not compressible
    static const uint32_t MAX_PRIMARY = 0xfeff0000;  // U+FFFF

    /** Primary lead byte for special tags, not used as a primary lead byte in resolved CEs. */
    static const uint8_t SPECIAL_BYTE = 0xff;

    /**
     * The lowest "special" CE32 value.
     * This value itself is used to indicate a fallback to the base collator,
     * regardless of the semantics of its tag bit field,
     * to minimize the fastpath lookup code.
     */
    static const uint32_t MIN_SPECIAL_CE32 = 0xff000000;

    static const uint32_t UNASSIGNED_CE32 = 0xffffffff;  // Compute an unassigned-implicit CE.

    /** No CE: End of input. Only used in runtime code, not stored in data. */
    static const uint32_t NO_CE_WEIGHT = 1;
    static const int64_t NO_CE = 0x100010001;  // Primary/secondary/tertiary weights = 1.

    /**
     * Special-CE32 tags, from bits 23..20 of a special 32-bit CE.
     * Bits 19..0 are used for data.
     */
    enum {
        /**
         * Tags 0..5 are used for Latin mini expansions
         * of two simple CEs [pp, 05, tt] [00, ss, 05].
         * Bits 23..16: Single-byte primary weight pp=00..5F of the first CE.
         * Bits 15.. 8: Tertiary weight tt of the first CE.
         * Bits  7.. 0: Secondary weight ss of the second CE.
         */
        MAX_LATIN_EXPANSION_TAG = 5,
        /**
         * Points to 1..16 non-special 32-bit CE32s.
         * Bits 19..4: Index into uint32_t table.
         * Bits  3..0: Length-1.
         */
        EXPANSION32_TAG = 6,
        /**
         * Points to 1..16 64-bit CEs.
         * Bits 19..4: Index into CE table.
         * Bits  3..0: Length-1.
         */
        EXPANSION_TAG = 7,
        /**
         * Points to prefix trie.
         * Bits 19..0: Index into prefix/contraction data.
         */
        PREFIX_TAG = 8,
        /**
         * Points to contraction data.
         * Bits 19..0: Index into prefix/contraction data.
         */
        CONTRACTION_TAG = 9,
        /**
         * Used only in the collation data builder.
         * Data bits point into a builder-specific data structure with non-final data.
         */
        BUILDER_CONTEXT_TAG = 10,
        /**
         * Decimal digit.
         * Bits 19..4: Index into uint32_t table for non-CODAN CE32.
         * Bits  3..0: Digit value 0..9.
         */
        DIGIT_TAG = 11,
        /**
         * Hiragana character, except U+3099..309C inherit their Hiragana-ness
         * from the preceding character.
         * TODO: Consider adding 3099..309C to the unsafe-backward set. (Do we need Hiragana quaternaries in backward iteration?)
         * Bits 19..0: Index into uint32_t table for normal CE32.
         */
        HIRAGANA_TAG = 12,
        /**
         * Tag for a Hangul syllable.
         */
        HANGUL_TAG = 13,
        /**
         * Tag for CEs with primary weights in code point order.
         * Bits 19..4: Lower 16 bits of the base code point.
         * Bits  3..0: Per-code point primary-weight increment minus 1.
         */
        OFFSET_TAG = 14,
        /**
         * Implicit CE tag. Compute an unassigned-implicit CE.
         * Also used for U+0000, for moving the NUL-termination handling
         * from the regular fastpath into specials-handling code.
         *
         * The data bits are 0 for U+0000, otherwise 0xfffff (UNASSIGNED_CE32=0xffffffff).
         */
        IMPLICIT_TAG = 15
    };

    /**
     * Flag bit: A subclass needs to perform the FCD check on the input text
     * and deliver normalized text.
     */
    static const int8_t CHECK_FCD = 1;
    /**
     * Flag bit: A subclass needs to look for Hangul syllables and decompose them into Jamos.
     */
    static const int8_t DECOMP_HANGUL = 2;
    /**
     * Flag bit: COllate Digits As Numbers.
     * Treat digit sequences as numbers with CE sequences in numeric order,
     * rather than returning a normal CE for each digit.
     */
    static const int8_t CODAN = 4;

    static inline UBool isSpecialCE32(uint32_t ce32) {
        // Java: Emulate unsigned-int less-than comparison.
        // return (ce32^0x80000000)>=0x7f000000;
        return ce32 >= MIN_SPECIAL_CE32;
    }

    static inline int32_t getSpecialCE32Tag(uint32_t ce32) {
        return (int32_t)((ce32 >> 20) & 0xf);
    }

    /** Returns a 64-bit CE from a non-special CE32. */
    static inline int64_t ceFromCE32(uint32_t ce32) {
        uint32_t tertiary = ce32 & 0xff;
        if(tertiary > 1) {
            // normal form ppppsstt -> pppp0000ss00tt00
            return ((int64_t)(ce32 & 0xffff0000) << 32) | ((ce32 & 0xff00) << 16) | (tertiary << 8);
        } else if(tertiary == 1) {
            // long-primary form pppppp01 -> pppppp00050000500
            return ((int64_t)(ce32 - 1) << 32) | COMMON_SEC_AND_TER_CE;
        } else /* tertiary == 0 */ {
            // long-secondary form sssstt00 -> 00000000sssstt00,
            // including the tertiary-ignorable, all-zero CE
            // Java: Use a mask to work around sign extension.
            // return (long)ce32&0xffffffff;
            return ce32;
        }
    }

    /**
     * Computes a CE from a 3-byte base primary and a code point offset.
     * See OFFSET_TAG.
     */
    static int64_t getCEFromThreeByteOffset(uint32_t basePrimary, UBool isCompressible,
                                            int32_t offset) {
        // Extract the third byte, minus the minimum byte value,
        // plus the offset, modulo the number of usable byte values, plus the minimum.
        offset += ((int32_t)(basePrimary >> 8) & 0xff) - 3;
        uint32_t primary = (uint32_t)((offset % 253) + 3) << 8;
        offset /= 253;
        // Same with the second byte,
        // but reserve the PRIMARY_COMPRESSION_LOW_BYTE and high byte if necessary.
        if(isCompressible) {
            offset += ((int32_t)(basePrimary >> 16) & 0xff) - 4;
            primary |= (uint32_t)((offset % 251) + 4) << 16;
            offset /= 251;
        } else {
            offset += ((int32_t)(basePrimary >> 16) & 0xff) - 3;
            primary |= (uint32_t)((offset % 253) + 3) << 16;
            offset /= 253;
        }
        // First byte, assume no further overflow.
        primary |= (basePrimary & 0xff000000) + (uint32_t)(offset << 24);
        return ((int64_t)primary << 32) | COMMON_SEC_AND_TER_CE;
    }

    /**
     * Returns the unassigned-character implicit primary weight for any valid code point c.
     */
    static uint32_t unassignedPrimaryFromCodePoint(UChar32 c) {
        // Create a gap before U+0000. Use c=-1 for [first unassigned].
        ++c;
        // Fourth byte: 18 values, every 14th byte value (gap of 13).
        uint32_t primary = 3 + (c % 18) * 14;
        c /= 18;
        // Third byte: 253 values.
        primary |= (3 + (c % 253)) << 8;
        c /= 253;
        // Second byte: 251 values 04..FE excluding the primary compression bytes.
        primary |= (4 + (c % 251)) << 16;
        // One lead byte covers all code points (c < 0x11710E = 1*251*253*18).
        return primary | (UNASSIGNED_IMPLICIT_BYTE << 24);
    }
    // TODO: Set [first unassigned] to unassignedPrimaryFromCodePoint(-1).
    // TODO: Set [last unassigned] to unassignedPrimaryFromCodePoint(0x10ffff).

    static inline int64_t unassignedCEFromCodePoint(UChar32 c) {
        int64_t ce = unassignedPrimaryFromCodePoint(c);
        return (ce << 32) | COMMON_SEC_AND_TER_CE;
    }

    static inline uint32_t reorder(const uint8_t reorderTable[256], uint32_t primary) {
        return ((uint32_t)reorderTable[primary >> 24] << 24) | (primary & 0xffffff);
    }

private:
    Collation();  // No instantiation.
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATION_H__
