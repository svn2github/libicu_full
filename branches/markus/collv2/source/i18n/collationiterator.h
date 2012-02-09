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
              ces(NULL), cesIndex(-1), cesLength(0),
              start(s), pos(s), limit(length>=0 ? s+length : NULL) {
        trie=data->getTrie();
    }

    /**
     * Returns the next collation element.
     * Optimized inline fastpath for the most common types of text and data.
     */
    inline int64_t nextCE() {
        if(cesIndex>=0) { return nextBufferedCE(); }
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

    // TODO: Separate handling of unsafe-backwards (check set, spanBack, collect CEs, deliver those backwards)
    // and simple/safe-backwards (get a CE going backwards, handle prefixes but no contractions).
    inline int64_t previousCE() {
        if(cesIndex>0) { return previousBufferedCE(); }
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
        // TODO: Check here data->isUnsafeBackward(c)?
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

    /**
     * TODO: Rethink
     *
     * Returns the previous CE when c has an UNSAFE_BACKWARD_TAG.
     * Must only be called by CollationData.previousCEFromSpecialCE32().
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
            if(!CollationData.isUnsafeCodePoint(c)) {
                break;
            }
        }
        // Ensure that we don't see CEs from a later-in-text expansion.
        cesIndex=-1;
        ceBuffer.clear();
        // Go forward and collect the CEs.
        int64_t ce;
        while(ce=nextCE()!=Collation::NO_CE) {
            ceBuffer.append(ce);
        }
        // TODO: assert 0!=ceBuffer.length()
        restoreLimit();
        backwardNumCodePoints(numBackward);
        // Use the collected CEs and return the last one.
        ces=ceBuffer.getBuffer();
        cesIndex=cesLength=ceBuffer.length();
        return ces[--cesIndex];
        // TODO: Does this method deliver backward-iteration offsets tight enough
        // for string search? Is this equivalent to how v1 behaves?
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

    inline void forwardNumCodePoints(int32_t num) {
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

    inline void backwardNumCodePoints(int32_t num) {
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
        // Java: return long with both c and ce32.
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
        savedLimit=limit;  // TODO: NULL?
        limit=pos+U16_LENGTH(c);
    }

    /** Restores the iteration limit from before saveLimitAndSetAfter(). */
    virtual void restoreLimit() {
        limit=savedLimit;
    }

    // UTF-16 string pointers.
    // limit can be NULL for NUL-terminated strings.
    // This class assumes that whole code points are stored within [start..limit[.
    // That is, a trail surrogate at start or a lead surrogate at limit-1
    // will be assumed to be surrogate code points rather than attempting to pair it
    // with a surrogate retrieved from the subclass.
    const UChar *start, *pos, *limit;
    // TODO: getter for limit, so that caller can find out length of NUL-terminated text?

    // Main lookup trie of the data object.
    const UTrie2 *trie;  // TODO: protected, for possibly optimized subclass CE lookup?

    // TODO: Do we need to support switching iteration direction?
    // If so, then nextCE() and previousCE() must count how many code points
    // resulted in their CE or ces[], and when they exhaust ces[] they need to
    // check if the signed code point count is in the right direction;
    // if not, move by that much in the opposite direction.
    // For example, if previousCE() read 3 code points, set ces[],
    // and then we switch to nextCE() and it exhausts those ces[],
    // then we need to skip forward over those 3 code points before reading
    // more text.
    // We might be able to isolate the direction-switching code in a separate
    // function, although the normal code probably has to track the code point offset.

private:
    /**
     * Returns the next buffered CE.
     * Requires 0<=cesIndex<=cesMaxIndex.
     */
    int64_t nextBufferedCE() {
        int64_t ce;
        if(ces!=NULL) {
            ce=ces[cesIndex];
        } else {
            ce=ceFromCE32(ce32s[cesIndex]);
        }
        if(cesIndex<cesMaxIndex) {
            ++cesIndex;
        } else {
            cesIndex=-1;
        }
        // TODO: Jump by delta code points if direction changed?
        return ce;
    }

    /**
     * Returns the previous buffered CE.
     * Requires cesIndex>0.
     */
    int64_t previousBufferedCE() {
        --cesIndex;
        int64_t ce;
        if(ces!=NULL) {
            ce=ces[cesIndex];
        } else {
            ce=ceFromCE32(ce32s[cesIndex]);
        }
        if(cesIndex==0) { cesIndex=-1; }
        // TODO: Jump by delta code points if direction changed?
        return ce;
    }

    int64_t nextCEFromSpecialCE32(const CollationData *data, UChar32 c, uint32_t ce32) const {
        int32_t tag=getSpecialCE32Tag(ce32);
        if(tag<=Collation::MAX_LATIN_EXPANSION_TAG) {
            U_ASSERT(ce32!=MIN_SPECIAL_CE32);
            setNextCE(((ce32&0xff)<<24)|COMMON_TERTIARY_CE);  // TODO: Store both CEs?
            return ((int64_t)(ce32&0xff0000)<<40)|COMMON_SECONDARY_CE|(ce32&0xff00);
        }
        for(;;) {  // loop while ce32 is special
            // TODO: Share code with previousCEFromSpecialCE32().
            switch(tag) {
            case Collation::EXPANSION32_TAG:
                int32_t index=(ce32>>4)&0xffff;
                setCE32s(uint32s+index+1, ce32&0xf);
                return ceFromCE32[index];
            case Collation::EXPANSION_TAG:
                int32_t index=(ce32>>4)&0xffff;
                setCEs(ces+index+1, ce32&0xf);
                return ces[index];
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
            /**
            * Decimal digit.
            * Bits 19..4: Index into uint32_t table for non-CODAN CE32.
            * Bits  3..0: Digit value 0..9.
            */
            case Collation::DIGIT_TAG:
                // TODO
                return Collation::NO_CE;
            /**
            * Tag for a Hangul syllable. TODO: or Jamo?
            * TODO: data?
            */
            case Collation::HANGUL_TAG:
                // TODO
                return Collation::NO_CE;
            /**
            * Tag for CEs with primary weights in code point order.
            * Bits 19..4: Lower 16 bits of the base code point.
            * Bits  3..0: Per-code point primary-weight increment minus 1.
            */
            case Collation::OFFSET_TAG:
                UChar32 baseCp=(c&0x1f0000)|(((UChar32)ce32>>4)&0xffff);
                int32_t increment=(ce32&0xf)+1;
                int32_t offset=c-baseCp;
                ce32=UTRIE2_GET32(trie, baseCp);
                // TODO: ce32 must be a long-primary pppppp00. Add offset*increment.
                return (int64_t)ce32<<32;
            case Collation::IMPLICIT_TAG:
                int64_t ce=unassignedPrimaryFromCodePoint(c);
                return (ce<<32)|Collation::COMMON_SEC_AND_TER_CE;
            }
        }
    }

    int64_t previousCEFromSpecialCE32(const CollationData *data, UChar32 c, uint32_t ce32) const {
        return Collation::NO_CE;  // TODO
    }

    // Lists of CEs. At most one is not NULL.
    const int64_t *ces;
    const uint32_t *ce32s;
    int32_t cesIndex, cesMaxIndex;

    // Either data is for the base collator (typically UCA) and base is NULL,
    // or data is for a tailoring and base is not NULL.
    const CollationData *data;
    const CollationData *base;

    // TODO: Flag/getter/setter for CODAN.

    // Buffer for skipped combining marks during discontiguous contraction handling.
    UnicodeString skipBuffer;

    // See saveLimitAndSetAfter();
    const UChar *savedLimit;

    // 64-bit-CE buffer for backward iteration and other complicated cases.
    // TODO: MaybeStackArray? UVector64?

    // TODO: Flag or UErrorCode for memory allocation errors? protected?
};

class FCDCollationIterator : public CollationIterator {
    // TODO: On-the-fly FCD check.
    // Remember maximum known boundaries of already-FCD text.
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONITERATOR_H__
