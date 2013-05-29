/*  
*******************************************************************************
*
*   Copyright (C) 1999-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  collationweights.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001mar08 as ucol_wgt.cpp
*   created by: Markus W. Scherer
*
*   This file contains code for allocating n collation element weights
*   between two exclusive limits.
*   It is used only internally by the collation tailoring builder.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "cmemory.h"
#include "collation.h"
#include "collationweights.h"
#include "uarrsort.h"
#include "uassert.h"

#ifdef UCOL_DEBUG
#   include <stdio.h>
#endif

U_NAMESPACE_BEGIN

/* collation element weight allocation -------------------------------------- */

/* helper functions for CE weights */

static inline int32_t
lengthOfWeight(uint32_t weight) {
    if((weight&0xffffff)==0) {
        return 1;
    } else if((weight&0xffff)==0) {
        return 2;
    } else if((weight&0xff)==0) {
        return 3;
    } else {
        return 4;
    }
}

static inline uint32_t
getWeightTrail(uint32_t weight, int32_t length) {
    return (uint32_t)(weight>>(8*(4-length)))&0xff;
}

static inline uint32_t
setWeightTrail(uint32_t weight, int32_t length, uint32_t trail) {
    length=8*(4-length);
    return (uint32_t)((weight&(0xffffff00<<length))|(trail<<length));
}

static inline uint32_t
getWeightByte(uint32_t weight, int32_t idx) {
    return getWeightTrail(weight, idx); /* same calculation */
}

static inline uint32_t
setWeightByte(uint32_t weight, int32_t idx, uint32_t byte) {
    uint32_t mask; /* 0xffffffff except a 00 "hole" for the index-th byte */

    idx*=8;
    if(idx<32) {
        mask=((uint32_t)0xffffffff)>>idx;
    } else {
        // Do not use uint32_t>>32 because on some platforms that does not shift at all
        // while we need it to become 0.
        // PowerPC: 0xffffffff>>32 = 0           (wanted)
        // x86:     0xffffffff>>32 = 0xffffffff  (not wanted)
        //
        // ANSI C99 6.5.7 Bitwise shift operators:
        // "If the value of the right operand is negative
        // or is greater than or equal to the width of the promoted left operand,
        // the behavior is undefined."
        mask=0;
    }
    idx=32-idx;
    mask|=0xffffff00<<idx;
    return (uint32_t)((weight&mask)|(byte<<idx));
}

static inline uint32_t
truncateWeight(uint32_t weight, int32_t length) {
    return (uint32_t)(weight&(0xffffffff<<(8*(4-length))));
}

static inline uint32_t
incWeightTrail(uint32_t weight, int32_t length) {
    return (uint32_t)(weight+(1UL<<(8*(4-length))));
}

static inline uint32_t
decWeightTrail(uint32_t weight, int32_t length) {
    return (uint32_t)(weight-(1UL<<(8*(4-length))));
}

CollationWeights::CollationWeights()
        : middleLength(0), rangeCount(0) {
    for(int32_t i = 0; i < 5; ++i) {
        minBytes[i] = maxBytes[i] = 0;
    }
}

void
CollationWeights::initForPrimary(UBool compressible) {
    middleLength=1;
    minBytes[1] = Collation::MERGE_SEPARATOR_BYTE + 1;
    maxBytes[1] = Collation::TRAIL_WEIGHT_BYTE;
    if(compressible) {
        minBytes[2] = Collation::PRIMARY_COMPRESSION_LOW_BYTE + 1;
        maxBytes[2] = Collation::PRIMARY_COMPRESSION_HIGH_BYTE - 1;
    } else {
        minBytes[2] = 2;
        maxBytes[2] = 0xff;
    }
    minBytes[3] = 2;
    maxBytes[3] = 0xff;
    minBytes[4] = 2;
    maxBytes[4] = 0xff;
}

void
CollationWeights::initForSecondary() {
    // We use only the lower 16 bits for secondary weights.
    middleLength=3;
    minBytes[1] = 0;
    maxBytes[1] = 0;
    minBytes[2] = 0;
    maxBytes[2] = 0;
    minBytes[3] = Collation::MERGE_SEPARATOR_BYTE + 1;
    maxBytes[3] = 0xff;
    minBytes[4] = 2;
    maxBytes[4] = 0xff;
}

void
CollationWeights::initForTertiary() {
    // We use only the lower 16 bits for tertiary weights.
    middleLength=3;
    minBytes[1] = 0;
    maxBytes[1] = 0;
    minBytes[2] = 0;
    maxBytes[2] = 0;
    // We use only 6 bits per byte.
    // The other bits are used for case & quaternary weights.
    minBytes[3] = Collation::MERGE_SEPARATOR_BYTE + 1;
    maxBytes[3] = 0x3f;
    minBytes[4] = 2;
    maxBytes[4] = 0x3f;
}

uint32_t
CollationWeights::incWeight(uint32_t weight, int32_t length) {
    for(;;) {
        uint32_t byte=getWeightByte(weight, length);
        if(byte<maxBytes[length]) {
            return setWeightByte(weight, length, byte+1);
        } else {
            // Roll over, set this byte to the minimum and increment the previous one.
            weight=setWeightByte(weight, length, minBytes[length]);
            --length;
        }
    }
}

int32_t
CollationWeights::lengthenRange(WeightRange &range) {
    int32_t length=range.length2+1;
    range.start=setWeightTrail(range.start, length, minBytes[length]);
    range.end=setWeightTrail(range.end, length, maxBytes[length]);
    range.count2*=countBytes(length);
    range.length2=length;
    return length;
}

/* for uprv_sortArray: sort ranges in weight order */
static int32_t U_CALLCONV
compareRanges(const void * /*context*/, const void *left, const void *right) {
    uint32_t l, r;

    l=((const CollationWeights::WeightRange *)left)->start;
    r=((const CollationWeights::WeightRange *)right)->start;
    if(l<r) {
        return -1;
    } else if(l>r) {
        return 1;
    } else {
        return 0;
    }
}

UBool
CollationWeights::getWeightRanges(uint32_t lowerLimit, uint32_t upperLimit) {
    U_ASSERT(lowerLimit != 0);
    U_ASSERT(upperLimit != 0);

    /* get the lengths of the limits */
    int32_t lowerLength=lengthOfWeight(lowerLimit);
    int32_t upperLength=lengthOfWeight(upperLimit);

#ifdef UCOL_DEBUG
    printf("length of lower limit 0x%08lx is %ld\n", lowerLimit, lowerLength);
    printf("length of upper limit 0x%08lx is %ld\n", upperLimit, upperLength);
#endif
    U_ASSERT(lowerLength>=middleLength);
    // Permit upperLength<middleLength: The upper limit for secondaries is 0x10000.

    if(lowerLimit>=upperLimit) {
#ifdef UCOL_DEBUG
        printf("error: no space between lower & upper limits\n");
#endif
        return FALSE;
    }

    /* check that neither is a prefix of the other */
    if(lowerLength<upperLength) {
        if(lowerLimit==truncateWeight(upperLimit, lowerLength)) {
#ifdef UCOL_DEBUG
            printf("error: lower limit 0x%08lx is a prefix of upper limit 0x%08lx\n", lowerLimit, upperLimit);
#endif
            return FALSE;
        }
    }
    /* if the upper limit is a prefix of the lower limit then the earlier test lowerLimit>=upperLimit has caught it */

    WeightRange lower[5], middle, upper[5]; /* [0] and [1] are not used - this simplifies indexing */
    uprv_memset(lower, 0, sizeof(lower));
    uprv_memset(&middle, 0, sizeof(middle));
    uprv_memset(upper, 0, sizeof(upper));

    /*
     * With the limit lengths of 1..4, there are up to 7 ranges for allocation:
     * range     minimum length
     * lower[4]  4
     * lower[3]  3
     * lower[2]  2
     * middle    1
     * upper[2]  2
     * upper[3]  3
     * upper[4]  4
     *
     * We are now going to calculate up to 7 ranges.
     * Some of them will typically overlap, so we will then have to merge and eliminate ranges.
     */
    uint32_t weight=lowerLimit;
    for(int32_t length=lowerLength; length>middleLength; --length) {
        uint32_t trail=getWeightTrail(weight, length);
        if(trail<maxBytes[length]) {
            lower[length].start=incWeightTrail(weight, length);
            lower[length].end=setWeightTrail(weight, length, maxBytes[length]);
            lower[length].length=length;
            lower[length].count=maxBytes[length]-trail;
        }
        weight=truncateWeight(weight, length-1);
    }
    middle.start=incWeightTrail(weight, middleLength);

    weight=upperLimit;
    for(int32_t length=upperLength; length>middleLength; --length) {
        uint32_t trail=getWeightTrail(weight, length);
        if(trail>minBytes[length]) {
            upper[length].start=setWeightTrail(weight, length, minBytes[length]);
            upper[length].end=decWeightTrail(weight, length);
            upper[length].length=length;
            upper[length].count=trail-minBytes[length];
        }
        weight=truncateWeight(weight, length-1);
    }
    middle.end=decWeightTrail(weight, middleLength);

    /* set the middle range */
    middle.length=middleLength;
    if(middle.end>=middle.start) {
        middle.count=(int32_t)((middle.end-middle.start)>>(8*(4-middleLength)))+1;
    } else {
        /* eliminate overlaps */

        /* remove the middle range */
        middle.count=0;

        /* reduce or remove the lower ranges that go beyond upperLimit */
        for(int32_t length=4; length>middleLength; --length) {
            if(lower[length].count>0 && upper[length].count>0) {
                uint32_t start=upper[length].start;
                uint32_t end=lower[length].end;

                if(end>=start || incWeight(end, length)==start) {
                    /* lower and upper ranges collide or are directly adjacent: merge these two and remove all shorter ranges */
                    start=lower[length].start;
                    end=lower[length].end=upper[length].end;
                    /*
                     * merging directly adjacent ranges needs to subtract the 0/1 gaps in between;
                     * it may result in a range with count>countBytes
                     */
                    lower[length].count=
                        (int32_t)(getWeightTrail(end, length)-getWeightTrail(start, length)+1+
                                  countBytes(length)*(getWeightByte(end, length-1)-getWeightByte(start, length-1)));
                    upper[length].count=0;
                    while(--length>middleLength) {
                        lower[length].count=upper[length].count=0;
                    }
                    break;
                }
            }
        }
    }

#ifdef UCOL_DEBUG
    /* print ranges */
    for(int32_t length=4; length>=2; --length) {
        if(lower[length].count>0) {
            printf("lower[%ld] .start=0x%08lx .end=0x%08lx .count=%ld\n", length, lower[length].start, lower[length].end, lower[length].count);
        }
    }
    if(middle.count>0) {
        printf("middle   .start=0x%08lx .end=0x%08lx .count=%ld\n", middle.start, middle.end, middle.count);
    }
    for(int32_t length=2; length<=4; ++length) {
        if(upper[length].count>0) {
            printf("upper[%ld] .start=0x%08lx .end=0x%08lx .count=%ld\n", length, upper[length].start, upper[length].end, upper[length].count);
        }
    }
#endif

    /* copy the ranges, shortest first, into the result array */
    rangeCount=0;
    if(middle.count>0) {
        uprv_memcpy(ranges, &middle, sizeof(WeightRange));
        rangeCount=1;
    }
    for(int32_t length=middleLength+1; length<=4; ++length) {
        /* copy upper first so that later the middle range is more likely the first one to use */
        if(upper[length].count>0) {
            uprv_memcpy(ranges+rangeCount, upper+length, sizeof(WeightRange));
            ++rangeCount;
        }
        if(lower[length].count>0) {
            uprv_memcpy(ranges+rangeCount, lower+length, sizeof(WeightRange));
            ++rangeCount;
        }
    }
    return rangeCount>0;
}

/*
 * call getWeightRanges and then determine heuristically
 * which ranges to use for a given number of weights between (excluding)
 * two limits
 */
UBool
CollationWeights::allocWeights(uint32_t lowerLimit, uint32_t upperLimit, int32_t n) {
#ifdef UCOL_DEBUG
    puts("");
#endif

    if(!getWeightRanges(lowerLimit, upperLimit)) {
#ifdef UCOL_DEBUG
        printf("error: unable to get Weight ranges\n");
#endif
        return FALSE;
    }

    /* what is the maximum number of weights with these ranges? */
    int32_t maxCount=0;
    for(int32_t i=0; i<rangeCount; ++i) {
        int32_t count=ranges[i].count;
        for(int32_t j = ranges[i].length + 1; j <= 4; ++j) {
            count *= countBytes(j);
        }
        maxCount+=count;
    }
    if(maxCount>=n) {
#ifdef UCOL_DEBUG
        printf("the maximum number of %lu weights is sufficient for n=%lu\n", maxCount, n);
#endif
    } else {
#ifdef UCOL_DEBUG
        printf("error: the maximum number of %lu weights is insufficient for n=%lu\n", maxCount, n);
#endif
        return FALSE;
    }

    /* set the length2 and count2 fields */
    for(int32_t i=0; i<rangeCount; ++i) {
        ranges[i].length2=ranges[i].length;
        ranges[i].count2=ranges[i].count;
    }

    /* try until we find suitably large ranges */
    int32_t lengthCounts[6]; /* [0] unused, [5] to make index checks unnecessary */
    for(;;) {
        /* get the smallest number of bytes in a range */
        int32_t minLength=ranges[0].length2;

        /* sum up the number of elements that fit into ranges of each byte length */
        uprv_memset(lengthCounts, 0, sizeof(lengthCounts));
        for(int32_t i=0; i<rangeCount; ++i) {
            lengthCounts[ranges[i].length2]+=ranges[i].count2;
        }

        /* now try to allocate n elements in the available short ranges */
        if(n<=(lengthCounts[minLength]+lengthCounts[minLength+1])) {
            /* trivial cases, use the first few ranges */
            maxCount=0;
            rangeCount=0;
            do {
                maxCount+=ranges[rangeCount].count2;
                ++rangeCount;
            } while(n>maxCount);
#ifdef UCOL_DEBUG
            printf("take first %ld ranges\n", rangeCount);
#endif
            break;
        } else if(minLength < 4 && n<=ranges[0].count2*countBytes(minLength + 1)) {
            /* easy case, just make this one range large enough by lengthening it once more, possibly split it */

            /* calculate how to split the range between minLength (count1) and minLength+1 (count2) */
            int32_t perPrefix1 = 1;  // number of weights per ranges[0] prefix
            for(int32_t j = ranges[0].length + 1; j <= minLength; ++j) {
                perPrefix1 *= countBytes(j);
            }
            int32_t perPrefix2 = perPrefix1 * countBytes(minLength + 1);
            int32_t count2=(n+perPrefix2-1)/perPrefix2;  // number of prefixes with longer weights
            int32_t count1=ranges[0].count-count2;  // number of prefixes with minLength weights

            /* split the range */
#ifdef UCOL_DEBUG
            printf("split the first range %ld:%ld\n", count1, count2);
#endif
            if(count1<1) {
                rangeCount=1;

                /* lengthen the entire range to minLength+1 */
                lengthenRange(ranges[0]);
            } else {
                /* really split the range */

                /* create a new range with the end and initial and current length of the old one */
                rangeCount=2;
                ranges[1].end=ranges[0].end;
                ranges[1].length=ranges[0].length;
                ranges[1].length2=minLength;

                /* set the end of the first range according to count1 */
                int32_t i=ranges[0].length;
                uint32_t byte=getWeightByte(ranges[0].start, i)+count1-1;

                /*
                 * ranges[0].count and count1 may be >countBytes
                 * from merging adjacent ranges;
                 * byte>maxByte is possible
                 */
                if(byte<=maxBytes[i]) {
                    ranges[0].end=setWeightByte(ranges[0].start, i, byte);
                } else /* byte>maxByte */ {
                    ranges[0].end=setWeightByte(incWeight(ranges[0].start, i-1), i, byte-countBytes(i));
                }

                /* set the start of the second range to immediately follow the end of the first one */
                ranges[1].start=incWeight(ranges[0].end, i);

                // Set the bytes in the end weight at length+1..length2 to maxByte,
                // and in the following start weight to minByte
                for(int32_t j = i + 1; j <= minLength; ++j) {
                    ranges[0].end = setWeightByte(ranges[0].end, j, maxBytes[j]);
                    ranges[1].start = setWeightByte(ranges[1].start, j, minBytes[j]);
                }

                /* set the count values (informational) */
                ranges[0].count=count1;
                ranges[1].count=count2;

                ranges[0].count2 = count1 * perPrefix1;
                ranges[1].count2 = count2 * perPrefix1;  // will be *countBytes when lengthened

                /* lengthen the second range to minLength+1 */
                lengthenRange(ranges[1]);
            }
            break;
        }

        /* no good match, lengthen all minLength ranges and iterate */
#ifdef UCOL_DEBUG
        printf("lengthen the short ranges from %ld bytes to %ld and iterate\n", minLength, minLength+1);
#endif
        for(int32_t i=0; ranges[i].length2==minLength; ++i) {
            lengthenRange(ranges[i]);
        }
    }

    if(rangeCount>1) {
        /* sort the ranges by weight values */
        UErrorCode errorCode=U_ZERO_ERROR;
        uprv_sortArray(ranges, rangeCount, sizeof(WeightRange), compareRanges, NULL, FALSE, &errorCode);
        /* ignore error code: we know that the internal sort function will not fail here */
    }

#ifdef UCOL_DEBUG
    puts("final ranges:");
    for(int32_t i=0; i<rangeCount; ++i) {
        printf("ranges[%ld] .start=0x%08lx .end=0x%08lx .length=%ld .length2=%ld .count=%ld .count2=%lu\n",
               i, ranges[i].start, ranges[i].end, ranges[i].length, ranges[i].length2, ranges[i].count, ranges[i].count2);
    }
#endif

    return rangeCount;
}

uint32_t
CollationWeights::nextWeight() {
    if(rangeCount<=0) {
        return 0xffffffff;
    } else {
        /* get the next weight */
        uint32_t weight=ranges[0].start;
        if(weight==ranges[0].end) {
            /* this range is finished, remove it and move the following ones up */
            if(--rangeCount>0) {
                uprv_memmove(ranges, ranges+1, rangeCount*sizeof(WeightRange));
            }
        } else {
            /* increment the weight for the next value */
            ranges[0].start=incWeight(weight, ranges[0].length2);
        }

        return weight;
    }
}

U_NAMESPACE_END

#if 0 // #ifdef UCOL_DEBUG

// TODO: move to collationtest.cpp
static void
testAlloc(uint32_t lowerLimit, uint32_t upperLimit, uint32_t n, UBool enumerate) {
    WeightRange ranges[8];
    int32_t rangeCount;

    rangeCount=ucol_allocWeights(lowerLimit, upperLimit, n, ranges);
    if(enumerate) {
        uint32_t weight;

        while(n>0) {
            weight=ucol_nextWeight(ranges, &rangeCount);
            if(weight==0xffffffff) {
                printf("error: 0xffffffff with %lu more weights to go\n", n);
                break;
            }
            printf("    0x%08lx\n", weight);
            --n;
        }
    }
}

extern int
main(int argc, const char *argv[]) {
#if 0
#endif
    testAlloc(0x364214fc, 0x44b87d23, 5, FALSE);
    testAlloc(0x36421500, 0x44b87d23, 5, FALSE);
    testAlloc(0x36421500, 0x44b87d23, 20, FALSE);
    testAlloc(0x36421500, 0x44b87d23, 13700, FALSE);
    testAlloc(0x36421500, 0x38b87d23, 1, FALSE);
    testAlloc(0x36421500, 0x38b87d23, 20, FALSE);
    testAlloc(0x36421500, 0x38b87d23, 200, TRUE);
    testAlloc(0x36421500, 0x38b87d23, 13700, FALSE);
    testAlloc(0x36421500, 0x37b87d23, 13700, FALSE);
    testAlloc(0x36ef1500, 0x37b87d23, 13700, FALSE);
    testAlloc(0x36421500, 0x36b87d23, 13700, FALSE);
    testAlloc(0x36b87122, 0x36b87d23, 13700, FALSE);
    testAlloc(0x49000000, 0x4a600000, 13700, FALSE);
    testAlloc(0x9fffffff, 0xd0000000, 13700, FALSE);
    testAlloc(0x9fffffff, 0xd0000000, 67400, FALSE);
    testAlloc(0x9fffffff, 0xa0030000, 67400, FALSE);
    testAlloc(0x9fffffff, 0xa0030000, 40000, FALSE);
    testAlloc(0xa0000000, 0xa0030000, 40000, FALSE);
    testAlloc(0xa0031100, 0xa0030000, 40000, FALSE);
#if 0
#endif
    return 0;
}

#endif

#endif /* #if !UCONFIG_NO_COLLATION */
