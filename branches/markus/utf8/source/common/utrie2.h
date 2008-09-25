/*
******************************************************************************
*
*   Copyright (C) 2001-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  utrie2.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2008aug16 (starting from a copy of utrie.h)
*   created by: Markus W. Scherer
*/

#ifndef __UTRIE2_H__
#define __UTRIE2_H__

#include "unicode/utypes.h"
#include "udataswp.h"

U_CDECL_BEGIN

struct UTrie;  /* forward declaration */
#ifndef __UTRIE_H__
typedef struct UTrie UTrie;
#endif

/**
 * \file
 *
 * This is a common implementation of a Unicode trie.
 * It is a kind of compressed, serializable table of 16- or 32-bit values associated with
 * Unicode code points (0..0x10ffff).
 *
 * This is the second common version of a Unicode trie (hence the name UTrie2).
 * Compared with UTrie version 1:
 * - Still splitting BMP code points 11:5 bits for index and data table lookups.
 * - The "folding" mechanism is removed. In UTrie version 1, this somewhat
 *   hard-to-explain mechanism was meant to be used for optimized UTF-16
 *   processing, with application-specific encoding of indexing bits
 *   in the lead surrogate data for the associated supplementary code points.
 * - No separate data for lead surrogate code _units_ vs. code _points_.
 * - For the last single-value code point range (ending with U+10ffff),
 *   the starting code point ("highStart") and the value are stored.
 * - For supplementary code points U+10000..highStart-1 a three-table lookup
 *   (two index tables and one data table) is used. The first index
 *   is truncated, omitting both the BMP portion and the high range.
 * - There is a special small index for 2-byte UTF-8, and the innitial data
 *   entries are designed for fast 1/2-byte UTF-8 lookup.
 */

enum UTrie2ValueBits {
    UTRIE2_16_VALUE_BITS,
    UTRIE2_32_VALUE_BITS,
    UTRIE2_COUNT_VALUE_BITS
};
typedef enum UTrie2ValueBits UTrie2ValueBits;

/**
 * Trie constants, defining shift widths, index array lengths, etc.
 */
enum {
    /** Shift size for getting the index-1 table offset. */
    UTRIE2_SHIFT_1=6+5,

    /** Shift size for getting the index-2 table offset. */
    UTRIE2_SHIFT_2=5,

    /**
     * Difference between the two shift sizes,
     * for getting an index-1 offset from an index-2 offset. 6=11-5
     */
    UTRIE2_SHIFT_1_2=UTRIE2_SHIFT_1-UTRIE2_SHIFT_2,

    /**
     * Number of index-1 entries for the BMP.
     * This part of the index-1 table is omitted from the serialized form.
     */
    UTRIE2_OMITTED_BMP_INDEX_1_LENGTH=0x10000>>UTRIE2_SHIFT_1,

    /** Number of code points per index-1 table entry. 2048=0x800 */
    UTRIE2_CP_PER_INDEX_1_ENTRY=1<<UTRIE2_SHIFT_1,

    /** Number of entries in an index-2 block. 64=0x40 */
    UTRIE2_INDEX_2_BLOCK_LENGTH=1<<UTRIE2_SHIFT_1_2,

    /** Mask for getting the lower bits for the in-index-2-block offset. */
    UTRIE2_INDEX_2_MASK=UTRIE2_INDEX_2_BLOCK_LENGTH-1,

    /** Number of entries in a data block. 32=0x20 */
    UTRIE2_DATA_BLOCK_LENGTH=1<<UTRIE2_SHIFT_2,

    /** Mask for getting the lower bits for the in-data-block offset. */
    UTRIE2_DATA_MASK=UTRIE2_DATA_BLOCK_LENGTH-1,

    /**
     * Shift size for shifting left the index array values.
     * Increases possible data size with 16-bit index values at the cost
     * of compactability.
     * This requires data blocks to be aligned by UTRIE2_DATA_GRANULARITY.
     */
    UTRIE2_INDEX_SHIFT=2,

    /** The alignment size of a data block. Also the granularity for compaction. */
    UTRIE2_DATA_GRANULARITY=1<<UTRIE2_INDEX_SHIFT,

    /* Fixed layout of the first part of the index array. ------------------- */

    /**
     * The BMP part of the index-2 table is fixed and linear and starts at offset 0.
     * Length=2048=0x800=0x10000>>UTRIE2_SHIFT_2.
     */
    UTRIE2_INDEX_2_OFFSET=0,
    UTRIE2_INDEX_2_BMP_LENGTH=0x10000>>UTRIE2_SHIFT_2,

    /**
     * The 2-byte UTF-8 version of the index-2 table follows at offset 2048=0x800.
     * Length 32 for lead bytes C0..DF, regardless of UTRIE2_SHIFT_2.
     */
    UTRIE2_UTF8_2B_INDEX_2_OFFSET=UTRIE2_INDEX_2_BMP_LENGTH,
    UTRIE2_UTF8_2B_INDEX_2_LENGTH=0x800>>6,  /* U+0800 is the first code point after 2-byte UTF-8 */

    /**
     * The index-1 table, only used for supplementary code points, at offset 2080=0x820.
     * Variable length, for code points up to highStart, where the last single-value range starts.
     * Maximum length 512=0x200=0x100000>>UTRIE2_SHIFT_1.
     * (For 0x100000 supplementary code points U+10000..U+10ffff.)
     *
     * The part of the index-2 table for supplementary code points starts
     * after this index-1 table.
     *
     * Both the index-1 table and the following part of the index-2 table
     * are omitted completely if there is only BMP data.
     */
    UTRIE2_INDEX_1_OFFSET=UTRIE2_UTF8_2B_INDEX_2_OFFSET+UTRIE2_UTF8_2B_INDEX_2_LENGTH,
    UTRIE2_MAX_INDEX_1_LENGTH=0x100000>>UTRIE2_SHIFT_1,

    /*
     * Fixed layout of the first part of the data array. -----------------------
     * Starts with 2 blocks (128=0x80 entries) for ASCII.
     */

    /**
     * The illegal-UTF-8 data block follows the ASCII block, at offset 128=0x80.
     * Used with linear access for single bytes 0..0xbf for simple error handling.
     */
    UTRIE2_BAD_UTF8_DATA_OFFSET=0x80,

    /** The start of non-linear-ASCII data blocks, at offset 192=0xc0. */
    UTRIE2_DATA_START_OFFSET=0xc0
};

/**
 * Maximum length of the runtime index array.
 * Limited by its own 16-bit index values, and by uint16_t UTrie2Header.indexLength.
 */
#define UTRIE2_MAX_INDEX_LENGTH 0xffff

/**
 * Maximum length of the runtime data array.
 * Limited by 16-bit index values that are left-shifted by UTRIE2_INDEX_SHIFT,
 * and by uint16_t UTrie2Header.dataLength.
 */
#define UTRIE2_MAX_DATA_LENGTH (0xffff<<UTRIE2_INDEX_SHIFT)

/**
 * Number of bytes for a dummy trie.
 * A dummy trie is an empty runtime trie, used when a real data trie cannot be loaded.
 * The number of bytes works for tries with 32-bit data (worst case).
 * This is at least a multiple of 4, for easy stack array definitions like
 *   uint32_t dummy[UTRIE2_DUMMY_SIZE/4];
 * which guarantee proper alignment.
 *
 * BMP index-2 table,
 * plus a minimal data table with linear ASCII,
 * plus the highValue and a few reserved entries.
 *
 * @see utrie2_unserializeDummy
 */
#define UTRIE2_DUMMY_SIZE (UTRIE2_INDEX_1_OFFSET*2+(UTRIE2_DATA_START_OFFSET+UTRIE2_DATA_GRANULARITY)*4)

/**
 * Run-time Trie structure.
 *
 * Either the data table is 16 bits wide and accessed via the index
 * pointer, with each index item increased by indexLength;
 * in this case, data32==NULL, and data16 is used for direct ASCII access.
 *
 * Or the data table is 32 bits wide and accessed via the data32 pointer.
 */
struct UTrie2 {
    const uint16_t *index;
    const uint16_t *bmpIndex2;  /* for fast BMP access */
    const uint16_t *data16;     /* for fast UTF-8 ASCII access, if 16b data */
    const uint32_t *data32;     /* NULL if 16b data is used via index */

    int32_t indexLength, dataLength;
    uint16_t index2NullOffset, dataNullOffset;
    uint32_t initialValue;
    /** Value returned for out-of-range code points and illegal UTF-8. */
    uint32_t errorValue;

    /* Start of the last range which ends at U+10ffff, and its value. */
    UChar32 highStart;
    int32_t highValueIndex;
};

typedef struct UTrie2 UTrie2;

/**
 * Internal function for part of the UTRIE2_U8_NEXTxx() macro implementations.
 * Do not call directly.
 * @internal
 */
U_INTERNAL int32_t U_EXPORT2
utrie2_internalU8NextIndex(const UTrie2 *trie, UChar32 c,
                           const uint8_t *src, const uint8_t *limit);

/**
 * Internal function for part of the UTRIE2_U8_PREVxx() macro implementations.
 * Do not call directly.
 * @internal
 */
U_INTERNAL int32_t U_EXPORT2
utrie2_internalU8PrevIndex(const UTrie2 *trie, UChar32 c,
                           const uint8_t *start, const uint8_t *src);


/** Internal trie getter from a BMP code point. Returns the data index. */
#define _UTRIE2_INDEX_FROM_BMP(trie, c) \
    (((int32_t)((trie)->index[(c)>>UTRIE2_SHIFT_2]) \
    <<UTRIE2_INDEX_SHIFT)+ \
    ((c)&UTRIE2_DATA_MASK))

/** Internal trie getter from a supplementary code point. Returns the data index. */
#define _UTRIE2_INDEX_FROM_SUPP(trie, c) \
    ((c)>=(trie)->highStart ? \
        (trie)->highValueIndex : \
        ((int32_t)((trie)->index[ \
            (trie)->index[(UTRIE2_INDEX_1_OFFSET-UTRIE2_OMITTED_BMP_INDEX_1_LENGTH)+ \
                          ((c)>>UTRIE2_SHIFT_1)]+ \
            (((c)>>UTRIE2_SHIFT_2)&UTRIE2_INDEX_2_MASK)]) \
        <<UTRIE2_INDEX_SHIFT)+ \
        ((c)&UTRIE2_DATA_MASK))

/**
 * Internal trie getter from a code point, without checking that c is in 0..10FFFF.
 * Returns the data index.
 */
#define _UTRIE2_INDEX_FROM_CP(trie, c) \
    ((c)<=0xffff ? \
        _UTRIE2_INDEX_FROM_BMP(trie, c) : \
        _UTRIE2_INDEX_FROM_SUPP(trie, c))

/** Internal trie getter from a BMP code point. Returns the data. */
#define _UTRIE2_GET_FROM_BMP(trie, data, c) \
    (trie)->data[_UTRIE2_INDEX_FROM_BMP(trie, c)]

/** Internal trie getter from a supplementary code point. Returns the data. */
#define _UTRIE2_GET_FROM_SUPP(trie, data, c) \
    (trie)->data[_UTRIE2_INDEX_FROM_SUPP(trie, c)]

/**
 * Internal trie getter from a code point, without checking that c is in 0..10FFFF.
 * Returns the data.
 */
#define _UTRIE2_GET_UNSAFE(trie, data, c) \
    (trie)->data[_UTRIE2_INDEX_FROM_CP(trie, c)]

/** Internal next-post-increment: get the next code point (c) and its data. */
#define _UTRIE2_NEXT(trie, data, src, limit, c, result) { \
    { \
        uint16_t __c2; \
        (c)=*(src)++; \
        if(!U16_IS_LEAD(c) || (src)==(limit) || !U16_IS_TRAIL(__c2=*(src))) { \
            (result)=_UTRIE2_GET_FROM_BMP(trie, data, c); \
        } else { \
            ++(src); \
            (c)=U16_GET_SUPPLEMENTARY((c), __c2); \
            (result)=_UTRIE2_GET_FROM_SUPP((trie), data, (c)); \
        } \
    } \
}

/** Internal pre-decrement-previous: get the previous code point (c) and its data */
#define _UTRIE2_PREV(trie, data, start, src, c, result) { \
    { \
        uint16_t __c2; \
        (c)=*--(src); \
        if(!U16_IS_TRAIL(c) || (src)==(start) || !U16_IS_LEAD(__c2=*((src)-1))) { \
            (result)=_UTRIE2_GET_FROM_BMP(trie, data, c); \
        } else { \
            --(src); \
            (c)=U16_GET_SUPPLEMENTARY(__c2, (c)); \
            (result)=_UTRIE2_GET_FROM_SUPP((trie), data, (c)); \
        } \
    } \
}

/** Internal UTF-8 next-post-increment: get the next code point's data. */
#define _UTRIE2_U8_NEXT(trie, ascii, data, src, limit, result) { \
    uint8_t __lead=(uint8_t)*(src)++; \
    if(__lead<0xc0) { \
        (result)=(trie)->ascii[__lead]; \
    } else { \
        uint8_t __t1, __t2; \
        if( /* handle U+0000..U+07FF inline */ \
            __lead<0xe0 && (src)<(limit) && \
            (__t1=(uint8_t)(*(src)-0x80))<=0x3f \
        ) { \
            ++(src); \
            (result)=(trie)->data[ \
                (trie)->index[(UTRIE2_UTF8_2B_INDEX_2_OFFSET-0xc0)+__lead]+ \
                __t1]; \
        } else if( /* handle U+0000..U+CFFF inline */ \
            __lead<0xed && ((src)+1)<(limit) && \
            (__t1=(uint8_t)(*(src)-0x80))<=0x3f && (__lead>0xe0 || __t1>=0x20) && \
            (__t2=(uint8_t)(*((src)+1)-0x80))<= 0x3f \
        ) { \
            (src)+=2; \
            (result)=(trie)->data[ \
                ((int32_t)((trie)->index[((__lead-0xe0)<<(12-UTRIE2_SHIFT_2))+ \
                                         (__t1<<(6-UTRIE2_SHIFT_2))+(__t2>>UTRIE2_SHIFT_2)]) \
                <<UTRIE2_INDEX_SHIFT)+ \
                (__t2&UTRIE2_DATA_MASK)]; \
        } else { \
            int32_t __index=utrie2_internalU8NextIndex((trie), __lead, (const uint8_t *)(src), \
                                                                       (const uint8_t *)(limit)); \
            (src)+=__index&7; \
            (result)=(trie)->data[__index>>3]; \
        } \
    } \
}

/** Internal UTF-8 pre-decrement-previous: get the previous code point's data. */
#define _UTRIE2_U8_PREV(trie, ascii, data, start, src, result) { \
    uint8_t __b=(uint8_t)*--(src); \
    if(__b<0x80) { \
        (result)=(trie)->ascii[__b]; \
    } else { \
        int32_t __index=utrie2_internalU8PrevIndex((trie), __b, (const uint8_t *)(start), \
                                                                (const uint8_t *)(src)); \
        (src)-=__index&7; \
        (result)=(trie)->data[__index>>3]; \
    } \
}

/* Public UTrie2 API -------------------------------------------------------- */

/**
 * Return a 16-bit trie value from a code point, with range checking.
 * Returns trie->errorValue if c is not in the range 0..U+10ffff.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point
 * @return (uint16_t) The code point's trie value.
 */
#define UTRIE2_GET16(trie, c) \
    (((uint32_t)(c)<=0x10ffff) ? \
        _UTRIE2_GET_UNSAFE((trie), index, (c)) : \
        (uint16_t)((trie)->errorValue))

/**
 * Return a 32-bit trie value from a code point, with range checking.
 * Returns trie->errorValue if c is not in the range 0..U+10ffff.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point
 * @return (uint32_t) The code point's trie value.
 */
#define UTRIE2_GET32(trie, c) \
    (((uint32_t)(c)<=0x10ffff) ? \
        _UTRIE2_GET_UNSAFE((trie), data32, (c)) : \
        (trie)->errorValue)

/**
 * Return a 16-bit trie value from a BMP code point (<=U+ffff).
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point, must be 0<=c<=U+ffff
 * @return (uint16_t) The code point's trie value.
 */
#define UTRIE2_GET16_FROM_BMP(trie, c) _UTRIE2_GET_FROM_BMP((trie), index, c)

/**
 * Return a 32-bit trie value from a BMP code point (<=U+ffff).
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point, must be 0<=c<=U+ffff
 * @return (uint32_t) The code point's trie value.
 */
#define UTRIE2_GET32_FROM_BMP(trie, c) _UTRIE2_GET_FROM_BMP((trie), data32, c)

/**
 * Return a 16-bit trie value from a supplementary code point (U+10000..U+10ffff).
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point, must be U+10000<=c<=U+10ffff
 * @return (uint16_t) The code point's trie value.
 */
#define UTRIE2_GET16_FROM_SUPP(trie, c) _UTRIE2_GET_FROM_SUPP((trie), index, c)

/**
 * Return a 32-bit trie value from a supplementary code point (U+10000..U+10ffff).
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point, must be U+10000<=c<=U+10ffff
 * @return (uint32_t) The code point's trie value.
 */
#define UTRIE2_GET32_FROM_SUPP(trie, c) _UTRIE2_GET_FROM_SUPP((trie), data32, c)

/**
 * Return a 16-bit trie value from a code point (<=U+10ffff) without range checking.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point, must be 0<=c<=U+10ffff
 * @return (uint16_t) The code point's trie value.
 */
#define UTRIE2_GET16_UNSAFE(trie, c) _UTRIE2_GET_UNSAFE((trie), index, (c))

/**
 * Return a 32-bit trie value from a code point (<=U+10ffff) without range checking.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param c (UChar32, in) the input code point, must be 0<=c<=U+10ffff
 * @return (uint32_t) The code point's trie value.
 */
#define UTRIE2_GET32_UNSAFE(trie, c) _UTRIE2_GET_UNSAFE((trie), data32, (c))

/**
 * Get the next code point (UChar32 c, out), post-increment src,
 * and get a 16-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param src (const UChar *, in/out) the source text pointer
 * @param limit (const UChar *, in) the limit pointer for the text, or NULL if NUL-terminated
 * @param c (UChar32, out) variable for the code point
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE2_NEXT16(trie, src, limit, c, result) _UTRIE2_NEXT(trie, index, src, limit, c, result)

/**
 * Get the next code point (UChar32 c, out), post-increment src,
 * and get a 32-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param src (const UChar *, in/out) the source text pointer
 * @param limit (const UChar *, in) the limit pointer for the text, or NULL if NUL-terminated
 * @param c (UChar32, out) variable for the code point
 * @param result (uint32_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE2_NEXT32(trie, src, limit, c, result) _UTRIE2_NEXT(trie, data32, src, limit, c, result)

/**
 * Get the previous code point (UChar32 c, out), pre-decrement src,
 * and get a 16-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param start (const UChar *, in) the start pointer for the text
 * @param src (const UChar *, in/out) the source text pointer
 * @param c (UChar32, out) variable for the code point
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE2_PREV16(trie, start, src, c, result) _UTRIE2_PREV(trie, index, start, src, c, result)

/**
 * Get the previous code point (UChar32 c, out), pre-decrement src,
 * and get a 32-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param start (const UChar *, in) the start pointer for the text
 * @param src (const UChar *, in/out) the source text pointer
 * @param c (UChar32, out) variable for the code point
 * @param result (uint32_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE2_PREV32(trie, start, src, c, result) _UTRIE2_PREV(trie, data32, start, src, c, result)

/**
 * UTF-8: Post-increment src and get a 16-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param src (const char *, in/out) the source text pointer
 * @param limit (const char *, in) the limit pointer for the text (must not be NULL)
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE2_U8_NEXT16(trie, src, limit, result)\
    _UTRIE2_U8_NEXT(trie, data16, index, src, limit, result)

/**
 * UTF-8: Post-increment src and get a 32-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param src (const char *, in/out) the source text pointer
 * @param limit (const char *, in) the limit pointer for the text (must not be NULL)
 * @param result (uint16_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE2_U8_NEXT32(trie, src, limit, result) \
    _UTRIE2_U8_NEXT(trie, data32, data32, src, limit, result)

/**
 * UTF-8: Pre-decrement src and get a 16-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param start (const char *, in) the start pointer for the text
 * @param src (const char *, in/out) the source text pointer
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE2_U8_PREV16(trie, start, src, result) \
    _UTRIE2_U8_PREV(trie, data16, index, start, src, result)

/**
 * UTF-8: Pre-decrement src and get a 32-bit value from the trie.
 *
 * @param trie (const UTrie2 *, in) a pointer to the runtime trie structure
 * @param start (const char *, in) the start pointer for the text
 * @param src (const char *, in/out) the source text pointer
 * @param result (uint16_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE2_U8_PREV32(trie, start, src, result) \
    _UTRIE2_U8_PREV(trie, data32, data32, start, src, result)

/**
 * Unserialize a trie from 32-bit-aligned memory.
 * Inverse of unewtrie2_serialize().
 * Fills the UTrie2 runtime trie structure with the settings for the trie data.
 *
 * @param trie a pointer to the runtime trie structure
 * TODO: update ###
 * @param data a pointer to 32-bit-aligned memory containing trie data
 * @param length the number of bytes available at data
 * @param pErrorCode an in/out ICU UErrorCode
 * @return the number of bytes at data taken up by the trie data
 */
U_CAPI int32_t U_EXPORT2
utrie2_unserialize(UTrie2 *trie, UTrie2ValueBits valueBits,
                   const void *data, int32_t length, UErrorCode *pErrorCode);

/**
 * "Unserialize" a dummy trie.
 * A dummy trie is an empty runtime trie, used when a real data trie cannot
 * be loaded.
 *
 * The input memory is filled so that the trie always returns the initialValue,
 * or the leadUnitValue for lead surrogate code points.### TODO
 * The Latin-1 part is always set up to be linear.
 *
 * @param trie a pointer to the runtime trie structure
 * @param data a pointer to 32-bit-aligned memory to be filled with the dummy trie data
 * @param capacity the number of bytes available at data (recommended to use UTRIE2_DUMMY_SIZE)
 * @param initialValue the initial value that is set for all code points
 * @param leadUnitValue the value for lead surrogate code _units_ that do not
 *                      have associated supplementary data
 * @param pErrorCode an in/out ICU UErrorCode
 *
 * @see UTRIE2_DUMMY_SIZE
 * @see unewtrie2_open
 */
U_CAPI int32_t U_EXPORT2
utrie2_unserializeDummy(UTrie2 *trie,
                        UTrie2ValueBits valueBits,
                        uint32_t initialValue, uint32_t errorValue,
                        void *data, int32_t capacity,
                        UErrorCode *pErrorCode);

/* enumeration callback types */

/**
 * Callback from utrie2_enum(), extracts a uint32_t value from a
 * trie value. This value will be passed on to the UTrie2EnumRange function.
 *
 * @param context an opaque pointer, as passed into utrie2_enum()
 * @param value a value from the trie
 * @return the value that is to be passed on to the UTrie2EnumRange function
 */
typedef uint32_t U_CALLCONV
UTrie2EnumValue(const void *context, uint32_t value);

/**
 * Callback from utrie2_enum(), is called for each contiguous range
 * of code points with the same value as retrieved from the trie and
 * transformed by the UTrie2EnumValue function.
 *
 * The callback function can stop the enumeration by returning FALSE.
 *
 * @param context an opaque pointer, as passed into utrie2_enum()
 * @param start the first code point in a contiguous range with value
 * @param limit one past the last code point in a contiguous range with value
 * @param value the value that is set for all code points in [start..limit[
 * @return FALSE to stop the enumeration
 */
typedef UBool U_CALLCONV
UTrie2EnumRange(const void *context, UChar32 start, UChar32 limit, uint32_t value);

/**
 * Enumerate efficiently all values in a trie.
 * For each entry in the trie, the value to be delivered is passed through
 * the UTrie2EnumValue function.
 * The value is unchanged if that function pointer is NULL.
 *
 * For each contiguous range of code points with a given value,
 * the UTrie2EnumRange function is called.
 *
 * @param trie a pointer to the runtime trie structure
 * @param enumValue a pointer to a function that may transform the trie entry value,
 *                  or NULL if the values from the trie are to be used directly
 * @param enumRange a pointer to a function that is called for each contiguous range
 *                  of code points with the same value
 * @param context an opaque pointer that is passed on to the callback functions
 */
U_CAPI void U_EXPORT2
utrie2_enum(const UTrie2 *trie,
            UTrie2EnumValue *enumValue, UTrie2EnumRange *enumRange, const void *context);

/**
 * TODO
 */
U_CAPI int32_t U_EXPORT2
utrie2_getVersion(const void *data, int32_t length, UBool anyEndianOk);

/**
 * TODO
 * @return Allocated memory with the UTrie2 data arrays.
 *         The caller must uprv_free() this memory once trie2 is not used any more.
 */
U_CAPI void * U_EXPORT2
utrie2_fromUTrie(UTrie2 *trie2, const UTrie *trie1,
                 uint32_t errorValue, UBool copyLeadCUNotCP,
                 UErrorCode *pErrorCode);

/**
 * Swap a serialized UTrie2.
 * @internal
 */
U_CAPI int32_t U_EXPORT2
utrie2_swap(const UDataSwapper *ds,
            const void *inData, int32_t length, void *outData,
            UErrorCode *pErrorCode);

/* Building a trie ---------------------------------------------------------- */

/** Build-time trie structure. */
struct UNewTrie2;
typedef struct UNewTrie2 UNewTrie2;

/**
 * Open a build-time trie structure.
 * TODO: Adjust docs.
 *
 *
 *
 * The size of the build-time data array is specified to avoid allocating a large
 * array in all cases. The array itself can also be passed in.
 *
 * Although the trie is never fully expanded to a linear array, especially when
 * unewtrie2_setRange32() is used, the data array could be large during build time.
 * The maximum length is
 * UTRIE2_MAX_BUILD_TIME_DATA_LENGTH=0x110000+UTRIE2_DATA_BLOCK_LENGTH+0x400.
 * (Number of Unicode code points + one null block +
 *  possible duplicate entries for 1024 lead surrogates.)
 *
 * @param fillIn a pointer to a UNewTrie2 structure to be initialized (will not be released), or
 *               NULL if one is to be allocated
 * @param aliasData a pointer to a data array to be used (will not be released), or
 *                  NULL if one is to be allocated
 * @param maxDataLength the capacity of aliasData (if not NULL) or
 *                      the length of the data array to be allocated
 * @param initialValue the initial value that is set for all code points
 * @param leadUnitValue the value for lead surrogate code _units_ that do not
 *                      have associated supplementary data ### TODO
 * @param latin1Linear a flag indicating whether the Latin-1 range is to be allocated and
 *                     kept in a linear, contiguous part of the data array
 * @return a pointer to the allocated and initialized new UNewTrie2
 */
U_CAPI UNewTrie2 * U_EXPORT2
unewtrie2_open(uint32_t initialValue, uint32_t errorValue, UErrorCode *pErrorCode);

/**
 * Clone a build-time trie structure with all entries.
 *
 * @param other the build-time trie structure to clone
 * @return a pointer to the new UNewTrie2 clone
 */
U_CAPI UNewTrie2 * U_EXPORT2
unewtrie2_clone(const UNewTrie2 *other);

/**
 * Close a build-time trie structure, and release memory
 * that was allocated by unewtrie2_open() or unewtrie2_clone().
 *
 * @param trie the build-time trie
 */
U_CAPI void U_EXPORT2
unewtrie2_close(UNewTrie2 *trie);

/**
 * Get the data array of a build-time trie.
 * The data may be modified, but entries that are equal before
 * must still be equal after modification.
 *
 * @param trie the build-time trie
 * @param pLength (out) a pointer to a variable that receives the number
 *                of entries in the data array
 * @return the data array
 */
U_CAPI uint32_t * U_EXPORT2
unewtrie2_getData(const UNewTrie2 *trie, int32_t *pLength);

/**
 * Get a value from a code point as stored in the build-time trie.
 *
 * @param trie the build-time trie
 * @param c the code point
 * @return the value
 */
U_CAPI uint32_t U_EXPORT2
unewtrie2_get32(const UNewTrie2 *trie, UChar32 c);

/**
 * TODO: Copy docs from utrie2_enum().
 */
U_CAPI void U_EXPORT2
unewtrie2_enum(const UNewTrie2 *trie,
               UTrie2EnumValue *enumValue, UTrie2EnumRange *enumRange, const void *context);

/**
 * Enumerate the UNewTrie2 values for the 1024=0x400 code points
 * corresponding to a given lead surrogate.
 * Used by data builder code that sets special lead surrogate code point values
 * to optimize UTF-16 string processing.
 * TODO: @param...
 */
U_CAPI void U_EXPORT2
unewtrie2_enumForLeadSurrogate(const UNewTrie2 *trie, UChar32 lead,
                               UTrie2EnumValue *enumValue, UTrie2EnumRange *enumRange,
                               const void *context);

/**
 * Set a value for a code point.
 *
 * @param trie the build-time trie
 * @param c the code point
 * @param value the value
 * @return FALSE if a failure occurred (illegal argument or data array overrun)
 */
U_CAPI UBool U_EXPORT2
unewtrie2_set32(UNewTrie2 *trie, UChar32 c, uint32_t value);

/**
 * Set a value in a range of code points [start..limit[.
 * All code points c with start<=c<limit will get the value if
 * overwrite is TRUE or if the old value is 0.
 *
 * @param trie the build-time trie
 * @param start the first code point to get the value
 * @param limit one past the last code point to get the value
 * @param value the value
 * @param overwrite flag for whether old non-initial values are to be overwritten
 * @return FALSE if a failure occurred (illegal argument or data array overrun)
 */
U_CAPI UBool U_EXPORT2
unewtrie2_setRange32(UNewTrie2 *trie,
                     UChar32 start, UChar32 limit,
                     uint32_t value, UBool overwrite);

/**
 * TODO doc, steal some from unserializeDummy().
 */
U_CAPI void * U_EXPORT2
unewtrie2_build(UNewTrie2 *newTrie, UTrie2ValueBits valueBits,
                UTrie2 *trie, UErrorCode *pErrorCode);

/**
 * Compact the build-time trie after all values are set, and then
 * serialize it into 32-bit aligned memory.
 *
 * After this, the trie can only be serizalized again and/or closed;
 * no further values can be added.
 *
 * @see utrie2_unserialize()
 *
 * @param trie the build-time trie
 * ### valueBits
 * @param data a pointer to 32-bit-aligned memory for the trie data
 * @param capacity the number of bytes available at data
 * @param ### reduceTo16Bits flag for whether the values are to be reduced to a
 *                       width of 16 bits for serialization and runtime
 * @param pErrorCode a UErrorCode argument; among other possible error codes:
 * - U_BUFFER_OVERFLOW_ERROR if the data storage block is too small for serialization
 * - U_MEMORY_ALLOCATION_ERROR if the trie data array is too small
 * - U_INDEX_OUTOFBOUNDS_ERROR if the index or data arrays are too long
 *                             after compaction for serialization
 *
 * @return the number of bytes written for the trie
 */
U_CAPI int32_t U_EXPORT2
unewtrie2_serialize(UNewTrie2 *trie, UTrie2ValueBits valueBits,
                    void *data, int32_t capacity,
                    UErrorCode *pErrorCode);

U_CDECL_END

#endif
