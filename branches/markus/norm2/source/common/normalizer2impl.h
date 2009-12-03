/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2impl.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#ifndef __NORMALIZER2IMPL_H__
#define __NORMALIZER2IMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/udata.h"
#include "unicode/unistr.h"
#include "unormimp.h"

U_NAMESPACE_BEGIN

inline void assertNotBogus(const UnicodeString &s, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && s.isBogus()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    }
}

class Normalizer2Data : public UMemory {
public:
    Normalizer2Data();
    ~Normalizer2Data();
    void load(const char *packageName, const char *name, UErrorCode &errorCode);

    uint16_t getNorm16(UChar32 c) const { return UTRIE2_GET16(trie, c); }
    uint16_t getNorm16FromBMP(UChar c) const { return UTRIE2_GET16(trie, c); }
    uint16_t getNorm16FromSingleLead(UChar c) const {
        return UTRIE2_GET16_FROM_U16_SINGLE_LEAD(trie, c);
    }
    uint16_t getNorm16FromSupplementary(UChar32 c) const { return UTRIE2_GET16_FROM_SUPP(trie, c); }
    uint16_t getNorm16FromSurrogatePair(UChar c, UChar c2) const {
        return getNorm16FromSupplementary(U16_GET_SUPPLEMENTARY(c, c2));
    }

    UBool isMaybeOrNonZeroCC(uint16_t norm16) const { return norm16>=indexes[IX_MIN_MAYBE_YES]; }
    // TODO: static UBool isInert(uint16_t norm16) const { return norm16==0; }
    // TODO: static UBool isJamoL(uint16_t norm16) const { return norm16==1; }
    // TODO: static UBool isJamoVT(uint16_t norm16) const { return norm16==JAMO_VT; }
    UBool isHangul(uint16_t norm16) const { return norm16==indexes[IX_MIN_YES_NO]; }
    UBool isCompYesAndZeroCC(uint16_t norm16) const { return norm16<indexes[IX_MIN_NO_NO]; }
    // TODO: UBool isCompYesOrMaybe(uint16_t norm16) const {
    //     return norm16<indexes[IX_MIN_NO_NO] || indexes[IX_MIN_MAYBE_YES]<=norm16;
    // }
    UBool isDecompYes(uint16_t norm16) const {
        return norm16<indexes[IX_MIN_YES_NO] || indexes[IX_MIN_MAYBE_YES]<=norm16;
    }
    UBool isDecompYesAndZeroCC(uint16_t norm16) const {
        return norm16<indexes[IX_MIN_YES_NO] ||
               (indexes[IX_MIN_MAYBE_YES]<=norm16 && (norm16<=MIN_NORMAL_MAYBE_YES ||
                                                      norm16==JAMO_VT));
    }
    /**
     * A little faster and simpler than isDecompYesAndZeroCC() but does not include
     * the MaybeYes which combine-forward and have ccc=0.
     * (Standard Unicode 5.2 normalization does not have such characters.)
     */
    UBool isMostDecompYesAndZeroCC(uint16_t norm16) const {
        return norm16<indexes[IX_MIN_YES_NO] || norm16==MIN_NORMAL_MAYBE_YES || norm16==JAMO_VT;
    }
    UBool isDecompNoAlgorithmic(uint16_t norm16) const { return norm16>=indexes[IX_LIMIT_NO_NO]; }

    uint8_t getCC(uint16_t norm16) const {
        if(norm16>=MIN_NORMAL_MAYBE_YES) {
            return (uint8_t)norm16;
        }
        if(norm16<indexes[IX_MIN_NO_NO] || indexes[IX_LIMIT_NO_NO]<=norm16) {
            return 0;
        }
        return getCCFromNoNo(norm16);
    }
    uint8_t getCCFromYesOrMaybe(uint16_t norm16) const {
        return norm16>=MIN_NORMAL_MAYBE_YES ? (uint8_t)norm16 : 0;
    }

    // Requires algorithmic-NoNo.
    UChar32 mapAlgorithmic(UChar32 c, uint16_t norm16) const {
        return c+norm16-(indexes[IX_MIN_MAYBE_YES]-MAX_DELTA-1);
    }

    // Requires indexes[IX_MIN_YES_NO]<norm16<indexes[IX_LIMIT_NO_NO].
    const uint16_t *getMapping(uint16_t norm16) const { return extraData+norm16; }

    UChar32 getMinDecompNoCodePoint() const { return indexes[IX_MIN_DECOMP_NO_CP]; }
    UChar32 getMinCompNoMaybeCodePoint() const { return indexes[IX_MIN_COMP_NO_MAYBE_CP]; }
    const UTrie2 *getTrie() const { return trie; }

    enum {
        MIN_CCC_LCCC_CP=0x300
    };

    enum {
        MIN_YES_YES_WITH_CC=0xff01,
        JAMO_VT=0xff00,
        MIN_NORMAL_MAYBE_YES=0xfe00,
        MAX_DELTA=0x40
    };

    enum {
        // Byte offsets from the start of the data, after the generic header.
        IX_NORM_TRIE_OFFSET,
        IX_EXTRA_DATA_OFFSET,
        IX_FCD_TRIE_OFFSET,
        IX_RESERVED3_OFFSET,
        IX_RESERVED4_OFFSET,
        IX_RESERVED5_OFFSET,
        IX_RESERVED6_OFFSET,
        IX_RESERVED7_OFFSET,
        IX_RESERVED8_OFFSET,
        IX_TOTAL_SIZE,

        // Code point thresholds for quick check codes.
        IX_MIN_DECOMP_NO_CP,
        IX_MIN_COMP_NO_MAYBE_CP,

        // Norm16 value thresholds for quick check combinations and types of extra data.
        IX_MIN_YES_NO,
        IX_MIN_NO_NO,
        IX_LIMIT_NO_NO,
        IX_MIN_MAYBE_YES,
        IX_RESERVED16,
        IX_RESERVED17,
        IX_RESERVED18,
        IX_RESERVED19,

        IX_COUNT
    };

    enum {
        MAPPING_HAS_CCC_LCCC_WORD=0x80,
        MAPPING_PLUS_COMPOSITION_LIST=0x40,
        // 0x20 is reserved
        MAPPING_LENGTH_MASK=0x1f
    };

    enum {
        COMP_1_LAST_TUPLE=0x8000,
        COMP_1_TRIPLE=1,
        COMP_1_TRAIL_LIMIT=0x3400,
        COMP_1_TRAIL_MASK=0x7ffe,
        COMP_1_TRAIL_SHIFT=9,  // 10-1 for the "triple" bit
        COMP_2_TRAIL_SHIFT=6,
        COMP_2_TRAIL_MASK=0xffc0
    };
private:
    static UBool U_CALLCONV
    isAcceptable(void *context, const char *type, const char *name, const UDataInfo *pInfo);

    uint8_t getCCFromNoNo(uint16_t norm16) const {
        const uint16_t *mapping=getMapping(norm16);
        if(*mapping&MAPPING_HAS_CCC_LCCC_WORD) {
            return (uint8_t)mapping[1];
        } else {
            return 0;
        }
    }

    UDataMemory *memory;
    UVersionInfo dataVersion;
    int32_t indexes[IX_COUNT];  // copied not pointed to remove an indirection
    UTrie2 *trie;
    const uint16_t *maybeYesCompositions;
    const uint16_t *extraData;  // mappings and/or compositions for yesYes, yesNo & noNo characters
};

class ReorderingBuffer : public UMemory {
public:
    ReorderingBuffer(const Normalizer2Data &normData, UnicodeString &dest) :
        data(normData), str(dest),
        start(NULL), reorderStart(NULL), limit(NULL),
        remainingCapacity(0), lastCC(0) {}
    ~ReorderingBuffer() {
        if(start!=NULL) {
            str.releaseBuffer((int32_t)(limit-start));
        }
    }
    UBool init();

    UBool isEmpty() const { return start!=limit; }
    UChar *getStart() { return start; }
    UChar *getLimit() { return limit; }

    UBool append(UChar32 c, uint8_t cc) {
        return (c<=0xffff) ?
            appendBMP((UChar)c, cc) :
            appendSupplementary(c, cc);
    }
    // s must be in NFD, otherwise change the implementation.
    UBool append(const UChar *s, int32_t length, uint8_t leadCC, uint8_t trailCC);
    UBool appendZeroCC(const UChar *s, int32_t length);
    void removeZeroCCSuffix(int32_t length);
private:
    /*
     * TODO: Revisit whether it makes sense to track reorderStart.
     * It is set to after the last known character with cc<=1,
     * which stops previousCC() before it reads that character and looks up its cc.
     * previousCC() is normally only called from insert().
     * In other words, reorderStart speeds up the insertion of a combining mark
     * into a multi-combining mark sequence where it does not belong at the end.
     * This might not be worth the trouble.
     * On the other hand, it's not a huge amount of trouble.
     *
     * We probably need it for UNORM_SIMPLE_APPEND.
     */

    UBool appendBMP(UChar c, uint8_t cc) {
        if(remainingCapacity==0 && !resize(1)) {
            return FALSE;
        }
        if(lastCC<=cc || cc==0) {
            *limit++=c;
            lastCC=cc;
            if(cc<=1) {
                reorderStart=limit;
            }
        } else {
            insert(c, cc);
        }
        --remainingCapacity;
        return TRUE;
    }
    UBool appendSupplementary(UChar32 c, uint8_t cc);
    void insert(UChar32 c, uint8_t cc);
    static UChar *writeCodePoint(UChar *p, UChar32 c) {
        if(c<=0xffff) {
            *p++=(UChar)c;
        } else {
            p[0]=U16_LEAD(c);
            p[1]=U16_TRAIL(c);
            p+=2;
        }
        return p;  // TODO: necessary?
    }
    UBool resize(int32_t appendLength);

    const Normalizer2Data &data;
    UnicodeString &str;
    UChar *start, *reorderStart, *limit;
    int32_t remainingCapacity;
    uint8_t lastCC;

    // private backward iterator
    void setIterator() { codePointStart=limit; }
    void skipPrevious();  // Requires start<codePointStart.
    uint8_t previousCC();  // Returns 0 if there is no previous character.

    UChar *codePointStart, *codePointLimit;
};

class Normalizer2Impl : public UMemory {
public:
    static Normalizer2Impl *createInstance(const char *packageName,
                                           const char *name,
                                           UErrorCode &errorCode);
    static Normalizer2Impl *getNFCInstance(UErrorCode &errorCode);
    static Normalizer2Impl *getNFKCInstance(UErrorCode &errorCode);
    static Normalizer2Impl *getNFKC_CFInstance(UErrorCode &errorCode);

    void decompose(const UChar *src, int32_t srcLength,
                   UnicodeString &dest,
                   UErrorCode &errorCode) const;
    void decomposeAndAppend(const UChar *src, int32_t srcLength,
                            UnicodeString &dest,
                            UBool doDecompose,
                            UErrorCode &errorCode) const;
    void compose(const UChar *src, int32_t srcLength,
                 UnicodeString &dest,
                 UErrorCode &errorCode) const;
    void composeAndAppend(const UChar *src, int32_t srcLength,
                          UnicodeString &dest,
                          UBool doCompose,
                          UErrorCode &errorCode) const;
private:
    Normalizer2Impl() {}

    UBool decompose(const UChar *src, int32_t srcLength, ReorderingBuffer &buffer) const;
    UBool decompose(UChar32 c, uint16_t norm16, ReorderingBuffer &buffer) const;
    UBool compose(const UChar *src, int32_t srcLength, ReorderingBuffer &buffer) const;

    /**
     * Is c a composition starter?
     * True if its decomposition begins with a character that has
     * ccc=0 && NFC_QC=Yes (isCompYesAndZeroCC()).
     * As a shortcut, this is true if c itself has ccc=0 && NFC_QC=Yes
     * (isCompYesAndZeroCC()) so we need not decompose.
     */
    UBool isCompStarter(UChar32 c, uint16_t norm16) const;
    const UChar *findPreviousCompStarter(const UChar *start, const UChar *p) const;
    const UChar *findNextCompStarter(const UChar *p, const UChar *limit) const;

    Normalizer2Data data;
};

class DecomposeNormalizer2 : public Normalizer2 {
public:
    DecomposeNormalizer2(const Normalizer2Impl &ni) : Normalizer2(ni) {}

    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const;
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const;
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const;
};

class ComposeNormalizer2 : public Normalizer2 {
public:
    ComposeNormalizer2(const Normalizer2Impl &ni) : Normalizer2(ni) {}

    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const;
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const;
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION
#endif  // __NORMALIZER2IMPL_H__
