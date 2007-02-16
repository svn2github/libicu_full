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
 * TODO: Briefly describe final data structures.
 */
class BMPSet : public UMemory {
public:
    BMPSet(const UnicodeSet &parent);

    /*
     * Virtual destructor. Not necessary because subclasses do not create
     * additional objects, but keeps compilers happy.
     * TODO: Remove in final, unsubclassed implementation.
     */
    virtual ~BMPSet() {}

    // TODO: clone()

    virtual UBool contains(UChar32 c) const;

    /*
     * Span the substring for which each character c has tf==contains(c).
     * It must be length>0, limit=s+length and tf==0 or 1.
     * @return The string pointer which limits the span.
     * TODO: Un-virtual in final implementation.
     */
    virtual const UChar *span(const UChar *s, const UChar *limit, UBool tf) const;
    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const = 0;

// TODO: Make private in final implementation.
protected:
    void initBits();
    void setBits(int32_t blockIndex, uint32_t bits);
    void setOnes(int32_t startIndex, int32_t limitIndex);
    void setLeadValues();

    inline UBool containsSlow(UChar32 c, int32_t lo, int32_t hi) const;
    inline UBool containsSlow(UChar32 c) const;

    UBool asciiBytes[0xc0];
    uint32_t asciiBits[4];

    int8_t leadValues[256];

    /*
     * One bit per code point from U+0000..U+07FF.
     * The bits are organized vertically; consecutive code points
     * correspond to the same bit positions in consecutive table words.
     * With code point parts
     *   lead=c{10..6}
     *   trail=c{5..0}
     * it is set.contains(c)==(table7FFa[lead] bit trail)
     */
    uint32_t table7FFa[64];
    /*
     * One bit per code point from U+0000..U+07FF.
     * The bits are organized horizontally; consecutive code points
     * correspond to consecutive bits.
     * With code point parts
     *   lead=c{10..6}
     *   trail=c{5..0}
     * it is set.contains(c)==(table7FFb[trail] bit lead)
     */
    int64_t table7FFb[32];

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

    const UnicodeSet &set;
};

inline UBool BMPSet::containsSlow(UChar32 c, int32_t lo, int32_t hi) const {
    return (UBool)(set.findCodePoint(c, lo, hi) & 1);
}

inline UBool BMPSet::containsSlow(UChar32 c) const {
    return (UBool)(set.findCodePoint(c) & 1);
}

class BMPSetASCIIBytes2BHorizontal : public BMPSet {
public:
    BMPSetASCIIBytes2BHorizontal(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBits2BHorizontal : public BMPSet {
public:
    BMPSetASCIIBits2BHorizontal(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVertical : public BMPSet {
public:
    BMPSetASCIIBytes2BVertical(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVerticalFull : public BMPSet {
public:
    BMPSetASCIIBytes2BVerticalFull(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVerticalOnly : public BMPSet {
public:
    BMPSetASCIIBytes2BVerticalOnly(const UnicodeSet &parent);

    virtual UBool contains(UChar32 c) const;
    virtual const UChar *span(const UChar *s, const UChar *limit, UBool tf) const;
    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytesOnly : public BMPSet {
public:
    BMPSetASCIIBytesOnly(const UnicodeSet &parent);

    virtual UBool contains(UChar32 c) const;
    virtual const UChar *span(const UChar *s, const UChar *limit, UBool tf) const;
    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVerticalPrecheck : public BMPSet {
public:
    BMPSetASCIIBytes2BVerticalPrecheck(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVerticalPrecheckFull : public BMPSet {
public:
    BMPSetASCIIBytes2BVerticalPrecheckFull(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetLeadValues : public BMPSet {
public:
    BMPSetLeadValues(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVerticalLenient : public BMPSet {
public:
    BMPSetASCIIBytes2BVerticalLenient(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

class BMPSetASCIIBytes2BVerticalLenient2 : public BMPSet {
public:
    BMPSetASCIIBytes2BVerticalLenient2(const UnicodeSet &parent);

    virtual const uint8_t *spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const;
};

U_NAMESPACE_END

#endif
