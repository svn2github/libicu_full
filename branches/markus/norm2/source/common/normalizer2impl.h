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

#if !UCONFIG_NO_NORMALIZATION && defined(XP_CPLUSPLUS)

#include "unicode/normalizer2.h"
#include "unicode/udata.h"
#include "unicode/unistr.h"
#include "unormimp.h"

U_NAMESPACE_BEGIN

class NormalizerData : public UMemory {
public:
    // TODO: load() & unload()
    uint16_t getNorm16(UChar32 c) const { return UTRIE2_GET16(&trie, c); }
    uint16_t getNorm16FromBMP(UChar c) const { return UTRIE2_GET16(&trie, c); }
    uint16_t getNorm16FromSingleLead(UChar c) const { return UTRIE2_GET16_FROM_U16_SINGLE_LEAD(&trie, c); }
    uint16_t getNorm16FromSupplementary(UChar32 c) const { return UTRIE2_GET16_FROM_SUPP(&trie, c); }
    uint16_t getNorm16FromSurrogatePair(UChar c, UChar c2) const {
        return getNorm16FromSupplementary(U16_GET_SUPPLEMENTARY(c, c2));
    }

    UBool isMaybeOrNonZeroCC(uint16_t norm16) { return norm16>=minMaybeYes; }
    // TODO: static UBool isInert(uint16_t norm16) { return norm16==0; }
    // TODO: static UBool isJamoL(uint16_t norm16) { return norm16==1; }
    // TODO: static UBool isJamoVT(uint16_t norm16) { return norm16==MIN_YES_YES_WITH_CC; }
    UBool isHangul(uint16_t norm16) const { return norm16==minYesNo; }
    UBool isCompYesAndZeroCC(uint16_t norm16) const { return norm16<minNoNo; }
    // TODO: UBool isCompYesOrMaybe(uint16_t norm16) const { return norm16<minNoNo || minMaybeYes<=norm16; }
    UBool isDecompYes(uint16_t norm16) const { return norm16<minYesNo || minMaybeYes<=norm16; }
    // TODO: UBool isDecompYesAndZeroCC(uint16_t norm16) const {
    //     return norm16<minYesNo ||
    //            (minMaybeYes<=norm16 && (norm16<=MIN_NORMAL_MAYBE_YES ||
    //                                     norm16==MIN_YES_YES_WITH_CC));
    // }
    /**
     * A little faster and simpler than isDecompYesAndZeroCC() but does not include
     * the MaybeYes which combine-forward and have ccc=0.
     * (Standard Unicode 5.2 normalization does not have such characters.)
     */
    UBool isMostDecompYesAndZeroCC(uint16_t norm16) const {
        return norm16<minYesNo || norm16==MIN_NORMAL_MAYBE_YES || norm16==MIN_YES_YES_WITH_CC;
    }
    UBool isNoNoAlgorithmic(uint16_t norm16) const { return norm16>=limitNoNo; }

    uint8_t getCC(uint16_t norm16) const {
        if(norm16>=MIN_NORMAL_MAYBE_YES) {
            return (uint8_t)norm16;
        }
        if(norm16<minNoNo || limitNoNo<=norm16) {
            return 0;
        }
        return getCCFromNoNo(norm16);
    }
    uint8_t getCCFromYesOrMaybe(uint16_t norm16) const {
        return norm16>=MIN_NORMAL_MAYBE_YES ? (uint8_t)norm16 : 0;
    }

    // Requires algorithmic-NoNo.
    UChar32 mapAlgorithmic(UChar32 c, uint16_t norm16) const {
        return c+norm16-(minMaybeYes-MAX_DELTA-1);
    }

    // Requires minYesNo<norm16<limitNoNo.
    const uint16_t *getMapping(uint16_t norm16) const { return mappings+norm16; }

    UChar getMinDecompNoCodePoint() const { return minDecompNoCP; }
    const UTrie2 &getTrie() const { return trie; }

    enum {
        MIN_YES_YES_WITH_CC=0xff00,
        MIN_NORMAL_MAYBE_YES=0xfe00,
        MAX_DELTA=0x20
    };

    enum {
        MAPPING_HAS_CCC_LCCC_WORD=0x80,
        MAPPING_PLUS_COMPOSITION_LIST=0x40,
        // 0x20 is reserved
        MAPPING_LENGTH_MASK=0x1f
    };
private:
    uint8_t getCCFromNoNo(uint16_t norm16) const {
        const uint16_t *mapping=getMapping(norm16);
        if(*mapping&MAPPING_HAS_CCC_LCCC_WORD) {
            return (uint8_t)mapping[1];
        } else {
            return 0;
        }
    }

    UTrie2 trie;
    const int32_t *indexes;
    const uint16_t *compositions;
    const uint16_t *mappings;
    UDataMemory *memory;

    UChar minDecompNoCP;

    uint16_t minYesNo, minNoNo, minMaybeYes, limitNoNo;
};

class ReorderingBuffer : public UMemory {
public:
    ReorderingBuffer(const NormalizerData &normData, UnicodeString &dest) :
        data(normData), str(dest),
        start(NULL), reorderStart(NULL), limit(NULL),
        remainingCapacity(0), lastCC(0) {}
    ~ReorderingBuffer() {
        if(start!=NULL) {
            str.releaseBuffer((int32_t)(limit-start));
        }
    }
    UBool init(UNormalizationAppendMode appendMode);

    UBool append(UChar32 c, uint8_t cc) {
        return (c<=0xffff) ?
            appendBMP((UChar)c, cc) :
            appendSupplementary(c, cc);
    }
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
    void resetReorderStart();
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
        return p;
    }
    UBool resize(int32_t appendLength);

    const NormalizerData &data;
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

class NormalizerImpl : public UObject {
    void decompose(const UChar *src, int32_t srcLength,
                   UnicodeString &dest,
                   UNormalizationAppendMode appendMode,
                   UErrorCode &errorCode);
private:
    UBool decompose(const UChar *src, int32_t srcLength, ReorderingBuffer &buffer);
    UBool decompose(UChar32 c, uint16_t norm16, ReorderingBuffer &buffer);

    /**
     * Is c a composition starter?
     * True if its decomposition begins with a character that has
     * ccc=0 && NFC_QC=Yes (isCompYesAndZeroCC()).
     * As a shortcut, this is true if c itself has ccc=0 && NFC_QC=Yes
     * (isCompYesAndZeroCC()) so we need not decompose.
     */
    UBool isCompStarter(UChar32 c, uint16_t norm16);
    const UChar *findPreviousCompStarter(const UChar *start, const UChar *p);
    const UChar *findNextCompStarter(const UChar *p, const UChar *limit);

    NormalizerData data;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION && defined(XP_CPLUSPLUS)
#endif  // __NORMALIZER2IMPL_H__
