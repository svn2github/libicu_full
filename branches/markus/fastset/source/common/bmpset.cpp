/*
******************************************************************************
*
*   Copyright (C) 2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  bmpset.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007jan29
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/uniset.h"
#include "cmemory.h"
#include "bmpset.h"

U_NAMESPACE_BEGIN

enum {
    LV_BAD_SINGLE=0x80,
    LV_MULTI_2=0x90,
    LV_MULTI_3=0xa0,
    LV_MULTI_4=0xb0,
    LV_MULTI_5=0xc0,
    LV_MULTI_6=0xd0,

    LV_COUNT_SHIFT=4,
    LV_COUNT_MASK=7,

    LV_2B_TF=0,
    LV_2B_MIXED=1,
    LV_3B_TF=2,
    LV_3B_MIXED=3,
    LV_4B=4,
    LV_ILLEGAL=5,
    // Types 6 & 7 are not used.
    LV_TYPE_MASK=7,

    LV_TF_SHIFT=3,
    LV_TF_TRUE=1<<LV_TF_SHIFT
};

BMPSet::BMPSet(const UnicodeSet &parent) : set(parent) {
    uprv_memset(asciiBytes, 0, sizeof(asciiBytes));
    uprv_memset(asciiBits, 0, sizeof(asciiBits));
    uprv_memset(table7FFa, 0, sizeof(table7FFa));
    uprv_memset(table7FFb, 0, sizeof(table7FFb));
    uprv_memset(bmpBlockBits, 0, sizeof(bmpBlockBits));

    initBits();

    setLeadValues();

    /*
     * Set the list indexes for binary searches for
     * U+0800, U+1000, U+2000, .., U+F000, U+10000.
     * U+0800 is the first 3-byte-UTF-8 code point. Lower code points are
     * looked up in the bit tables.
     * The last pair of indexes is for finding supplementary code points.
     */
    list4kStarts[0]=set.findCodePoint(0x800);
    int32_t i;
    for(i=1; i<=0x10; ++i) {
        list4kStarts[i]=set.findCodePoint(i<<12);
    }
    list4kStarts[0x11]=set.len-1;
}

void BMPSet::initBits() {
    const UChar32 *list=set.list;
    int32_t listLength=set.len;

    uint32_t bits;
    UChar32 start, limit;
    int32_t listIndex, prevIndex, i, j;

    bits=0;
    start=0;
    listIndex=prevIndex=0;
    for(;;) {
        if(listIndex<listLength) {
            start=list[listIndex++];
        } else {
            start=0x10000;
        }
        i=start>>5;
        // Not necessary to actually set all-zero bits because the
        // constructor reset all bits already.
        if(prevIndex<i && bits!=0) {
            // Finish the end of the previous range.
            setBits(prevIndex++, bits);

            // Fill all-zero entries between ranges.
            bits=0;
            // while(prevIndex<i) {
            //     setBits(prevIndex++, bits);
            // }
        }
        if(start>0xffff) {
            // Stop when the current range is beyond the BMP.
            break;
        }

        // Set the upper block bits from the start code point.
        // They will be intersected below with the bits up to
        // before the limit code point.
        bits|=~(((uint32_t)1<<(start&0x1f))-1);

        if(listIndex<listLength) {
            limit=list[listIndex++];
            if(limit>0x10000) {
                limit=0x10000;
            }
        } else {
            limit=0x10000;
        }
        j=limit>>5;
        if(i<j) {
            // Set bits for the start of the range.
            setBits(i++, bits);

            // Fill all-one entries inside the range.
            bits=0xffffffff;
            if(i<j) {
                setOnes(i, j);
                i=j;
            }
            // while(i<j) {
            //     setBits(i++, bits);
            // }
        }
        /* i==j */

        // Intersect the lower block bits up to the end code point.
        bits&=((uint32_t)1<<(limit&0x1f))-1;

        prevIndex=j;
    }

    /*
     * Override some bmpBlockBits to all-zeroes for 3-byte lookup of
     * U+0000..U+07FF (from non-shortest forms)
     * and U+D800..U+DFFF (surrogate code points)
     * to enable non-mixed UTF-8 fastpath for the entire BMP.
     * Treat an illegal sequence as part of the FALSE span.
     */
    bits=~0x10001;          // Lead byte 0xE0.
    for(i=0; i<32; ++i) {   // First half of 4k block.
        bmpBlockBits[i]&=bits;
    }
    bits=~(0x10001<<0xd);   // Lead byte 0xED.
    for(i=32; i<64; ++i) {
        bmpBlockBits[i]&=bits;
    }
}

/*
 * Set the bits for the 32 consecutive code points from
 * start=(blockIndex<<5).
 * For each odd block index, the even sibling must be set first.
 */
void BMPSet::setBits(int32_t blockIndex, uint32_t bits) {
    int32_t i, limit;

    if(blockIndex<(128>>5)) {
        // Set ASCII flags.
        asciiBits[blockIndex]=bits;

        uint32_t b=bits;
        i=blockIndex<<5;
        limit=i+32;
        while(i<limit && b!=0) {
            asciiBytes[i++]=(UBool)(b&1);
            b>>=1;
        }
    }

    if(blockIndex<(0x800>>5)) {
        // Set flags for 0..07FF.
        if((blockIndex&1)==0) {
            table7FFb[blockIndex>>1]=bits;
        } else {
            table7FFb[blockIndex>>1]|=(int64_t)bits<<32;
        }

        if((blockIndex&1)==0) {
            i=0;
            limit=32;
        } else {
            i=32;
            limit=64;
        }

        uint32_t b=bits;
        uint32_t bit=(uint32_t)1<<(blockIndex>>1);
        while(i<limit && b!=0) {
            if(b&1) {
                table7FFa[i]|=bit;
            }
            ++i;
            b>>=1;
        }
    }

    // Set flags for 64-blocks in the BMP.

    // Fields from the code point block corresponding to
    // lead and trail bytes from UTF-8 three-byte sequences.
    int32_t lead=blockIndex>>7;
    int32_t trail=(blockIndex>>1)&0x3f;

    if(bits==0) {
        // Leave all bits 0 indicating an all-zero block.
    } else if(bits==0xffffffff) {
        bits=1;  // Set one bit indicating an all-one block.
    } else {
        bits=0x10001;  // Set two bits indicating a mixed block.
    }
    bits<<=lead;

    if((blockIndex&1)==0) {
        bmpBlockBits[trail]|=bits;
    } else {
        uint32_t mask=0x10001<<lead;
        if((bmpBlockBits[trail]&mask)!=bits) {
            bmpBlockBits[trail]|=mask;
        }
    }
}

/*
 * Set all-one bits for code points from
 * start=(startIndex<<5) to just before
 * limit=(limitIndex<<5).
 * Faster than
 *   while(i<j) { setBits(i++, 0xffffffff); }
 * For each odd block index, the even sibling must be set first.
 */
void BMPSet::setOnes(int32_t startIndex, int32_t limitIndex) {
    int32_t i, limit;

    if(startIndex==limitIndex) {
        return;
    }

    // Handle halves of 64-blocks via setBits().
    if(startIndex&1) {
        setBits(startIndex++, 0xffffffff);
    }
    if(limitIndex&1) {
        setBits(--limitIndex, 0xffffffff);
    }
    if(startIndex==limitIndex) {
        return;
    }

    if(startIndex<(128>>5)) {
        // Set ASCII flags.
        i=startIndex;
        if(limitIndex<(128>>5)) {
            limit=limitIndex;
        } else {
            limit=128>>5;
        }
        while(i<limit) {
            asciiBits[i++]=0xffffffff;
        }

        i=startIndex<<5;
        if(limitIndex<(128>>5)) {
            limit=limitIndex<<5;
        } else {
            limit=128;
        }
        while(i<limit) {
            asciiBytes[i++]=1;
        }
    }

    // Now work with 64-blocks.
    startIndex>>=1;
    limitIndex>>=1;

    if(startIndex<(0x800>>6)) {
        // Set flags for 0..07FF.
        i=startIndex;
        if(limitIndex<(0x800>>6)) {
            limit=limitIndex;
        } else {
            limit=0x800>>6;
        }
        while(i<limit) {
            table7FFb[i++]=INT64_C(0xffffffffffffffff);
        }

        uint32_t bits=~(((uint32_t)1<<startIndex)-1);
        if(limitIndex<(0x800>>6)) {
            bits&=((uint32_t)1<<limitIndex)-1;
        }
        for(i=0; i<64; ++i) {
            table7FFa[i]|=bits;
        }
    }

    // Set flags for 64-blocks in the BMP.

    // Fields from the code point block corresponding to
    // lead and trail bytes from UTF-8 three-byte sequences.
    int32_t lead=startIndex>>6;
    int32_t trail=startIndex&0x3f;
    int32_t limitLead=limitIndex>>6;
    int32_t limitTrail=limitIndex&0x3f;

    i=startIndex;
    limit=limitIndex;

    // Set one bit indicating an all-one block.
    uint32_t bits=(uint32_t)1<<lead;

    if(lead==limitLead) {
        // Partial vertical bit column.
        while(trail<limitTrail) {
            bmpBlockBits[trail++]|=bits;
        }
    } else {
        // Partial vertical bit column,
        // followed by a bit rectangle,
        // followed by another partial vertical bit column.
        if(trail>0) {
            do {
                bmpBlockBits[trail++]|=bits;
            } while(trail<64);
            ++lead;
        }
        if(lead<limitLead) {
            bits=~((1<<lead)-1);
            bits&=(1<<limitLead)-1;
            for(trail=0; trail<64; ++trail) {
                bmpBlockBits[trail]|=bits;
            }
        }
        bits=1<<limitLead;
        for(trail=0; trail<limitTrail; ++trail) {
            bmpBlockBits[trail]|=bits;
        }
    }
}

void BMPSet::setLeadValues() {
    int32_t i, j;

    uprv_memcpy(leadValues, asciiBytes, 128);
    for(i=0x80; i<0xc0; ++i) {
        leadValues[i]=(int8_t)(LV_BAD_SINGLE | LV_ILLEGAL);
    }
    leadValues[0xc0]=leadValues[0xc1]=(int8_t)(LV_MULTI_2 | LV_ILLEGAL);
    for(i=0xc2; i<0xe0; ++i) {
        uint32_t mask=(uint32_t)1<<(i&0x1f);
        uint32_t bit=table7FFa[0]&mask;
        for(j=1; j<64 && bit==(table7FFa[j]&mask); ++j) {}
        if(j==64) {
            leadValues[i]=(int8_t)(LV_MULTI_2 | LV_2B_TF | ((bit!=0)?LV_TF_TRUE:0));
        } else {
            leadValues[i]=(int8_t)(LV_MULTI_2 | LV_2B_MIXED);
        }
    }
    for(; i<0xf0; ++i) {
        uint32_t mask=0x10001<<(i&0xf);
        uint32_t bits=bmpBlockBits[0]&mask;
        j=1;
        if(bits!=mask) {
            for(j=1; j<64 && bits==(bmpBlockBits[j]&mask); ++j) {}
        }
        if(j==64) {
            leadValues[i]=(int8_t)(LV_MULTI_3 | LV_3B_TF | ((bits!=0)?LV_TF_TRUE:0));
        } else {
            leadValues[i]=(int8_t)(LV_MULTI_3 | LV_3B_MIXED);
        }
    }
    for(; i<0xf5; ++i) {
        leadValues[i]=(int8_t)(LV_MULTI_4 | LV_4B);
    }
    for(; i<0xf8; ++i) {
        leadValues[i]=(int8_t)(LV_MULTI_4 | LV_ILLEGAL);
    }
    for(; i<0xfc; ++i) {
        leadValues[i]=(int8_t)(LV_MULTI_5 | LV_ILLEGAL);
    }
    leadValues[0xfc]=leadValues[0xfd]=(int8_t)(LV_MULTI_6 | LV_ILLEGAL);
    leadValues[0xfe]=leadValues[0xff]=(int8_t)(LV_BAD_SINGLE | LV_ILLEGAL);
}

/*
 * Default contains().
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0800..U+FFFF.
 */
UBool
BMPSet::contains(UChar32 c) const {
    if((uint32_t)c<=0x7f) {
        return (UBool)asciiBytes[c];
    } else if((uint32_t)c<=0x7ff) {
        return (UBool)((table7FFa[c&0x3f]&((uint32_t)1<<(c>>6)))!=0);
    } else if((uint32_t)c<0xd800 || (c>=0xe000 && c<=0xffff)) {
        int lead=c>>12;
        uint32_t twoBits=(bmpBlockBits[(c>>6)&0x3f]>>lead)&0x10001;
        if(twoBits<=1) {
            // All 64 code points with the same bits 15..6
            // are either in the set or not.
            return (UBool)twoBits;
        } else {
            // Look up the code point in its 4k block of code points.
            return containsSlow(c, list4kStarts[lead], list4kStarts[lead+1]);
        }
    } else if((uint32_t)c<=0x10ffff) {
        // surrogate or supplementary code point
        return containsSlow(c, list4kStarts[0xd], list4kStarts[0x11]);
    } else {
        return FALSE;
    }
}

/*
 * Default span() for UTF-16.
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0800..U+FFFF.
 * Check for sufficient length for trail unit for each surrogate pair.
 * Handle single surrogates as surrogate code points as usual.
 */
const UChar *
BMPSet::span(const UChar *s, const UChar *limit, UBool tf) const {
    UChar c, c2;

    if(tf) {
        // span
        do {
            c=*s;
            if(c<=0x7f) {
                if(!asciiBytes[c]) {
                    break;
                }
            } else if(c<=0x7ff) {
                if((table7FFa[c&0x3f]&((uint32_t)1<<(c>>6)))==0) {
                    break;
                }
            } else if(c<0xd800 || c>=0xe000) {
                int lead=c>>12;
                uint32_t twoBits=(bmpBlockBits[(c>>6)&0x3f]>>lead)&0x10001;
                if(twoBits<=1) {
                    // All 64 code points with the same bits 15..6
                    // are either in the set or not.
                    if(twoBits==0) {
                        break;
                    }
                } else {
                    // Look up the code point in its 4k block of code points.
                    if(!containsSlow(c, list4kStarts[lead], list4kStarts[lead+1])) {
                        break;
                    }
                }
            } else if(c>=0xdc00 || (s+1)==limit || (c2=s[1])<0xdc00 || c2>=0xe000) {
                // surrogate code point
                if(!containsSlow(c, list4kStarts[0xd], list4kStarts[0xe])) {
                    break;
                }
            } else {
                // surrogate pair
                if(!containsSlow(U16_GET_SUPPLEMENTARY(c, c2), list4kStarts[0x10], list4kStarts[0x11])) {
                    break;
                }
                ++s;
            }
        } while(++s<limit);
    } else {
        // span not
        do {
            c=*s;
            if(c<=0x7f) {
                if(asciiBytes[c]) {
                    break;
                }
            } else if(c<=0x7ff) {
                if((table7FFa[c&0x3f]&((uint32_t)1<<(c>>6)))!=0) {
                    break;
                }
            } else if(c<0xd800 || c>=0xe000) {
                int lead=c>>12;
                uint32_t twoBits=(bmpBlockBits[(c>>6)&0x3f]>>lead)&0x10001;
                if(twoBits<=1) {
                    // All 64 code points with the same bits 15..6
                    // are either in the set or not.
                    if(twoBits!=0) {
                        break;
                    }
                } else {
                    // Look up the code point in its 4k block of code points.
                    if(containsSlow(c, list4kStarts[lead], list4kStarts[lead+1])) {
                        break;
                    }
                }
            } else if(c>=0xdc00 || (s+1)==limit || (c2=s[1])<0xdc00 || c2>=0xe000) {
                // surrogate code point
                if(containsSlow(c, list4kStarts[0xd], list4kStarts[0xe])) {
                    break;
                }
            } else {
                // surrogate pair
                if(containsSlow(U16_GET_SUPPLEMENTARY(c, c2), list4kStarts[0x10], list4kStarts[0x11])) {
                    break;
                }
                ++s;
            }
        } while(++s<limit);
    }
    return s;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized horizontally.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Check for sufficient trail bytes for each multi-byte character.
 * Check validity.
 */
BMPSetASCIIBytes2BHorizontal::BMPSetASCIIBytes2BHorizontal(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BHorizontal::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b, t1, t2;

    do {
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else {
            if(b>=0xe0) {
                if( /* handle U+0000..U+FFFF inline */
                    b<0xf0 &&
                    (limit-s)>=2 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                ) {
                    b&=0xf;
                    uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                    if(twoBits<=1) {
                        // All 64 code points with this lead byte and middle trail byte
                        // are either in the set or not.
                        if(twoBits!=tf) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                    }
                    s+=2;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    s<limit &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFb[b&0x1f]&(INT64_C(1)<<t1))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // No fast path for this sequence.
            int32_t i=0;
            UChar32 c;
            if(U8_IS_LEAD(b)) {
                c=utf8_nextCharSafeBody(s, &i, (int32_t)(limit-s), b, -1);
            } else {
                c=-1;
            }
            if(c<0) {
                // Treat an illegal sequence as part of the FALSE span.
                if(tf) {
                    return s-1;
                }
            } else {
                // Handle correct but "complicated" 3- or 4-byte sequences for U+0800 and up.
                // Set b to the correct list4kStarts[] pair.
                if(b<0xf0) {
                    b&=0xf;  // BMP code point U+0800<=c<=U+FFFF.
                } else {
                    b=0x10;  // Supplementary code point U+10000<=c<=U+10FFFF.
                }
                if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                    return s-1;
                }
            }
            s+=i;
        }
    } while(s<limit);
    return limit;
}

/*
 * ASCII: Look up bits.
 * 2-byte characters: Bits organized horizontally.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Check for sufficient trail bytes for each multi-byte character.
 * Check validity.
 */
BMPSetASCIIBits2BHorizontal::BMPSetASCIIBits2BHorizontal(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBits2BHorizontal::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b, t1, t2;

    do {
        b=*s++;
        if((int8_t)b>=0) {
            if(((asciiBits[b>>5]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                return s-1;
            }
        } else {
            if(b>=0xe0) {
                if( /* handle U+0000..U+FFFF inline */
                    b<0xf0 &&
                    (limit-s)>=2 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                ) {
                    b&=0xf;
                    uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                    if(twoBits<=1) {
                        // All 64 code points with this lead byte and middle trail byte
                        // are either in the set or not.
                        if(twoBits!=tf) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                    }
                    s+=2;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    s<limit &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFb[b&0x1f]&(INT64_C(1)<<t1))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // No fast path for this sequence.
            int32_t i=0;
            UChar32 c;
            if(U8_IS_LEAD(b)) {
                c=utf8_nextCharSafeBody(s, &i, (int32_t)(limit-s), b, -1);
            } else {
                c=-1;
            }
            if(c<0) {
                // Treat an illegal sequence as part of the FALSE span.
                if(tf) {
                    return s-1;
                }
            } else {
                // Handle correct but "complicated" 3- or 4-byte sequences for U+0800 and up.
                // Set b to the correct list4kStarts[] pair.
                if(b<0xf0) {
                    b&=0xf;  // BMP code point U+0800<=c<=U+FFFF.
                } else {
                    b=0x10;  // Supplementary code point U+10000<=c<=U+10FFFF.
                }
                if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                    return s-1;
                }
            }
            s+=i;
        }
    } while(s<limit);
    return limit;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Check for sufficient trail bytes for each multi-byte character.
 * Check validity.
 */
BMPSetASCIIBytes2BVertical::BMPSetASCIIBytes2BVertical(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BVertical::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b, t1, t2;

    do {
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else {
            if(b>=0xe0) {
                if( /* handle U+0000..U+FFFF inline */
                    b<0xf0 &&
                    (limit-s)>=2 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                ) {
                    b&=0xf;
                    uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                    if(twoBits<=1) {
                        // All 64 code points with this lead byte and middle trail byte
                        // are either in the set or not.
                        if(twoBits!=tf) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                    }
                    s+=2;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    s<limit &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFa[t1]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // No fast path for this sequence.
            int32_t i=0;
            UChar32 c;
            if(U8_IS_LEAD(b)) {
                c=utf8_nextCharSafeBody(s, &i, (int32_t)(limit-s), b, -1);
            } else {
                c=-1;
            }
            if(c<0) {
                // Treat an illegal sequence as part of the FALSE span.
                if(tf) {
                    return s-1;
                }
            } else {
                // Handle correct but "complicated" 3- or 4-byte sequences for U+0800 and up.
                // Set b to the correct list4kStarts[] pair.
                if(b<0xf0) {
                    b&=0xf;  // BMP code point U+0800<=c<=U+FFFF.
                } else {
                    b=0x10;  // Supplementary code point U+10000<=c<=U+10FFFF.
                }
                if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                    return s-1;
                }
            }
            s+=i;
        }
    } while(s<limit);
    return limit;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF,
 *                    with mixed for illegal ranges.
 * Check for sufficient trail bytes for each multi-byte character.
 * Check validity.
 */
BMPSetASCIIBytes2BVerticalFull::BMPSetASCIIBytes2BVerticalFull(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BVerticalFull::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b, t1, t2, t3;

    do {
#if 1
        b=*s;
        if((int8_t)b>=0) {
            // ASCII
            if(tf) {
                do {
                    if(!asciiBytes[b] || ++s==limit) {
                        return s;
                    }
                    b=*s;
                } while((int8_t)b>=0);
            } else {
                do {
                    if(asciiBytes[b] || ++s==limit) {
                        return s;
                    }
                    b=*s;
                } while((int8_t)b>=0);
            }
        }
        ++s;  // Advance past the lead byte.
#else
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else
#endif
        {
            if(b>=0xe0) {
                if(b<0xf0) {
                    if( /* handle U+0000..U+FFFF inline */
                        (limit-s)>=2 &&
                        (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                        (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                    ) {
                        b&=0xf;
                        uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                        if(twoBits<=1) {
                            // All 64 code points with this lead byte and middle trail byte
                            // are either in the set or not.
                            if(twoBits!=tf) {
                                return s-1;
                            }
                        } else {
                            // Look up the code point in its 4k block of code points.
                            UChar32 c=(b<<12)|(t1<<6)|t2;
                            if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                                return s-1;
                            }
                        }
                        s+=2;
                        continue;
                    }
                } else if( /* handle U+10000..U+10FFFF inline */
                    (limit-s)>=3 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f &&
                    (t3=(uint8_t)(s[2]-0x80)) <= 0x3f
                ) {
                    // Treat an illegal sequence as part of the FALSE span.
                    UChar32 c=((UChar32)(b-0xf0)<<18)|((UChar32)t1<<12)|(t2<<6)|t3;
                    if( (   0x10000<=c && c<=0x10ffff &&
                            containsSlow(c, list4kStarts[0x10], list4kStarts[0x11])
                        ) != tf
                    ) {
                        return s-1;
                    }
                    s+=3;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    s<limit &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFa[t1]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // Treat an illegal sequence as part of the FALSE span.
            // Handle each byte of an illegal sequence separately to simplify the code;
            // no need to optimize error handling.
            if(tf) {
                return s-1;
            }
        }
    } while(s<limit);
    return limit;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use binary search for U+0800..U+D7FF and U+E000..U+FFFF.
 * Check for sufficient trail bytes for each multi-byte character.
 * Check validity.
 */
BMPSetASCIIBytes2BVerticalOnly::BMPSetASCIIBytes2BVerticalOnly(const UnicodeSet &parent) : BMPSet(parent) {}

UBool
BMPSetASCIIBytes2BVerticalOnly::contains(UChar32 c) const {
    if((uint32_t)c<=0x7f) {
        return (UBool)asciiBytes[c];
    } else if((uint32_t)c<=0x7ff) {
        return (UBool)((table7FFa[c&0x3f]&((uint32_t)1<<(c>>6)))!=0);
    } else if((uint32_t)c<=0xffff) {
        int lead=c>>12;
        // Look up the code point in its 4k block of code points.
#if 1
        int32_t lo=list4kStarts[lead], hi=list4kStarts[lead+1];
        return (lo==hi) ? (UBool)(lo&1) : containsSlow(c, lo, hi);
#else
        return containsSlow(c, list4kStarts[lead], list4kStarts[lead+1]);
#endif
    } else if((uint32_t)c<=0x10ffff) {
        // surrogate pair
        return containsSlow(c, list4kStarts[0x10], list4kStarts[0x11]);
    } else {
        return FALSE;
    }
}

const UChar *
BMPSetASCIIBytes2BVerticalOnly::span(const UChar *s, const UChar *limit, UBool tf) const {
    UChar c, c2;

    do {
        c=*s;
        if(c<=0x7f) {
            if(asciiBytes[c]!=tf) {
                break;
            }
        } else if(c<=0x7ff) {
            if(((table7FFa[c&0x3f]&((uint32_t)1<<(c>>6)))!=0) != tf) {
                break;
            }
        } else if(c<0xd800 || c>=0xdc00 || (s+1)==limit || (c2=s[1])<0xdc00 || c2>=0xe000) {
            int lead=c>>12;
            // Look up the code point in its 4k block of code points.
#if 1
            int32_t lo=list4kStarts[lead], hi=list4kStarts[lead+1];
            if(((lo==hi) ? (UBool)(lo&1) : containsSlow(c, lo, hi)) != tf) {
                break;
            }
#else
            if(containsSlow(c, list4kStarts[lead], list4kStarts[lead+1]) != tf) {
                break;
            }
#endif
        } else {
            // surrogate pair
            if(containsSlow(U16_GET_SUPPLEMENTARY(c, c2), list4kStarts[0x10], list4kStarts[0x11]) != tf) {
                break;
            }
            ++s;
        }
    } while(++s<limit);

    return s;
}

const uint8_t *
BMPSetASCIIBytes2BVerticalOnly::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b, t1, t2, t3;

    do {
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else {
            if(b>=0xe0) {
                if(b>0xe0 && b<0xed) {
                    if( /* handle U+1000..U+CFFF inline */
                        (limit-s)>=2 &&
                        (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                        (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                    ) {
                        b&=0xf;
                        // Look up the code point in its 4k block of code points.
#if 1
                        int32_t lo=list4kStarts[b], hi=list4kStarts[b+1];
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(((lo==hi) ? (UBool)(lo&1) : containsSlow(c, lo, hi)) != tf) {
                            return s-1;
                        }
#else
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
#endif
                        s+=2;
                        continue;
                    }
                } else if(b<0xf0) {
                    if( /* handle U+0800..U+0FFF, U+D000..U+D7FF and U+E000..U+FFFF inline */
                        (limit-s)>=2 &&
                        (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                        (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                    ) {
                        b&=0xf;
                        // Look up the code point in its 4k block of code points.
                        // Treat an illegal sequence as part of the FALSE span.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if( (   (b==0 ? c>=0x800 : (b>0xd || c<=0xd7ff)) &&
                                containsSlow(c, list4kStarts[b], list4kStarts[b+1])
                            ) != tf
                        ) {
                            return s-1;
                        }
                        s+=2;
                        continue;
                    }
                } else if( /* handle U+10000..U+10FFFF inline */
                    (limit-s)>=3 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f &&
                    (t3=(uint8_t)(s[2]-0x80)) <= 0x3f
                ) {
                    // Treat an illegal sequence as part of the FALSE span.
                    UChar32 c=((UChar32)(b-0xf0)<<18)|((UChar32)t1<<12)|(t2<<6)|t3;
                    if( (   0x10000<=c && c<=0x10ffff &&
                            containsSlow(c, list4kStarts[0x10], list4kStarts[0x11])
                        ) != tf
                    ) {
                        return s-1;
                    }
                    s+=3;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    s<limit &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFa[t1]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // Treat an illegal sequence as part of the FALSE span.
            // Handle each byte of an illegal sequence separately to simplify the code;
            // no need to optimize error handling.
            if(tf) {
                return s-1;
            }
        }
    } while(s<limit);
    return limit;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Use binary search.
 * 3-byte characters: Use binary search for U+0800..U+D7FF and U+E000..U+FFFF.
 * Check for sufficient trail bytes for each multi-byte character.
 * Check validity.
 */
BMPSetASCIIBytesOnly::BMPSetASCIIBytesOnly(const UnicodeSet &parent) : BMPSet(parent) {}

UBool
BMPSetASCIIBytesOnly::contains(UChar32 c) const {
    if((uint32_t)c<=0x7f) {
        return (UBool)asciiBytes[c];
    } else if((uint32_t)c<=0x7ff) {
        return containsSlow(c, 0, list4kStarts[0]);
    } else if((uint32_t)c<=0xffff) {
        int lead=c>>12;
        // Look up the code point in its 4k block of code points.
        return containsSlow(c, list4kStarts[lead], list4kStarts[lead+1]);
    } else if((uint32_t)c<=0x10ffff) {
        // surrogate pair
        return containsSlow(c, list4kStarts[0x10], list4kStarts[0x11]);
    } else {
        return FALSE;
    }
}

const UChar *
BMPSetASCIIBytesOnly::span(const UChar *s, const UChar *limit, UBool tf) const {
    UChar c, c2;

    do {
        c=*s;
        if(c<=0x7f) {
            if(asciiBytes[c]!=tf) {
                break;
            }
        } else if(c<=0x7ff) {
            if(containsSlow(c, 0, list4kStarts[0]) != tf) {
                break;
            }
        } else if(c<0xd800 || c>=0xdc00 || (s+1)==limit || (c2=s[1])<0xdc00 || c2>=0xe000) {
            int lead=c>>12;
            // Look up the code point in its 4k block of code points.
            if(containsSlow(c, list4kStarts[lead], list4kStarts[lead+1]) != tf) {
                break;
            }
        } else {
            // surrogate pair
            if(containsSlow(U16_GET_SUPPLEMENTARY(c, c2), list4kStarts[0x10], list4kStarts[0x11]) != tf) {
                break;
            }
            ++s;
        }
    } while(++s<limit);

    return s;
}

const uint8_t *
BMPSetASCIIBytesOnly::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b, t1, t2, t3;

    do {
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else {
            if(b>=0xe0) {
                if(b>0xe0 && b<0xed) {
                    if( /* handle U+1000..U+CFFF inline */
                        (limit-s)>=2 &&
                        (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                        (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                    ) {
                        b&=0xf;
                        // Look up the code point in its 4k block of code points.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                        s+=2;
                        continue;
                    }
                } else if(b<0xf0) {
                    if( /* handle U+0800..U+0FFF, U+D000..U+D7FF and U+E000..U+FFFF inline */
                        (limit-s)>=2 &&
                        (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                        (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                    ) {
                        b&=0xf;
                        // Look up the code point in its 4k block of code points.
                        // Treat an illegal sequence as part of the FALSE span.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if( (   (b==0 ? c>=0x800 : (b>0xd || c<=0xd7ff)) &&
                                containsSlow(c, list4kStarts[b], list4kStarts[b+1])
                            ) != tf
                        ) {
                            return s-1;
                        }
                        s+=2;
                        continue;
                    }
                } else if( /* handle U+10000..U+10FFFF inline */
                    (limit-s)>=3 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f &&
                    (t3=(uint8_t)(s[2]-0x80)) <= 0x3f
                ) {
                    // Treat an illegal sequence as part of the FALSE span.
                    UChar32 c=((UChar32)(b-0xf0)<<18)|((UChar32)t1<<12)|(t2<<6)|t3;
                    if( (   0x10000<=c && c<=0x10ffff &&
                            containsSlow(c, list4kStarts[0x10], list4kStarts[0x11])
                        ) != tf
                    ) {
                        return s-1;
                    }
                    s+=3;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    s<limit &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    UChar32 c=((UChar32)(b&0x1f)<<6)|t1;
                    if(containsSlow(c, 0, list4kStarts[0]) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // Treat an illegal sequence as part of the FALSE span.
            // Handle each byte of an illegal sequence separately to simplify the code;
            // no need to optimize error handling.
            if(tf) {
                return s-1;
            }
        }
    } while(s<limit);
    return limit;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Precheck for sufficient trail bytes at end of string only once per span.
 * Check validity.
 */
BMPSetASCIIBytes2BVerticalPrecheck::BMPSetASCIIBytes2BVerticalPrecheck(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BVerticalPrecheck::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    const uint8_t *limit0=limit;

    /*
     * Make sure that the last 1/2/3-byte sequence before limit is complete
     * or runs into a lead byte.
     * In the span loop compare s with limit only once
     * per multi-byte character.
     * (4-byte sequences are always fully validity-checked.)
     */
    uint8_t b=*(limit-1);
    if((int8_t)b<0) {
        // b>=0x80: lead or trail byte
        if(b<0xc0) {
            // single trail byte, check for preceding 3- or 4-byte lead byte
            if(length>=2 && (b=*(limit-2))>=0xe0) {
                limit-=2;
            }
        } else {
            // lead byte with no trail bytes
            --limit;
        }
    }

    uint8_t t1, t2;

    while(s<limit) {
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else {
            if(b>=0xe0) {
                if( /* handle U+0000..U+FFFF inline */
                    b<0xf0 &&
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                ) {
                    b&=0xf;
                    uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                    if(twoBits<=1) {
                        // All 64 code points with this lead byte and middle trail byte
                        // are either in the set or not.
                        if(twoBits!=tf) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                    }
                    s+=2;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFa[t1]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // No fast path for this sequence.
            int32_t i=0;
            UChar32 c;
            if(U8_IS_LEAD(b)) {
                c=utf8_nextCharSafeBody(s, &i, (int32_t)(limit-s), b, -1);
            } else {
                c=-1;
            }
            if(c<0) {
                // Treat an illegal sequence as part of the FALSE span.
                if(tf) {
                    return s-1;
                }
            } else {
                // Handle correct but "complicated" 3- or 4-byte sequences for U+0800 and up.
                // Set b to the correct list4kStarts[] pair.
                if(b<0xf0) {
                    b&=0xf;  // BMP code point U+0800<=c<=U+FFFF.
                } else {
                    b=0x10;  // Supplementary code point U+10000<=c<=U+10FFFF.
                }
                if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                    return s-1;
                }
            }
            s+=i;
        }
    }

    if(!tf) {
        // Treat the trailing illegal sequence as part of the FALSE span.
        return limit0;
    } else {
        return limit;
    }
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF,
 *                    with mixed for illegal ranges.
 * Precheck for sufficient trail bytes at end of string only once per span.
 * Check validity.
 */
BMPSetASCIIBytes2BVerticalPrecheckFull::BMPSetASCIIBytes2BVerticalPrecheckFull(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BVerticalPrecheckFull::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b=*s;
    if((int8_t)b>=0) {
        // Initial all-ASCII span.
        if(tf) {
            do {
                if(!asciiBytes[b] || ++s==limit) {
                    return s;
                }
                b=*s;
            } while((int8_t)b>=0);
        } else {
            do {
                if(asciiBytes[b] || ++s==limit) {
                    return s;
                }
                b=*s;
            } while((int8_t)b>=0);
        }
        length=(int32_t)(limit-s);
    }

    const uint8_t *limit0=limit;

    /*
     * Make sure that the last 1/2/3/4-byte sequence before limit is complete
     * or runs into a lead byte.
     * In the span loop compare s with limit only once
     * per multi-byte character.
     */
    b=*(limit-1);
    if((int8_t)b<0) {
        // b>=0x80: lead or trail byte
        if(b<0xc0) {
            // single trail byte, check for preceding 3- or 4-byte lead byte
            if(length>=2 && (b=*(limit-2))>=0xe0) {
                limit-=2;
            } else if(b<0xc0 && b>=0x80 && length>=3 && (b=*(limit-3))>=0xf0) {
                // 4-byte lead byte with only two trail bytes
                limit-=3;
            }
        } else {
            // lead byte with no trail bytes
            --limit;
        }
    }

    uint8_t t1, t2, t3;

    while(s<limit) {
#if 1
        b=*s;
        if((int8_t)b>=0) {
            // ASCII
            if(tf) {
                do {
                    if(!asciiBytes[b] || ++s==limit) {
                        return s;
                    }
                    b=*s;
                } while((int8_t)b>=0);
            } else {
                do {
                    if(asciiBytes[b]) {
                        return s;
                    } else if(++s==limit) {
                        // Treat the trailing illegal sequence as part of the FALSE span.
                        return limit0;
                    }
                    b=*s;
                } while((int8_t)b>=0);
            }
        }
        ++s;  // Advance past the lead byte.
#else
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else
#endif
        {
            if(b>=0xe0) {
                if(b<0xf0) {
                    if( /* handle U+0000..U+FFFF inline */
                        (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                        (t2=(uint8_t)(s[1]-0x80)) <= 0x3f
                    ) {
                        b&=0xf;
                        uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                        if(twoBits<=1) {
                            // All 64 code points with this lead byte and middle trail byte
                            // are either in the set or not.
                            if(twoBits!=tf) {
                                return s-1;
                            }
                        } else {
                            // Look up the code point in its 4k block of code points.
                            UChar32 c=(b<<12)|(t1<<6)|t2;
                            if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                                return s-1;
                            }
                        }
                        s+=2;
                        continue;
                    }
                } else if( /* handle U+10000..U+10FFFF inline */
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f &&
                    (t3=(uint8_t)(s[2]-0x80)) <= 0x3f
                ) {
                    // Treat an illegal sequence as part of the FALSE span.
                    UChar32 c=((UChar32)(b-0xf0)<<18)|((UChar32)t1<<12)|(t2<<6)|t3;
                    if( (   0x10000<=c && c<=0x10ffff &&
                            containsSlow(c, list4kStarts[0x10], list4kStarts[0x11])
                        ) != tf
                    ) {
                        return s-1;
                    }
                    s+=3;
                    continue;
                }
            } else /* b<0xe0 */ {
                if( /* handle U+0080..U+07FF inline */
                    b>=0xc2 &&
                    (t1=(uint8_t)(*s-0x80)) <= 0x3f
                ) {
                    if(((table7FFa[t1]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
            }

            // Treat an illegal sequence as part of the FALSE span.
            // Handle each byte of an illegal sequence separately to simplify the code;
            // no need to optimize error handling.
            if(tf) {
                return s-1;
            }
        }
    }

    if(!tf) {
        // Treat the trailing illegal sequence as part of the FALSE span.
        return limit0;
    } else {
        return limit;
    }
}

/*
 * For each lead byte, look up a lead value byte which indicates
 * - ASCII vs. multi-byte
 * - number of trail bytes
 * - TRUE or FALSE for all code points under this lead byte, for some types
 * - type: 2/3/4-byte UTF-8 characters vs. illegal, all TRUE/FALSE vs. mixed
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Precheck for sufficient trail bytes at end of string only once per span.
 * Check validity.
 */
BMPSetLeadValues::BMPSetLeadValues(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetLeadValues::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    const uint8_t *limit0=limit;

    /*
     * Make sure that the last 1/2/3/4-byte sequence before limit is complete
     * or runs into a lead byte.
     * In the span loop compare s with limit only once
     * per multi-byte character.
     */
    uint8_t b=*(limit-1);
    if((int8_t)b<0) {
        // b>=0x80: lead or trail byte
        if(b<0xc0) {
            // single trail byte, check for preceding 3- or 4-byte lead byte
            if(length>=2 && (b=*(limit-2))>=0xe0) {
                limit-=2;
            } else if(b<0xc0 && b>=0x80 && length>=3 && (b=*(limit-3))>=0xf0) {
                // 4-byte lead byte with only two trail bytes
                limit-=3;
            }
        } else {
            // lead byte with no trail bytes
            --limit;
        }
    }

    uint8_t t1, t2, t3;
    int8_t lv;

    while(s<limit) {
        b=*s++;
        lv=leadValues[b];
        if(lv>=0) {
            if(lv!=tf) {
                return s-1;
            }
        } else {
            switch(lv&LV_TYPE_MASK) {
            case LV_2B_TF:
                if((uint8_t)(*s-0x80) <= 0x3f) {
                    if(((lv&LV_TF_TRUE)!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
                break;
            case LV_2B_MIXED:
                if((t1=(uint8_t)(*s-0x80)) <= 0x3f) {
                    if(((table7FFa[t1]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                    continue;
                }
                break;
            case LV_3B_TF:
                if((uint8_t)(s[0]-0x80) <= 0x3f && (uint8_t)(s[1]-0x80) <= 0x3f) {
                    if(((lv&LV_TF_TRUE)!=0) != tf) {
                        return s-1;
                    }
                    s+=2;
                    continue;
                }
                break;
            case LV_3B_MIXED:
                if((t1=(uint8_t)(s[0]-0x80)) <= 0x3f && (t2=(uint8_t)(s[1]-0x80)) <= 0x3f) {
                    b&=0xf;
                    uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                    if(twoBits<=1) {
                        // All 64 code points with this lead byte and middle trail byte
                        // are either in the set or not.
                        if(twoBits!=tf) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        // Treat an illegal sequence as part of the FALSE span.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                    }
                    s+=2;
                    continue;
                }
                break;
            case LV_4B:
                if(
                    (t1=(uint8_t)(s[0]-0x80)) <= 0x3f &&
                    (t2=(uint8_t)(s[1]-0x80)) <= 0x3f &&
                    (t3=(uint8_t)(s[2]-0x80)) <= 0x3f
                ) {
                    // Treat an illegal sequence as part of the FALSE span.
                    UChar32 c=((UChar32)(b-0xf0)<<18)|((UChar32)t1<<12)|(t2<<6)|t3;
                    if( (   0x10000<=c && c<=0x10ffff &&
                            containsSlow(c, list4kStarts[0x10], list4kStarts[0x11])
                        ) != tf
                    ) {
                        return s-1;
                    }
                    s+=3;
                    continue;
                }
                break;
            case LV_ILLEGAL:
            default:
                break;
            }

            // Treat an illegal sequence as part of the FALSE span.
            // Handle each byte of an illegal sequence separately to simplify the code;
            // no need to optimize error handling.
            if(tf) {
                return s-1;
            }
        }
    }

    if(!tf) {
        // Treat the trailing illegal sequence as part of the FALSE span.
        return limit0;
    } else {
        return limit;
    }
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Precheck for sufficient trail bytes at end of string only once per span.
 * Lenient, only prevent buffer overrun.
 */
BMPSetASCIIBytes2BVerticalLenient::BMPSetASCIIBytes2BVerticalLenient(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BVerticalLenient::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b=*s;
    if((int8_t)b>=0) {
        // Initial all-ASCII span.
        if(tf) {
            do {
                if(!asciiBytes[b] || ++s==limit) {
                    return s;
                }
                b=*s;
            } while((int8_t)b>=0);
        } else {
            do {
                if(asciiBytes[b] || ++s==limit) {
                    return s;
                }
                b=*s;
            } while((int8_t)b>=0);
        }
        length=(int32_t)(limit-s);
    }

    const uint8_t *limit0=limit;

    /*
     * Make sure that the last 1/2/3/4-byte sequence before limit is complete.
     * In the span loop compare s with limit only once
     * per multi-byte character.
     */
    switch(length) {  // The caller enforces length>0.
    default:  // >=3
        if(*(limit-3)>=0xf0) {
            // 4-byte lead byte with only two more bytes left
            limit-=3;
            break;
        }
    case 2:
        if(*(limit-2)>=0xe0) {
            // 3/4-byte lead byte with only one more byte left
            limit-=2;
            break;
        }
    case 1:
        if((int8_t)*(limit-1)<0) {
            // trail or lead byte with no more byte left
            --limit;
            break;
        }
    }
    // The span loop may go beyond the reduced limit but
    // will not exceed the original limit.

    uint8_t t1, t2;

    while(s<limit) {
#if 1
        b=*s;
        if((int8_t)b>=0) {
            // ASCII
            if(tf) {
                do {
                    if(!asciiBytes[b]) {
                        return s;
                    } else if(++s==limit) {
                        // Treat the trailing illegal sequence as part of the span.
                        return limit0;
                    }
                    b=*s;
                } while((int8_t)b>=0);
            } else {
                do {
                    if(asciiBytes[b]) {
                        return s;
                    } else if(++s==limit) {
                        // Treat the trailing illegal sequence as part of the span.
                        return limit0;
                    }
                    b=*s;
                } while((int8_t)b>=0);
            }
        }
        ++s;  // Advance past the lead byte.
#else
        b=*s++;
        if((int8_t)b>=0) {
            if(asciiBytes[b]!=tf) {
                return s-1;
            }
        } else
#endif
        {
            if(b>=0xe0) {
                if(b<0xf0) {
                    /* U+0800..U+FFFF */
                    b&=0xf;
                    t1=(uint8_t)(s[0]&0x3f);
                    uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                    if(twoBits<=1) {
                        // All 64 code points with this lead byte and middle trail byte
                        // are either in the set or not.
                        if(twoBits!=tf) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        t2=(uint8_t)(s[1]&0x3f);
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                            return s-1;
                        }
                    }
                    s+=2;
                } else /* b>=0xf0 */ {
                    /* supplementary code points U+10000..U+10FFFF */
                    b&=0xf;
                    UChar32 c=((UChar32)b<<18)|((UChar32)(s[0]&0x3f)<<12)|((s[1]&0x3f)<<6)|(s[2]&0x3f);
                    // Broken sequence: c<0x10000 or c>0x10ffff is harmless for containsSlow()
                    // (it returns its lo or hi parameter)
                    // and we don't care about the result.
                    if(containsSlow(c, list4kStarts[0x10], list4kStarts[0x11]) != tf) {
                        return s-1;
                    }
                    s+=3;
                }
                // else count b as part of the span and continue
            } else /* b<0xe0 */ {
                /* U+0080..U+07FF */
                if(((table7FFa[*s&0x3f]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                    return s-1;
                }
                ++s;
            }
        }
    }

    // Treat the trailing illegal sequence as part of the span.
    return limit0;
}

/*
 * ASCII: Look up bytes.
 * 2-byte characters: Bits organized vertically.
 * 3-byte characters: Use zero/one/mixed data per 64-block in U+0000..U+FFFF.
 * Precheck for sufficient trail bytes at end of string only once per span.
 * (Different precheck from previous version.)
 * Lenient, only prevent buffer overrun.
 */
BMPSetASCIIBytes2BVerticalLenient2::BMPSetASCIIBytes2BVerticalLenient2(const UnicodeSet &parent) : BMPSet(parent) {}

const uint8_t *
BMPSetASCIIBytes2BVerticalLenient2::spanUTF8(const uint8_t *s, int32_t length, const uint8_t *limit, UBool tf) const {
    uint8_t b=*s;
    if((int8_t)b>=0) {
        // Initial all-ASCII span.
        if(tf) {
            do {
                if(!asciiBytes[b] || ++s==limit) {
                    return s;
                }
                b=*s;
            } while((int8_t)b>=0);
        } else {
            do {
                if(asciiBytes[b] || ++s==limit) {
                    return s;
                }
                b=*s;
            } while((int8_t)b>=0);
        }
        length=(int32_t)(limit-s);
    }

    /*
     * Reduce the limit by 3 so that the span loop can compare s with limit
     * only once per multi-byte character without exceeding the limit.
     * If the span loop runs into the reduced limit, then go into a slow loop.
     * A more sophisticated loop is possible but reduces performance for
     * short spans by increasing per-span overhead.
     *
     * The span loop may go beyond the reduced limit but
     * will not exceed the original limit.
     */
    if(length>3) {
        limit-=3;

        uint8_t t1, t2;

        while(s<limit) {
#if 1
            b=*s;
            if((int8_t)b>=0) {
                // ASCII
                if(tf) {
                    do {
                        if(!asciiBytes[b]) {
                            return s;
                        } else if(++s==limit) {
                            goto endloop;
                        }
                        b=*s;
                    } while((int8_t)b>=0);
                } else {
                    do {
                        if(asciiBytes[b]) {
                            return s;
                        } else if(++s==limit) {
                            goto endloop;
                        }
                        b=*s;
                    } while((int8_t)b>=0);
                }
            }
            ++s;  // Advance past the lead byte.
#else
            b=*s++;
            if((int8_t)b>=0) {
                if(asciiBytes[b]!=tf) {
                    return s-1;
                }
            } else
#endif
            {
                if(b>=0xe0) {
                    if(b<0xf0) {
                        /* U+0800..U+FFFF */
                        b&=0xf;
                        t1=(uint8_t)(s[0]&0x3f);
                        uint32_t twoBits=(bmpBlockBits[t1]>>b)&0x10001;
                        if(twoBits<=1) {
                            // All 64 code points with this lead byte and middle trail byte
                            // are either in the set or not.
                            if(twoBits!=tf) {
                                return s-1;
                            }
                        } else {
                            // Look up the code point in its 4k block of code points.
                            t2=(uint8_t)(s[1]&0x3f);
                            UChar32 c=(b<<12)|(t1<<6)|t2;
                            if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != tf) {
                                return s-1;
                            }
                        }
                        s+=2;
                    } else /* b>=0xf0 */ {
                        /* supplementary code points U+10000..U+10FFFF */
                        b&=0xf;
                        UChar32 c=((UChar32)b<<18)|((UChar32)(s[0]&0x3f)<<12)|((s[1]&0x3f)<<6)|(s[2]&0x3f);
                        // Broken sequence: c<0x10000 or c>0x10ffff is harmless for containsSlow()
                        // (it returns its lo or hi parameter)
                        // and we don't care about the result.
                        if(containsSlow(c, list4kStarts[0x10], list4kStarts[0x11]) != tf) {
                            return s-1;
                        }
                        s+=3;
                    }
                    // else count b as part of the span and continue
                } else /* b<0xe0 */ {
                    /* U+0080..U+07FF */
                    if(((table7FFa[*s&0x3f]&((uint32_t)1<<(b&0x1f)))!=0) != tf) {
                        return s-1;
                    }
                    ++s;
                }
            }
        }
    }
endloop:

    // Slow loop for the last 3 bytes of the string.
    UChar32 c;
    int32_t start=0;
    int32_t prev;
    while((prev=start)<length) {
        U8_NEXT(s, start, length, c);
        // Broken sequence: c<0 or c>0x10ffff is harmless for containsSlow()
        // (it returns its lo or hi parameter)
        // and we don't care about the result.
        if(tf!=containsSlow(c)) {
            break;
        }
    }
    return s+prev;
}

U_NAMESPACE_END
