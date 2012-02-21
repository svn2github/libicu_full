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
    CollationIterator(const Normalizer2Impl &nfc,
                      const CollationData *d,
                      const UChar *s, const UChar *lim)
            // Optimization: Skip initialization of fields that are not used
            // until they are set together with other state changes.
            : start(s), pos(s), limit(lim),
              // unused until a limit is saved -- savedLimit(NULL),
              nfcImpl(nfc),
              cesIndex(-1),  // unused while cesIndex<0 -- cesMaxIndex(0), ces(NULL),
              hiragana(0),
              data(d) {
        trie = data->getTrie();
    }

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
            if(c == 0 && limit == NULL) {
                // Handle NUL-termination. (Not needed in Java.)
                limit = --pos;
                return Collation::NO_CE;
            }
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
    int8_t getHiragana() const { return hiragana; }

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

    void forwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
        while(num > 0) {
            // Go forward in the inner buffer as far as possible.
            while(pos != limit) {
                UChar32 c = *pos;
                if(c == 0 && limit == NULL) {
                    limit = pos;
                    break;
                }
                ++pos;
                --num;
                if(U16_IS_LEAD(c) && pos != limit && U16_IS_TRAIL(*pos)) {
                    ++pos;
                }
                if(num == 0) {
                    return;
                }
            }
            // Try to go forward by one code point via the iterator,
            // then return to the inner buffer in case the subclass moved that forward.
            if(handleNextCodePoint(errorCode) < 0) {
                return;
            }
            --num;
        }
    }

    void backwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
        while(num > 0) {
            // Go backward in the inner buffer as far as possible.
            while(pos != start) {
                UChar32 c = *--pos;
                --num;
                if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(*(pos-1))) {
                    --pos;
                }
                if(num == 0) {
                    return;
                }
            }
            // Try to go backward by one code point via the iterator,
            // then return to the inner buffer in case the subclass moved that backward.
            if(handlePreviousCodePoint(errorCode) < 0) {
                return;
            }
            --num;
        }
    }

    // TODO: setText(start, pos, limit)  ?

protected:
    /**
     * Returns the next code point, or <0 if none, assuming pos==limit.
     * Post-increment semantics.
     */
    virtual UChar32 handleNextCodePoint(UErrorCode &errorCode) {
        return U_SENTINEL;
    }

    /**
     * Returns the previous code point, or <0 if none, assuming pos==start.
     * Pre-decrement semantics.
     */
    virtual UChar32 handlePreviousCodePoint(UErrorCode &errorCode) {
        return U_SENTINEL;
    }

    /**
     * Saves the current iteration limit for later,
     * and sets it to after c which was read by previousCodePoint() or equivalent.
     */
    virtual void saveLimitAndSetAfter(UChar32 c) {
        savedLimit = limit;  // TODO: Need to do something special with limit==NULL?
        limit = pos + U16_LENGTH(c);
    }

    /** Restores the iteration limit from before saveLimitAndSetAfter(). */
    virtual void restoreLimit() {
        limit = savedLimit;
    }

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

    const Normalizer2Impl &nfcImpl;

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
                                  UErrorCode &errorCode) const {
        for(;;) {  // Loop while ce32 is special.
            // TODO: Share code with previousCEFromSpecialCE32().
            int32_t tag = Collation::getSpecialCE32Tag(ce32);
            if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
                U_ASSERT(ce32 != MIN_SPECIAL_CE32);
                setLatinExpansion(ce32);
                return ces[0];
            }
            switch(tag) {
            case Collation::EXPANSION32_TAG:
                setCE32s((ce32 >> 4) & 0xffff, (int32_t)ce32 & 0xf);
                cesIndex = (cesMaxIndex > 0) ? 1 : -1;
                return ces[0];
            case Collation::EXPANSION_TAG:
                ces = d->getCEs((ce32 >> 4) & 0xffff);
                cesMaxIndex = (int32_t)ce32 & 0xf;
                cesIndex = (cesMaxIndex > 0) ? 1 : -1;
                return ces[0];
            /**
            * Points to prefix trie.
            * Bits 19..0: Index into prefix/contraction data.
            */
            case Collation::PREFIX_TAG:
                // TODO
                return 0;
            case Collation::CONTRACTION_TAG:
                ce32 = nextCE32FromContraction(d, ce32, errorCode);
                if(ce32 != 1) {
                    // Normal result from contiguous contraction.
                    break;
                } else {
                    // CEs from a discontiguous contraction plus the skipped combining marks.
                    return ces[0];
                }
            /**
            * Used only in the collation data builder.
            * Data bits point into a builder-specific data structure with non-final data.
            */
            case Collation::BUILDER_CONTEXT_TAG:
                // TODO: Call virtual method.
                return 0;
            case Collation::DIGIT_TAG:
                if(codan) {
                    // TODO
                    // int32_t digit=(int32_t)ce32&0xf;
                    return 0;
                } else {
                    // Fetch the non-CODAN CE32 and continue.
                    ce32 = *d->getCE32s((ce32 >> 4) & 0xffff);
                    break;
                }
            case Collation::HIRAGANA_TAG:
                hiragana = (0x3099 <= c && c <= 0x309c) ? -1 : 1;
                // Fetch the normal CE32 and continue.
                ce32 = *d->getCE32s(ce32 & 0xfffff);
                break;
            case Collation::HANGUL_TAG:
                // TODO
                return 0;
            case Collation::OFFSET_TAG:
                UChar32 baseCp = (c & 0x1f0000) | (((UChar32)ce32 >> 4) & 0xffff);
                int32_t increment = (ce32 & 0xf) + 1;
                int32_t offset = c - baseCp;
                ce32=UTRIE2_GET32(trie, baseCp);
                // ce32 must be a long-primary pppppp00.
                U_ASSERT(!Collation::isSpecialCE32(ce32) && (ce32 & 0xff) == 0);
                // TODO: Add offset*increment.
                return (int64_t)ce32 << 32;
            case Collation::IMPLICIT_TAG:
                int64_t ce = unassignedPrimaryFromCodePoint(c);
                return (ce << 32) | Collation::COMMON_SEC_AND_TER_CE;
            }
            if(!Collation::isSpecialCE32(ce32)) {
                return Collation::ceFromCE32(ce32);
            }
        }
    }

    uint32_t nextCE32FromContraction(const CollationData *d, uint32_t ce32,
                                     UErrorCode &errorCode) const {
        UBool maybeDiscontiguous = (UBool)(ce32 & 1);  // TODO: Builder set this if any suffix ends with cc != 0.
        const uint16_t *p = d->getContext((int32_t)(ce32 >> 1) & 0x7ffff);
        ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
        UChar32 lowestChar = p[2];
        p += 3;
        UChar32 c = nextCodePoint(errorCode);
        if(c < 0) {
            // No more text.
            return ce32;
        }
        if(c < lowestChar) {
            // The next code point is too low for either a match
            // or a combining mark that would be skipped in discontiguous contraction.
            // TODO: Builder:
            // lowestChar = lowest code point of the first ones that could start a match.
            // If this is a combining mark with combining class c1, then set instead to the lowest character
            // that has combining class in the range 1..c1-1.
            // If the character is supplemental, set to U+FFFF.
            // TODO: This is simpler at runtime than checking for the combining class then,
            // and might be good enough for Latin performance. Keep or redesign?
            backwardNumCodePoints(1, errorCode);
            return ce32;
        }
        // Number of code points read beyond the original code point.
        // Only needed as input to nextCE32FromDiscontiguousContraction().
        int32_t lookAhead = 1;
        // Number of code points read since the last match value.
        int32_t sinceMatch = 1;
        // Assume that we only need a contiguous match,
        // and therefore need not remember the suffixes state from before a mismatch for retrying.
        UCharsTrie suffixes(p);
        UStringTrieResult match = suffixes.firstForCodePoint(c);
        for(;;) {
            if(match == USTRINGTRIE_NO_MATCH) {
                if(maybeDiscontiguous) {
                    return nextCE32FromDiscontiguousContraction(
                        d, p, ce32, lookAhead, sinceMatch, c, errorCode);
                }
                break;
            }
            if(USTRINGTRIE_HAS_VALUE(match)) {
                ce32 = (uint32_t)suffixes.getValue();
                sinceMatch = 0;
            }
            if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
            if((c = nextCodePoint(errorCode)) < 0) { break; }
            ++lookAhead;
            ++sinceMatch;
            match = suffixes.nextForCodePoint(c);
        }
        backwardNumCodePoints(sinceMatch, errorCode);
        return ce32;
    }
    // TODO: How can we match the second code point of a precomposed Tibetan vowel mark??
    //    Add contractions with the skipped mark as an expansion??
    //    Forbid contractions with the problematic characters??

    uint32_t nextCE32FromDiscontiguousContraction(
            const CollationData *d, const uint16_t *p, uint32_t ce32,
            int32_t lookAhead, int32_t sinceMatch, UChar32 c,
            UErrorCode &errorCode) const {
        // UCA 3.3.2 Contractions:
        // Contractions that end with non-starter characters
        // are known as discontiguous contractions.
        // ... discontiguous contractions must be detected in input text
        // whenever the final sequence of non-starter characters could be rearranged
        // so as to make a contiguous matching sequence that is canonically equivalent.

        // UCA: http://www.unicode.org/reports/tr10/#S2.1
        // S2.1 Find the longest initial substring S at each point that has a match in the table.
        // S2.1.1 If there are any non-starters following S, process each non-starter C.
        // S2.1.2 If C is not blocked from S, find if S + C has a match in the table.
        //     Note: A non-starter in a string is called blocked
        //     if there is another non-starter of the same canonical combining class or zero
        //     between it and the last character of canonical combining class 0.
        // S2.1.3 If there is a match, replace S by S + C, and remove C.

        // First: Is a discontiguous contraction even possible?
        uint16_t fcd16 = nfcImpl.getFCD16(c);
        UChar32 nextCp;
        if(fcd16 <= 0xff || (nextCp = nextCodePoint(errorCode)) < 0) {
            // The non-matching c is a starter (which blocks all further non-starters),
            // or there is no further text.
            backwardNumCodePoints(sinceMatch, errorCode);
            return ce32;
        }
        ++lookAhead;
        ++sinceMatch;
        uint8_t prevCC = (uint8_t)fcd16;
        fcd16 = nfcImpl.getFCD16(nextCp);
        if(prevCC >= (fcd16 >> 8)) {
            // The next code point after c is a starter (S2.1.1 "process each non-starter"),
            // or blocked by c (S2.1.2).
            backwardNumCodePoints(sinceMatch, errorCode);
            return ce32;
        }

        // We have read and matched (lookAhead-2) code points,
        // read non-matching c and peeked ahead at nextCp.
        // Replay the partial match so far, return to the state before the mismatch,
        // and continue matching with nextCp.
        UCharsTrie suffixes(p);
        if(lookAhead > 2) {
            backwardNumCodePoints(lookAhead, errorCode);
            suffixes.firstForCodePoint(nextCodePoint(errorCode));
            for(int32_t i = 3; i < lookAhead; ++i) {
                suffixes.nextForCodePoint(nextCodePoint(errorCode));
            }
            // Skip c (which did not match) and nextCp (which we will try now).
            forwardNumCodePoints(2, errorCode);
        }

        // Buffer for skipped combining marks.
        UnicodeString skipBuffer(c);
        int32_t skipLengthAtMatch = 0;
        UCharsTrie.State state;
        suffixes.saveState(state);
        c = nextCp;
        for(;;) {
            prevCC = (uint8_t)fcd16;
            UStringTrieResult match = suffixes.nextForCodePoint(c);
            if(match == USTRINGTRIE_NO_MATCH) {
                skipBuffer.append(c);
                suffixes.resetToState(state);
            } else {
                if(USTRINGTRIE_HAS_VALUE(match)) {
                    ce32 = (uint32_t)suffixes.getValue();
                    sinceMatch = 0;
                    skipLengthAtMatch = skipBuffer.length();
                }
                if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
                suffixes.saveState(state);
            }
            if((c = nextCodePoint(errorCode)) < 0) { break; }
            ++sinceMatch;
            fcd16 = nfcImpl.getFCD16(c);
            if(prevCC >= (fcd16 >> 8)) {
                // The next code point after c is a starter (S2.1.1 "process each non-starter"),
                // or blocked by c (S2.1.2).
                break;
            }
        }
        if(skipLengthAtMatch > 0) {
            // We did get a match after skipping one or more combining marks.
            // TODO: Map ce32 to ces[].
            // Append CEs from the combining marks that we skipped before the match.
            for(int32_t i = 0; i < skipLengthAtMatch; i += U16_LENGTH(c)) {
                c = skipBuffer.char32At(i);
                ce32 = d->getCE32(c);  // TODO: fallback?
                // TODO: ce32 -> ces[]
            }
            ce32 = 1;  // Signal to nextCEFromSpecialCE32() that the result is in ces[].
        }
        backwardNumCodePoints(sinceMatch, errorCode);
        return ce32;
    }

    /**
     * Sets 2 buffered CEs from a Latin mini-expansion CE32.
     * Sets cesIndex=cesMaxIndex=1.
     */
    void setLatinExpansion(uint32_t ce32) {
        forwardCEs[0] = ((int64_t)(ce32 & 0xff0000) << 40) | COMMON_SECONDARY_CE | (ce32 & 0xff00);
        forwardCEs[1] = ((ce32 & 0xff) << 24) | COMMON_TERTIARY_CE;
        ces = forwardCEs.getBuffer();
        cesIndex = cesMaxIndex = 1;
    }

    /**
     * Sets buffered CEs from CE32s.
     */
    void setCE32s(const CollationData *d, int32_t expIndex, int32_t max) {
        ces = forwardCEs.getBuffer();
        const uint32_t *ce32s = d->getCE32s(expIndex);
        cesMaxIndex = max;
        for(int32_t i = 0; i <= max; ++i) {
            forwardCEs[i] = ceFromCE32(ce32s[i]);
        }
    }

    // Main lookup trie of the data object.
    const UTrie2 *trie;

    // List of CEs.
    int32_t cesIndex, cesMaxIndex;
    const int64_t *ces;

    int8_t hiragana;

    // TODO: Flag/getter/setter for CODAN.

    const CollationData *data;

    // 64-bit-CE buffer for forward and safe-backward iteration
    // (computed expansions and CODAN CEs).
    MaybeStackArray/*??*/ forwardCEs;
};

/**
 * Checks the input text for FCD, passes already-FCD segments into the base iterator,
 * and normalizes other segments on the fly.
 */
class U_I18N_API FCDCollationIterator : public CollationIterator {
public:
    FCDCollationIterator(const Normalizer2Impl &nfc,
                         const CollationData *data,
                         const UChar *s, const UChar *lim,
                         UErrorCode &errorCode)
            : CollationIterator(nfc, data, s, s),
              rawStart(s), segmentStart(s), segmentLimit(s), rawLimit(lim),
              smallSteps(TRUE),
              buffer(nfc, fcd) {
        if(U_SUCCESS(errorCode)) {
            buffer.init(2, errorCode);
        }
    }

    void setSmallSteps(UBool small) { smallSteps = small; }

protected:
    virtual UChar32 handleNextCodePoint(UErrorCode &errorCode) {
        if(U_FAILURE(errorCode) || segmentLimit == rawLimit) { return U_SENTINEL; }
        if(limit != segmentLimit) {
            // The previous segment had to be normalized
            // and was pointing into the fcd string.
            start = pos = limit = segmentLimit;
        }
        segmentStart = segmentLimit;
        const UChar *p = segmentLimit;
        uint8_t prevCC = 0;
        for(;;) {
            // So far, we have limit<=segmentLimit<=p,
            // [limit, p[ passes the FCD test,
            // and segmentLimit is at the last FCD boundary before or on p.
            // Advance p by one code point, fetch its fcd16 value,
            // and continue the incremental FCD test.
            const UChar *q = p;
            uint16_t fcd16 = nfcImpl.nextFCD16(p, rawLimit);
            if(fcd16 == 0) {
                if(*(p - 1) == 0) {
                    if(rawLimit == NULL) {
                        // We hit the NUL terminator; remember its pointer.
                        segmentLimit = rawLimit == --p;
                        if(limit == rawLimit) { return U_SENTINEL; }
                        break;
                    }
                } else if(p != rawLimit && U16_IS_TRAIL(*p)) {
                    // nextFCD16() probably just skipped a lead surrogate
                    // rather than the whole code point.
                    ++p;
                }
                segmentLimit = p;
                prevCC = 0;
            } else {
                uint8_t leadCC = (uint8_t)(fcd16 >> 8);
                if(prevCC > leadCC && leadCC != 0) {
                    // Fails FCD test.
                    if(limit != segmentLimit) {
                        // Deliver the already-FCD text segment so far.
                        limit = segmentLimit;
                        break;
                    }
                    // Find the next FCD boundary and normalize.
                    do {
                        segmentLimit = p;
                    } while((fcd16 & 0xff) > 1 &&
                            p != rawLimit && (fcd16 = nfcImpl.nextFCD16(p, rawLimit)) > 0xff);
                    buffer.remove();
                    nfcImpl.makeFCD(limit, segmentLimit, &buffer, errorCode);
                    if(U_FAILURE(errorCode)) { return U_SENTINEL; }
                    // Switch collation processing into the FCD buffer
                    // with the result of normalizing [limit, segmentLimit[.
                    start = pos = buffer.getStart();
                    limit = buffer.getLimit();
                    break;
                }
                prevCC = (uint8_t)(fcd16 & 0xff);
                if(prevCC <= 1) {
                    segmentLimit = p;  // FCD boundary after the [q, p[ code point.
                } else if(leadCC == 0) {
                    segmentLimit = q;  // FCD boundary before the [q, p[ code point.
                }
            }
            if(p == rawLimit) {
                limit = segmentLimit = p;
                break;
            }
            if(smallSteps && (segmentLimit - limit) >= 5) {
                // Compromise: During string comparison, where differences are often
                // found after a few characters, we do not want to read ahead too far.
                // However, we do want to go forward several characters
                // which will then be handled in the base class fastpath.
                limit = segmentLimit;
                break;
            }
        }
        //  Return the next code point at pos != limit; no need to check for NUL-termination.
        UChar32 c = *pos++;
        UChar trail;
        if(U16_IS_LEAD(c) && pos != limit && U16_IS_TRAIL(trail = *pos)) {
            ++pos;
            return U16_GET_SUPPLEMENTARY(c, trail);
        } else {
            return c;
        }
    }

    virtual UChar32 handlePreviousCodePoint(UErrorCode &errorCode) {
        if(U_FAILURE(errorCode) || segmentStart == rawStart) { return U_SENTINEL; }
        if(start != segmentStart) {
            // The previous segment had to be normalized
            // and was pointing into the fcd string.
            start = pos = limit = segmentStart;
        }
        segmentLimit = segmentStart;
        const UChar *p = segmentStart;
        uint8_t nextCC = 0;
        for(;;) {
            // So far, we have p<=segmentStart<=start,
            // [p, start[ passes the FCD test,
            // and segmentStart is at the first FCD boundary on or after p.
            // Go back with p by one code point, fetch its fcd16 value,
            // and continue the incremental FCD test.
            const UChar *q = p;
            uint16_t fcd16 = nfcImpl.previousFCD16(rawStart, p);
            if(fcd16 == 0) {
                segmentStart = p;
                nextCC = 0;
            } else {
                uint8_t trailCC = (uint8_t)fcd16;
                if(trailCC > nextCC && nextCC != 0) {
                    // Fails FCD test.
                    if(start != segmentStart) {
                        // Deliver the already-FCD text segment so far.
                        start = segmentStart;
                        break;
                    }
                    // Find the next FCD boundary and normalize.
                    do {
                        segmentStart = p;
                    } while(fcd16 > 0xff &&
                            p != rawStart && ((fcd16 = nfcImpl.previousFCD16(rawStart, p)) & 0xff) > 1);
                    buffer.remove();
                    nfcImpl.makeFCD(segmentStart, start, &buffer, errorCode);
                    if(U_FAILURE(errorCode)) { return U_SENTINEL; }
                    // Switch collation processing into the FCD buffer
                    // with the result of normalizing [segmentStart, start[.
                    start = pos = buffer.getStart();
                    limit = buffer.getStart();
                    break;
                }
                nextCC = (uint8_t)(fcd16 >> 8);
                if(nextCC == 0) {
                    segmentStart = p;  // FCD boundary before the [p, q[ code point.
                } else if(trailCC <= 1) {
                    segmentStart = q;  // FCD boundary after the [p, q[ code point.
                }
            }
            if(p == rawStart) {
                start = segmentStart = p;
                break;
            }
        }
        // Return the previous code point before pos != start.
        UChar32 c = *--pos;
        UChar lead;
        if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(lead = *(pos - 1))) {
            --pos;
            return U16_GET_SUPPLEMENTARY(lead, c);
        } else {
            return c;
        }
    }

    virtual void saveLimitAndSetAfter(UChar32 c) {
        // TODO
        // savedLimit = limit;  // TODO: Need to do something special with limit==NULL?
        // limit = pos + U16_LENGTH(c);
    }

    /** Restores the iteration limit from before saveLimitAndSetAfter(). */
    virtual void restoreLimit() {
        // TODO
        // limit = savedLimit;
    }

private:
    // Text pointers: The input text is [rawStart, rawLimit[
    // where rawLimit can be NULL for NUL-terminated text.
    // segmentStart and segmentLimit point into the text and indicate
    // the start and exclusive end of the text segment currently being processed.
    // They are at FCD boundaries.
    // Either the current text segment already passes the FCD test
    // and segmentStart==start<=pos<=limit==segmentLimit,
    // or the current segment had to be normalized so that
    // [segmentStart, segmentLimit[ turned into the fcd string,
    // corresponding to buffer.getStart()==start<=pos<=limit==buffer.getLimit().
    const UChar *rawStart;
    const UChar *segmentStart;
    const UChar *segmentLimit;
    // rawLimit==NULL for a NUL-terminated string.
    const UChar *rawLimit;
    // We make small steps for string comparisons and larger steps for sort key generation.
    UBool smallSteps;
    UnicodeString fcd;
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

    // TODO: So far, this is just the initial code collection moved out of the
    // base CollationIterator.
    // Add delegation to "fwd." etc.
    // Move this implementation into a separate .cpp file.

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
    int64_t previousCE(UErrorCode &errorCode) {
        if(cesIndex > 0) {
            // Return the previous buffered CE.
            int64_t ce = ces[--cesIndex];
            if(cesIndex == 0) { cesIndex = -1; }
            // TODO: Jump by delta code points if the direction changed?
            return ce;
        }
        // TODO: Do we need to handle hiragana going backward?
        // Note: v1 ucol_IGetPrevCE() does not handle 3099..309C inheriting the Hiragana-ness from the preceding character.
        UChar32 c;
        uint32_t ce32;
        if(pos != start) {
            UTRIE2_U16_PREV32(trie, start, pos, c, ce32);
        } else {
            c = handlePreviousCodePoint(errorCode);
            if(c < 0) {
                return Collation::NO_CE;
            }
            ce32 = data->getCE32(c);
        }
        if(data->isUnsafeBackward(c)) {
            return previousCEUnsafe(c);
        }
        // Simple, safe-backwards iteration:
        // Get a CE going backwards, handle prefixes but no contractions.
        int64_t ce;
        if(ce32 < Collation::MIN_SPECIAL_CE32) {  // Forced-inline of isSpecialCE32(ce32).
            // Normal CE from the main data.
            ce = Collation::ceFromCE32(ce32);
        }
        const CollationData *d;
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
        return previousCEFromSpecialCE32(d, c, ce32, errorCode);
    }

private:
    int64_t previousCEFromSpecialCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                      UErrorCode &errorCode) const {
        for(;;) {  // Loop while ce32 is special.
            int32_t tag = Collation::getSpecialCE32Tag(ce32);
            if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
                U_ASSERT(ce32 != MIN_SPECIAL_CE32);
                setLatinExpansion(ce32);
                return ces[1];
            }
            switch(tag) {
            case Collation::EXPANSION32_TAG:
                setCE32s((ce32 >> 4) & 0xffff, (int32_t)ce32 & 0xf);
                cesIndex = cesMaxIndex;
                return ces[cesMaxIndex];
            case Collation::EXPANSION_TAG:
                ces = d->getCEs((ce32 >> 4) & 0xffff);
                cesMaxIndex = (int32_t)ce32 & 0xf;
                cesIndex = cesMaxIndex;
                return ces[cesMaxIndex];
            case Collation::PREFIX_TAG:
                // TODO
                return 0;
            case Collation::CONTRACTION_TAG:
                // TODO: Must not occur. Backward contractions are handled by previousCEUnsafe().
                return 0;
            case Collation::BUILDER_CONTEXT_TAG:
                // TODO: Call virtual method.
                return 0;
            case Collation::DIGIT_TAG:
                if(codan) {
                    // TODO
                    // int32_t digit=(int32_t)ce32&0xf;
                    return 0;
                } else {
                    // Fetch the non-CODAN CE32 and continue.
                    ce32 = *d->getCE32s((ce32 >> 4) & 0xffff);
                    break;
                }
            case Collation::HIRAGANA_TAG:
                // TODO: Do we need to handle hiragana going backward?
                // Fetch the normal CE32 and continue.
                ce32 = *d->getCE32s(ce32 & 0xfffff);
                break;
            case Collation::HANGUL_TAG:
                // TODO
                return 0;
            case Collation::OFFSET_TAG:
                // TODO: Share code with nextCEFromSpecialCE32().
                return 0;
            case Collation::IMPLICIT_TAG:
                int64_t ce = unassignedPrimaryFromCodePoint(c);
                return (ce << 32) | Collation::COMMON_SEC_AND_TER_CE;  // TODO: Turn both lines together into a method.
            }
            if(!Collation::isSpecialCE32(ce32)) {
                return Collation::ceFromCE32(ce32);
            }
        }
    }

    /**
     * Returns the previous CE when data->isUnsafeBackward(c).
     */
    int64_t previousCEUnsafe(UChar32 c, UErrorCode &errorCode) {
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
        int32_t numBackward = 1;
        while(c = previousCodePoint(errorCode) >= 0) {
            ++numBackward;
            if(!data->isUnsafeBackward(c)) {
                break;
            }
        }
        // Ensure that we don't see CEs from a later-in-text expansion.
        cesIndex = -1;
        backwardCEs.clear();
        // Go forward and collect the CEs.
        int64_t ce;
        while(ce = nextCE(errorCode) != Collation::NO_CE) {
            backwardCEs.append(ce, errorCode);
        }
        restoreLimit();
        backwardNumCodePoints(numBackward, errorCode);
        // Use the collected CEs and return the last one.
        U_ASSERT(0 != backwardCEs.length());
        ces = backwardCEs.getBuffer();
        cesIndex = cesMaxIndex=backwardCEs.length() - 1;
        return ces[cesIndex];
        // TODO: Does this method deliver backward-iteration offsets tight enough
        // for string search? Is this equivalent to how v1 behaves?
    }

    CollationIterator &fwd;

    // 64-bit-CE buffer for unsafe-backward iteration.
    MaybeStackArray/*??*/ backwardCEs;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONITERATOR_H__
