/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/udata.h"
#include "unicode/ustring.h"
#include "normalizer2impl.h"

U_NAMESPACE_BEGIN

// TODO: move to utrie2.h
class UTrie2StringIterator : public UMemory {
public:
    UTrie2StringIterator(const UTrie2 *t, const UChar *p) :
        trie(t), codePointStart(p), codePointLimit(p), codePoint(U_SENTINEL) {}

    const UTrie2 *trie;
    const UChar *codePointStart, *codePointLimit;
    UChar32 codePoint;
};

class BackwardUTrie2StringIterator : public UTrie2StringIterator {
public:
    BackwardUTrie2StringIterator(const UTrie2 *t, const UChar *s, const UChar *p) :
        UTrie2StringIterator(t, p), start(s) {}

    uint16_t previous16() {
        codePointLimit=codePointStart;
        if(start>=codePointStart) {
            codePoint=U_SENTINEL;
            return 0;
        }
        uint16_t result;
        UTRIE2_U16_PREV16(trie, start, codePointStart, codePoint, result);
        return result;
    }

    const UChar *start;
};

class ForwardUTrie2StringIterator : public UTrie2StringIterator {
public:
    // Iteration limit l can be NULL.
    // In that case, the caller must detect c==0 and stop.
    ForwardUTrie2StringIterator(const UTrie2 *t, const UChar *p, const UChar *l) :
        UTrie2StringIterator(t, p), limit(l) {}

    uint16_t next16() {
        codePointStart=codePointLimit;
        if(codePointLimit==limit) {
            codePoint=U_SENTINEL;
            return 0;
        }
        uint16_t result;
        UTRIE2_U16_NEXT16(trie, codePointLimit, limit, codePoint, result);
        return result;
    }

    const UChar *limit;
};

UBool ReorderingBuffer::init(UNormalizationAppendMode appendMode) {
    int32_t length=str.length();
    start=str.getBuffer(-1);
    if(start==NULL) {
        return FALSE;
    }
    limit=start+length;
    remainingCapacity=str.getCapacity()-length;
    if(appendMode==UNORM_SIMPLE_APPEND) {
        reorderStart=limit;
    } else {
        resetReorderStart();
    }
    return TRUE;
}

void ReorderingBuffer::resetReorderStart() {
    reorderStart=start;
    setIterator();
    lastCC=previousCC();
    // Set reorderStart after the last code point with cc<=1 if there is one.
    if(lastCC>1) {
        while(previousCC()>1) {}
    }
    reorderStart=codePointLimit;
}

UBool ReorderingBuffer::appendSupplementary(UChar32 c, uint8_t cc) {
    if(remainingCapacity<2 && !resize(2)) {
        return FALSE;
    }
    if(lastCC<=cc || cc==0) {
        limit[0]=U16_LEAD(c);
        limit[1]=U16_TRAIL(c);
        limit+=2;
        lastCC=cc;
        if(cc<=1) {
            reorderStart=limit;
        }
    } else {
        insert(c, cc);
    }
    remainingCapacity-=2;
    return TRUE;
}

UBool ReorderingBuffer::append(const UChar *s, int32_t length, uint8_t leadCC, uint8_t trailCC) {
    if(length==0) {
        return TRUE;
    }
    if(remainingCapacity<length && !resize(length)) {
        return FALSE;
    }
    remainingCapacity-=length;
    for(;;) {
        if(lastCC<=leadCC || leadCC==0) {
            if(leadCC<=1) {
                reorderStart=limit+1;  // Ok if not a code point boundary.
            }
            const UChar *sLimit=s+length;
            do { *limit++=*s++; } while(s<sLimit);
            lastCC=trailCC;
            if(trailCC<=1) {
                reorderStart=limit;
            }
            break;
        } else {
            // TODO: iterator? using _NORM_MIN_WITH_LEAD_CC?
            int32_t i=0;
            UChar32 c;
            U16_NEXT(s, i, length, c);
            insert(c, leadCC);  // insert first code point
            if(i<length) {
                s+=i;
                length-=i;
                U16_NEXT(s, i, length, c);
                if(i<length) {
                    leadCC=data.getCCFromYesOrMaybe(data.getNorm16(c));
                } else {
                    leadCC=trailCC;
                }
                // continue appending the rest of the string
            } else {
                break;
            }
        }
    }
    return TRUE;
}

UBool ReorderingBuffer::appendZeroCC(const UChar *s, int32_t length) {
    if(length==0) {
        return TRUE;
    }
    if(remainingCapacity<length && !resize(length)) {
        return FALSE;
    }
    u_memcpy(limit, s, length);
    limit+=length;
    remainingCapacity-=length;
    lastCC=0;
    reorderStart=limit;
    return TRUE;
}

void ReorderingBuffer::removeZeroCCSuffix(int32_t length) {
    if(length<(limit-start)) {
        limit-=length;
        remainingCapacity+=length;
    } else {
        limit=start;
        remainingCapacity=str.getCapacity();
    }
    // TODO: resetReorderStart(); ??
    reorderStart=limit;
}

UBool ReorderingBuffer::resize(int32_t appendLength) {
    int32_t reorderStartIndex=(int32_t)(reorderStart-start);
    int32_t length=(int32_t)(limit-start);
    str.releaseBuffer(length);
    int32_t newCapacity=length+appendLength;
    int32_t doubleCapacity=2*str.getCapacity();
    if(newCapacity<doubleCapacity) {
        if(doubleCapacity<1024) {
            newCapacity=1024;
        } else {
            newCapacity=doubleCapacity;
        }
    }
    start=str.getBuffer(newCapacity);
    if(start==NULL) {
        return FALSE;
    }
    reorderStart=start+reorderStartIndex;
    limit=start+length;
    remainingCapacity=str.getCapacity()-length;
    return TRUE;
}

void ReorderingBuffer::skipPrevious() {
    codePointLimit=codePointStart;
    UChar c=*--codePointStart;
    if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(*(codePointStart-1))) {
        --codePointStart;
    }
}

uint8_t ReorderingBuffer::previousCC() {
    codePointLimit=codePointStart;
    if(reorderStart>=codePointStart) {
        return 0;
    }
    UChar c=*--codePointStart;
    if(c<_NORM_MIN_WITH_LEAD_CC) {
        return 0;
    }

    UChar c2;
    uint16_t norm16;
    if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(c2=*(codePointStart-1))) {
        --codePointStart;
        norm16=data.getNorm16FromSurrogatePair(c2, c);
    } else {
        norm16=data.getNorm16FromBMP(c);
    }
    return data.getCCFromYesOrMaybe(norm16);
}

// Inserts c somewhere before the last character.
// Requires 0<cc<lastCC which implies reorderStart<limit.
void ReorderingBuffer::insert(UChar32 c, uint8_t cc) {
    for(setIterator(), skipPrevious(); previousCC()>cc;) {}
    // insert c at codePointLimit, after the character with prevCC<=cc
    int32_t length=U16_LENGTH(c);
    UChar *q=limit;
    UChar *r=limit+=length;
    do {
        *--r=*--q;
    } while(codePointLimit!=q);
    writeCodePoint(q, c);
    if(cc<=1) {
        reorderStart=r;
    }
}

UBool Normalizer2Impl::decompose(const UChar *src, int32_t srcLength, ReorderingBuffer &buffer) {
    const UChar *limit;
    if(srcLength>=0) {
        limit=src+srcLength;  // string with length
    } else /* srcLength==-1 */ {
        limit=NULL;  // zero-terminated string
    }

    UChar minNoCP=data.getMinDecompNoCodePoint();

    U_ALIGN_CODE(16);

    for(;;) {
        // count code units below the minimum or with irrelevant data for the quick check
        UChar32 c;
        uint16_t norm16=0;
        const UChar *prevSrc=src;
        if(limit==NULL) {
            while((c=*src)<minNoCP ?
                  c!=0 : data.isMostDecompYesAndZeroCC(norm16=data.getNorm16FromSingleLead(c))) {
                ++src;
            }
        } else {
            while(src!=limit &&
                  ((c=*src)<minNoCP ||
                   data.isMostDecompYesAndZeroCC(norm16=data.getNorm16FromSingleLead(c)))) {
                ++src;
            }
        }

        // copy these code units all at once
        if(src!=prevSrc) {
            if(!buffer.appendZeroCC(prevSrc, (int32_t)(src-prevSrc))) {
                return FALSE;
            }
        }

        if(limit==NULL ? c==0 : src==limit) {
            break;  // end of source reached
        }

        // Check one above-minimum, relevant code point.
        ++src;
        UChar c2;
        if(U16_IS_LEAD(c) && src!=limit && U16_IS_TRAIL(c2=*src)) {
            c=U16_GET_SUPPLEMENTARY(c, c2);
            norm16=data.getNorm16FromSupplementary(c);
        }
        if(!decompose(c, norm16, buffer)) {
            return FALSE;
        }
    }
    return TRUE;
}

UBool Normalizer2Impl::decompose(UChar32 c, uint16_t norm16, ReorderingBuffer &buffer) {
    // Only loops for 1:1 algorithmic mappings.
    for(;;) {
        // get the decomposition and the lead and trail cc's
        uint8_t cc;
        if(data.isDecompYes(norm16)) {
            cc=data.getCCFromYesOrMaybe(norm16);  // c does not decompose
        } else if(data.isHangul(norm16)) {
            // Hangul syllable: decompose algorithmically
            UChar jamos[3];
            UChar c2;
            c-=HANGUL_BASE;
            c2=(UChar)(c%JAMO_T_COUNT);
            c/=JAMO_T_COUNT;
            jamos[0]=(UChar)(JAMO_L_BASE+c/JAMO_V_COUNT);
            jamos[1]=(UChar)(JAMO_V_BASE+c%JAMO_V_COUNT);
            if(c2!=0) {
                jamos[2]=(UChar)(JAMO_T_BASE+c2);
            }
            return buffer.appendZeroCC(jamos, c2==0 ? 2 : 3);
        } else if(data.isNoNoAlgorithmic(norm16)) {
            UChar32 c2=data.mapAlgorithmic(c, norm16);
            if(c==c2) {
                return TRUE;  // c is removed (maps to an empty string).
            }
            c=c2;
            norm16=data.getNorm16(c);
            continue;
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=data.getMapping(norm16);
            int32_t length=*mapping&Normalizer2Data::MAPPING_LENGTH_MASK;
            uint8_t leadCC, trailCC;
            trailCC=(uint8_t)(*mapping>>8);
            if(*mapping++&Normalizer2Data::MAPPING_HAS_CCC_LCCC_WORD) {
                leadCC=(uint8_t)(*mapping++>>8);
            } else {
                leadCC=0;
            }
            return buffer.append((const UChar *)mapping, length, leadCC, trailCC);
        }
        return buffer.append(c, cc);
    }
}

UBool Normalizer2Impl::isCompStarter(UChar32 c, uint16_t norm16) {
    // Partial copy of the decompose(c) function.
    for(;;) {
        if(data.isCompYesAndZeroCC(norm16)) {
            return TRUE;
        } else if(data.isMaybeOrNonZeroCC(norm16)) {
            return FALSE;
        } else if(data.isNoNoAlgorithmic(norm16)) {
            UChar32 c2=data.mapAlgorithmic(c, norm16);
            if(c==c2) {
                return FALSE;  // c is removed (maps to an empty string).
            }
            c=c2;
            norm16=data.getNorm16(c);
            continue;
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=data.getMapping(norm16);
            int32_t length=*mapping&Normalizer2Data::MAPPING_LENGTH_MASK;
            if((*mapping++&Normalizer2Data::MAPPING_HAS_CCC_LCCC_WORD) && (*mapping++&0xff00)) {
                return FALSE;  // non-zero leadCC
            }
            if(length==0) {
                return FALSE;
            }
            int32_t i=0;
            UChar32 c;
            U16_NEXT_UNSAFE(mapping, i, c);
            return data.isCompYesAndZeroCC(data.getNorm16(c));
        }
    }
}

const UChar *Normalizer2Impl::findPreviousCompStarter(const UChar *start, const UChar *p) {
    BackwardUTrie2StringIterator iter(&data.getTrie(), start, p);
    uint16_t norm16;
    do {
        norm16=iter.previous16();
    } while(!isCompStarter(iter.codePoint, norm16));
    return iter.codePointStart;
}

const UChar *Normalizer2Impl::findNextCompStarter(const UChar *p, const UChar *limit) {
    ForwardUTrie2StringIterator iter(&data.getTrie(), p, limit);
    uint16_t norm16;
    do {
        norm16=iter.next16();
    } while(!isCompStarter(iter.codePoint, norm16));
    return iter.codePointStart;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION

// Normalizer2 unmodifiable? Yes, please...
//   Singletons?? Prefer to have Normalizer2Impl singletons but separate Normalizer2 instances.
// Builder code needs to impose limitations:
// _NORM_MIN_WITH_LEAD_CC>=0x300
// Hangul & Jamo properties, except if excluded
// Might be trouble if mapping Hangul and/or Jamo to something else.
// Consider not supporting an exclusions set at runtime.
//   Otherwise need to pull nx_contains() into the ReorderingBuffer etc.
//   Can support Unicode 3.2 normalization via UnicodeSet span outside of normalization calls.
