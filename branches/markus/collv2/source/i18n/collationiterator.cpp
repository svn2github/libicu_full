/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationiterator.cpp
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucharstrie.h"
#include "unicode/ustringtrie.h"
#include "charstr.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "normalizer2impl.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

int32_t
CEBuffer::doAppend(int32_t length, int64_t ce, UErrorCode &errorCode) {
    // length == buffer.getCapacity()
    if(U_FAILURE(errorCode)) { return length; }
    int32_t capacity = buffer.getCapacity();
    int32_t newCapacity;
    if(capacity < 1000) {
        newCapacity = 4 * capacity;
    } else {
        newCapacity = 2 * capacity;
    }
    int64_t *p = buffer.resize(newCapacity, length);
    if(p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    } else {
        p[length++] = ce;
    }
    return length;
}

// State of combining marks skipped in discontiguous contraction.
// We create a state object on first use and keep it around deactivated between uses.
class SkippedState {
public:
    // Born active but empty.
    SkippedState() : pos(0), skipLengthAtMatch(0) {}
    void clear() {
        oldBuffer.remove();
        pos = 0;
        // The newBuffer is reset by setFirstSkipped().
    }

    UBool isEmpty() const { return oldBuffer.isEmpty(); }

    UBool hasNext() const { return pos < oldBuffer.length(); }

    // Requires hasNext().
    UChar32 next() {
        UChar32 c = oldBuffer.char32At(pos);
        pos += U16_LENGTH(c);
        return c;
    }

    // Accounts for one more input code point read beyond the end of the marks buffer.
    void incBeyond() {
        U_ASSERT(!hasNext());
        ++pos;
    }

    // Goes backward through the skipped-marks buffer.
    // Returns the number of code points read beyond the skipped marks
    // that need to be backtracked through normal input.
    int32_t backwardNumCodePoints(int32_t n) {
        int32_t length = oldBuffer.length();
        int32_t beyond = pos - length;
        if(beyond > 0) {
            if(beyond >= n) {
                // Not back far enough to re-enter the oldBuffer.
                pos -= n;
                return n;
            } else {
                // Back out all beyond-oldBuffer code points and re-enter the buffer.
                pos = oldBuffer.moveIndex32(length, beyond - n);
                return beyond;
            }
        } else {
            // Go backwards from inside the oldBuffer.
            pos = oldBuffer.moveIndex32(pos, -n);
            return 0;
        }
    }

    void setFirstSkipped(UChar32 c) {
        skipLengthAtMatch = 0;
        newBuffer.setTo(c);
    }

    void skip(UChar32 c) {
        newBuffer.append(c);
    }

    void recordMatch() { skipLengthAtMatch = newBuffer.length(); }

    // Replaces the characters we consumed with the newly skipped ones.
    void replaceMatch() {
        // Note: UnicodeString.replace() pins pos to at most length().
        oldBuffer.replace(0, pos, newBuffer, 0, skipLengthAtMatch);
        pos = 0;
    }

    void saveTrieState(const UCharsTrie &trie) { trie.saveState(state); }
    void resetToTrieState(UCharsTrie &trie) const { trie.resetToState(state); }

private:
    // Combining marks skipped in previous discontiguous-contraction matching.
    // After that discontiguous contraction was completed, we start reading them from here.
    UnicodeString oldBuffer;
    // Combining marks newly skipped in current discontiguous-contraction matching.
    // These might have been read from the normal text or from the oldBuffer.
    UnicodeString newBuffer;
    // Reading index in oldBuffer,
    // or counter for how many code points have been read beyond oldBuffer (pos-oldBuffer.length()).
    int32_t pos;
    // newBuffer.length() at the time of the last matching character.
    // When a partial match fails, we back out skipped and partial-matching input characters.
    int32_t skipLengthAtMatch;
    // We save the trie state before we attempt to match a character,
    // so that we can skip it and try the next one.
    UCharsTrie::State state;
};

void
CollationIterator::forwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
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

void
CollationIterator::backwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
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

UChar32
CollationIterator::handleNextCodePoint(UErrorCode & /* errorCode */) {
    U_ASSERT(pos == limit);
    return U_SENTINEL;
}

UChar32
CollationIterator::handlePreviousCodePoint(UErrorCode & /* errorCode */) {
    U_ASSERT(pos == start);
    return U_SENTINEL;
}

const UChar *
CollationIterator::saveLimitAndSetAfter(UChar32 c) {
    const UChar *savedLimit = limit;
    limit = pos + U16_LENGTH(c);
    return savedLimit;
}

void
CollationIterator::restoreLimit(const UChar *savedLimit) {
    limit = savedLimit;
}

int64_t
CollationIterator::nextCEFromSpecialCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                         UErrorCode &errorCode) {
    for(;;) {  // Loop while ce32 is special.
        int32_t tag = Collation::getSpecialCE32Tag(ce32);
        if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
            U_ASSERT(ce32 != Collation::MIN_SPECIAL_CE32);
            setLatinExpansion(ce32);
            return ces[0];
        }
        switch(tag) {
        case Collation::EXPANSION32_TAG:
            setCE32s(d, (ce32 >> 3) & 0x1ffff, (int32_t)ce32 & 7);
            cesIndex = (cesMaxIndex > 0) ? 1 : -1;
            return ces[0];
        case Collation::EXPANSION_TAG: {
            ces = d->getCEs((ce32 >> 3) & 0x1ffff);
            int32_t length = (int32_t)ce32 & 7;
            if(length == 0) { length = (int32_t)*ces++; }
            cesMaxIndex = length - 1;
            cesIndex = (cesMaxIndex > 0) ? 1 : -1;
            return ces[0];
        }
        case Collation::PREFIX_TAG:
            backwardNumCodePoints(1, errorCode);
            ce32 = getCE32FromPrefix(d, ce32, errorCode);
            forwardNumCodePoints(1, errorCode);
            break;
        case Collation::CONTRACTION_TAG:
            ce32 = nextCE32FromContraction(d, c, ce32, errorCode);
            if(ce32 != 0x100) {
                // Normal result from contiguous contraction.
                break;
            } else {
                // CEs from a discontiguous contraction plus the skipped combining marks.
                return ces[0];
            }
        case Collation::BUILDER_CONTEXT_TAG:
            // Used only in the collation data builder.
            // Data bits point into a builder-specific data structure with non-final data.
            ce32 = d->nextCE32FromBuilderContext(*this, ce32, errorCode);
            break;
        case Collation::DIGIT_TAG:
            if(flags & Collation::CODAN) {
                // Collect digits, omit leading zeros.
                CharString digits;
                for(;;) {
                    char digit = (char)(ce32 & 0xf);
                    if(digit != 0 || !digits.isEmpty()) {
                        digits.append(digit, errorCode);
                    }
                    c = nextCodePoint(errorCode);
                    if(c < 0) { break; }
                    ce32 = data->getCE32(c);
                    if(ce32 == Collation::MIN_SPECIAL_CE32) {
                        ce32 = data->getBase()->getCE32(c);
                    }
                    if(!Collation::isSpecialCE32(ce32) ||
                        Collation::DIGIT_TAG != Collation::getSpecialCE32Tag(ce32)
                    ) {
                        backwardNumCodePoints(1, errorCode);
                        break;
                    }
                }
                int32_t length = digits.length();
                if(length == 0) {
                    // A string of only "leading" zeros.
                    // Just use the NUL terminator in the digits buffer.
                    length = 1;
                }
                setCodanCEs(digits.data(), length, errorCode);
                cesIndex = (cesMaxIndex > 0) ? 1 : -1;
                return ces[0];
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
            setHangulExpansion(ce32);
            cesIndex = 1;
            return ces[0];
        case Collation::OFFSET_TAG:
            return getCEFromOffsetCE32(d, c, ce32);
        case Collation::IMPLICIT_TAG:
            if((ce32 & 1) == 0) {
                U_ASSERT(c == 0);
                if(limit == NULL) {
                    // Handle NUL-termination. (Not needed in Java.)
                    limit = --pos;
                    return Collation::NO_CE;
                } else {
                    // Fetch the normal ce32 for U+0000 and continue.
                    ce32 = *d->getCE32s(0);
                    break;
                }
            } else {
                return Collation::unassignedCEFromCodePoint(c);
            }
        }
        if(!Collation::isSpecialCE32(ce32)) {
            return Collation::ceFromCE32(ce32);
        }
    }
}

int64_t
CollationIterator::getCEFromOffsetCE32(const CollationData *d, UChar32 c, uint32_t ce32) {
    UChar32 baseCp = (c & 0x1f0000) | (((UChar32)ce32 >> 4) & 0xffff);
    int32_t offset = (c - baseCp) * ((ce32 & 0xf) + 1);  // delta * increment
    ce32 = d->getCE32(baseCp);
    // ce32 must be a long-primary pppppp01.
    U_ASSERT(!Collation::isSpecialCE32(ce32) && (ce32 & 0xff) == 1);
    --ce32;  // Turn the long-primary CE32 into a primary weight pppppp00.
    return Collation::getCEFromThreeByteOffset(ce32, d->isCompressiblePrimary(ce32), offset);
}

uint32_t
CollationIterator::getCE32FromPrefix(const CollationData *d, uint32_t ce32,
                                     UErrorCode &errorCode) {
    const uint16_t *p = d->getContext((int32_t)ce32 & 0xfffff);
    ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no prefix match.
    p += 2;
    // Number of code points read before the original code point.
    int32_t lookBehind = 0;
    UCharsTrie prefixes(p);
    for(;;) {
        UChar32 c = previousCodePoint(errorCode);
        if(c < 0) { break; }
        ++lookBehind;
        UStringTrieResult match = prefixes.nextForCodePoint(c);
        if(USTRINGTRIE_HAS_VALUE(match)) {
            ce32 = (uint32_t)prefixes.getValue();
        }
        if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
    }
    forwardNumCodePoints(lookBehind, errorCode);
    return ce32;
}

UChar32
CollationIterator::nextSkippedCodePoint(UErrorCode &errorCode) {
    if(skipped != NULL && skipped->hasNext()) { return skipped->next(); }
    UChar32 c = nextCodePoint(errorCode);
    if(skipped != NULL && !skipped->isEmpty() && c >= 0) { skipped->incBeyond(); }
    return c;
}

void
CollationIterator::backwardNumSkipped(int32_t n, UErrorCode &errorCode) {
    if(skipped != NULL && !skipped->isEmpty()) {
        n = skipped->backwardNumCodePoints(n);
    }
    backwardNumCodePoints(n, errorCode);
}

uint32_t
CollationIterator::nextCE32FromContraction(const CollationData *d, UChar32 originalCp, uint32_t ce32,
                                           UErrorCode &errorCode) {
    // originalCp: Only needed as input to nextCE32FromDiscontiguousContraction().
    UBool maybeDiscontiguous = (UBool)(ce32 & 1);
    // TODO: Builder set this if any suffix ends with cc != 0.
    // TODO: Slightly better would be if we knew the max trailing cc of any suffix.
    // Then we could check if the non-matching character's cc<maxCC.
    // The cost would be an additional 16-bit unit before the trie.
    // The benefit might be limited if the maxCC is normally high.
    const uint16_t *p = d->getContext((int32_t)(ce32 >> 1) & 0x7ffff);
    ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
    UChar32 lowestChar = p[2];
    p += 3;
    UChar32 c = nextSkippedCodePoint(errorCode);
    if(c < 0) {
        // No more text.
        return ce32;
    }
    if(c < lowestChar) {
        // The next code point is too low for either a match
        // or a combining mark that would be skipped in discontiguous contraction.
        // TODO: Builder:
        // lowestChar = lowest code point of the first ones that could start a match.
        // If the character is supplemental, set to U+FFFF.
        // If there are combining marks, then find the lowest combining class c1 among them,
        // then set instead to the lowest character (below what we have so far)
        // that has combining class in the range 1..c1-1.
        // Actually test characters with lccc!=0 and look for their tccc=1..c1-1.
        // TODO: This is simpler at runtime than checking for the combining class then,
        // and might be good enough for Latin performance. Keep or redesign?
        backwardNumSkipped(1, errorCode);
        return ce32;
    }
    // Number of code points read beyond the original code point.
    // Only needed as input to nextCE32FromDiscontiguousContraction().
    int32_t lookAhead = 1;
    // Normally we only need a contiguous match,
    // and therefore need not remember the suffixes state from before a mismatch for retrying.
    // If we are already processing skipped combining marks, then we do track the state.
    UCharsTrie suffixes(p);
    if(skipped != NULL && !skipped->isEmpty()) { skipped->saveTrieState(suffixes); }
    UStringTrieResult match = suffixes.firstForCodePoint(c);
    for(;;) {
        if(match == USTRINGTRIE_NO_MATCH) {
            if(maybeDiscontiguous) {
                return nextCE32FromDiscontiguousContraction(
                    d, originalCp, suffixes, ce32, lookAhead, c, errorCode);
            }
            backwardNumSkipped(1, errorCode);
            break;
        }
        U_ASSERT(match != USTRINGTRIE_NO_VALUE);  // same as USTRINGTRIE_HAS_VALUE(match)
        ce32 = (uint32_t)suffixes.getValue();
        if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
        if((c = nextSkippedCodePoint(errorCode)) < 0) { break; }
        ++lookAhead;
        if(skipped != NULL && !skipped->isEmpty()) { skipped->saveTrieState(suffixes); }
        match = suffixes.nextForCodePoint(c);
    }
    return ce32;
}
// TODO: How can we match the second code point of a precomposed Tibetan vowel mark??
//    Add contractions with the skipped mark as an expansion??
//    Forbid contractions with the problematic characters??

uint32_t
CollationIterator::nextCE32FromDiscontiguousContraction(
        const CollationData *d, UChar32 originalCp,
        UCharsTrie &suffixes, uint32_t ce32,
        int32_t lookAhead, UChar32 c,
        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }

    // UCA section 3.3.2 Contractions:
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
    uint16_t fcd16 = d->getFCD16(c);
    UChar32 nextCp;
    if(fcd16 <= 0xff || (nextCp = nextSkippedCodePoint(errorCode)) < 0) {
        // The non-matching c is a starter (which blocks all further non-starters),
        // or there is no further text.
        backwardNumSkipped(1, errorCode);
        return ce32;
    }
    ++lookAhead;
    uint8_t prevCC = (uint8_t)fcd16;
    fcd16 = d->getFCD16(nextCp);
    if(prevCC >= (fcd16 >> 8)) {
        // The next code point after c is a starter (S2.1.1 "process each non-starter"),
        // or blocked by c (S2.1.2).
        backwardNumSkipped(2, errorCode);
        return ce32;
    }

    // We have read and matched (lookAhead-2) code points,
    // read non-matching c and peeked ahead at nextCp.
    // Return to the state before the mismatch and continue matching with nextCp.
    if(skipped == NULL || skipped->isEmpty()) {
        if(skipped == NULL) {
            skipped = new SkippedState();
            if(skipped == NULL) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return 0;
            }
        }
        suffixes.reset();
        if(lookAhead > 2) {
            // Replay the partial match so far.
            backwardNumCodePoints(lookAhead, errorCode);
            suffixes.firstForCodePoint(nextCodePoint(errorCode));
            for(int32_t i = 3; i < lookAhead; ++i) {
                suffixes.nextForCodePoint(nextCodePoint(errorCode));
            }
            // Skip c (which did not match) and nextCp (which we will try now).
            forwardNumCodePoints(2, errorCode);
        }
        skipped->saveTrieState(suffixes);
    } else {
        // Reset to the trie state before the failed match of c.
        skipped->resetToTrieState(suffixes);
    }

    skipped->setFirstSkipped(c);
    // Number of code points read since the last match (at this point: c and nextCp).
    int32_t sinceMatch = 2;
    c = nextCp;
    for(;;) {
        prevCC = (uint8_t)fcd16;
        UStringTrieResult match = suffixes.nextForCodePoint(c);
        if(match == USTRINGTRIE_NO_MATCH) {
            skipped->skip(c);
            skipped->resetToTrieState(suffixes);
        } else {
            U_ASSERT(match != USTRINGTRIE_NO_VALUE);  // same as USTRINGTRIE_HAS_VALUE(match)
            ce32 = (uint32_t)suffixes.getValue();
            sinceMatch = 0;
            skipped->recordMatch();
            if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
            skipped->saveTrieState(suffixes);
        }
        if((c = nextSkippedCodePoint(errorCode)) < 0) { break; }
        ++sinceMatch;
        fcd16 = d->getFCD16(c);
        if(prevCC >= (fcd16 >> 8)) {
            // The next code point after c is a starter (S2.1.1 "process each non-starter"),
            // or blocked by c (S2.1.2).
            break;
        }
    }
    backwardNumSkipped(sinceMatch, errorCode);
    UBool isTopDiscontiguous = skipped->isEmpty();
    skipped->replaceMatch();
    if(isTopDiscontiguous && !skipped->isEmpty()) {
        // We did get a match after skipping one or more combining marks,
        // and we are not in a recursive discontiguous contraction.
        // Append CEs from the contraction ce32
        // and then from the combining marks that we skipped before the match.
        cesMaxIndex = -1;
        appendCEsFromCE32(d, originalCp, ce32, errorCode);
        // Fetch CE32s for skipped combining marks from the normal data, with fallback,
        // rather than from the CollationData where we found the contraction.
        while(skipped->hasNext()) {
            appendCEsFromCp(skipped->next(), errorCode);
            // Note: A nested discontiguous-contraction match
            // replaces consumed combining marks with newly skipped ones
            // and resets the reading position to the beginning.
        }
        skipped->clear();
        cesIndex = 1;  // Caller returns ces[0].
        ce32 = 0x100;  // Signal to nextCEFromSpecialCE32() that the result is in ces[].
    }
    return ce32;
}

void
CollationIterator::appendCEsFromCp(UChar32 c, UErrorCode &errorCode) {
    const CollationData *d;
    uint32_t ce32 = data->getCE32(c);
    if(ce32 == Collation::MIN_SPECIAL_CE32) {
        d = data->getBase();
        ce32 = d->getCE32(c);
    } else {
        d = data;
    }
    appendCEsFromCE32(d, c, ce32, errorCode);
}

void
CollationIterator::appendCEsFromCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                     UErrorCode &errorCode) {
    int32_t cesLength = cesMaxIndex + 1;
    int64_t ce;
    for(;;) {  // Loop while ce32 is special.
        if(!Collation::isSpecialCE32(ce32)) {
            ce = Collation::ceFromCE32(ce32);
            break;
        }
        int32_t tag = Collation::getSpecialCE32Tag(ce32);
        if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
            U_ASSERT(ce32 != Collation::MIN_SPECIAL_CE32);
            cesLength = forwardCEs.append(cesLength,
                ((int64_t)(ce32 & 0xff0000) << 40) |
                Collation::COMMON_SECONDARY_CE |
                (ce32 & 0xff00), errorCode);
            ce = ((ce32 & 0xff) << 24) | Collation::COMMON_TERTIARY_CE;
            break;
        // if-else-if rather than switch so that "break;" leaves the loop.
        } else if(tag == Collation::EXPANSION32_TAG) {
            const uint32_t *ce32s = d->getCE32s((int32_t)(ce32 >> 3) & 0x1ffff);
            int32_t length = (int32_t)ce32 & 7;
            if(length == 0) { length = (int32_t)*ce32s++; }
            for(int32_t i = 0; i < length; ++i) {
                cesLength = forwardCEs.append(cesLength, Collation::ceFromCE32(ce32s[i]), errorCode);
            }
            cesMaxIndex = cesLength - 1;
            return;
        } else if(tag == Collation::EXPANSION_TAG) {
            const int64_t *expCEs = d->getCEs((int32_t)(ce32 >> 3) & 0x1ffff);
            int32_t length = (int32_t)ce32 & 7;
            if(length == 0) { length = (int32_t)*expCEs++; }
            for(int32_t i = 0; i < length; ++i) {
                cesLength = forwardCEs.append(cesLength, expCEs[i], errorCode);
            }
            cesMaxIndex = cesLength - 1;
            return;
        } else if(tag == Collation::CONTRACTION_TAG) {
            ce32 = nextCE32FromContraction(d, c, ce32, errorCode);
            U_ASSERT(ce32 != 0x100);
            // continue
        } else if(tag == Collation::OFFSET_TAG) {
            ce = getCEFromOffsetCE32(d, c, ce32);
            break;
        } else if(tag == Collation::IMPLICIT_TAG) {
            U_ASSERT((ce32 & 1) != 0);
            ce = Collation::unassignedCEFromCodePoint(c);
            break;
        } else {
        // case Collation::PREFIX_TAG:
        // case Collation::BUILDER_CONTEXT_TAG:
        // case Collation::DIGIT_TAG:
        // case Collation::HIRAGANA_TAG:
        // case Collation::HANGUL_TAG:
            if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
            return;
        }
    }
    cesMaxIndex = forwardCEs.append(cesLength, ce, errorCode) - 1;
}

void
CollationIterator::setCodanCEs(const char *digits, int32_t length, UErrorCode &errorCode) {
    cesMaxIndex = 0;
    if(U_FAILURE(errorCode)) {
        forwardCEs[0] = 0;
        return;
    }
    U_ASSERT(length > 0);
    U_ASSERT(length == 1 || digits[0] != 0);
    uint32_t zeroPrimary = data->getZeroPrimary();
    // Note: We use primary byte values 3..255: digits are not compressible.
    if(length <= 5) {
        // Very dense encoding for small numbers.
        int32_t value = digits[0];
        for(int32_t i = 1; i < length; ++i) {
            value = value * 10 + digits[i];
        }
        if(value <= 31) {
            // Two-byte primary for 0..31, good for days & months.
            uint32_t primary = zeroPrimary | ((3 + value) << 16);
            forwardCEs[0] = ((int64_t)primary << 32) | Collation::COMMON_SEC_AND_TER_CE;
            return;
        }
        value -= 32;
        if(value < 40 * 253) {
            // Three-byte primary for 32..10151, good for years.
            // 10151 = 32+40*253-1
            uint32_t primary = zeroPrimary |
                ((3 + 32 + value / 253) << 16) | ((3 + value % 253) << 8);
            forwardCEs[0] = ((int64_t)primary << 32) | Collation::COMMON_SEC_AND_TER_CE;
            return;
        }
    }
    // value > 10151, length >= 5

    // The second primary byte 75..255 indicates the number of digit pairs (3..183),
    // then we generate primary bytes with those pairs.
    // Omit trailing 00 pairs.
    // Decrement the value for the last pair.
    if(length > 2 * 183) {
        // Overflow
        uint32_t primary = zeroPrimary | 0xffff00;
        forwardCEs[0] = ((int64_t)primary << 32) | Collation::COMMON_SEC_AND_TER_CE;
        return;
    }
    // Set the exponent. 3 pairs->75, 4 pairs->76, ..., 183 pairs->255.
    int32_t numPairs = (length + 1) / 2;
    uint32_t primary = zeroPrimary | ((75 - 3 + numPairs) << 16);
    // Find the length without trailing 00 pairs.
    while(digits[length - 1] == 0 && digits[length - 2] == 0) {
        length -= 2;
    }
    // Read the first pair.
    uint32_t pair;
    int32_t pos;
    if(length & 1) {
        // Only "half a pair" if we have an odd number of digits.
        pair = digits[0];
        pos = 1;
    } else {
        pair = digits[0] * 10 + digits[1];
        pos = 2;
    }
    pair = 11 + 2 * pair;
    // Add the pairs of digits between pos and length.
    int32_t shift = 8;
    int32_t cesLength = 0;
    while(pos < length) {
        if(shift == 0) {
            // Every three pairs/bytes we need to store a 4-byte-primary CE
            // and start with a new CE with the '0' primary lead byte.
            primary |= pair;
            cesLength = forwardCEs.append(cesLength,
                ((int64_t)primary << 32) | Collation::COMMON_SEC_AND_TER_CE, errorCode);
            primary = zeroPrimary;
            shift = 16;
        } else {
            primary |= pair << shift;
            shift -= 8;
        }
        pair = 11 + 2 * (digits[pos] * 10 + digits[pos + 1]);
        pos += 2;
    }
    primary |= (pair - 1) << shift;
    cesLength = forwardCEs.append(cesLength,
        ((int64_t)primary << 32) | Collation::COMMON_SEC_AND_TER_CE, errorCode);
    ces = forwardCEs.getBuffer();
    cesMaxIndex = cesLength;
}

void
CollationIterator::setHangulExpansion(UChar32 c) {
    const int64_t *jamoCEs = data->getJamoCEs();
    c -= Hangul::HANGUL_BASE;
    UChar32 t = c % Hangul::JAMO_T_COUNT;
    c /= Hangul::JAMO_T_COUNT;
    if(t == 0) {
        cesMaxIndex = 1;
    } else {
        // offset 39 = 19 + 21 - 1:
        // 19 = JAMO_L_COUNT
        // 21 = JAMO_T_COUNT
        // -1 = omit t==0
        forwardCEs[2] = jamoCEs[39 + t];
        cesMaxIndex = 2;
    }
    UChar32 v = c % Hangul::JAMO_V_COUNT;
    c /= Hangul::JAMO_V_COUNT;
    forwardCEs[0] = jamoCEs[c];
    forwardCEs[1] = jamoCEs[19 + v];
    ces = forwardCEs.getBuffer();
}

void
CollationIterator::setLatinExpansion(uint32_t ce32) {
    forwardCEs[0] = ((int64_t)(ce32 & 0xff0000) << 40) |
                    Collation::COMMON_SECONDARY_CE | (ce32 & 0xff00);
    forwardCEs[1] = ((ce32 & 0xff) << 24) | Collation::COMMON_TERTIARY_CE;
    ces = forwardCEs.getBuffer();
    cesIndex = cesMaxIndex = 1;
}

void
CollationIterator::setCE32s(const CollationData *d, int32_t expIndex, int32_t length) {
    ces = forwardCEs.getBuffer();
    const uint32_t *ce32s = d->getCE32s(expIndex);
    if(length == 0) { length = (int32_t)*ce32s++; }
    cesMaxIndex = length - 1;
    for(int32_t i = 0; i < length; ++i) {
        forwardCEs[i] = Collation::ceFromCE32(ce32s[i]);
    }
}

FCDCollationIterator::FCDCollationIterator(
        const CollationData *data, int8_t iterFlags,
        const UChar *s, const UChar *lim,
        UErrorCode &errorCode)
        : CollationIterator(data, iterFlags, s, s),
          rawStart(s), segmentStart(s), segmentLimit(s), rawLimit(lim),
          lengthBeforeLimit(0),
          smallSteps(TRUE),
          nfcImpl(data->getNFCImpl()),
          buffer(nfcImpl, normalized) {
    if(U_SUCCESS(errorCode)) {
        buffer.init(2, errorCode);
    }
}

UChar32
FCDCollationIterator::handleNextCodePoint(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || segmentLimit == rawLimit) { return U_SENTINEL; }
    U_ASSERT(pos == limit);
    if(lengthBeforeLimit != 0) {
        int32_t length = (int32_t)(limit - start);
        if(lengthBeforeLimit <= length) {
            // We have reached the end of the saveLimitAndSetAfter() range.
            return U_SENTINEL;
        }
        lengthBeforeLimit -= length;
    }
    if(limit != segmentLimit) {
        // The previous segment had to be normalized
        // and was pointing into the normalized string.
        start = pos = limit = segmentLimit;
    }
    segmentStart = segmentLimit;
    if((flags & Collation::CHECK_FCD) == 0) {
        U_ASSERT((flags & Collation::DECOMP_HANGUL) != 0);
        return nextCodePointDecompHangul(errorCode);
    }
    const UChar *p = segmentLimit;
    uint8_t prevCC = 0;
    for(;;) {
        // So far, we have limit<=segmentLimit<=p,
        // [limit, p[ passes the FCD test,
        // and segmentLimit is at the last FCD boundary before or on p.
        // Advance p by one code point, fetch its fcd16 value,
        // and continue the incremental FCD test.
        const UChar *q = p;
        UChar32 c = *p++;
        if(c < 0x180) {
            if(c == 0) {
                if(rawLimit == NULL) {
                    // We hit the NUL terminator; remember its pointer.
                    segmentLimit = rawLimit = q;
                    if(limit == rawLimit) { return U_SENTINEL; }
                    limit = rawLimit;
                    break;
                }
                segmentLimit = p;
                prevCC = 0;
            } else {
                prevCC = (uint8_t)nfcImpl.getFCD16FromBelow180(c);  // leadCC == 0
                if(prevCC <= 1) {
                    segmentLimit = p;  // FCD boundary after the [q, p[ code point.
                } else {
                    segmentLimit = q;  // FCD boundary before the [q, p[ code point.
                }
            }
        } else if(!nfcImpl.singleLeadMightHaveNonZeroFCD16(c)) {
            if(c >= 0xac00) {
                if((flags & Collation::DECOMP_HANGUL) && c <= 0xd7a3) {
                    if(limit != q) {
                        // Deliver the non-Hangul text segment so far.
                        // We know there is an FCD boundary before the Hangul syllable.
                        limit = segmentLimit = q;
                        break;
                    }
                    segmentLimit = p;
                    // TODO: Create UBool ReorderingBuffer::setToDecomposedHangul(UChar32 c, UErrorCode &errorCode);
                    buffer.remove();
                    UChar jamos[3];
                    int32_t length = Hangul::decompose(c, jamos);
                    if(!buffer.appendZeroCC(jamos, jamos + length, errorCode)) { return U_SENTINEL; }
                    start = buffer.getStart();
                    limit = buffer.getLimit();
                    break;
                } else if(U16_IS_LEAD(c) && p != rawLimit && U16_IS_TRAIL(*p)) {
                    // c is the lead surrogate of an inert supplementary code point.
                    ++p;
                }
            }
            segmentLimit = p;
            prevCC = 0;
        } else {
            UChar c2;
            if(U16_IS_LEAD(c) && p != rawLimit && U16_IS_TRAIL(c2 = *p)) {
                c = U16_GET_SUPPLEMENTARY(c, c2);
                ++p;
            }
            uint16_t fcd16 = nfcImpl.getFCD16FromNormData(c);
            uint8_t leadCC = (uint8_t)(fcd16 >> 8);
            if(leadCC != 0 && prevCC > leadCC) {
                // Fails FCD test.
                if(limit != segmentLimit) {
                    // Deliver the already-FCD text segment so far.
                    limit = segmentLimit;
                    break;
                }
                // Find the next FCD boundary and normalize.
                do {
                    segmentLimit = p;
                } while(p != rawLimit && (fcd16 = nfcImpl.nextFCD16(p, rawLimit)) > 0xff);
                buffer.remove();
                nfcImpl.decompose(limit, segmentLimit, &buffer, errorCode);
                if(U_FAILURE(errorCode)) { return U_SENTINEL; }
                // Switch collation processing into the FCD buffer
                // with the result of normalizing [limit, segmentLimit[.
                start = buffer.getStart();
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
    U_ASSERT(start < limit);
    if(lengthBeforeLimit != 0) {
        if(lengthBeforeLimit < (int32_t)(limit - start)) {
            limit = start + lengthBeforeLimit;
        }
    }
    pos = start;
    // Return the next code point at pos != limit; no need to check for NUL-termination.
    return simpleNext();
}

UChar32
FCDCollationIterator::nextCodePointDecompHangul(UErrorCode &errorCode) {
    // Only called from handleNextCodePoint() after checking for rawLimit etc.
    const UChar *p = segmentLimit;
    for(;;) {
        // So far, we have limit==segmentLimit<=p,
        // and [limit, p[ does not contain Hangul syllables.
        // Advance p by one code point and check for a Hangul syllable.
        UChar32 c = *p++;
        if(c < 0xac00) {
            if(c == 0 && rawLimit == NULL) {
                // We hit the NUL terminator; remember its pointer.
                segmentLimit = rawLimit = p - 1;
                if(limit == rawLimit) { return U_SENTINEL; }
                limit = rawLimit;
                break;
            }
        } else if(c <= 0xd7a3) {
            if(limit != (p - 1)) {
                // Deliver the non-Hangul text segment so far.
                limit = segmentLimit = p - 1;
                break;
            }
            segmentLimit = p;
            // TODO: Create UBool ReorderingBuffer::setToDecomposedHangul(UChar32 c, UErrorCode &errorCode);
            buffer.remove();
            UChar jamos[3];
            int32_t length = Hangul::decompose(c, jamos);
            if(!buffer.appendZeroCC(jamos, jamos + length, errorCode)) { return U_SENTINEL; }
            start = buffer.getStart();
            limit = buffer.getLimit();
            break;
        } else if(U16_IS_LEAD(c) && p != rawLimit && U16_IS_TRAIL(*p)) {
            // c is the lead surrogate of a supplementary code point.
            ++p;
        }
        if(p == rawLimit) {
            limit = segmentLimit = p;
            break;
        }
        if(smallSteps && (p - limit) >= 5) {
            // Compromise: During string comparison, where differences are often
            // found after a few characters, we do not want to read ahead too far.
            // However, we do want to go forward several characters
            // which will then be handled in the base class fastpath.
            limit = segmentLimit = p;
            break;
        }
    }
    U_ASSERT(start < limit);
    if(lengthBeforeLimit != 0) {
        if(lengthBeforeLimit < (int32_t)(limit - start)) {
            limit = start + lengthBeforeLimit;
        }
    }
    pos = start;
    // Return the next code point at pos != limit; no need to check for NUL-termination.
    return simpleNext();
}
// TODO: Force decomposition if 0!=lccc!=tccc (->0F73, 0F75, 0F81)
// to avoid problems with discontiguous-contraction matching that
// skips the decompositions' lead characters.

UChar32
FCDCollationIterator::handlePreviousCodePoint(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || segmentStart == rawStart) { return U_SENTINEL; }
    U_ASSERT(pos == start);
    if(start != segmentStart) {
        // The previous segment had to be normalized
        // and was pointing into the normalized string.
        start = pos = limit = segmentStart;
    }
    segmentLimit = segmentStart;
    if((flags & Collation::CHECK_FCD) == 0) {
        U_ASSERT((flags & Collation::DECOMP_HANGUL) != 0);
        return previousCodePointDecompHangul(errorCode);
    }
    const UChar *p = segmentStart;
    uint8_t nextCC = 0;
    for(;;) {
        // So far, we have p<=segmentStart<=start,
        // [p, start[ passes the FCD test,
        // and segmentStart is at the first FCD boundary on or after p.
        // Go back with p by one code point, fetch its fcd16 value,
        // and continue the incremental FCD test.
        const UChar *q = p;
        UChar32 c = *--p;
        uint16_t fcd16;
        if(c < 0x180) {
            fcd16 = nfcImpl.getFCD16FromBelow180(c);
        } else if(c < 0xac00) {
            if(!nfcImpl.singleLeadMightHaveNonZeroFCD16(c)) {
                fcd16 = 0;
            } else {
                fcd16 = nfcImpl.getFCD16FromNormData(c);
            }
        } else if(c <= 0xd7a3) {
            if(flags & Collation::DECOMP_HANGUL) {
                if(start != q) {
                    // Deliver the non-Hangul text segment so far.
                    // We know there is an FCD boundary after the Hangul syllable.
                    start = segmentStart = q;
                    break;
                }
                segmentStart = p;
                // TODO: Create UBool ReorderingBuffer::setToDecomposedHangul(UChar32 c, UErrorCode &errorCode);
                buffer.remove();
                UChar jamos[3];
                int32_t length = Hangul::decompose(c, jamos);
                if(!buffer.appendZeroCC(jamos, jamos + length, errorCode)) { return U_SENTINEL; }
                start = buffer.getStart();
                limit = buffer.getLimit();
                break;
            } else {
                fcd16 = 0;
            }
        } else {
            UChar c2;
            if(U16_IS_TRAIL(c) && p != rawStart && U16_IS_LEAD(c2 = *(p - 1))) {
                c = U16_GET_SUPPLEMENTARY(c2, c);
                --p;
            }
            fcd16 = nfcImpl.getFCD16FromNormData(c);
        }
        if(fcd16 == 0) {
            segmentStart = p;
            nextCC = 0;
        } else {
            uint8_t trailCC = (uint8_t)fcd16;
            if(nextCC != 0 && trailCC > nextCC) {
                // Fails FCD test.
                if(start != segmentStart) {
                    // Deliver the already-FCD text segment so far.
                    start = segmentStart;
                    break;
                }
                // Find the previous FCD boundary and normalize.
                while(p != rawStart && (fcd16 = nfcImpl.previousFCD16(rawStart, p)) > 0xff) {}
                segmentStart = p;
                buffer.remove();
                nfcImpl.decompose(segmentStart, start, &buffer, errorCode);
                if(U_FAILURE(errorCode)) { return U_SENTINEL; }
                // Switch collation processing into the FCD buffer
                // with the result of normalizing [segmentStart, start[.
                start = buffer.getStart();
                limit = buffer.getLimit();
                break;
            }
            nextCC = (uint8_t)(fcd16 >> 8);
            if(nextCC == 0) {
                segmentStart = p;  // FCD boundary before the [p, q[ code point.
            }
        }
        if(p == rawStart) {
            start = segmentStart = p;
            break;
        }
        if((start - segmentStart) >= 8) {
            // Go back several characters at a time, for the base class fastpath.
            start = segmentStart;
            break;
        }
    }
    U_ASSERT(start < limit);
    if(lengthBeforeLimit != 0) {
        lengthBeforeLimit += (int32_t)(limit - start);
    }
    pos = limit;
    // Return the previous code point before pos != start.
    return simplePrevious();
}

UChar32
FCDCollationIterator::previousCodePointDecompHangul(UErrorCode &errorCode) {
    // Only called from handleNextCodePoint() after checking for rawStart etc.
    const UChar *p = segmentStart;
    for(;;) {
        // So far, we have p<=segmentStart==start,
        // and [p, start[ does not contain Hangul syllables.
        // Go back with p by one code point and check for a Hangul syllable.
        UChar32 c = *--p;
        if(c < 0xac00) {
            // Nothing to be done.
        } else if(c <= 0xd7a3) {
            if(start != (p + 1)) {
                // Deliver the non-Hangul text segment so far.
                start = segmentStart = p + 1;
                break;
            }
            segmentStart = p;
            // TODO: Create UBool ReorderingBuffer::setToDecomposedHangul(UChar32 c, UErrorCode &errorCode);
            buffer.remove();
            UChar jamos[3];
            int32_t length = Hangul::decompose(c, jamos);
            if(!buffer.appendZeroCC(jamos, jamos + length, errorCode)) { return U_SENTINEL; }
            start = buffer.getStart();
            limit = buffer.getLimit();
            break;
        } else {
            if(U16_IS_TRAIL(c) && p != rawStart && U16_IS_LEAD(*(p - 1))) {
                --p;
            }
        }
        if(p == rawStart) {
            start = segmentStart = p;
            break;
        }
        if((start - p) >= 8) {
            // Go back several characters at a time, for the base class fastpath.
            start = segmentStart = p;
            break;
        }
    }
    U_ASSERT(start < limit);
    if(lengthBeforeLimit != 0) {
        lengthBeforeLimit += (int32_t)(limit - start);
    }
    pos = limit;
    // Return the previous code point before pos != start.
    return simplePrevious();
}

const UChar *
FCDCollationIterator::saveLimitAndSetAfter(UChar32 c) {
    limit = pos + U16_LENGTH(c);
    lengthBeforeLimit = (int32_t)(limit - start);
    return NULL;
}

void
FCDCollationIterator::restoreLimit(const UChar * /* savedLimit */) {
    if(start == segmentStart) {
        limit = segmentLimit;
    } else {
        limit = buffer.getLimit();
    }
}

// TODO: TwoWayCollationIterator:
// So far, this is just the initial code collection moved out of the base CollationIterator.
// Add delegation to "fwd." etc.
// Move this implementation into a separate .cpp file?

int64_t
TwoWayCollationIterator::previousCE(UErrorCode &errorCode) {
    if(fwd.cesIndex > 0) {
        // Return the previous buffered CE.
        int64_t ce = fwd.ces[--fwd.cesIndex];
        if(fwd.cesIndex == 0) { fwd.cesIndex = -1; }
        // TODO: Jump by delta code points if the direction changed?
        return ce;
    }
    // TODO: Do we need to handle hiragana going backward?
    // Note: v1 ucol_IGetPrevCE() does not handle 3099..309C inheriting the Hiragana-ness from the preceding character.
    UChar32 c;
    uint32_t ce32;
    if(fwd.pos != fwd.start) {
        UTRIE2_U16_PREV32(fwd.trie, fwd.start, fwd.pos, c, ce32);
    } else {
        c = fwd.handlePreviousCodePoint(errorCode);
        if(c < 0) {
            return Collation::NO_CE;
        }
        ce32 = fwd.data->getCE32(c);
    }
    if(fwd.data->isUnsafeBackward(c)) {
        return previousCEUnsafe(c, errorCode);
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
        d = fwd.data->getBase();
        ce32 = d->getCE32(c);
        if(!Collation::isSpecialCE32(ce32)) {
            // Normal CE from the base data.
            return Collation::ceFromCE32(ce32);
        }
    } else {
        d = fwd.data;
    }
    return previousCEFromSpecialCE32(d, c, ce32, errorCode);
}

int64_t
TwoWayCollationIterator::previousCEFromSpecialCE32(
        const CollationData *d, UChar32 c, uint32_t ce32,
        UErrorCode &errorCode) {
    for(;;) {  // Loop while ce32 is special.
        int32_t tag = Collation::getSpecialCE32Tag(ce32);
        if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
            U_ASSERT(ce32 != Collation::MIN_SPECIAL_CE32);
            fwd.setLatinExpansion(ce32);
            return fwd.ces[1];
        }
        switch(tag) {
        case Collation::EXPANSION32_TAG:
            fwd.setCE32s(d, (ce32 >> 3) & 0x1ffff, (int32_t)ce32 & 7);
            fwd.cesIndex = fwd.cesMaxIndex;
            return fwd.ces[fwd.cesMaxIndex];
        case Collation::EXPANSION_TAG: {
            fwd.ces = d->getCEs((ce32 >> 3) & 0x1ffff);
            int32_t length = (int32_t)ce32 & 7;
            if(length == 0) { length = (int32_t)*fwd.ces++; }
            fwd.cesMaxIndex = length - 1;
            fwd.cesIndex = fwd.cesMaxIndex;
            return fwd.ces[fwd.cesMaxIndex];
        }
        case Collation::PREFIX_TAG:
            ce32 = fwd.getCE32FromPrefix(d, ce32, errorCode);
            break;
        case Collation::CONTRACTION_TAG:
            // Must not occur. Backward contractions are handled by previousCEUnsafe().
            if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
            return 0;
        case Collation::BUILDER_CONTEXT_TAG:
            // Backward iteration does not support builder data.
            if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
            return 0;
        case Collation::DIGIT_TAG:
            if(fwd.flags & Collation::CODAN) {
                // Collect digits, omit leading zeros.
                CharString digits;
                int32_t numLeadingZeros = 0;
                for(;;) {
                    char digit = (char)(ce32 & 0xf);
                    if(digit == 0) {
                        ++numLeadingZeros;
                    } else {
                        numLeadingZeros = 0;
                    }
                    digits.append(digit, errorCode);
                    c = fwd.previousCodePoint(errorCode);
                    if(c < 0) { break; }
                    ce32 = fwd.data->getCE32(c);
                    if(ce32 == Collation::MIN_SPECIAL_CE32) {
                        ce32 = fwd.data->getBase()->getCE32(c);
                    }
                    if(!Collation::isSpecialCE32(ce32) ||
                        Collation::DIGIT_TAG != Collation::getSpecialCE32Tag(ce32)
                    ) {
                        fwd.forwardNumCodePoints(1, errorCode);
                        break;
                    }
                }
                int32_t length = digits.length() - numLeadingZeros;
                if(length == 0) {
                    // A string of only "leading" zeros.
                    length = 1;
                }
                // Reverse the digit string.
                char *p = digits.data();
                char *q = p + length - 1;
                while(p < q) {
                    char digit = *p;
                    *p++ = *q;
                    *q-- = digit;
                }
                fwd.setCodanCEs(digits.data(), length, errorCode);
                fwd.cesIndex = fwd.cesMaxIndex;
                return fwd.ces[fwd.cesMaxIndex];
            } else {
                // Fetch the non-CODAN CE32 and continue.
                ce32 = *d->getCE32s((ce32 >> 4) & 0xffff);
                break;
            }
        case Collation::HIRAGANA_TAG:
            // TODO: Do we need to handle hiragana going backward? (I.e., do string search or the CollationElementIterator need it?)
            // Fetch the normal CE32 and continue.
            ce32 = *d->getCE32s(ce32 & 0xfffff);
            break;
        case Collation::HANGUL_TAG:
            fwd.setHangulExpansion(ce32);
            fwd.cesIndex = fwd.cesMaxIndex;
            return fwd.ces[fwd.cesMaxIndex];
        case Collation::OFFSET_TAG:
            return CollationIterator::getCEFromOffsetCE32(d, c, ce32);
        case Collation::IMPLICIT_TAG:
            if((ce32 & 1) == 0) {
                U_ASSERT(c == 0);
                // Fetch the normal ce32 for U+0000 and continue.
                ce32 = *d->getCE32s(0);
                break;
            } else {
                return Collation::unassignedCEFromCodePoint(c);
            }
        }
        if(!Collation::isSpecialCE32(ce32)) {
            return Collation::ceFromCE32(ce32);
        }
    }
}

int64_t
TwoWayCollationIterator::previousCEUnsafe(UChar32 c, UErrorCode &errorCode) {
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
    // TODO: Verify that the v1 code uses a backward buffer and gets into trouble
    // with a prefix match that would need to move before the backward buffer.
    const UChar *savedLimit = fwd.saveLimitAndSetAfter(c);
    // Find the first safe character before c.
    int32_t numBackward = 1;
    while((c = fwd.previousCodePoint(errorCode)) >= 0) {
        ++numBackward;
        if(!fwd.data->isUnsafeBackward(c)) {
            break;
        }
    }
    // Ensure that we don't see CEs from a later-in-text expansion.
    fwd.cesIndex = -1;
    // Go forward and collect the CEs.
    int32_t cesLength = 0;
    int64_t ce;
    while((ce = nextCE(errorCode)) != Collation::NO_CE) {  // TODO: fwd.nextCE()??
        cesLength = backwardCEs.append(cesLength, ce, errorCode);
    }
    fwd.restoreLimit(savedLimit);
    fwd.backwardNumCodePoints(numBackward, errorCode);
    // Use the collected CEs and return the last one.
    U_ASSERT(0 != cesLength);
    fwd.ces = backwardCEs.getBuffer();
    fwd.cesIndex = fwd.cesMaxIndex = cesLength - 1;
    return fwd.ces[fwd.cesIndex];
    // TODO: Does this method deliver backward-iteration offsets tight enough
    // for string search? Is this equivalent to how v1 behaves?
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
