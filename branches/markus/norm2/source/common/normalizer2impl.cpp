/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2impl.cpp
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
#include "cmemory.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "utrie2.h"

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

class UTrie2Singleton {
public:
    UTrie2Singleton(SimpleSingleton &s) : singleton(s) {}
    void deleteInstance() {
        utrie2_close((UTrie2 *)singleton.fInstance);
        singleton.reset();
    }
    UTrie2 *getInstance(InstantiatorFn *instantiator, const void *context,
                        UErrorCode &errorCode) {
        void *duplicate;
        UTrie2 *instance=(UTrie2 *)singleton.getInstance(instantiator, context, duplicate, errorCode);
        utrie2_close((UTrie2 *)duplicate);
        return instance;
    }
private:
    SimpleSingleton &singleton;
};

// ReorderingBuffer -------------------------------------------------------- ***

UBool ReorderingBuffer::init() {
    int32_t length=str.length();
    start=str.getBuffer(-1);
    if(start==NULL) {
        return FALSE;
    }
    limit=start+length;
    remainingCapacity=str.getCapacity()-length;
    reorderStart=start;
    if(start==limit) {
        lastCC=0;
    } else {
        setIterator();
        lastCC=previousCC();
        // Set reorderStart after the last code point with cc<=1 if there is one.
        if(lastCC>1) {
            while(previousCC()>1) {}
        }
        reorderStart=codePointLimit;
    }
    return TRUE;
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
    if(lastCC<=leadCC || leadCC==0) {
        if(trailCC<=1) {
            reorderStart=limit+length;
        } else if(leadCC<=1) {
            reorderStart=limit+1;  // Ok if not a code point boundary.
        }
        const UChar *sLimit=s+length;
        do { *limit++=*s++; } while(s!=sLimit);
        lastCC=trailCC;
    } else {
        int32_t i=0;
        UChar32 c;
        U16_NEXT(s, i, length, c);
        insert(c, leadCC);  // insert first code point
        while(i<length) {
            U16_NEXT(s, i, length, c);
            if(i<length) {
                // s must be in NFD, otherwise we need to use getCC().
                leadCC=Normalizer2Impl::getCCFromYesOrMaybe(impl.getNorm16(c));
            } else {
                leadCC=trailCC;
            }
            append(c, leadCC);
        }
    }
    return TRUE;
}

UBool ReorderingBuffer::appendZeroCC(UChar32 c) {
    int32_t cpLength=U16_LENGTH(c);
    if(remainingCapacity<cpLength && !resize(cpLength)) {
        return FALSE;
    }
    remainingCapacity-=cpLength;
    if(cpLength==1) {
        *limit++=(UChar)c;
    } else {
        limit[0]=U16_LEAD(c);
        limit[1]=U16_TRAIL(c);
        limit+=2;
    }
    lastCC=0;
    reorderStart=limit;
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
    lastCC=0;
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
    if(c<Normalizer2Impl::MIN_CCC_LCCC_CP) {
        return 0;
    }

    UChar c2;
    uint16_t norm16;
    if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(c2=*(codePointStart-1))) {
        --codePointStart;
        norm16=impl.getNorm16FromSurrogatePair(c2, c);
    } else {
        norm16=impl.getNorm16FromBMP(c);
    }
    return Normalizer2Impl::getCCFromYesOrMaybe(norm16);
}

// Inserts c somewhere before the last character.
// Requires 0<cc<lastCC which implies reorderStart<limit.
void ReorderingBuffer::insert(UChar32 c, uint8_t cc) {
    for(setIterator(), skipPrevious(); previousCC()>cc;) {}
    // insert c at codePointLimit, after the character with prevCC<=cc
    UChar *q=limit;
    UChar *r=limit+=U16_LENGTH(c);
    do {
        *--r=*--q;
    } while(codePointLimit!=q);
    writeCodePoint(q, c);
    if(cc<=1) {
        reorderStart=r;
    }
}

// Normalizer2Impl --------------------------------------------------------- ***

Normalizer2Impl::~Normalizer2Impl() {
    udata_close(memory);
    utrie2_close(normTrie);
    UTrie2Singleton(fcdTrieSingleton).deleteInstance();
}

UBool U_CALLCONV
Normalizer2Impl::isAcceptable(void *context,
                              const char *type, const char *name,
                              const UDataInfo *pInfo) {
    if(
        pInfo->size>=20 &&
        pInfo->isBigEndian==U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily==U_CHARSET_FAMILY &&
        pInfo->dataFormat[0]==0x4e &&    /* dataFormat="Nrm2" */
        pInfo->dataFormat[1]==0x72 &&
        pInfo->dataFormat[2]==0x6d &&
        pInfo->dataFormat[3]==0x32 &&
        pInfo->formatVersion[0]==1
    ) {
        Normalizer2Impl *me=(Normalizer2Impl *)context;
        uprv_memcpy(me->dataVersion, pInfo->dataVersion, 4);
        return TRUE;
    } else {
        return FALSE;
    }
}

void
Normalizer2Impl::load(const char *packageName, const char *name, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }
    memory=udata_openChoice(packageName, "nrm", name, isAcceptable, this, &errorCode);
    if(U_FAILURE(errorCode)) {
        return;
    }
    const uint8_t *inBytes=(const uint8_t *)udata_getMemory(memory);
    const int32_t *inIndexes=(const int32_t *)inBytes;
    int32_t indexesLength=inIndexes[IX_NORM_TRIE_OFFSET]/4;
    if(indexesLength<=IX_MIN_MAYBE_YES) {
        errorCode=U_INVALID_FORMAT_ERROR;  // Not enough indexes.
        return;
    }
    // Copy the indexes. Take care of possible growth of the array.
    if(indexesLength>IX_COUNT) {
        indexesLength=IX_COUNT;
    }
    uprv_memcpy(indexes, inIndexes, indexesLength*4);
    if(indexesLength<IX_COUNT) {
        uprv_memset(indexes+indexesLength, 0, (IX_COUNT-indexesLength)*4);
    }

    int32_t offset=indexes[IX_NORM_TRIE_OFFSET];
    int32_t nextOffset=indexes[IX_EXTRA_DATA_OFFSET];
    normTrie=utrie2_openFromSerialized(UTRIE2_16_VALUE_BITS,
                                       inBytes+offset, nextOffset-offset, NULL,
                                       &errorCode);
    if(U_FAILURE(errorCode)) {
        return;
    }

    offset=nextOffset;
    maybeYesCompositions=(const uint16_t *)(inBytes+offset);
    extraData=maybeYesCompositions+(MIN_NORMAL_MAYBE_YES-indexes[IX_MIN_MAYBE_YES]);
}

uint8_t Normalizer2Impl::getTrailCCFromCompYesAndZeroCC(const UChar *cpStart, const UChar *cpLimit) const {
    uint16_t prevNorm16;
    if(cpStart==(cpLimit-1)) {
        prevNorm16=getNorm16FromBMP(*cpStart);
    } else {
        prevNorm16=getNorm16FromSurrogatePair(cpStart[0], cpStart[1]);
    }
    if(prevNorm16<indexes[IX_MIN_YES_NO]) {
        return 0;  // yesYes has ccc=tccc=0
    } else {
        return (uint8_t)(*getMapping(prevNorm16)>>8);  // tccc from noNo
    }
}

UBool Normalizer2Impl::decompose(const UChar *src, int32_t srcLength,
                                 ReorderingBuffer &buffer) const {
    const UChar *limit;
    if(srcLength>=0) {
        limit=src+srcLength;  // string with length
    } else /* srcLength==-1 */ {
        limit=NULL;  // zero-terminated string
    }

    UChar32 minNoCP=indexes[IX_MIN_DECOMP_NO_CP];
    uint16_t norm16=0;

    U_ALIGN_CODE(16);

    for(;;) {
        // count code units below the minimum or with irrelevant data for the quick check
        UChar32 c;
        const UChar *prevSrc=src;
        if(limit==NULL) {
            while((c=*src)<minNoCP ?
                  c!=0 : isMostDecompYesAndZeroCC(norm16=getNorm16FromSingleLead(c))) {
                ++src;
            }
        } else {
            while(src!=limit &&
                  ((c=*src)<minNoCP ||
                   isMostDecompYesAndZeroCC(norm16=getNorm16FromSingleLead(c)))) {
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
        if(U16_IS_LEAD(c)) {
            UChar c2;
            if(src!=limit && U16_IS_TRAIL(c2=*src)) {
                ++src;
                c=U16_GET_SUPPLEMENTARY(c, c2);
                norm16=getNorm16FromSupplementary(c);
            } else {
                // Data for lead surrogate code *point* not code *unit*. Normally 0.
                norm16=getNorm16FromBMP((UChar)c);
            }
        }
        if(!decompose(c, norm16, buffer)) {
            return FALSE;
        }
    }
    return TRUE;
}

// Decompose a short piece of text which is likely to contain characters that
// fail the quick check loop and/or where the quick check loop's overhead
// is unlikely to be amortized.
// Called by the compose() and makeFCD() implementations.
UBool Normalizer2Impl::decomposeShort(const UChar *src, const UChar *limit,
                                      ReorderingBuffer &buffer) const {
    while(src<limit) {
        UChar32 c;
        uint16_t norm16;
        UTRIE2_U16_NEXT16(normTrie, src, limit, c, norm16);
        if(!decompose(c, norm16, buffer)) {
            return FALSE;
        }
    }
    return TRUE;
}

UBool Normalizer2Impl::decompose(UChar32 c, uint16_t norm16, ReorderingBuffer &buffer) const {
    // Only loops for 1:1 algorithmic mappings.
    for(;;) {
        // get the decomposition and the lead and trail cc's
        if(isDecompYes(norm16)) {
            // c does not decompose
            return buffer.append(c, getCCFromYesOrMaybe(norm16));
        } else if(isHangul(norm16)) {
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
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
            norm16=getNorm16(c);
            continue;
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            int32_t length=*mapping&MAPPING_LENGTH_MASK;
            uint8_t leadCC, trailCC;
            trailCC=(uint8_t)(*mapping>>8);
            if(*mapping++&MAPPING_HAS_CCC_LCCC_WORD) {
                leadCC=(uint8_t)(*mapping++>>8);
            } else {
                leadCC=0;
            }
            return buffer.append((const UChar *)mapping, length, leadCC, trailCC);
        }
    }
}

void Normalizer2Impl::decompose(const UChar *src, int32_t srcLength,
                                UnicodeString &dest,
                                UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return;
    }
    dest.remove();
    ReorderingBuffer buffer(*this, dest);
    if(!buffer.init() || !decompose(src, srcLength, buffer)) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        dest.setToBogus();
    }
}

void Normalizer2Impl::decomposeAndAppend(const UChar *src, int32_t srcLength,
                                         UnicodeString &dest, UBool doDecompose,
                                         UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) {
        return;
    }
    if(dest.isBogus()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    ReorderingBuffer buffer(*this, dest);
    if(!buffer.init()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if(doDecompose) {
        if(!decompose(src, srcLength, buffer)) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        }
        return;
    }
    // Just merge the strings at the boundary.
    if(srcLength<0) {
        srcLength=u_strlen(src);
    }
    ForwardUTrie2StringIterator iter(normTrie, src, src+srcLength);
    uint16_t first16, norm16;
    first16=norm16=iter.next16();
    while(!isDecompYesAndZeroCC(norm16)) {
        norm16=iter.next16();
    };
    if(!buffer.append(src, (int32_t)(iter.codePointStart-src), getCC(first16), 0)) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if(!buffer.appendZeroCC(iter.codePointStart, srcLength-(int32_t)(iter.codePointStart-src))) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
    }
}

/*
 * Finds the recomposition result for
 * a forward-combining "lead" character,
 * specified with a pointer to its compositions list,
 * and a backward-combining "trail" character.
 *
 * If the lead and trail characters combine, then this function returns
 * the following "compositeAndFwd" value:
 * Bits 21..1  composite character
 * Bit      0  set if the composite is a forward-combining starter
 * otherwise it returns -1.
 *
 * The compositions list has (trail, compositeAndFwd) pair entries,
 * encoded as either pairs or triples of 16-bit units.
 * The last entry has the high bit of its first unit set.
 *
 * The list is sorted by ascending trail characters (there are no duplicates).
 * A linear search is used.
 *
 * See normalizer2impl.h for a more detailed description
 * of the compositions list format.
 */
int32_t Normalizer2Impl::combine(const uint16_t *list, UChar32 trail) {
    uint16_t key1, firstUnit;
    if(trail<COMP_1_TRAIL_LIMIT) {
        // trail character is 0..33FF
        // result entry may have 2 or 3 units
        key1=(uint16_t)(trail<<1);
        while(key1>(firstUnit=*list)) {
            list+=2+(firstUnit&COMP_1_TRIPLE);
        }
        if(key1==(firstUnit&COMP_1_TRAIL_MASK)) {
            if(firstUnit&COMP_1_TRIPLE) {
                return ((int32_t)list[1]<<16)|list[2];
            } else {
                return list[1];
            }
        }
    } else {
        // trail character is 3400..10FFFF
        // result entry has 3 units
        key1=(uint16_t)(COMP_1_TRAIL_LIMIT+
                        ((trail>>COMP_1_TRAIL_SHIFT))&
                         ~COMP_1_TRIPLE);
        uint16_t key2=(uint16_t)(trail<<COMP_2_TRAIL_SHIFT);
        uint16_t secondUnit;
        for(;;) {
            if(key1>(firstUnit=*list)) {
                list+=2+(firstUnit&COMP_1_TRIPLE);
            } else if(key1==(firstUnit&COMP_1_TRAIL_MASK)) {
                if(key2>(secondUnit=list[1])) {
                    if(firstUnit&COMP_1_LAST_TUPLE) {
                        break;
                    } else {
                        list+=3;
                    }
                } else if(key2==(secondUnit&COMP_2_TRAIL_MASK)) {
                    return ((int32_t)(secondUnit&~COMP_2_TRAIL_MASK)<<16)|list[2];
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }
    return -1;
}

/*
 * Recomposes the text in [p..limit[
 * (which is in NFD - decomposed and canonically ordered),
 * and returns how much shorter the string became.
 *
 * Note that recomposition never lengthens the text:
 * Any character consists of either one or two code units;
 * a composition may contain at most one more code unit than the original starter,
 * while the combining mark that is removed has at least one code unit.
 */
void Normalizer2Impl::recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex,
                                UBool onlyContiguous) const {
    UChar *p=buffer.getStart()+recomposeStartIndex;
    UChar *limit=buffer.getLimit();

    UChar *starter, *pRemove, *q, *r;
    const uint16_t *compositionsList;
    UChar32 c, compositeAndFwd;
    uint16_t norm16;
    uint8_t cc, prevCC;
    UBool starterIsSupplementary;

    // Some of the following variables are not used until we have a forward-combining starter
    // and are only initialized now to avoid compiler warnings.
    compositionsList=NULL;  // used as indicator for whether we have a forward-combining starter
    starter=NULL;
    starterIsSupplementary=FALSE;
    prevCC=0;

    for(;;) {
        UTRIE2_U16_NEXT16(normTrie, p, limit, c, norm16);
        cc=getCCFromYesOrMaybe(norm16);
        if( // this character combines backward and
            isMaybe(norm16) &&
            // we have seen a starter that combines forward and
            compositionsList!=NULL &&
            // the backward-combining character is not blocked
            (prevCC<cc || prevCC==0)
        ) {
            if(isJamoVT(norm16)) {
                // c is a Jamo V/T, see if we can compose it with the previous character.
                if(c<JAMO_T_BASE) {
                    // c is a Jamo Vowel, compose with previous Jamo L and following Jamo T.
                    UChar prev=(UChar)(*starter-JAMO_L_BASE);
                    if(prev<JAMO_L_COUNT) {
                        pRemove=p-1;
                        UChar syllable=(UChar)(HANGUL_BASE+(prev*JAMO_V_COUNT+(c-JAMO_V_BASE))*JAMO_T_COUNT);
                        UChar t;
                        if(p!=limit && (t=(UChar)(*p-JAMO_T_BASE))<JAMO_T_COUNT) {
                            ++p;
                            syllable+=t;  // The next character was a Jamo T.
                        }
                        *starter=syllable;
                        // remove the Jamo V/T
                        q=pRemove;
                        r=p;
                        while(r<limit) {
                            *q++=*r++;
                        }
                        limit=q;
                        p=pRemove;
                    }
                }
                /*
                 * No "else" for Jamo T:
                 * Since the input is in NFD, there are no Hangul LV syllables that
                 * a Jamo T could combine with.
                 * All Jamo Ts are combined above when handling Jamo Vs.
                 */
                if(p==limit) {
                    break;
                }
                compositionsList=NULL;
                continue;
            } else if((compositeAndFwd=combine(compositionsList, c))>=0) {
                // The starter and the combining mark (c) do combine.
                UChar32 composite=compositeAndFwd>>1;

                // Replace the starter with the composite, remove the combining mark.
                pRemove=p-U16_LENGTH(c);  // pRemove & p: start & limit of the combining mark
                if(starterIsSupplementary) {
                    if(U_IS_SUPPLEMENTARY(composite)) {
                        // both are supplementary
                        starter[0]=U16_LEAD(composite);
                        starter[1]=U16_TRAIL(composite);
                    } else {
                        *starter=(UChar)composite;
                        // The composite is shorter than the starter,
                        // move the intermediate characters forward one.
                        starterIsSupplementary=FALSE;
                        q=starter+1;
                        r=q+1;
                        while(r<pRemove) {
                            *q++=*r++;
                        }
                        --pRemove;
                    }
                } else if(U_IS_SUPPLEMENTARY(composite)) {
                    // The composite is longer than the starter,
                    // move the intermediate characters back one.
                    starterIsSupplementary=TRUE;
                    ++starter;  // temporarily increment for the loop boundary
                    q=pRemove;
                    r=++pRemove;
                    while(starter<q) {
                        *--r=*--q;
                    }
                    *starter=U16_TRAIL(composite);
                    *--starter=U16_LEAD(composite);  // undo the temporary increment
                } else {
                    // both are on the BMP
                    *starter=(UChar)composite;
                }

                /* remove the combining mark by moving the following text over it */
                if(pRemove<p) {
                    q=pRemove;
                    r=p;
                    while(r<limit) {
                        *q++=*r++;
                    }
                    limit=q;
                    p=pRemove;
                }
                // Keep prevCC because we removed the combining mark.

                if(p==limit) {
                    break;
                }
                // Is the composite a starter that combines forward?
                if(compositeAndFwd&1) {
                    compositionsList=
                        getCompositionsListForComposite(getNorm16(composite));
                } else {
                    compositionsList=NULL;
                }

                // We combined; continue with looking for compositions.
                continue;
            }
        }

        // no combination this time
        prevCC=cc;
        if(p==limit) {
            break;
        }

        // If c did not combine, then check if it is a starter.
        if(cc==0) {
            // Found a new starter.
            if((compositionsList=getCompositionsListForDecompYesAndZeroCC(norm16))!=NULL) {
                // It may combine with something, prepare for it.
                if(U_IS_BMP(c)) {
                    starterIsSupplementary=FALSE;
                    starter=p-1;
                } else {
                    starterIsSupplementary=TRUE;
                    starter=p-2;
                }
            }
        } else if(onlyContiguous) {
            // FCC: no discontiguous compositions; any intervening character blocks.
            compositionsList=NULL;
        }
    }
    buffer.setReorderingLimitAndLastCC(limit, prevCC);
}

UBool Normalizer2Impl::compose(const UChar *src, int32_t srcLength,
                               ReorderingBuffer &buffer,
                               UBool onlyContiguous) const {
    const UChar *limit;
    if(srcLength>=0) {
        limit=src+srcLength;  // string with length
    } else /* srcLength==-1 */ {
        limit=NULL;  // zero-terminated string
    }

    /*
     * prevStarter points to the last character before the current one
     * that is a composition starter with ccc==0 and quick check "yes".
     * Keeping track of prevStarter saves us looking for a composition starter
     * when we find a "no" or "maybe".
     *
     * When we back out from prevSrc back to prevStarter,
     * then we also remove those same characters (which had been simply copied)
     * from the ReorderingBuffer.
     * Therefore, at all times, the [prevStarter..prevSrc[ source units
     * must correspond 1:1 to destination units at the end of the destination buffer.
     */
    const UChar *prevStarter=src;

    UChar32 minNoMaybeCP=indexes[IX_MIN_COMP_NO_MAYBE_CP];
    uint16_t norm16=0;

    U_ALIGN_CODE(16);

    for(;;) {
        // count code units below the minimum or with irrelevant data for the quick check
        UChar32 c;
        const UChar *prevSrc=src;
        if(limit==NULL) {
            while((c=*src)<minNoMaybeCP ?
                  c!=0 : isCompYesAndZeroCC(norm16=getNorm16FromSingleLead(c))) {
                ++src;
            }
        } else {
            while(src!=limit &&
                  ((c=*src)<minNoMaybeCP ||
                   isCompYesAndZeroCC(norm16=getNorm16FromSingleLead(c)))) {
                ++src;
            }
        }

        // copy these code units all at once
        if(src!=prevSrc) {
            if(!buffer.appendZeroCC(prevSrc, (int32_t)(src-prevSrc))) {
                return FALSE;
            }
            // Set prevStarter to the last character in the quick check loop.
            prevStarter=src-1;
            if(U16_IS_TRAIL(*prevStarter) && prevSrc<prevStarter && U16_IS_LEAD(*(prevStarter-1))) {
                --prevStarter;
            }
            // The start of the current character (c).
            prevSrc=src;
        }

        if(limit==NULL ? c==0 : src==limit) {
            break;  // end of source reached
        }

        ++src;
        /*
         * norm16 is for c=*(src-1) which has "no" or "maybe" properties, and/or ccc!=0.
         * Check for Jamo V/T, then for surrogates and regular characters.
         * c is not a Hangul syllable or Jamo L because those have "yes" properties.
         */
        if(isJamoVT(norm16)) {
            UChar prev=buffer.lastUChar();
            if(c<JAMO_T_BASE) {
                // c is a Jamo Vowel, compose with previous Jamo L and following Jamo T.
                prev=(UChar)(prev-JAMO_L_BASE);
                if(prev<JAMO_L_COUNT) {
                    UChar syllable=(UChar)(HANGUL_BASE+(prev*JAMO_V_COUNT+(c-JAMO_V_BASE))*JAMO_T_COUNT);
                    UChar t;
                    if(src!=limit && (t=(UChar)(*src-JAMO_T_BASE))<JAMO_T_COUNT) {
                        ++src;
                        syllable+=t;  // The next character was a Jamo T.
                        prevStarter=src;
                    }
                    buffer.lastUChar()=syllable;
                    continue;
                }
            } else if(isHangulWithoutJamoT(prev)) {
                // c is a Jamo Trailing consonant,
                // compose with previous Hangul LV that does not contain a Jamo T.
                buffer.lastUChar()=(UChar)(prev+c-JAMO_T_BASE);
                prevStarter=src;
                continue;
            }
            // The Jamo V/T did not compose into a Hangul syllable.
            if(!buffer.appendBMP((UChar)c, 0)) {
                return FALSE;
            }
            continue;
        } else if(U16_IS_LEAD(c)) {
            UChar c2;
            if(src!=limit && U16_IS_TRAIL(c2=*src)) {
                ++src;
                c=U16_GET_SUPPLEMENTARY(c, c2);
                norm16=getNorm16FromSupplementary(c);
            } else {
                // Data for lead surrogate code *point* not code *unit*. Normally 0.
                norm16=getNorm16FromBMP((UChar)c);
            }
            if(isCompYesAndZeroCC(norm16)) {
                prevStarter=prevSrc;
                if(!buffer.append(c, 0)) {
                    return FALSE;
                }
                continue;
            }
        }
        /*
         * Now isCompYesAndZeroCC(norm16) is false, that is, norm16>=indexes[IX_MIN_NO_NO].
         * c is either a "noNo" (has a mapping) or a "maybeYes" (combines backward)
         * or has ccc!=0.
         */

        /*
         * Source buffer pointers:
         *
         *  all done      quick check   current char  not yet
         *                "yes" but     (c)           processed
         *                may combine
         *                forward
         * [-------------[-------------[-------------[-------------[
         * |             |             |             |             |
         * orig. src     prevStarter   prevSrc       src           limit
         *
         *
         * Destination buffer pointers inside the ReorderingBuffer:
         *
         *  all done      might take    not filled yet
         *                characters for
         *                reordering
         * [-------------[-------------[-------------[
         * |             |             |             |
         * start         reorderStart  limit         |
         *                             +remainingCap.+
         */
        if(norm16>=MIN_YES_YES_WITH_CC) {
            uint8_t cc=(uint8_t)norm16;  // cc!=0
            if( onlyContiguous &&  // FCC
                buffer.getLastCC()==0 && prevStarter<prevSrc &&
                // buffer.getLastCC()==0 && prevStarter<prevSrc tell us that
                // [prevStarter..prevSrc[ (which is exactly one character under these conditions)
                // passed the quick check "yes && ccc==0" test.
                // Check whether the last character was a "yesYes" or a "yesNo".
                // If a "yesNo", then we get its trailing ccc from its
                // mapping and check for canonical order.
                // All other cases are ok.
                getTrailCCFromCompYesAndZeroCC(prevStarter, prevSrc)>cc
            ) {
                // Fails FCD test, need to decompose and contiguously recompose.
            } else {
                if(!buffer.append(c, cc)) {
                    return FALSE;
                }
                continue;
            }
        }

        /*
         * Find appropriate boundaries around this character,
         * decompose the source text from between the boundaries,
         * and recompose it.
         *
         * We may need to remove the last few characters from the ReorderingBuffer
         * to account for source text that was copied or appended
         * but needs to take part in the recomposition.
         */

        /*
         * Find the last composition starter in [prevStarter..src[.
         * It is either the decomposition of the current character (at prevSrc),
         * or prevStarter.
         */
        if(isCompStarter(c, norm16)) {
            prevStarter=prevSrc;
        } else {
            buffer.removeZeroCCSuffix((int32_t)(prevSrc-prevStarter));
        }

        // Find the next composition starter in [src..limit[ -
        // modifies src to point to the next starter.
        src=(UChar *)findNextCompStarter(src, limit);

        // Decompose [prevStarter..src[ into the buffer and then recompose that part of it.
        int32_t recomposeStartIndex=buffer.length();
        if(!decomposeShort(prevStarter, src, buffer)) {
            return FALSE;
        }
        recompose(buffer, recomposeStartIndex, onlyContiguous);

        // Move to the next starter. We never need to look back before this point again.
        prevStarter=src;
    }
    return TRUE;
}

void Normalizer2Impl::compose(const UChar *src, int32_t srcLength,
                              UnicodeString &dest,
                              UBool onlyContiguous,
                              UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return;
    }
    dest.remove();
    ReorderingBuffer buffer(*this, dest);
    if(!buffer.init() || !compose(src, srcLength, buffer, onlyContiguous)) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        dest.setToBogus();
    }
}

void Normalizer2Impl::composeAndAppend(const UChar *src, int32_t srcLength,
                                       UnicodeString &dest,
                                       UBool doCompose,
                                       UBool onlyContiguous,
                                       UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) {
        return;
    }
    if(dest.isBogus()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    ReorderingBuffer buffer(*this, dest);
    if(!buffer.init()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if(!buffer.isEmpty()) {
        const UChar *firstStarterInSrc=findNextCompStarter(src,
                                                           srcLength>=0 ? src+srcLength : NULL);
        if(src!=firstStarterInSrc) {
            const UChar *lastStarterInDest=findPreviousCompStarter(buffer.getStart(),
                                                                   buffer.getLimit());
            UnicodeString middle(lastStarterInDest,
                                 (int32_t)(buffer.getLimit()-lastStarterInDest));
            buffer.removeZeroCCSuffix((int32_t)(buffer.getLimit()-lastStarterInDest));
            middle.append(src, (int32_t)(firstStarterInSrc-src));
            if(!compose(middle.getBuffer(), middle.length(), buffer, onlyContiguous)) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            if(srcLength>=0) {
                srcLength-=(int32_t)(firstStarterInSrc-src);
            }
            src=firstStarterInSrc;
        }
    }
    if(doCompose ?
            !compose(src, srcLength, buffer, onlyContiguous) :
            !buffer.appendZeroCC(src, srcLength>=0 ? srcLength : u_strlen(src))) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
    }
}

UBool Normalizer2Impl::isCompStarter(UChar32 c, uint16_t norm16) const {
    // Partial copy of the decompose(c) function.
    for(;;) {
        if(isCompYesAndZeroCC(norm16)) {
            return TRUE;
        } else if(isMaybeOrNonZeroCC(norm16)) {
            return FALSE;
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
            norm16=getNorm16(c);
            continue;
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            int32_t length=*mapping&MAPPING_LENGTH_MASK;
            if((*mapping++&MAPPING_HAS_CCC_LCCC_WORD) && (*mapping++&0xff00)) {
                return FALSE;  // non-zero leadCC
            }
            if(length==0) {
                return FALSE;
            }
            int32_t i=0;
            UChar32 c;
            U16_NEXT_UNSAFE(mapping, i, c);
            return isCompYesAndZeroCC(getNorm16(c));
        }
    }
}

const UChar *Normalizer2Impl::findPreviousCompStarter(const UChar *start, const UChar *p) const {
    BackwardUTrie2StringIterator iter(normTrie, start, p);
    uint16_t norm16;
    do {
        norm16=iter.previous16();
    } while(!isCompStarter(iter.codePoint, norm16));
    return iter.codePointStart;
}

const UChar *Normalizer2Impl::findNextCompStarter(const UChar *p, const UChar *limit) const {
    ForwardUTrie2StringIterator iter(normTrie, p, limit);
    uint16_t norm16;
    do {
        norm16=iter.next16();
    } while(!isCompStarter(iter.codePoint, norm16));
    return iter.codePointStart;
}

class FCDTrieSingleton : public UTrie2Singleton {
public:
    FCDTrieSingleton(SimpleSingleton &s, Normalizer2Impl &ni, UErrorCode &ec) :
        UTrie2Singleton(s), impl(ni), errorCode(ec) {}
    UTrie2 *getInstance(UErrorCode &errorCode) {
        return UTrie2Singleton::getInstance(createInstance, this, errorCode);
    }
    static void *createInstance(const void *context, UErrorCode &errorCode);
    UBool rangeHandler(UChar32 start, UChar32 end, uint32_t value) {
        if(value!=0) {
            impl.setFCD16FromNorm16(start, end, (uint16_t)value, newFCDTrie, errorCode);
        }
        return U_SUCCESS(errorCode);
    }

    Normalizer2Impl &impl;
    UTrie2 *newFCDTrie;
    UErrorCode &errorCode;
};

U_CDECL_BEGIN

static UBool U_CALLCONV
enumRangeHandler(const void *context, UChar32 start, UChar32 end, uint32_t value) {
    return ((FCDTrieSingleton *)context)->rangeHandler(start, end, value);
}

U_CDECL_END

void *FCDTrieSingleton::createInstance(const void *context, UErrorCode &errorCode) {
    FCDTrieSingleton *me=(FCDTrieSingleton *)context;
    me->newFCDTrie=utrie2_open(0, 0, &errorCode);
    if(U_SUCCESS(errorCode)) {
        utrie2_enum(me->impl.getNormTrie(), NULL, enumRangeHandler, me);
        utrie2_freeze(me->newFCDTrie, UTRIE2_16_VALUE_BITS, &errorCode);
        if(U_SUCCESS(errorCode)) {
            return me->newFCDTrie;
        }
    }
    utrie2_close(me->newFCDTrie);
    return NULL;
}

void Normalizer2Impl::setFCD16FromNorm16(UChar32 start, UChar32 end, uint16_t norm16,
                                         UTrie2 *newFCDTrie, UErrorCode &errorCode) const {
    // Only loops for 1:1 algorithmic mappings.
    for(;;) {
        if(norm16>=MIN_NORMAL_MAYBE_YES) {
            norm16&=0xff;
            norm16|=norm16<<8;
        } else if(norm16<=indexes[IX_MIN_YES_NO] || indexes[IX_MIN_MAYBE_YES]<=norm16) {
            // no decomposition or Hangul syllable, all zeros
            break;
        } else if(indexes[IX_LIMIT_NO_NO]<=norm16) {
            int32_t delta=norm16-(indexes[IX_MIN_MAYBE_YES]-MAX_DELTA-1);
            if(start==end) {
                start+=delta;
                norm16=getNorm16(start);
                continue;
            } else {
                // the same delta leads from different original characters to different mappings
                do {
                    UChar32 c=start+delta;
                    setFCD16FromNorm16(c, c, getNorm16(c), newFCDTrie, errorCode);
                } while(++start<=end);
                break;
            }
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            if(*mapping&MAPPING_HAS_CCC_LCCC_WORD) {
                norm16=mapping[1]&0xff00;  // lccc
            } else {
                norm16=0;
            }
            norm16|=*mapping>>8;  // tccc
        }
        utrie2_setRange32(newFCDTrie, start, end, norm16, TRUE, &errorCode);
        break;
    }
}

const UTrie2 *Normalizer2Impl::getFCDTrie(UErrorCode &errorCode) const {
    // Logically const: Synchronized instantiation.
    Normalizer2Impl *me=const_cast<Normalizer2Impl *>(this);
    return FCDTrieSingleton(me->fcdTrieSingleton, *me, errorCode).getInstance(errorCode);
}

UBool
Normalizer2Impl::makeFCD(const UChar *src, int32_t srcLength,
                         ReorderingBuffer &buffer) const {
    const UChar *limit;
    if(srcLength>=0) {
        limit=src+srcLength;  // string with length
    } else /* srcLength==-1 */ {
        limit=NULL;  // zero-terminated string
    }

    // Note: In this function we use buffer.appendZeroCC() because we track
    // the lead and trail combining classes here, rather than leaving it to
    // the ReorderingBuffer.
    // The exception is the call to decomposeShort() which uses the buffer
    // in the normal way.

    // Tracks the last FCD-safe boundary, before lccc=0 or after tccc<=1.
    // Similar to the prevStarter in the compose() implementation.
    const UChar *prevBoundary=src;

    int32_t prevFCD16=0;
    uint16_t fcd16=0;

    U_ALIGN_CODE(16);

    for(;;) {
        // count code units with lccc==0
        UChar32 c;
        const UChar *prevSrc=src;
        if(limit==NULL) {
            for(;;) {
                c=*src;
                if(c<MIN_CCC_LCCC_CP) {
                    if(c==0) {
                        break;
                    }
                    prevFCD16=~c;
                } else if((fcd16=getFCD16FromSingleLead(c))<=0xff) {
                    prevFCD16=fcd16;
                } else {
                    break;
                }
                ++src;
            }
        } else {
            for(;;) {
                if(src==limit) {
                    break;
                } else if((c=*src)<MIN_CCC_LCCC_CP) {
                    prevFCD16=~c;
                } else if((fcd16=getFCD16FromSingleLead(c))<=0xff) {
                    prevFCD16=fcd16;
                } else {
                    break;
                }
                ++src;
            }
        }

        // copy these code units all at once
        if(src!=prevSrc) {
            if(!buffer.appendZeroCC(prevSrc, (int32_t)(src-prevSrc))) {
                return FALSE;
            }
            prevBoundary=src;
            if(prevFCD16<0) {
                // Fetching the fcd16 value was deferred for this below-U+0300 code point.
                // We know that its lccc==0.
                prevFCD16=getFCD16FromSingleLead((UChar)~prevFCD16);
                if(prevFCD16>1) {
                    --prevBoundary;
                }
            } else if(prevFCD16>1) {
                // We know that the previous character's lccc==0.
                if(U16_IS_TRAIL(*--prevBoundary) && prevSrc<prevBoundary && U16_IS_LEAD(*(prevBoundary-1))) {
                    --prevBoundary;
                }
            }
            // The start of the current character (c).
            prevSrc=src;
        }

        if(limit==NULL ? c==0 : src==limit) {
            break;  // end of source reached
        }

        ++src;
        // The current character (c) has a non-zero lead combining class.
        // Check for surrogates, proper order, and decompose locally if necessary.
        if(U16_IS_LEAD(c)) {
            UChar c2;
            if(src!=limit && U16_IS_TRAIL(c2=*src)) {
                ++src;
                c=U16_GET_SUPPLEMENTARY(c, c2);
                fcd16=getFCD16FromSupplementary(c);
            } else {
                // Data for lead surrogate code *point* not code *unit*. Normally 0.
                fcd16=getFCD16FromBMP((UChar)c);
            }
            if(fcd16<=0xff) {
                if(fcd16<=1) {
                    prevBoundary=src;
                } else {
                    prevBoundary=prevSrc;
                }
                if(!buffer.appendZeroCC(c)) {
                    return FALSE;
                }
                prevFCD16=fcd16;
                continue;
            }
        }
        // c at [prevSrc..src[ has lccc!=0.

        if((prevFCD16&0xff)<=(fcd16>>8)) {
            // proper order: prev tccc <= current lccc
            if((fcd16&0xff)<=1) {
                prevBoundary=src;
            }
            if(!buffer.appendZeroCC(c)) {
                return FALSE;
            }
            prevFCD16=fcd16;
            continue;
        } else {
            /*
             * Back out the part of the source that we copied already but
             * is now going to be decomposed.
             * prevSrc is set to after what was copied.
             */
            buffer.removeZeroCCSuffix((int32_t)(prevSrc-prevBoundary));
            /*
             * Find the part of the source that needs to be decomposed,
             * up to the next safe boundary.
             */
            src=findNextFCDBoundary(src, limit);
            /*
             * The source text does not fulfill the conditions for FCD.
             * Decompose and reorder a limited piece of the text.
             */
            if(!decomposeShort(prevBoundary, src, buffer)) {
                return FALSE;
            }
            prevBoundary=src;
            prevFCD16=0;
        }
    }
    return TRUE;
}

void
Normalizer2Impl::makeFCD(const UChar *src, int32_t srcLength,
                         UnicodeString &dest,
                         UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return;
    }
    dest.remove();
    ReorderingBuffer buffer(*this, dest);
    if(!buffer.init() || !makeFCD(src, srcLength, buffer)) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        dest.setToBogus();
    }
}

const UChar *Normalizer2Impl::findNextFCDBoundary(const UChar *p, const UChar *limit) const {
    ForwardUTrie2StringIterator iter(fcdTrie(), p, limit);
    uint16_t fcd16;
    for(;;) {
        fcd16=iter.next16();
        if(fcd16<=0xff) {
            return iter.codePointStart;
        } else if((fcd16&0xff)<=1) {
            return iter.codePointLimit;
        }
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION

// #if for code only for minMaybeYes<MIN_NORMAL_MAYBE_YES. Benchmark both ways.
// Consider not supporting an exclusions set at runtime.
//   Otherwise need to pull nx_contains() into the ReorderingBuffer etc.
//   Can support Unicode 3.2 normalization via UnicodeSet span outside of normalization calls.
