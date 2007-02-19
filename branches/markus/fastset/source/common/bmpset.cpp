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

BMPSet::BMPSet(const UnicodeSet &parent) : set(parent) {
    uprv_memset(asciiBytes, 0, sizeof(asciiBytes));
    uprv_memset(table7FF, 0, sizeof(table7FF));
    uprv_memset(bmpBlockBits, 0, sizeof(bmpBlockBits));

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

    initBits();
    overrideIllegal();
}

BMPSet::BMPSet(const BMPSet &otherBMPSet, const UnicodeSet &newParent) : set(newParent) {
    uprv_memcpy(asciiBytes, otherBMPSet.asciiBytes, sizeof(asciiBytes));
    uprv_memcpy(table7FF, otherBMPSet.table7FF, sizeof(table7FF));
    uprv_memcpy(bmpBlockBits, otherBMPSet.bmpBlockBits, sizeof(bmpBlockBits));
    uprv_memcpy(list4kStarts, otherBMPSet.list4kStarts, sizeof(list4kStarts));
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
}

/*
 * Set the bits for the 32 consecutive code points from
 * start=(blockIndex<<5).
 * For each odd block index, the even sibling must be set first.
 */
void BMPSet::setBits(int32_t blockIndex, uint32_t bits) {
    int32_t i, limit;

    if(blockIndex<(0x80>>5)) {
        // Set ASCII flags.
        uint32_t b=bits;
        i=blockIndex<<5;
        limit=i+32;
        while(i<limit && b!=0) {
            asciiBytes[i++]=(UBool)(b&1);
            b>>=1;
        }
        return;
    }

    if(blockIndex<(0x800>>5)) {
        // Set flags for 80..7FF.
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
                table7FF[i]|=bit;
            }
            ++i;
            b>>=1;
        }
        return;
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

    if(startIndex<(0x80>>5)) {
        // Set ASCII flags.
        i=startIndex<<5;
        if(limitIndex<(128>>5)) {
            limit=limitIndex<<5;
        } else {
            limit=0x80;
        }
        while(i<limit) {
            asciiBytes[i++]=1;
        }

        startIndex=(0x80>>5);
        if(startIndex>=limitIndex) {
            return;
        }
    }

    // Now work with 64-blocks.
    startIndex>>=1;
    limitIndex>>=1;

    if(startIndex<(0x800>>6)) {
        // Set flags for 80..7FF.
        uint32_t bits=~(((uint32_t)1<<startIndex)-1);
        if(limitIndex<(0x800>>6)) {
            bits&=((uint32_t)1<<limitIndex)-1;
        }
        for(i=0; i<64; ++i) {
            table7FF[i]|=bits;
        }

        startIndex=(0x800>>6);
        if(startIndex>=limitIndex) {
            return;
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

/*
 * Override some bits and bytes to the result of contains(FFFD)
 * for faster validity checking at runtime.
 * No need to set 0 values where they were reset to 0 in the constructor
 * and not modified by setBits() and setOnes().
 * (asciiBytes[] trail bytes, table7FF[] 0..7F, bmpBlockBits[] 0..7FF)
 * Need to set 0 values for surrogates D800..DFFF.
 */
void BMPSet::overrideIllegal() {
    uint32_t bits, mask;
    int32_t i;

    if(containsSlow(0xfffd, list4kStarts[0xf], list4kStarts[0x10])) {
        // contains(FFFD)==TRUE
        for(i=0x80; i<0xc0; ++i) {
            asciiBytes[i]=1;
        }

        bits=3;                 // Lead bytes 0xC0 and 0xC1.
        for(i=0; i<64; ++i) {
            table7FF[i]|=bits;
        }

        bits=1;                 // Lead byte 0xE0.
        for(i=0; i<32; ++i) {   // First half of 4k block.
            bmpBlockBits[i]|=bits;
        }

        mask=~(0x10001<<0xd);   // Lead byte 0xED.
        bits=1<<0xd;
        for(i=32; i<64; ++i) {  // Second half of 4k block.
            bmpBlockBits[i]=(bmpBlockBits[i]&mask)|bits;
        }
    } else {
        // contains(FFFD)==FALSE
        mask=~0x10001;          // Lead byte 0xE0.
        for(i=0; i<32; ++i) {   // First half of 4k block.
            bmpBlockBits[i]&=mask;
        }

        mask=~(0x10001<<0xd);   // Lead byte 0xED.
        for(i=32; i<64; ++i) {  // Second half of 4k block.
            bmpBlockBits[i]&=mask;
        }
    }
}

UBool
BMPSet::contains(UChar32 c) const {
    if((uint32_t)c<=0x7f) {
        return (UBool)asciiBytes[c];
    } else if((uint32_t)c<=0x7ff) {
        return (UBool)((table7FF[c&0x3f]&((uint32_t)1<<(c>>6)))!=0);
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
        // Out-of-range code points get FALSE, consistent with long-standing
        // behavior of UnicodeSet::contains(c).
        return FALSE;
    }
}

/*
 * Check for sufficient length for trail unit for each surrogate pair.
 * Handle single surrogates as surrogate code points as usual in ICU.
 */
const UChar *
BMPSet::span(const UChar *s, const UChar *limit, USetSpanCondition spanCondition) const {
    UChar c, c2;

    if(spanCondition) {
        // span
        do {
            c=*s;
            if(c<=0x7f) {
                if(!asciiBytes[c]) {
                    break;
                }
            } else if(c<=0x7ff) {
                if((table7FF[c&0x3f]&((uint32_t)1<<(c>>6)))==0) {
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
                if((table7FF[c&0x3f]&((uint32_t)1<<(c>>6)))!=0) {
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

/* Symmetrical with span(). */
const UChar *
BMPSet::spanBack(const UChar *s, const UChar *limit, USetSpanCondition spanCondition) const {
    UChar c, c2;

    if(spanCondition) {
        // span
        for(;;) {
            c=*(--limit);
            if(c<=0x7f) {
                if(!asciiBytes[c]) {
                    break;
                }
            } else if(c<=0x7ff) {
                if((table7FF[c&0x3f]&((uint32_t)1<<(c>>6)))==0) {
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
            } else if(c<0xdc00 || s==limit || (c2=*(limit-1))<0xd800 || c2>=0xdc00) {
                // surrogate code point
                if(!containsSlow(c, list4kStarts[0xd], list4kStarts[0xe])) {
                    break;
                }
            } else {
                // surrogate pair
                if(!containsSlow(U16_GET_SUPPLEMENTARY(c2, c), list4kStarts[0x10], list4kStarts[0x11])) {
                    break;
                }
                --limit;
            }
            if(s==limit) {
                return s;
            }
        }
    } else {
        // span not
        for(;;) {
            c=*(--limit);
            if(c<=0x7f) {
                if(asciiBytes[c]) {
                    break;
                }
            } else if(c<=0x7ff) {
                if((table7FF[c&0x3f]&((uint32_t)1<<(c>>6)))!=0) {
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
            } else if(c<0xdc00 || s==limit || (c2=*(limit-1))<0xd800 || c2>=0xdc00) {
                // surrogate code point
                if(containsSlow(c, list4kStarts[0xd], list4kStarts[0xe])) {
                    break;
                }
            } else {
                // surrogate pair
                if(containsSlow(U16_GET_SUPPLEMENTARY(c2, c), list4kStarts[0x10], list4kStarts[0x11])) {
                    break;
                }
                --limit;
            }
            if(s==limit) {
                return s;
            }
        }
    }
    return limit+1;
}

/*
 * Precheck for sufficient trail bytes at end of string only once per span.
 * Check validity.
 */
const uint8_t *
BMPSet::spanUTF8(const uint8_t *s, int32_t length, USetSpanCondition spanCondition) const {
    const uint8_t *limit=s+length;
    uint8_t b=*s;
    if((int8_t)b>=0) {
        // Initial all-ASCII span.
        if(spanCondition) {
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
     *
     * Give a trailing illegal sequence the same value as the result of contains(FFFD),
     * including it if that is part of the span, otherwise set limit0 to before
     * the truncated sequence.
     */
    b=*(limit-1);
    if((int8_t)b<0) {
        // b>=0x80: lead or trail byte
        if(b<0xc0) {
            // single trail byte, check for preceding 3- or 4-byte lead byte
            if(length>=2 && (b=*(limit-2))>=0xe0) {
                limit-=2;
                if(asciiBytes[0x80]!=spanCondition) {
                    limit0=limit;
                }
            } else if(b<0xc0 && b>=0x80 && length>=3 && (b=*(limit-3))>=0xf0) {
                // 4-byte lead byte with only two trail bytes
                limit-=3;
                if(asciiBytes[0x80]!=spanCondition) {
                    limit0=limit;
                }
            }
        } else {
            // lead byte with no trail bytes
            --limit;
            if(asciiBytes[0x80]!=spanCondition) {
                limit0=limit;
            }
        }
    }

    uint8_t t1, t2, t3;

    while(s<limit) {
        b=*s;
        if(b<0xc0) {
            // ASCII; or trail bytes with the result of contains(FFFD).
            if(spanCondition) {
                do {
                    if(!asciiBytes[b]) {
                        return s;
                    } else if(++s==limit) {
                        return limit0;
                    }
                    b=*s;
                } while(b<0xc0);
            } else {
                do {
                    if(asciiBytes[b]) {
                        return s;
                    } else if(++s==limit) {
                        return limit0;
                    }
                    b=*s;
                } while(b<0xc0);
            }
        }
        ++s;  // Advance past the lead byte.
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
                        if(twoBits!=spanCondition) {
                            return s-1;
                        }
                    } else {
                        // Look up the code point in its 4k block of code points.
                        UChar32 c=(b<<12)|(t1<<6)|t2;
                        if(containsSlow(c, list4kStarts[b], list4kStarts[b+1]) != spanCondition) {
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
                // Give an illegal sequence the same value as the result of contains(FFFD).
                UChar32 c=((UChar32)(b-0xf0)<<18)|((UChar32)t1<<12)|(t2<<6)|t3;
                if( (   (0x10000<=c && c<=0x10ffff) ?
                            containsSlow(c, list4kStarts[0x10], list4kStarts[0x11]) :
                            asciiBytes[0x80]
                    ) != spanCondition
                ) {
                    return s-1;
                }
                s+=3;
                continue;
            }
        } else /* 0xc0<=b<0xe0 */ {
            if( /* handle U+0000..U+07FF inline */
                (t1=(uint8_t)(*s-0x80)) <= 0x3f
            ) {
                if(((table7FF[t1]&((uint32_t)1<<(b&0x1f)))!=0) != spanCondition) {
                    return s-1;
                }
                ++s;
                continue;
            }
        }

        // Give an illegal sequence the same value as the result of contains(FFFD).
        // Handle each byte of an illegal sequence separately to simplify the code;
        // no need to optimize error handling.
        if(asciiBytes[0x80]!=spanCondition) {
            return s-1;
        }
    }

    return limit0;
}

/*
 * While going backwards through UTF-8 optimize only for ASCII.
 * Unlike UTF-16, UTF-8 is not forward-backward symmetrical, that is, it is not
 * possible to tell from the last byte in a multi-byte sequence how many
 * preceding bytes there should be. Therefore, going backwards through UTF-8
 * is much harder than going forward.
 */
int32_t
BMPSet::spanBackUTF8(const uint8_t *s, int32_t length, USetSpanCondition spanCondition) const {
    uint8_t b;

    do {
        b=s[--length];
        if((int8_t)b>=0) {
            // ASCII sub-span
            if(spanCondition) {
                do {
                    if(!asciiBytes[b]) {
                        return length+1;
                    } else if(length==0) {
                        return 0;
                    }
                    b=s[--length];
                } while((int8_t)b>=0);
            } else {
                do {
                    if(asciiBytes[b]) {
                        return length+1;
                    } else if(length==0) {
                        return 0;
                    }
                    b=s[--length];
                } while((int8_t)b>=0);
            }
        }

        int32_t prev=length;
        UChar32 c;
        if(b<0xc0) {
            // trail byte: collect a multi-byte character
            c=utf8_prevCharSafeBody(s, 0, &length, b, -1);
            if(c<0) {
                c=0xfffd;
            }
        } else {
            // lead byte in last-trail position
            c=0xfffd;
        }
        // c is a valid code point, not ASCII, not a surrogate
        if(c<=0x7ff) {
            if(((table7FF[c&0x3f]&((uint32_t)1<<(c>>6)))!=0) != spanCondition) {
                return prev+1;
            }
        } else if(c<=0xffff) {
            int lead=c>>12;
            uint32_t twoBits=(bmpBlockBits[(c>>6)&0x3f]>>lead)&0x10001;
            if(twoBits<=1) {
                // All 64 code points with the same bits 15..6
                // are either in the set or not.
                if(twoBits!=spanCondition) {
                    return prev+1;
                }
            } else {
                // Look up the code point in its 4k block of code points.
                if(containsSlow(c, list4kStarts[lead], list4kStarts[lead+1]) != spanCondition) {
                    return prev+1;
                }
            }
        } else {
            if(containsSlow(c, list4kStarts[0x10], list4kStarts[0x11]) != spanCondition) {
                return prev+1;
            }
        }
    } while(length>0);
    return 0;
}

U_NAMESPACE_END
