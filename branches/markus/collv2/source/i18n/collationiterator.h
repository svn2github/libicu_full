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

#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "normalizer2impl.h"

U_NAMESPACE_BEGIN

class SkippedState;
class UCharsTrie;

/**
 * Buffer for CEs.
 * Rather than its own position and length fields,
 * we share the cesIndex and cesMaxIndex of the CollationIterator.
 */
class CEBuffer {
public:
    CEBuffer() {}

    inline int32_t append(int32_t length, int64_t ce, UErrorCode &errorCode) {
        if(length < buffer.getCapacity()) {
            buffer[length++] = ce;
            return length;
        } else {
            return doAppend(length, ce, errorCode);
        }
    }

    inline int64_t &operator[](ptrdiff_t i) { return buffer[i]; }

    inline const int64_t *getBuffer() const { return buffer.getAlias(); }

private:
    CEBuffer(const CEBuffer &);
    void operator=(const CEBuffer &);

    int32_t doAppend(int32_t length, int64_t ce, UErrorCode &errorCode);

    MaybeStackArray<int64_t, 40> buffer;
};

/**
 * Collation element and character iterator.
 * Handles normalized UTF-16 text inline, with length or NUL-terminated.
 * Other text is handled by subclasses.
 *
 * This iterator only moves forward through CE space.
 * For random access, use the TwoWayCollationIterator.
 *
 * Forward iteration and random access are separate for
 * - minimal state & setup for the forward-only iterator
 * - modularization
 * - smaller units of code, easier to understand
 * - easier porting of partial functionality to other languages
 */
class U_I18N_API CollationIterator : public UObject {
public:
    CollationIterator(const CollationData *d, int8_t iterFlags,
                      const UChar *s, const UChar *lim)
            // Optimization: Skip initialization of fields that are not used
            // until they are set together with other state changes.
            : start(s), pos(s), limit(lim),
              flags(iterFlags),
              trie(d->getTrie()),
              cesIndex(-1),  // cesMaxIndex(0), ces(NULL), -- unused while cesIndex<0
              hiragana(0),
              data(d),
              skipped(NULL) {}

    inline void setFlags(int8_t f) { flags = f; }

    /**
     * Returns the next collation element.
     * Optimized inline fastpath for the most common types of text and data.
     */
    inline int64_t nextCE(UErrorCode &errorCode) {
        if(cesIndex >= 0) {
            // Return the next buffered CE.
            int64_t ce = ces[cesIndex];
            if(cesIndex < cesMaxIndex) {
                ++cesIndex;
            } else {
                cesIndex = -1;
            }
            return ce;
        }
        hiragana = 0;
        UChar32 c;
        uint32_t ce32;
        if(pos != limit) {
            UTRIE2_U16_NEXT32(trie, pos, limit, c, ce32);
        } else {
            c = handleNextCodePoint(errorCode);
            if(c < 0) {
                return Collation::NO_CE;
            }
            ce32 = data->getCE32(c);
        }
        // Java: Emulate unsigned-int less-than comparison.
        // int xce32 = ce32 ^ 0x80000000;
        // if(xce32 < 0x7f000000) { special }
        // if(xce32 == 0x7f000000) { fallback }
        if(ce32 < Collation::MIN_SPECIAL_CE32) {  // Forced-inline of isSpecialCE32(ce32).
            // Normal CE from the main data.
            return Collation::ceFromCE32(ce32);
        }
        const CollationData *d;
        // The compiler should be able to optimize the previous and the following
        // comparisons of ce32 with the same constant.
        if(ce32 == Collation::MIN_SPECIAL_CE32) {
            d = data->getBase();
            ce32 = d->getCE32(c);
            if(!Collation::isSpecialCE32(ce32)) {
                // Normal CE from the base data.
                return Collation::ceFromCE32(ce32);
            }
        } else {
            d = data;
        }
        return nextCEFromSpecialCE32(d, c, ce32, errorCode);
    }

    /**
     * Returns the Hiragana flag.
     * The caller must remember the previous flag value.
     * @return -1 inherit Hiragana-ness from previous character;
     *         0 not Hiragana; 1 Hiragana
     */
    inline int8_t getHiragana() const { return hiragana; }

    inline UChar32 nextCodePoint(UErrorCode &errorCode) {
        if(pos != limit) {
            UChar32 c = *pos;
            if(c == 0 && limit == NULL) {
                limit = pos;
                return U_SENTINEL;
            }
            ++pos;
            UChar trail;
            if(U16_IS_LEAD(c) && pos != limit && U16_IS_TRAIL(trail = *pos)) {
                ++pos;
                return U16_GET_SUPPLEMENTARY(c, trail);
            } else {
                return c;
            }
        }
        return handleNextCodePoint(errorCode);
    }

    inline UChar32 previousCodePoint(UErrorCode &errorCode) {
        if(pos != start) {
            UChar32 c = *--pos;
            UChar lead;
            if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(lead = *(pos - 1))) {
                --pos;
                return U16_GET_SUPPLEMENTARY(lead, c);
            } else {
                return c;
            }
        }
        return handlePreviousCodePoint(errorCode);
    }

    void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    // TODO: setText(start, pos, limit)  ?

protected:
    /**
     * Returns the next code point, or <0 if none, assuming pos==limit.
     * Post-increment semantics.
     */
    virtual UChar32 handleNextCodePoint(UErrorCode &errorCode);

    /**
     * Returns the previous code point, or <0 if none, assuming pos==start.
     * Pre-decrement semantics.
     */
    virtual UChar32 handlePreviousCodePoint(UErrorCode &errorCode);

    /**
     * Saves the current iteration limit for later,
     * and sets it to after c which was read by previousCodePoint() or equivalent.
     */
    virtual const UChar *saveLimitAndSetAfter(UChar32 c);

    /** Restores the iteration limit from before saveLimitAndSetAfter(). */
    virtual void restoreLimit(const UChar *savedLimit);

    // UTF-16 string pointers.
    // limit can be NULL for NUL-terminated strings.
    // This class assumes that whole code points are stored within [start..limit[.
    // That is, a trail surrogate at start or a lead surrogate at limit-1
    // will be assumed to be surrogate code points rather than attempting to pair it
    // with a surrogate retrieved from the subclass.
    const UChar *start, *pos, *limit;
    // TODO: getter for limit, so that caller can find out length of NUL-terminated text?

    int8_t flags;

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
    friend class TwoWayCollationIterator;

    int64_t nextCEFromSpecialCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                  UErrorCode &errorCode);

    /**
     * Computes a CE from c's ce32 which has the OFFSET_TAG.
     */
    static int64_t getCEFromOffsetCE32(const CollationData *d, UChar32 c, uint32_t ce32);

    uint32_t getCE32FromPrefix(const CollationData *d, uint32_t ce32,
                               UErrorCode &errorCode);

    UChar32 nextSkippedCodePoint(UErrorCode &errorCode);

    void backwardNumSkipped(int32_t n, UErrorCode &errorCode);

    uint32_t nextCE32FromContraction(const CollationData *d, UChar32 originalCp, uint32_t ce32,
                                     UErrorCode &errorCode);

    uint32_t nextCE32FromDiscontiguousContraction(
            const CollationData *d, UChar32 originalCp,
            UCharsTrie &suffixes, uint32_t ce32,
            int32_t lookAhead, UChar32 c,
            UErrorCode &errorCode);

    /**
     * Appends CEs for a combining mark that was skipped in discontiguous contraction.
     */
    void appendCEsFromCp(UChar32 c, UErrorCode &errorCode);

    /**
     * Appends CEs for a contraction result CE32,
     * or for the CE32 of a combining mark that was skipped in discontiguous contraction.
     */
    void appendCEsFromCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                           UErrorCode &errorCode);

    /**
     * Turns a string of digits (bytes 0..9)
     * into a sequence of CEs that will sort in numeric order.
     * CODAN = COllate Digits As Numbers.
     *
     * Sets ces and cesMaxIndex.
     *
     * The digits string must not be empty and must not have leading zeros.
     */
    void setCodanCEs(const char *digits, int32_t length, UErrorCode &errorCode);

    /**
     * Sets 2 or 3 buffered CEs from a Hangul syllable,
     * assuming that the constituent Jamos all have non-special CE32s.
     * Otherwise DECOMP_HANGUL would have to be set.
     *
     * Sets cesMaxIndex as necessary.
     * Does not set cesIndex;
     * caller needs to set cesIndex=1 for forward iteration,
     * or cesIndex=cesMaxIndex for backward iteration.
     */
    void setHangulExpansion(UChar32 c);

    /**
     * Sets 2 buffered CEs from a Latin mini-expansion CE32.
     * Sets cesIndex=cesMaxIndex=1.
     */
    void setLatinExpansion(uint32_t ce32);

    /**
     * Sets buffered CEs from CE32s.
     */
    void setCE32s(const CollationData *d, int32_t expIndex, int32_t max);

    // Main lookup trie of the data object.
    const UTrie2 *trie;

    // List of CEs.
    int32_t cesIndex, cesMaxIndex;
    const int64_t *ces;

    int8_t hiragana;

    const CollationData *data;
    SkippedState *skipped;

    // 64-bit-CE buffer for forward and safe-backward iteration
    // (computed expansions and CODAN CEs).
    CEBuffer forwardCEs;
};

/**
 * Checks the input text for FCD, passes already-FCD segments into the base iterator,
 * and normalizes other segments on the fly.
 */
class U_I18N_API FCDCollationIterator : public CollationIterator {
public:
    FCDCollationIterator(const CollationData *data, int8_t iterFlags,
                         const UChar *s, const UChar *lim,
                         UErrorCode &errorCode);

    inline void setSmallSteps(UBool small) { smallSteps = small; }

protected:
    virtual UChar32 handleNextCodePoint(UErrorCode &errorCode);

    inline UChar32 simpleNext() {
        UChar32 c = *pos++;
        UChar trail;
        if(U16_IS_LEAD(c) && pos != limit && U16_IS_TRAIL(trail = *pos)) {
            ++pos;
            return U16_GET_SUPPLEMENTARY(c, trail);
        } else {
            return c;
        }
    }

    UChar32 nextCodePointDecompHangul(UErrorCode &errorCode);

    virtual UChar32 handlePreviousCodePoint(UErrorCode &errorCode);

    inline UChar32 simplePrevious() {
        UChar32 c = *--pos;
        UChar lead;
        if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(lead = *(pos - 1))) {
            --pos;
            return U16_GET_SUPPLEMENTARY(lead, c);
        } else {
            return c;
        }
    }

    UChar32 previousCodePointDecompHangul(UErrorCode &errorCode);

    virtual const UChar *saveLimitAndSetAfter(UChar32 c);

    virtual void restoreLimit(const UChar * /* savedLimit */);

private:
    // Text pointers: The input text is [rawStart, rawLimit[
    // where rawLimit can be NULL for NUL-terminated text.
    // segmentStart and segmentLimit point into the text and indicate
    // the start and exclusive end of the text segment currently being processed.
    // They are at FCD boundaries.
    // Either the current text segment already passes the FCD test
    // and segmentStart==start<=pos<=limit==segmentLimit,
    // or the current segment had to be normalized so that
    // [segmentStart, segmentLimit[ turned into the normalized string,
    // corresponding to buffer.getStart()==start<=pos<=limit==buffer.getLimit().
    const UChar *rawStart;
    const UChar *segmentStart;
    const UChar *segmentLimit;
    // rawLimit==NULL for a NUL-terminated string.
    const UChar *rawLimit;
    // Normally zero.
    // Between calls to saveLimitAndSetAfter() and restoreLimit(),
    // it tracks the positive number of normalized UChars
    // between the start pointer and the temporary iteration limit.
    int32_t lengthBeforeLimit;
    // We make small steps for string comparisons and larger steps for sort key generation.
    UBool smallSteps;

    const Normalizer2Impl &nfcImpl;
    UnicodeString normalized;
    ReorderingBuffer buffer;
};

/**
 * Collation element and character iterator.
 * This iterator delegates to another CollationIterator
 * for forward iteration and character iteration,
 * and adds API for backward iteration.
 */
class U_I18N_API TwoWayCollationIterator : public UObject {
public:
    TwoWayCollationIterator(CollationIterator &iter) : fwd(iter) {}

    int64_t nextCE(UErrorCode &errorCode) {
        return fwd.nextCE(errorCode);
    }
    // TODO: Jump by delta code points if direction changed? (See ICU ticket #9104.)
    // If so, then copy nextCE() to a not-inline slowNextCE()
    // which keeps track of the text movements together with previousCE()
    // and is used by the CollationElementIterator.
    // Keep the normal, inline nextCE() maximally fast and efficient.

    /**
     * Returns the previous collation element.
     */
    int64_t previousCE(UErrorCode &errorCode);

private:
    int64_t previousCEFromSpecialCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                      UErrorCode &errorCode);

    /**
     * Returns the previous CE when data->isUnsafeBackward(c).
     */
    int64_t previousCEUnsafe(UChar32 c, UErrorCode &errorCode);

    CollationIterator &fwd;

    // 64-bit-CE buffer for unsafe-backward iteration.
    CEBuffer backwardCEs;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONITERATOR_H__
