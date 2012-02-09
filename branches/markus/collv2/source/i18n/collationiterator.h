/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationiterator.h
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONITERATOR_H__
#define __COLLATIONITERATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

/**
 * Collation element and character iterator.
 * Handles normalized UTF-16 text inline, with length or NUL-terminated.
 * Other text is handled by subclasses.
 */
class CollationIterator : public UObject {
public:
    CollationIterator(const CollationData *tailoring, const CollationData *baseData,
                      const UChar *s, int32_t length)
            : data(tailoring), base(baseData),
              ces(NULL), cesIndex(-1), cesMaxIndex(0),
              start(s), pos(s), limit(length>=0 ? s+length : NULL),
              savedLimit(NULL) {
        trie=data->getTrie();
    }

    /**
     * Returns the next collation element.
     * Optimized inline fastpath for the most common types of text and data.
     */
    inline int64_t nextCE() {
        if(cesIndex>=0) {
            // Return the next buffered CE.
            int64_t ce=ces[cesIndex];
            if(cesIndex<cesMaxIndex) {
                ++cesIndex;
            } else {
                cesIndex=-1;
            }
            return ce;
        }
        UChar32 c;
        uint32_t ce32;
        if(pos!=limit) {
            UTRIE2_U16_NEXT32(trie, pos, limit, c, ce32);
            if(c==0 && limit==NULL) {
                // Handle NUL-termination. (Not needed in Java.)
                limit=--pos;
                return Collation::NO_CE;
            }
        } else {
            ce32=handleNextCE32(c);
            if(c<0) {
                return ce32;  // Collation::NO_CE
            }
        }
        // Java: Emulate unsigned-int less-than comparison.
        // int xce32=ce32^0x80000000;
        // if(xce32<0x7f000000) { special }
        // if(xce32==0x7f000000) { fallback }
        if(ce32<Collation::MIN_SPECIAL_CE32) {  // Forced-inline of isSpecialCE32(ce32).
            // Normal CE from the main data.
            return Collation::ceFromCE32(ce32);
        }
        const CollationData *d;
        if(ce32==Collation::MIN_SPECIAL_CE32) {
            ce32=base->getCE32(c);
            if(!Collation::isSpecialCE32(ce32)) {
                // Normal CE from the base data.
                return Collation::ceFromCE32(ce32);
            } else {
                d=base;
            }
        } else {
            d=data;
        }
        return nextCEFromSpecialCE32(d, c, ce32);
    }
    // TODO: Jump by delta code points if direction changed? (See ICU ticket #9104.)
    // If so, then copy nextCE() to a not-inline slowNextCE()
    // which keeps track of the text movements together with previousCE()
    // and is used by the CollationElementIterator.
    // Keep the normal, inline nextCE() maximally fast and efficient.

    /**
     * Returns the next collation element.
     */
    int64_t previousCE() {
        if(cesIndex>0) {
            // Return the previous buffered CE.
            int64_t ce=ces[--cesIndex];
            if(cesIndex==0) { cesIndex=-1; }
            // TODO: Jump by delta code points if the direction changed?
            return ce;
        }
        UChar32 c;
        uint32_t ce32;
        if(pos!=start) {
            UTRIE2_U16_PREV32(trie, start, pos, c, ce32);
        } else {
            ce32=handlePreviousCE32(c);
            if(c<0) {
                return ce32;  // Collation::NO_CE
            }
        }
        if(data->isUnsafeBackward(c)) {
            return previousCEUnsafe(c);
        }
        // Simple, safe-backwards iteration:
        // Get a CE going backwards, handle prefixes but no contractions.
        int64_t ce;
        if(ce32<Collation::MIN_SPECIAL_CE32) {  // Forced-inline of isSpecialCE32(ce32).
            // Normal CE from the main data.
            ce=Collation::ceFromCE32(ce32);
        }
        const CollationData *d;
        if(ce32==Collation::MIN_SPECIAL_CE32) {
            ce32=base->getCE32(c);
            if(!Collation::isSpecialCE32(ce32)) {
                // Normal CE from the base data.
                return Collation::ceFromCE32(ce32);
            } else {
                d=base;
            }
        } else {
            d=data;
        }
        return previousCEFromSpecialCE32(d, c, ce32);
    }

    inline UChar32 nextCodePoint() {
        if(pos!=limit) {
            UChar32 c=*pos;
            if(c==0 && limit==NULL) {
                limit=pos;
                return U_SENTINEL;
            }
            ++pos;
            UChar trail;
            if(U16_IS_LEAD(c) && pos!=limit && U16_IS_TRAIL(trail=*pos)) {
                ++pos;
                return U16_GET_SUPPLEMENTARY(c, trail);
            } else {
                return c;
            }
        }
        return handleNextCodePoint();
    }

    inline UChar32 previousCodePoint() {
        if(pos!=start) {
            UChar32 c=*--pos;
            UChar lead;
            if(U16_IS_TRAIL(c) && pos!=start && U16_IS_LEAD(lead=*(pos-1))) {
                --pos;
                return U16_GET_SUPPLEMENTARY(lead, c);
            } else {
                return c;
            }
        }
        return handlePreviousCodePoint();
    }

    void forwardNumCodePoints(int32_t num) {
        while(num>0) {
            // Go forward in the inner buffer as far as possible.
            while(pos!=limit) {
                UChar32 c=*pos;
                if(c==0 && limit==NULL) {
                    limit=pos;
                    break;
                }
                ++pos;
                --num;
                if(U16_IS_LEAD(c) && pos!=limit && U16_IS_TRAIL(*pos)) {
                    ++pos;
                }
                if(num==0) {
                    return;
                }
            }
            // Try to go forward by one code point via the iterator,
            // then return to the inner buffer in case the subclass moved that forward.
            if(handleNextCodePoint()<0) {
                return;
            }
            --num;
        }
    }

    void backwardNumCodePoints(int32_t num) {
        while(num>0) {
            // Go backward in the inner buffer as far as possible.
            while(pos!=start) {
                UChar32 c=*--pos;
                --num;
                if(U16_IS_TRAIL(c) && pos!=start && U16_IS_LEAD(*(pos-1))) {
                    --pos;
                }
                if(num==0) {
                    return;
                }
            }
            // Try to go backward by one code point via the iterator,
            // then return to the inner buffer in case the subclass moved that backward.
            if(handlePreviousCodePoint()<0) {
                return;
            }
            --num;
        }
    }

    // TODO: setText(start, pos, limit)  ?

protected:
    /**
     * Returns U_SENTINEL and Collation::NO_CE if we are at the end of the input, or:
     * A subclass may have an implementation reading from an iterator
     * or checking for FCD on the fly etc.
     */
    virtual uint32_t handleNextCE32(UChar32 &c) {
        // Java: return a long with both
        // c (lower 32 bits, use cast to int) and ce32 (upper 32 bits, use >>>32).
        c=U_SENTINEL;
        return Collation::NO_CE;
    }

    virtual uint32_t handlePreviousCE32(UChar32 &c) {
        c=U_SENTINEL;
        return Collation::NO_CE;
    }

    virtual UChar32 handleNextCodePoint() {
        return U_SENTINEL;
    }

    virtual UChar32 handlePreviousCodePoint() {
        return U_SENTINEL;
    }

    /**
     * Saves the current iteration limit for later,
     * and sets it to after c which was read by previousCodePoint() or equivalent.
     */
    virtual void saveLimitAndSetAfter(UChar32 c) {
        savedLimit=limit;  // TODO: Need to do something special with limit==NULL?
        limit=pos+U16_LENGTH(c);
    }

    /** Restores the iteration limit from before saveLimitAndSetAfter(). */
    virtual void restoreLimit() {
        limit=savedLimit;
    }

    // Either data is for the base collator (typically UCA) and base is NULL,
    // or data is for a tailoring and base is not NULL.
    const CollationData *data;
    const CollationData *base;

    // UTF-16 string pointers.
    // limit can be NULL for NUL-terminated strings.
    // This class assumes that whole code points are stored within [start..limit[.
    // That is, a trail surrogate at start or a lead surrogate at limit-1
    // will be assumed to be surrogate code points rather than attempting to pair it
    // with a surrogate retrieved from the subclass.
    const UChar *start, *pos, *limit;
    // TODO: getter for limit, so that caller can find out length of NUL-terminated text?

    // See saveLimitAndSetAfter();
    const UChar *savedLimit;

    // Main lookup trie of the data object.
    const UTrie2 *trie;  // TODO: protected, for possibly optimized subclass CE lookup?

    // TODO: Do we need to support changing iteration direction? (ICU ticket #9104.)
    // If so, then nextCE() (rather, a "slow" version of it)
    // and previousCE() must count how many code points
    // resulted in their CE or ces[], and when they exhaust ces[] they need to
    // check if the signed code point count is in the right direction;
    // if not, move by that much in the opposite direction.
    // For example, if previousCE() read 3 code points, set ces[],
    // and then we change to nextCE() and it exhausts those ces[],
    // then we need to skip forward over those 3 code points before reading
    // more text.

private:
    int64_t nextCEFromSpecialCE32(const CollationData *data, UChar32 c, uint32_t ce32) const {
        for(;;) {  // Loop while ce32 is special.
            // TODO: Share code with previousCEFromSpecialCE32().
            int32_t tag=Collation::getSpecialCE32Tag(ce32);
            if(tag<=Collation::MAX_LATIN_EXPANSION_TAG) {
                U_ASSERT(ce32!=MIN_SPECIAL_CE32);
                setLatinExpansion(ce32);
                return ces[0];
            }
            switch(tag) {
            case Collation::EXPANSION32_TAG:
                setCE32s((ce32>>4)&0xffff, (int32_t)ce32&0xf);
                cesIndex= (cesMaxIndex>0) ? 1 : -1;
                return ces[0];
            case Collation::EXPANSION_TAG:
                ces=data->getCEs((ce32>>4)&0xffff);
                cesMaxIndex=(int32_t)ce32&0xf;
                cesIndex= (cesMaxIndex>0) ? 1 : -1;
                return ces[0];
            /**
            * Points to prefix trie.
            * Bits 19..0: Index into prefix/contraction data.
            */
            case Collation::PREFIX_TAG:
                // TODO
                return Collation::NO_CE;
            /**
            * Points to contraction data.
            * Bits 19..0: Index into prefix/contraction data.
            */
            case Collation::CONTRACTION_TAG:
                // TODO
                return Collation::NO_CE;
            /**
            * Used only in the collation data builder.
            * Data bits point into a builder-specific data structure with non-final data.
            */
            case Collation::BUILDER_CONTEXT_TAG:
                // TODO: Call virtual method.
                return Collation::NO_CE;
            /**
            * Unused.
            */
            case Collation::RESERVED_TAG:
                // TODO: Probably need UErrorCode, on the method or on the iterator?
                return Collation::NO_CE;
            case Collation::DIGIT_TAG:
                if(codan) {
                    // TODO
                    // int32_t digit=(int32_t)ce32&0xf;
                    return Collation::NO_CE;
                } else {
                    // Fetch the non-CODAN CE32 and continue.
                    ce32=data->getCE32s((ce32>>4)&0xffff)[0];
                    break;
                }
            case Collation::HANGUL_TAG:
                // TODO
                return Collation::NO_CE;
            case Collation::OFFSET_TAG:
                UChar32 baseCp=(c&0x1f0000)|(((UChar32)ce32>>4)&0xffff);
                int32_t increment=(ce32&0xf)+1;
                int32_t offset=c-baseCp;
                ce32=UTRIE2_GET32(trie, baseCp);
                // ce32 must be a long-primary pppppp00.
                U_ASSERT(!Collation::isSpecialCE32(ce32) && (ce32&0xff)==0);
                // TODO: Add offset*increment.
                return (int64_t)ce32<<32;
            case Collation::IMPLICIT_TAG:
                int64_t ce=unassignedPrimaryFromCodePoint(c);
                return (ce<<32)|Collation::COMMON_SEC_AND_TER_CE;
            }
            if(!Collation::isSpecialCE32(ce32)) {
                return Collation::ceFromCE32(ce32);
            }
        }
    }

    int64_t previousCEFromSpecialCE32(const CollationData *data, UChar32 c, uint32_t ce32) const {
        for(;;) {  // Loop while ce32 is special.
            int32_t tag=Collation::getSpecialCE32Tag(ce32);
            if(tag<=Collation::MAX_LATIN_EXPANSION_TAG) {
                U_ASSERT(ce32!=MIN_SPECIAL_CE32);
                setLatinExpansion(ce32);
                return ces[1];
            }
            switch(tag) {
            case Collation::EXPANSION32_TAG:
                setCE32s((ce32>>4)&0xffff, (int32_t)ce32&0xf);
                cesIndex=cesMaxIndex;
                return ces[cesMaxIndex];
            case Collation::EXPANSION_TAG:
                ces=data->getCEs((ce32>>4)&0xffff);
                cesMaxIndex=(int32_t)ce32&0xf;
                cesIndex=cesMaxIndex;
                return ces[cesMaxIndex];
            case Collation::PREFIX_TAG:
                // TODO
                return Collation::NO_CE;
            case Collation::CONTRACTION_TAG:
                // TODO: Must not occur. Backward contractions are handled by previousCEUnsafe().
                return Collation::NO_CE;
            case Collation::BUILDER_CONTEXT_TAG:
                // TODO: Call virtual method.
                return Collation::NO_CE;
            case Collation::RESERVED_TAG:
                // TODO: Probably need UErrorCode, on the method or on the iterator?
                return Collation::NO_CE;
            case Collation::DIGIT_TAG:
                if(codan) {
                    // TODO
                    // int32_t digit=(int32_t)ce32&0xf;
                    return Collation::NO_CE;
                } else {
                    // Fetch the non-CODAN CE32 and continue.
                    ce32=data->getCE32s((ce32>>4)&0xffff)[0];
                    break;
                }
            case Collation::HANGUL_TAG:
                // TODO
                return Collation::NO_CE;
            case Collation::OFFSET_TAG:
                // TODO: Share code with nextCEFromSpecialCE32().
                return Collation::NO_CE;
            case Collation::IMPLICIT_TAG:
                int64_t ce=unassignedPrimaryFromCodePoint(c);
                return (ce<<32)|Collation::COMMON_SEC_AND_TER_CE;  // TODO: Turn both lines together into a method.
            }
            if(!Collation::isSpecialCE32(ce32)) {
                return Collation::ceFromCE32(ce32);
            }
        }
    }

    /**
     * Returns the previous CE when data->isUnsafeBackward(c).
     */
    int64_t previousCEUnsafe(UChar32 c) {
        // We just move through the input counting safe and unsafe code points
        // without collecting the unsafe-backward substring into a buffer and
        // switching to it.
        // This is to keep the logic simple. Otherwise we would have to handle
        // prefix matching going before the backward buffer, switching
        // to iteration and back, etc.
        // In the most important case of iterating over a normal string,
        // reading from the string itself is already maximally fast.
        // The only drawback there is that after getting the CEs we always
        // skip backward to the safe character rather than switching out
        // of a backwardBuffer.
        // But this should not be the common case for previousCE(),
        // and correctness and maintainability are more important than
        // complex optimizations.
        saveLimitAndSetAfter(c);
        // Find the first safe character before c.
        int32_t numBackward=1;
        while(c=previousCodePoint()>=0) {
            ++numBackward;
            if(!data->isUnsafeBackward(c)) {
                break;
            }
        }
        // Ensure that we don't see CEs from a later-in-text expansion.
        cesIndex=-1;
        backwardCEs.clear();
        // Go forward and collect the CEs.
        int64_t ce;
        while(ce=nextCE()!=Collation::NO_CE) {
            backwardCEs.append(ce);
        }
        restoreLimit();
        backwardNumCodePoints(numBackward);
        // Use the collected CEs and return the last one.
        U_ASSERT(0!=backwardCEs.length());
        ces=backwardCEs.getBuffer();
        cesIndex=cesMaxIndex=backwardCEs.length()-1;
        return ces[cesIndex];
        // TODO: Does this method deliver backward-iteration offsets tight enough
        // for string search? Is this equivalent to how v1 behaves?
    }

    /**
     * Sets 2 buffered CEs from a Latin mini-expansion CE32.
     * Sets cesIndex=cesMaxIndex=1.
     */
    void setLatinExpansion(uint32_t ce32) {
        forwardCEs[0]=((int64_t)(ce32&0xff0000)<<40)|COMMON_SECONDARY_CE|(ce32&0xff00);
        forwardCEs[1]=((ce32&0xff)<<24)|COMMON_TERTIARY_CE;
        ces=forwardCEs.getBuffer();
        cesIndex=cesMaxIndex=1;
    }

    /**
     * Sets buffered CEs from CE32s.
     */
    void setCE32s(int32_t expIndex, int32_t max) {
        ces=forwardCEs.getBuffer();
        const uint32_t *ce32s=data->getCE32s(expIndex);
        cesMaxIndex=max;
        for(int32_t i=0; i<=max; ++i) {
            forwardCEs[i]=ceFromCE32(ce32s[i]);
        }
    }

    // List of CEs.
    const int64_t *ces;
    int32_t cesIndex, cesMaxIndex;

    // TODO: Flag/getter/setter for CODAN.

    // Buffer for skipped combining marks during discontiguous contraction handling.
    UnicodeString skipBuffer;

    // 64-bit-CE buffer for forward and safe-backward iteration
    // (computed expansions and CODAN CEs).
    MaybeStackArray/*??*/ forwardCEs;

    // 64-bit-CE buffer for unsafe-backward iteration.
    MaybeStackArray/*??*/ backwardCEs;

    // TODO: Flag or UErrorCode for memory allocation errors? protected?
};

class FCDCollationIterator : public CollationIterator {
    // TODO: On-the-fly FCD check.
    // Remember maximum known boundaries of already-FCD text.
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONITERATOR_H__
