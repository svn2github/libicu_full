/*
*******************************************************************************
*
*   Copyright (C) 1999, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  store.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999dec11
*   created by: Markus W. Scherer
*
*   Store Unicode character properties efficiently for
*   random access.
*/

#include <stdio.h>
#include <stdlib.h>
#include "utypes.h"
#include "uchar.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "udata.h"
#include "unewdata.h"
#include "genprops.h"

/* ### */
#define DO_DEBUG_OUT 0

/* Unicode character properties file format ------------------------------------

The file format prepared and written here contains several data
structures that store indexes or data.

The contents is a parsed, binary form of several Unicode character
database files, mose prominently UnicodeData.txt.

Any Unicode code point from 0 to 0x10ffff can be looked up to get
the properties, if any, for that code point. This means that the input
to the lookup are 21-bit unsigned integers, with not all of the
21-bit range used.

It is assumed that client code keeps a uint16_t pointer
to the beginning of the data:

    const uint16 *p16;

Some indexes assume 32-bit units; although client code should only
cast the above pointer to (const uint32_t *), it is easier here
to talk about the result of the indexing with the definition of
another pointer variable for this:

    const uint32_t *p32=(const uint32_t *)p16;

Formally, the file contains the following structures:

    A const uint16_t exceptionsIndex; -- 32-bit index
    B const uint16_t ucharsIndex; -- 32-bit index
    C const uint16_t reservedIndex;
    D const uint16_t reservedIndex;

    E const uint16_t stage1[0x440]; -- 0x440=0x110000>>10
    F const uint16_t stage2[variable];
    G const uint16_t stage3[variable];
      (possible 1*uint16_t for padding to 4-alignment)

    H const uint32_t props32[variable];
    I const uint16_t exceptions[variable];
      (possible 1*uint16_t for padding to 4-alignment)

    J const UChar uchars[variable];

3-stage lookup and properties:

In order to condense the data for the 21-bit code space, several properties of
the Unicode code assignment are exploited:
- The code space is sparse.
- There are several 10k of consecutive codes with the same properties.
- Characters and scripts are allocated in groups of 16 code points.
- Inside blocks for scripts the properties are often repetitive.
- The 21-bit space is not fully used for Unicode.

The three-stage lookup organizes code points in groups of 16 in stage 3.
64 such groups are grouped again, resulting in blocks of 1k in stage 2.
The first stage is limited according to all code points being <0x110000.
Each stage contains indexes to groups or blocks of the next stage
in an n:1 manner, i.e., multiple entries of one stage may index the same
group or block in the next one.
In the second and third stages, groups of 64 or 16 may partially or completely
overlap to save space with repetitive properties.
In the properties table, only unique 32-bit words are stored to exploit
non-adjacent overlapping. This is why the third stage does not directly
contain the 32-bit properties words but only indexes to them.

The indexes in each stage take the offset in the data of the next block into
account to save additional arithmetic in the access.

With a given Unicode code point

    uint32_t c;

and 0<=c<0x110000, the lookup uses the three stage tables to
arrive at an index into the props32[] table containing the character
properties for c.
For some characters, not all of the properties can be efficiently encoded
using 32 bits. For them, the 32-bit word contains an index into the exceptions[]
array. Some exception entries, in turn, may contain indexes into the uchars[]
array of Unicode strings, especially for non-1:1 case mappings.

The first stage consumes the 11 most significant bits of the 21-bit code point
and results in an index into the second stage:

    uint16_t i2=p16[4+c>>10];

The second stage consumes bits 9 to 4 of c and results in an index into the
third stage:

    uint16_t i3=p16[i2+((c>>4)&0x3f)];

The third stage consumes bits 3 to 0 of c and results in a code point-
specific value, which itself is only an index into the props32[] table:

    uint16_t i=p16[i3+(c&0xf)];

There is finally the 32-bit encoded set of properties for c:

    uint32_t props=p32[i];

For some characters, this contains an index into the exceptions array:

    if(props&0x20) {
        uint16_t e=(uint16_t)(props>>20);
        ...
    }

The exception values are a variable number of uint16_t starting at

    const uint16_t *pe=p16+2*p16[0]+e;

The first uint16_t there contains flags about what values actually follow it.
Some of those may be indexes for case mappings or similar and point to strings
(zero-terminated) in the uchars[] array:

    ...
    uint16_t u=pe[depends on pe[0]];
    const UChar *pu=(const UChar *)(p32+p16[1])+u;

32-bit properties sets:

Each 32-bit properties word contains:

 0.. 4  general category
 5      has exception values
 6.. 9  BiDi category (the 5 explicit codes stored as one)
10      is mirrored
11..19  reserved
20..31  value according to bits 0..5:
        if(has exception) {
            exception index;
        } else switch(general category) {
        case Ll: delta to uppercase; -- same as titlecase
        case Lu: delta to lowercase; -- titlecase is same as c
        case Lt: delta to lowercase; -- uppercase is same as c
        case Mn: canonical category;
        case N*: numeric value;
        default: *;
        }

----------------------------------------------------------------------------- */

/* UDataInfo cf. udata.h */
static const UDataInfo dataInfo={
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    U_SIZEOF_UCHAR,
    0,

    0x55, 0x50, 0x72, 0x6f,     /* dataFormat="UPro" */
    1, 0, 0, 0,                 /* formatVersion */
    3, 0, 0, 0                  /* dataVersion */
};

/* definitions and arrays for the 3-stage lookup */
enum {
    STAGE_2_BITS=6, STAGE_3_BITS=4,
    STAGE_1_BITS=21-(STAGE_2_BITS+STAGE_3_BITS),

    STAGE_2_SHIFT=STAGE_3_BITS,
    STAGE_1_SHIFT=(STAGE_2_SHIFT+STAGE_2_BITS),

    /* number of entries per sub-table in each stage */
    STAGE_1_BLOCK=0x110000>>STAGE_1_SHIFT,
    STAGE_2_BLOCK=1<<STAGE_2_BITS,
    STAGE_3_BLOCK=1<<STAGE_3_BITS,

    /* number of code points per stage 1 index */
    STAGE_2_3_AREA=1<<STAGE_1_SHIFT,

    MAX_PROPS_COUNT=25000,
    MAX_UCHAR_COUNT=10000,
    MAX_EXCEPTIONS_COUNT=4096,
    MAX_STAGE_2_COUNT=MAX_PROPS_COUNT
};

static uint16_t stage1[STAGE_1_BLOCK], stage2[MAX_STAGE_2_COUNT],
                stage3[MAX_PROPS_COUNT], map[MAX_PROPS_COUNT];

/* stage1Top=STAGE_1_BLOCK never changes, stage2Top starts after the empty-properties-group */
static uint16_t stage2Top=STAGE_2_BLOCK, stage3Top;

/* props[] is used before, props32[] after compacting the array of properties */
static uint32_t props[MAX_PROPS_COUNT], props32[MAX_PROPS_COUNT];
static uint16_t propsTop=STAGE_3_BLOCK; /* the first props[] are always empty */

/* exceptions values */
static uint16_t exceptions[MAX_EXCEPTIONS_COUNT+20];
static uint16_t exceptionsTop=0;

/* Unicode characters, e.g. for special casing or decomposition */

static UChar uchars[MAX_UCHAR_COUNT+20];
static uint16_t ucharsTop=0;

/* statistics */

static uint16_t exceptionsCount=0;

/* prototypes --------------------------------------------------------------- */

static uint16_t
repeatFromStage2(uint16_t i2, uint16_t i2Limit, uint16_t i3Repeat, uint32_t x);

static int
compareProps(const void *l, const void *r);

static uint32_t
getProps2(uint32_t c, uint16_t *pI1, uint16_t *pI2, uint16_t *pI3, uint16_t *pI4);

static uint32_t
getProps(uint32_t c, uint16_t *pI1, uint16_t *pI2, uint16_t *pI3);

static void
setProps(uint32_t c, uint32_t x, uint16_t *pI1, uint16_t *pI2, uint16_t *pI3);

static uint16_t
allocStage2();

static uint16_t
allocProps();

static uint16_t
addUChars(const UChar *s, uint16_t length);

/* -------------------------------------------------------------------------- */

extern void
initStore() {
    icu_memset(stage1, 0, sizeof(stage1));
    icu_memset(stage2, 0, sizeof(stage2));
    icu_memset(stage3, 0, sizeof(stage3));
    icu_memset(map, 0, sizeof(map));
    icu_memset(props32, 0, sizeof(props32));
}

/* store a character's properties ------------------------------------------- */

extern void
addProps(Props *p) {
    /* map the explicit BiDi codes to one single value */
    static const uint8_t bidiMap[U_CHAR_DIRECTION_COUNT]={
	    0, 1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 15, 15, 11, 15, 15, 15, 12, 13
    };

    uint32_t x;
    int32_t value;
    uint16_t count;
    bool_t isMn, isNumber;

    /*
     * Simple ideas for reducing the number of bits for one character's
     * properties:
     *
     * Some fields are only used for characters of certain
     * general categories:
     * - casing fields for letters and others, not for
     *     numbers & Mn
     *   + uppercase not for uppercase letters
     *   + lowercase not for lowercase letters
     *   + titlecase not for titlecase letters
     *
     *   * most of the time, uppercase=titlecase
     * - numeric fields for various digit & other types
     * - canonical combining classes for non-spacing marks (Mn)
     * * the above is not always true, for all three cases
     *
     * Using the same bits for alternate fields saves some space.
     *
     * For the canonical categories, there are only few actually used.
     * They can be stored using 5 bits.
     *
     * In the BiDi categories, the 5 explicit codes are only ever
     * assigned 1:1 to 5 well-known code points. Storing only one
     * value for all "explicit codes" gets this down to 4 bits.
     * Client code then needs to check for this special value
     * and replace it by the real one using a 5-element table.
     *
     * The general categories Mn & Me, non-spacing & enclosing marks,
     * are always NSM, and NSM are always of those categories.
     *
     * Digit values can often be derived from the code point value
     * itself in a simple way.
     *
     */

    /* count the case mappings and other values competing for the value bit field */
    x=0;
    value=0;
    count=0;
    isMn= p->generalCategory==U_NON_SPACING_MARK;
    isNumber= genCategoryNames[p->generalCategory][0]=='N';

    if(p->upperCase!=0) {
        /* verify that no numbers and no Mn have case mappings */
        if(!(isMn || isNumber)) {
            value=(int32_t)p->code-(int32_t)p->upperCase;
        } else {
            x=1<<5;
        }
        ++count;
    }
    if(p->lowerCase!=0) {
        /* verify that no numbers and no Mn have case mappings */
        if(!(isMn || isNumber)) {
            value=(int32_t)p->lowerCase-(int32_t)p->code;
        } else {
            x=1<<5;
        }
        ++count;
    }
    if(p->upperCase!=p->titleCase) {
        /* verify that no numbers and no Mn have case mappings */
        if(!(isMn || isNumber)) {
            value=(int32_t)p->code-(int32_t)p->titleCase;
        } else {
            x=1<<5;
        }
        ++count;
    }
    if(p->canonicalCombining>0) {
        /* verify that only Mn has a canonical combining class */
        if(isMn) {
            value=p->canonicalCombining;
        } else {
            x=1<<5;
        }
        ++count;
    }
    if(p->numericValue!=0) {
        /* verify that only numeric categories have numeric values */
        if(isNumber) {
            value=p->numericValue;
        } else {
            x=1<<5;
        }
        ++count;
    }
    if(p->denominator!=0) {
        /* verification for numeric category covered by the above */
        value=p->denominator;
        ++count;
    }

    if(count>1 || x!=0 || value<-2048 || 2047<value) {
        /* this code point needs exception values */
        if(DO_DEBUG_OUT /* ### beVerbose */) {
            if(x!=0) {
                printf("*** code 0x%06x needs an exception because it is irregular\n", p->code);
            } else if(count==1) {
                printf("*** code 0x%06x needs an exception because its value would be %ld\n", p->code, value);
            } else {
                printf("*** code 0x%06x needs an exception because it has %u values\n", p->code, count);
            }
        }

        ++exceptionsCount;
        x=1<<5;

        /* ### allocate and create exception values */
        value=-exceptionsCount;
    } else {
    }

    x|=
        p->generalCategory |
        bidiMap[p->bidi]<<6UL |
        p->isMirrored<<10UL |
        (uint32_t)value<<20;

    setProps(p->code, x, &count, &count, &count);

#if 0
    /* verify that no numbers and no Mn have case mappings */
    /* this is not 100% true either (see 0345;COMBINING GREEK YPOGEGRAMMENI) */
    if( (genCategoryNames[p->generalCategory][0]=='N' ||
         p->generalCategory==U_NON_SPACING_MARK) &&
        count>0
    ) {
        printf("*** code 0x%06x: number category or Mn but case mapping\n", p->code);
    } else if(count>1) {
        /* see for which characters there are two case mappings */
        /* there are some, but few (12) */
        printf("*** code 0x%06x: more than one case mapping\n", p->code);
    }

    /* verify that { Mn, Me } if and only if NSM */
    if( (p->generalCategory==U_NON_SPACING_MARK ||
         p->generalCategory==U_ENCLOSING_MARK)
        ^
        (p->bidi==U_DIR_NON_SPACING_MARK)) {
        printf("*** code 0x%06x: bidi class does not fit expected range ***\n", p->code);
    }

    /*
     * "Higher-hanging fruit":
     * For some sets of fields, there are fewer sets of values
     * than the product of the numbers of values per field.
     * This means that storing one single value for more than
     * one field and later looking up both field values in a table
     * saves space.
     * Examples:
     * - general category & BiDi
     *
     * There are only few common displacements between a code point
     * and its case mappings. Store deltas. Store codes for few
     * occuring deltas.
     */
#endif
}

/* areas of same properties ------------------------------------------------- */

extern void
repeatProps() {
    /* first and last code points in repetitive areas */
    static const uint32_t areas[][2]={
        { 0x3400, 0x4db5 },     /* CJK ext. A */
        { 0x4e00, 0x9fa5 },     /* CJK */
        { 0xac00, 0xd7a3 },     /* Hangul */
        { 0xd800, 0xdb7f },     /* High Surrogates */
        { 0xdb80, 0xdbff },     /* Private Use High Surrogates */
        { 0xdc00, 0xdfff },     /* Low Surrogates */
        { 0xe000, 0xf8ff },     /* Private Use */
        { 0xf0000, 0x10ffff }   /* Private Use */
    };

    /*
     * Set the repetitive properties for the big, known areas of all the same
     * character properties. Most of those will share the same stage 2 and 3
     * tables.
     *
     * Assumptions:
     * - each area starts at a code point that is a multiple of 16
     * - for each area, except the plane 15/16 private use one,
     *   the first and last code points are defined in UnicodeData.txt
     *   and thus already set
     * - there may be some properties already stored for some code points,
     *   especially in the Private Use areas
     */

    uint16_t i1, i2, i3, j3, i1Limit, i2Repeat, i3Repeat, area;
    uint32_t x, start, limit;

    /* set the properties for the plane 15/16 Private Use area */
    setProps(0xf0000, getProps(0xe000, &i1, &i2, &i3), &i1, &i2, &i3);

    /* fill in the repetitive properties */
    for(area=0; area<sizeof(areas)/sizeof(areas[0]); ++area) {
        start=areas[area][0];
        limit=areas[area][1]+1;

        /* get the properties from the preset first code point */
        x=getProps(start, &i1, &i2, &i3);

        /* i1, i2, and i3 are set for the start code point */
        /* assume that i3 is the beginning of a stage 3 block (see assumptions above) */

        /* is this stage 3 block suitable for setting it everywhere? (set i3Repeat) */
        for(j3=1;;) {
            if(!(props[i3+j3]==0 || props[i3+j3]==x)) {
                /* this block contains different properties, we need a new one */
                i3Repeat=allocProps();
                break;
            }
            if(++j3==STAGE_3_BLOCK) {
                /* this block is good */
                i3Repeat=i3;
                break;
            }
        }

        /* fill the repetitive block */
        for(j3=0; j3<STAGE_3_BLOCK; ++j3) {
            props[i3Repeat+j3]=x;
        }

        /*
         * now there are up to three sub-areas:
         * - a range of code points before the first full block for
         *   one stage 1 index
         * - a (big) range of code points within full blocks for
         *   stage 1 indexes
         * - a range of code points after the last full block for
         *   one stage 1 index
         * - if the start of an area is not the beginning of a full block,
         *   then the end of the area is at least the last code point
         *   in this same block
         */

        if((start&(STAGE_2_3_AREA-1))!=0) {
            /*
             * fill the area before the first full block;
             * assume that the start value is set and therefore
             * at least stage 2 is allocated
             */

            /* fill stages 2 & 3 */
            i2=repeatFromStage2(i2, (uint16_t)((i2+STAGE_2_BLOCK)&~(STAGE_2_BLOCK-1)), i3Repeat, x);

            /* this stage 2 block will not be suitable for repetition */
            i2Repeat=0;

            /* advance i1 to the first full block */
            ++i1;
        } else {
            uint16_t j2;

            /* is this stage 2 block suitable for setting it everywhere? (set i2Repeat) */
            for(j2=0;;) {
                if(!(stage2[i2+j2]==0 || stage2[i2+j2]==i3Repeat)) {
                    /* this block contains different indexes, we will need a new one */
                    i2Repeat=0;
                    break;
                }
                if(++j2==STAGE_2_BLOCK) {
                    /* this block is good, set and fill it */
                    i2Repeat=i2;
                    for(j2=0; j2<STAGE_2_BLOCK; ++j2) {
                        stage2[i2Repeat+j2]=i3Repeat;
                    }
                    break;
                }
            }
        }

        i1Limit=(uint16_t)(limit>>STAGE_1_SHIFT);
        if(i1<i1Limit) {
            /* fill whole blocks for stage 1 indexes */

            /* fill all the stages 1 to 3 */
            do {
                i2=stage1[i1];
                if(i2==0) {
                    /* set the index for common repeat block for stage 2 */
                    if(i2Repeat==0) {
                        /* allocate and fill a stage 2 block for this */
                        uint16_t j2;

                        i2Repeat=allocStage2();
                        for(j2=0; j2<STAGE_2_BLOCK; ++j2) {
                            stage2[i2Repeat+j2]=i3Repeat;
                        }
                    }
                    stage1[i1]=i2Repeat;
                } else {
                    /* some properties are set in this stage 2 block */
                    repeatFromStage2(i2, (uint16_t)(i2+STAGE_2_BLOCK), i3Repeat, x);
                }
            } while(++i1<i1Limit);
        }

        if((limit&(STAGE_2_3_AREA-1))!=0) {
            /* fill the area after the last full block */
            i2=stage1[i1];
            if(i2==0) {
                i2=allocStage2();
            }
            i2=repeatFromStage2(i2, (uint16_t)(i2+((limit>>STAGE_2_SHIFT)&(STAGE_2_BLOCK-1))), i3Repeat, x);

            /* does this area end in an incomplete stage 3 block? */
            j3=(uint16_t)(limit&(STAGE_3_BLOCK-1));
            if(j3!=0) {
                /* fill in properties in a last, incomplete stage 3 block */
                i3=stage2[i2];
                if(i3==0) {
                    stage2[i2]=i3=allocProps();
                }

                /* some properties are set in this stage 3 block */
                do {
                    if(props[i3]==0) {
                        props[i3]=x;
                    }
                    ++i3;
                } while(--j3>0);
            }
        }
    }
}

static uint16_t
repeatFromStage2(uint16_t i2, uint16_t i2Limit, uint16_t i3Repeat, uint32_t x) {
    /* set a section of a stage 2 table and its properties to x */
    uint16_t i3, j3;

    while(i2<i2Limit) {
        i3=stage2[i2];
        if(i3==0) {
            stage2[i2]=i3Repeat;
        } else {
            /* some properties are set in this stage 3 block */
            j3=STAGE_3_BLOCK;
            do {
                if(props[i3]==0) {
                    props[i3]=x;
                }
                ++i3;
            } while(--j3>0);
        }
        ++i2;
    }
    return i2;
}

/* compacting --------------------------------------------------------------- */

extern void
compactStage2() {
    /*
     * At this point, there are stage2Top indexes in stage2[].
     * stage2Top is a multiple of 64, and there are always 64 stage2[] entries
     * per stage 1 entry which do not overlap.
     * The first 64 stage2[] are always the empty ones.
     * We make them overlap appropriately here and fill every 64th entry in
     * map[] with the mapping from old to new properties indexes
     * in order to adjust the stage 1 tables.
     * This simple algorithm does not find arbitrary overlaps, but only those
     * where the last i indexes of the previous group and the first i of the
     * current one all have the same value.
     * This seems reasonable and yields linear performance.
     */
    uint16_t i, start, prevEnd, newStart, x;

    map[0]=0;
    newStart=STAGE_2_BLOCK;
    for(start=newStart; start<stage2Top;) {
        prevEnd=newStart-1;
        x=stage2[start];
        if(x==stage2[prevEnd]) {
            /* overlap by at least one */
            for(i=1; i<STAGE_2_BLOCK && x==stage2[start+i] && x==stage2[prevEnd-i]; ++i) {}

            /* overlap by i */
            map[start]=newStart-i;

            /* move the non-overlapping properties to their new positions */
            start+=i;
            for(i=STAGE_2_BLOCK-i; i>0; --i) {
                stage2[newStart++]=stage2[start++];
            }
        } else if(newStart<start) {
            /* move the properties to their new positions */
            map[start]=newStart;
            for(i=STAGE_2_BLOCK; i>0; --i) {
                stage2[newStart++]=stage2[start++];
            }
        } else /* no overlap && newStart==start */ {
            map[start]=start;
            newStart+=STAGE_2_BLOCK;
            start=newStart;
        }
    }

    /* we saved some space */
    if(beVerbose) {
        printf("compactStage2() reduced stage2Top from %u to %u\n", stage2Top, stage2Top-(start-newStart));
    }
    stage2Top-=(start-newStart);

    /* now adjust the stage 1 table */
    for(start=0; start<STAGE_1_BLOCK; ++start) {
        stage1[start]=map[stage1[start]];
    }
    if(DO_DEBUG_OUT) {
        /* ### debug output */
        uint16_t i1, i2, i3, i4;
        uint32_t c;
        for(c=0; c<0xffff; c+=307) {
            printf("properties(0x%06x)=0x%06x\n", c, getProps2(c, &i1, &i2, &i3, &i4));
        }
    }
}

extern void
compactStage3() {
    /*
     * At this point, there are stage3Top properties indexes in stage3[].
     * stage3Top is a multiple of 16, and there are always 16 stage3[] entries
     * per stage 2 entry which do not overlap.
     * The first 16 stage3[] are always the empty ones.
     * We make them overlap appropriately here and fill every 16th entry in
     * map[] with the mapping from old to new properties indexes
     * in order to adjust the stage 2 tables.
     * This simple algorithm does not find arbitrary overlaps, but only those
     * where the last i properties of the previous group and the first i of the
     * current one all have the same value.
     * This seems reasonable and yields linear performance.
     */
    uint16_t i, start, prevEnd, newStart, x;

    map[0]=0;
    newStart=STAGE_3_BLOCK;
    for(start=newStart; start<stage3Top;) {
        prevEnd=newStart-1;
        x=stage3[start];
        if(x==stage3[prevEnd]) {
            /* overlap by at least one */
            for(i=1; i<STAGE_3_BLOCK && x==stage3[start+i] && x==stage3[prevEnd-i]; ++i) {}

            /* overlap by i */
            map[start]=newStart-i;

            /* move the non-overlapping properties to their new positions */
            start+=i;
            for(i=STAGE_3_BLOCK-i; i>0; --i) {
                stage3[newStart++]=stage3[start++];
            }
        } else if(newStart<start) {
            /* move the properties to their new positions */
            map[start]=newStart;
            for(i=STAGE_3_BLOCK; i>0; --i) {
                stage3[newStart++]=stage3[start++];
            }
        } else /* no overlap && newStart==start */ {
            map[start]=start;
            newStart+=STAGE_3_BLOCK;
            start=newStart;
        }
    }

    /* we saved some space */
    if(beVerbose) {
        printf("compactStage3() reduced stage3Top from %u to %u\n", stage3Top, stage3Top-(start-newStart));
    }
    stage3Top-=(start-newStart);

    /* now adjust the stage 2 tables */
    for(start=0; start<stage2Top; ++start) {
        stage2[start]=map[stage2[start]];
    }
    if(DO_DEBUG_OUT) {
        /* ### debug output */
        uint16_t i1, i2, i3, i4;
        uint32_t c;
        for(c=0; c<0xffff; c+=307) {
            printf("properties(0x%06x)=0x%06x\n", c, getProps2(c, &i1, &i2, &i3, &i4));
        }
    }
}

extern void
compactProps() {
    /*
     * At this point, all the propsTop properties are in props[], but they
     * are not all unique.
     * Now we sort them, reduce them to unique ones in props32[], and
     * build an index in stage3[] from the old to the new indexes.
     * (The quick sort averages at N*log(N) with N=propsTop. The inverting
     * yields linear performance.)
     */

    /*
     * We are going to sort only an index table in map[] because we need this
     * index table anyway and qsort() does not allow to sort two tables together
     * directly. This will thus also reduce the amount of data moved around.
     */
    uint16_t i, oldIndex, newIndex;
    uint32_t x;
    if(DO_DEBUG_OUT) {
        /* ### debug output */
        uint16_t i1, i2, i3;
        uint32_t c;
        for(c=0; c<0xffff; c+=307) {
            printf("properties(0x%06x)=0x%06x\n", c, getProps(c, &i1, &i2, &i3));
        }
    }

    /* build the index table */
    for(i=propsTop; i>0;) {
        --i;
        map[i]=i;
    }

    /* do not reorder the first, empty entries */
    qsort(map+STAGE_3_BLOCK, propsTop-STAGE_3_BLOCK, 2, compareProps);

    /*
     * Now invert the reordered table and compact it in the same step.
     * The result will be props32[] having only unique properties words
     * and stage3[] having indexes to them.
     */
    newIndex=0;
    for(i=0; i<propsTop;) {
        /* set the first of a possible series of the same properties */
        oldIndex=map[i];
        props32[newIndex]=x=props[oldIndex];
        stage3[oldIndex]=newIndex;

        /* set the following same properties only in stage3 */
        while(++i<propsTop && x==props[map[i]]) {
            stage3[map[i]]=newIndex;
        }

        ++newIndex;
    }

    /* we saved some space */
    stage3Top=propsTop;
    propsTop=newIndex;
    if(beVerbose) {
        printf("compactProps() reduced propsTop from %u to %u\n", stage3Top, propsTop);
    }
    if(DO_DEBUG_OUT) {
        /* ### debug output */
        uint16_t i1, i2, i3, i4;
        uint32_t c;
        for(c=0; c<0xffff; c+=307) {
            printf("properties(0x%06x)=0x%06x\n", c, getProps2(c, &i1, &i2, &i3, &i4));
        }
    }
}

static int
compareProps(const void *l, const void *r) {
    uint32_t left=props[*(const uint16_t *)l], right=props[*(const uint16_t *)r];

    /* compare general categories first */
    int rc=(int)(left&0x1f)-(int)(right&0x1f);
    if(rc==0 && left!=right) {
        rc= left<right ? -1 : 1;
    }
    return rc;
}

/* generate output data ----------------------------------------------------- */

extern void
generateData() {
#if 0
    UNewDataMemory *pData;
    UErrorCode errorCode=U_ZERO_ERROR;
    uint32_t size;
    long dataLength;

    pData=udata_create(DATA_TYPE, DATA_NAME, &dataInfo,
                       haveCopyright ? U_COPYRIGHT_STRING : NULL, &errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "genprops: unable to create data memory, error %d\n", errorCode);
        exit(errorCode);
    }

    /* ### write the data */
    size=0;

    /* finish up */
    dataLength=udata_finish(pData, &errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "genprops: error %d writing the output file\n", errorCode);
        exit(errorCode);
    }

    if(dataLength!=(long)size) {
        fprintf(stderr, "genprops: data length %ld != calculated size %lu\n", dataLength, size);
        exit(U_INTERNAL_PROGRAM_ERROR);
    }
#endif

    if(beVerbose) {
        printf("number of code points with exceptions: %u\n", exceptionsCount);
        printf("data size: %lu\n",
            8+sizeof(stage1)+2*stage2Top+2*stage3Top+2*((stage2Top+stage3Top)&1)+4*propsTop);
    }
}

/* helpers ------------------------------------------------------------------ */

/* get properties after compacting them */
static uint32_t
getProps2(uint32_t c, uint16_t *pI1, uint16_t *pI2, uint16_t *pI3, uint16_t *pI4) {
    uint16_t i1, i2, i3, i4;

    *pI1=i1=(uint16_t)(c>>STAGE_1_SHIFT);
    *pI2=i2=stage1[i1]+(uint16_t)((c>>STAGE_2_SHIFT)&(STAGE_2_BLOCK-1));
    *pI3=i3=stage2[i2]+(uint16_t)(c&(STAGE_3_BLOCK-1));
    *pI4=i4=stage3[i3];
    return props32[i4];
}

/* get properties before compacting them */
static uint32_t
getProps(uint32_t c, uint16_t *pI1, uint16_t *pI2, uint16_t *pI3) {
    uint16_t i1, i2, i3;

    *pI1=i1=(uint16_t)(c>>STAGE_1_SHIFT);
    *pI2=i2=stage1[i1]+(uint16_t)((c>>STAGE_2_SHIFT)&(STAGE_2_BLOCK-1));
    *pI3=i3=stage2[i2]+(uint16_t)(c&(STAGE_3_BLOCK-1));
    return props[i3];
}

/* set properties before compacting them */
static void
setProps(uint32_t c, uint32_t x, uint16_t *pI1, uint16_t *pI2, uint16_t *pI3) {
    uint16_t i1, i2, i3;

    *pI1=i1=(uint16_t)(c>>STAGE_1_SHIFT);

    i2=stage1[i1];
    if(i2==0) {
        stage1[i1]=i2=allocStage2();
    }
    *pI2=i2+=(uint16_t)((c>>STAGE_2_SHIFT)&(STAGE_2_BLOCK-1));

    i3=stage2[i2];
    if(i3==0) {
        stage2[i2]=i3=allocProps();
    }
    *pI3=i3+=(uint16_t)(c&(STAGE_3_BLOCK-1));

    props[i3]=x;
}

static uint16_t
allocStage2() {
    uint16_t i=stage2Top;
    stage2Top+=STAGE_2_BLOCK;
    if(stage2Top>=MAX_STAGE_2_COUNT) {
        fprintf(stderr, "genprops: stage 2 overflow\n");
        exit(U_MEMORY_ALLOCATION_ERROR);
    }
    return i;
}

static uint16_t
allocProps() {
    uint16_t i=propsTop;
    propsTop+=STAGE_3_BLOCK;
    if(propsTop>=MAX_PROPS_COUNT) {
        fprintf(stderr, "genprops: properties overflow\n");
        exit(U_MEMORY_ALLOCATION_ERROR);
    }
    return i;
}

static uint16_t
addUChars(const UChar *s, uint16_t length) {
    uint16_t top=ucharsTop+length+1;
    UChar *p;

    if(top>=MAX_UCHAR_COUNT) {
        fprintf(stderr, "genprops: out of UChars memory\n");
        exit(U_MEMORY_ALLOCATION_ERROR);
    }
    p=uchars+ucharsTop;
    icu_memcpy(p, s, length);
    p[length]=0;
    ucharsTop=top;
    return (uint16_t)(p-uchars);
}
