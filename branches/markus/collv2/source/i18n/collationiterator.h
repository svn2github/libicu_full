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
class CEArray {
public:
    CEArray() {}
    ~CEArray();

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
    CEArray(const CEArray &);
    void operator=(const CEArray &);

    int32_t doAppend(int32_t length, int64_t ce, UErrorCode &errorCode);

    MaybeStackArray<int64_t, 40> buffer;
    // TODO: In the builder, limit the maximum expansion length to no more than
    // the initial length of this buffer. For example, length<=31.
};

/**
 * Collation element iterator and abstract character iterator.
 *
 * When a method returns a code point value, it must be in 0..10FFFF,
 * except it can be negative as a sentinel value.
 */
class U_I18N_API CollationIterator : public UObject {
public:
    CollationIterator(const CollationData *d)
            // Optimization: Skip initialization of fields that are not used
            // until they are set together with other state changes.
            : trie(d->trie),
              data(d),
              cesIndex(-1),  // cesMaxIndex(0), ces(NULL), -- unused while cesIndex<0
              skipped(NULL),
              numCpFwd(-1) {}

    virtual ~CollationIterator();

    const CollationData *getData() const { return data; }

    /**
     * Resets the iterator state and position to the beginning of the text.
     * Subclasses must override, and must call the parent class method.
     */
    virtual void resetToStart();

    /**
     * Returns the next collation element.
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
        UChar32 c;
        uint32_t ce32 = handleNextCE32(c, errorCode);
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
            if(c < 0) {
                return Collation::NO_CE;
            }
            d = data->base;
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
    // TODO: Jump by delta code points if direction changed? (See ICU ticket #9104.)
    // If so, then copy nextCE() to a not-inline slowNextCE()
    // which keeps track of the text movements together with previousCE()
    // and is used by the CollationElementIterator.
    // Keep the normal, inline nextCE() maximally fast and efficient.

    /**
     * Returns the previous collation element.
     */
    int64_t previousCE(CEArray &backwardCEs, UErrorCode &errorCode);

    /**
     * Returns the next code point (with post-increment).
     * Public for identical-level comparison and for testing.
     */
    virtual UChar32 nextCodePoint(UErrorCode &errorCode) = 0;

    /**
     * Returns the previous code point (with pre-decrement).
     * Public for identical-level comparison and for testing.
     */
    virtual UChar32 previousCodePoint(UErrorCode &errorCode) = 0;

protected:
    void reset();

    /**
     * Returns the next code point and its local CE32 value.
     * Returns Collation::MIN_SPECIAL_CE32 at the end of the text (c<0)
     * or when c's CE32 value is to be looked up in the base data (fallback).
     */
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    /**
     * Called when handleNextCE32() returns a LEAD_SURROGATE_TAG for a lead surrogate code unit.
     * Returns the trail surrogate in that case and advances past it,
     * if a trail surrogate follows the lead surrogate.
     * Otherwise returns any other code unit and does not advance.
     */
    virtual UChar handleGetTrailSurrogate();

    /**
     * Called when handleNextCE32() returns with c==0, to see whether it is a NUL terminator.
     * (Not needed in Java.)
     */
    virtual UBool foundNULTerminator();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode) = 0;

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode) = 0;

    // Main lookup trie of the data object.
    const UTrie2 *trie;
    const CollationData *data;

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

    uint32_t nextCE32FromContraction(
            const CollationData *d, UChar32 originalCp,
            const UChar *p, uint32_t ce32,
            UBool maybeDiscontiguous, UChar32 c,
            UErrorCode &errorCode);

    uint32_t nextCE32FromDiscontiguousContraction(
            const CollationData *d, UChar32 originalCp,
            UCharsTrie &suffixes, uint32_t ce32,
            int32_t lookAhead, UChar32 c,
            UErrorCode &errorCode);

    int64_t previousCEFromSpecialCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                      UErrorCode &errorCode);

    /**
     * Returns the previous CE when data->isUnsafeBackward(c).
     */
    int64_t previousCEUnsafe(CEArray &backwardCEs, UChar32 c, UErrorCode &errorCode);

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
     * Turns 1..254 digits into a sequence of CEs.
     * Called by setCodanCEs() for each segment of at most 254 digits.
     */
    void setCodanSegmentCEs(const char *digits, int32_t length, UErrorCode &errorCode);

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
    void setCE32s(const CollationData *d, int32_t expIndex, int32_t length);

    // List of CEs.
    int32_t cesIndex, cesMaxIndex;
    const int64_t *ces;

    SkippedState *skipped;

    // 64-bit-CE buffer for forward and safe-backward iteration
    // (computed expansions and CODAN CEs).
    CEArray forwardCEs;

    // Number of code points to read forward, or -1.
    // Used as a forward iteration limit in previousCEUnsafe().
    // TODO: Test this with contiguous & discontiguous contractions and digit specials!
    //       Try to start iterating backwards from all code point boundaries
    //       inside varied text.
    int32_t numCpFwd;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONITERATOR_H__
