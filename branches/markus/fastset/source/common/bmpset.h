/*
******************************************************************************
*
*   Copyright (C) 2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  bmpset.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007jan29
*   created by: Markus W. Scherer
*/

#ifndef __BMPSET_H__
#define __BMPSET_H__

#include "unicode/utypes.h"
#include "unicode/uniset.h"

U_NAMESPACE_BEGIN

/*
 * Helper class for frozen UnicodeSets, implements contains() and span()
 * optimized for BMP code points. Structured to be UTF-8-friendly.
 *
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF,
 *                    with mixed for illegal ranges.
 * Supplementary characters: Call contains() on the parent set.
 */
class BMPSet : public UMemory {
public:
    BMPSet(const UnicodeSet &parent);
    BMPSet(const BMPSet &otherBMPSet, const UnicodeSet &newParent);

    virtual UBool contains(UChar32 c) const;

    /*
     * Span the initial substring for which each character c has spanCondition==contains(c).
     * It must be s<limit and spanCondition==0 or 1.
     * @return The string pointer which limits the span.
     */
    const UChar *span(const UChar *s, const UChar *limit, USetSpanCondition spanCondition) const;

    /*
     * Span the trailing substring for which each character c has spanCondition==contains(c).
     * It must be s<limit and spanCondition==0 or 1.
     * @return The string pointer which starts the span.
     */
    const UChar *spanBack(const UChar *s, const UChar *limit, USetSpanCondition spanCondition) const;

    /*
     * Span the initial substring for which each character c has spanCondition==contains(c).
     * It must be length>0 and spanCondition==0 or 1.
     * @return The string pointer which limits the span.
     */
    const uint8_t *spanUTF8(const uint8_t *s, int32_t length, USetSpanCondition spanCondition) const;

    /*
     * Span the trailing substring for which each character c has spanCondition==contains(c).
     * It must be length>0 and spanCondition==0 or 1.
     * @return The start of the span.
     */
    int32_t spanBackUTF8(const uint8_t *s, int32_t length, USetSpanCondition spanCondition) const;

private:
    void initBits();
    void overrideIllegal();

    inline UBool containsSlow(UChar32 c, int32_t lo, int32_t hi) const;
    inline UBool containsSlow(UChar32 c) const;

    /*
     * One byte per ASCII character, or trail byte in lead position.
     * 0 or 1 for ASCII characters.
     * The value for trail bytes is the result of contains(FFFD)
     * for faster validity checking at runtime.
     */
    UBool asciiBytes[0xc0];

    /*
     * One bit per code point from U+0000..U+07FF.
     * The bits are organized vertically; consecutive code points
     * correspond to the same bit positions in consecutive table words.
     * With code point parts
     *   lead=c{10..6}
     *   trail=c{5..0}
     * it is set.contains(c)==(table7FF[trail] bit lead)
     *
     * Bits for 0..7F (non-shortest forms) are set to the result of contains(FFFD)
     * for faster validity checking at runtime.
     */
    uint32_t table7FF[64];

    /*
     * One bit per 64 BMP code points.
     * The bits are organized vertically; consecutive 64-code point blocks
     * correspond to the same bit position in consecutive table words.
     * With code point parts
     *   lead=c{15..12}
     *   t1=c{11..6}
     * test bits (lead+16) and lead in bmpBlockBits[t1].
     * If the upper bit is 0, then the lower bit indicates if contains(c)
     * for all code points in the 64-block.
     * If the upper bit is 1, then the block is mixed and set.contains(c)
     * must be called.
     *
     * Bits for 0..7FF (non-shortest forms) and D800..DFFF are set to
     * the result of contains(FFFD) for faster validity checking at runtime.
     */
    uint32_t bmpBlockBits[64];

    /*
     * Inversion list indexes for restricted binary searches in
     * set.findCodePoint(), from
     * set.findCodePoint(U+0800, U+1000, U+2000, .., U+F000, U+10000).
     * U+0800 is the first 3-byte-UTF-8 code point. Code points below U+0800 are
     * always looked up in the bit tables.
     * The last pair of indexes is for finding supplementary code points.
     */
    int32_t list4kStarts[18];

    /*
     * The parent set, for the slower contains() implementation
     * for mixed BMP blocks and for supplementary code points.
     */
    const UnicodeSet &set;
};

inline UBool BMPSet::containsSlow(UChar32 c, int32_t lo, int32_t hi) const {
    return (UBool)(set.findCodePoint(c, lo, hi) & 1);
}

inline UBool BMPSet::containsSlow(UChar32 c) const {
    return (UBool)(set.findCodePoint(c) & 1);
}

U_NAMESPACE_END

#endif
