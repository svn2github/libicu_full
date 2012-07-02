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

void
CollationData::setStrength(int32_t value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noStrength = options & ~STRENGTH_MASK;
    switch(value) {
    case UCOL_PRIMARY:
    case UCOL_SECONDARY:
    case UCOL_TERTIARY:
    case UCOL_QUATERNARY:
    case UCOL_IDENTICAL:
        options = noStrength | (value << STRENGTH_SHIFT);
        break;
    case UCOL_DEFAULT:
        options = noStrength | (defaultOptions & STRENGTH_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setFlag(int32_t bit, UColAttributeValue value,
                       int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    switch(value) {
    case UCOL_ON:
        options |= bit;
        break;
    case UCOL_OFF:
        options &= ~bit;
        break;
    case UCOL_DEFAULT:
        options = (options & ~bit) | (defaultOptions & bit);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setCaseFirst(UColAttributeValue value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noCaseFirst = options & ~CASE_FIRST_AND_UPPER_MASK;
    switch(value) {
    case UCOL_OFF:
        options = noCaseFirst;
        break;
    case UCOL_LOWER_FIRST:
        options = noCaseFirst | CASE_FIRST;
        break;
    case UCOL_UPPER_FIRST:
        options = noCaseFirst | CASE_FIRST_AND_UPPER_MASK;
        break;
    case UCOL_DEFAULT:
        options = noCaseFirst | (defaultOptions & CASE_FIRST_AND_UPPER_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setAlternateHandling(UColAttributeValue value,
                                    int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noAlternate = options & ~ALTERNATE_MASK;
    switch(value) {
    case UCOL_NON_IGNORABLE:
        options = noAlternate;
        break;
    case UCOL_SHIFTED:
        options = noAlternate | SHIFTED;
        break;
    case UCOL_DEFAULT:
        options = noAlternate | (defaultOptions & ALTERNATE_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

CEArray::~CEArray() {}

int32_t
CEArray::doAppend(int32_t length, int64_t ce, UErrorCode &errorCode) {
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

CollationIterator::~CollationIterator() {
    delete skipped;
}

void
CollationIterator::resetToStart() {
    reset();
}

void
CollationIterator::reset() {  // Needed as a separate function?
    cesIndex = -1;
    hiragana = 0;
    anyHiragana = FALSE;
    if(skipped != NULL) { skipped->clear(); }
}

uint32_t
CollationIterator::handleNextCE32(UChar32 &c, UErrorCode &errorCode) {
    c = nextCodePoint(errorCode);
    return (c < 0) ? Collation::MIN_SPECIAL_CE32 : data->getCE32(c);
}

UChar
CollationIterator::handleGetTrailSurrogate() {
    return 0;  // Must not be called.
}

UBool
CollationIterator::foundNULTerminator() {
    return FALSE;
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
            ces = d->ces + ((ce32 >> 3) & 0x1ffff);
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
        case Collation::CONTRACTION_TAG: {
            // Some portion of nextCE32FromContraction() pulled out here as an ASCII fast path,
            // avoiding the function call and the nextSkippedCodePoint() overhead.
            const UChar *p = d->contexts + ((int32_t)(ce32 >> 2) & 0x3ffff);
            uint32_t defaultCE32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
            p += 2;
            UChar32 nextCp = nextCodePoint(errorCode);
            if(nextCp < 0) {
                // No more text.
                ce32 = defaultCE32;
            } else if(nextCp < 0x300 && (ce32 & 2) != 0) {
                // The next code point is below U+0300
                // but all contraction suffixes start with characters >=U+0300.
                backwardNumCodePoints(1, errorCode);
                ce32 = defaultCE32;
            } else {
                ce32 = nextCE32FromContraction(d, c, p, defaultCE32,
                                               (ce32 & 1) != 0, nextCp, errorCode);
                if(ce32 == 0x100) {
                    // CEs from a discontiguous contraction plus the skipped combining marks.
                    return ces[0];
                }
            }
            break;
        }
#if 0  // TODO
        case Collation::BUILDER_CONTEXT_TAG:
            // Used only in the collation data builder.
            // Data bits point into a builder-specific data structure with non-final data.
            ce32 = 0;  // TODO: ?? d->nextCE32FromBuilderContext(*this, ce32, errorCode);
            break;
#endif
        case Collation::DIGIT_TAG:
            if(data->options & CollationData::CODAN) {
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
                        ce32 = data->base->getCE32(c);
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
                ce32 = d->ce32s[(ce32 >> 4) & 0xffff];
                break;
            }
        case Collation::HIRAGANA_TAG:
            if(0x3099 <= c && c <= 0x309c) {
                hiragana = -1;
            } else {
                hiragana = 1;
                anyHiragana = TRUE;
            }
            // Fetch the normal CE32 and continue.
            ce32 = d->ce32s[ce32 & 0xfffff];
            break;
        case Collation::HANGUL_TAG:
            setHangulExpansion(c);
            cesIndex = 1;
            return ces[0];
        case Collation::LEAD_SURROGATE_TAG:
            U_ASSERT(U16_IS_LEAD(c));
            UChar trail;
            if(U16_IS_TRAIL(trail = handleGetTrailSurrogate())) {
                c = U16_GET_SUPPLEMENTARY(c, trail);
                ce32 &= 3;
                if(ce32 == 0) {
                    ce32 = Collation::UNASSIGNED_CE32;  // unassigned-implicit
                } else if(ce32 == 1 ||
                        (ce32 = d->getCE32FromSupplementary(c)) == Collation::MIN_SPECIAL_CE32) {
                    // fall back to the base data
                    d = d->base;
                    ce32 = d->getCE32FromSupplementary(c);
                }
            } else {
                // c is an unpaired surrogate.
                ce32 = Collation::UNASSIGNED_CE32;
            }
            break;
        case Collation::OFFSET_TAG:
            return getCEFromOffsetCE32(d, c, ce32);
        case Collation::IMPLICIT_TAG:
            if((ce32 & 1) == 0) {
                U_ASSERT(c == 0);
                if(foundNULTerminator()) {
                    // Handle NUL-termination. (Not needed in Java.)
                    return Collation::NO_CE;
                } else {
                    // Fetch the normal ce32 for U+0000 and continue.
                    ce32 = d->ce32s[0];
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
    int64_t dataCE = d->ces[(int32_t)ce32 & 0xfffff];
    uint32_t p = (uint32_t)(dataCE >> 32);  // three-byte primary pppppp00
    int32_t lower32 = (int32_t)dataCE;  // base code point b & step s: bbbbbbss
    int32_t offset = (c - (lower32 >> 8)) * (lower32 & 0xff);  // delta * increment
    return Collation::getCEFromThreeByteOffset(p, d->isCompressiblePrimary(p), offset);
}

uint32_t
CollationIterator::getCE32FromPrefix(const CollationData *d, uint32_t ce32,
                                     UErrorCode &errorCode) {
    const UChar *p = d->contexts + ((int32_t)ce32 & 0xfffff);
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
CollationIterator::nextCE32FromContraction(const CollationData *d, UChar32 originalCp,
                                           const UChar *p, uint32_t ce32,
                                           UBool maybeDiscontiguous, UChar32 c, UErrorCode &errorCode) {
    // originalCp: Only needed as input to nextCE32FromDiscontiguousContraction().
    // c: next code point after originalCp

    // Number of code points read beyond the original code point.
    // Only needed as input to nextCE32FromDiscontiguousContraction().
    int32_t lookAhead = 1;
    // Number of code points read since the last match (initially only c).
    int32_t sinceMatch = 1;
    // Normally we only need a contiguous match,
    // and therefore need not remember the suffixes state from before a mismatch for retrying.
    // If we are already processing skipped combining marks, then we do track the state.
    UCharsTrie suffixes(p);
    if(skipped != NULL && !skipped->isEmpty()) { skipped->saveTrieState(suffixes); }
    UStringTrieResult match = suffixes.firstForCodePoint(c);
    for(;;) {
        UChar32 nextCp;
        if(USTRINGTRIE_HAS_VALUE(match)) {
            ce32 = (uint32_t)suffixes.getValue();
            if(!USTRINGTRIE_HAS_NEXT(match) || (c = nextSkippedCodePoint(errorCode)) < 0) {
                return ce32;
            }
            if(skipped != NULL && !skipped->isEmpty()) { skipped->saveTrieState(suffixes); }
            sinceMatch = 1;
        } else if(match == USTRINGTRIE_NO_MATCH || (nextCp = nextSkippedCodePoint(errorCode)) < 0) {
            // No match for c, or partial match (USTRINGTRIE_NO_VALUE) and no further text.
            // Back up if necessary, and try a discontiguous contraction.
            if(maybeDiscontiguous) {
                // The last character of at least one suffix has lccc!=0,
                // allowing for discontiguous contractions.
                // UCA S2.1.1 only processes non-starters immediately following
                // "a match in the table" (sinceMatch=1).
                if(sinceMatch > 1) {
                    // Return to the state after the last match.
                    // (Return to sinceMatch=0 and re-fetch the first partially-matched character.)
                    backwardNumSkipped(sinceMatch, errorCode);
                    c = nextSkippedCodePoint(errorCode);
                    lookAhead -= sinceMatch - 1;
                    sinceMatch = 1;
                }
                if(d->getFCD16(c) > 0xff) {
                    return nextCE32FromDiscontiguousContraction(
                        d, originalCp, suffixes, ce32, lookAhead, c, errorCode);
                }
            }
            break;
        } else {
            // Continue after partial match (USTRINGTRIE_NO_VALUE) for c.
            // It does not have a result value, therefore it is not itself "a match in the table".
            // If a partially-matched c has ccc!=0 then
            // it might be skipped in discontiguous contraction.
            c = nextCp;
            ++sinceMatch;
        }
        ++lookAhead;
        match = suffixes.nextForCodePoint(c);
    }
    backwardNumSkipped(sinceMatch, errorCode);
    return ce32;
}

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
    U_ASSERT(fcd16 > 0xff);  // The caller checked this already, as a shortcut.
    UChar32 nextCp = nextSkippedCodePoint(errorCode);
    if(nextCp < 0) {
        // No further text.
        backwardNumSkipped(1, errorCode);
        return ce32;
    }
    ++lookAhead;
    uint8_t prevCC = (uint8_t)fcd16;
    fcd16 = d->getFCD16(nextCp);
    if(fcd16 <= 0xff) {
        // The next code point after c is a starter (S2.1.1 "process each non-starter").
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
        UStringTrieResult match;
        // "If C is not blocked from S, find if S + C has a match in the table." (S2.1.2)
        if(prevCC < (fcd16 >> 8) && USTRINGTRIE_HAS_VALUE(match = suffixes.nextForCodePoint(c))) {
            // "If there is a match, replace S by S + C, and remove C." (S2.1.3)
            // Keep prevCC unchanged.
            ce32 = (uint32_t)suffixes.getValue();
            sinceMatch = 0;
            skipped->recordMatch();
            if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
            skipped->saveTrieState(suffixes);
        } else {
            // No match for "S + C", skip C.
            skipped->skip(c);
            skipped->resetToTrieState(suffixes);
            prevCC = (uint8_t)fcd16;
        }
        if((c = nextSkippedCodePoint(errorCode)) < 0) { break; }
        ++sinceMatch;
        fcd16 = d->getFCD16(c);
        if(fcd16 <= 0xff) {
            // The next code point after c is a starter (S2.1.1 "process each non-starter").
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
        ces = forwardCEs.getBuffer();
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
        d = data->base;
        ce32 = d->getCE32(c);
    } else {
        d = data;
    }
    appendCEsFromCE32(d, c, ce32, errorCode);
}

void
CollationIterator::appendCEsFromCE32(const CollationData *d, UChar32 c, uint32_t ce32,
                                     UErrorCode &errorCode) {
    // This is a partial duplicate of nextCEFromSpecialCE32().
    // nextCEFromSpecialCE32() handles all specials,
    // *sets* CEs at the beginning of forwardCEs assuming that the builder limits expansion lengths,
    // and returns the first CE.
    // appendCEsFromCE32() is only called for contraction results and skipped combining marks,
    // therefore handles only a subset of specials,
    // and *appends* CEs to forwardCEs, growing it as needed.
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
            const uint32_t *ce32s = d->ce32s + ((int32_t)(ce32 >> 3) & 0x1ffff);
            int32_t length = (int32_t)ce32 & 7;
            if(length == 0) { length = (int32_t)*ce32s++; }
            for(int32_t i = 0; i < length; ++i) {
                cesLength = forwardCEs.append(cesLength, Collation::ceFromCE32(ce32s[i]), errorCode);
            }
            cesMaxIndex = cesLength - 1;
            return;
        } else if(tag == Collation::EXPANSION_TAG) {
            const int64_t *expCEs = d->ces + ((int32_t)(ce32 >> 3) & 0x1ffff);
            int32_t length = (int32_t)ce32 & 7;
            if(length == 0) { length = (int32_t)*expCEs++; }
            for(int32_t i = 0; i < length; ++i) {
                cesLength = forwardCEs.append(cesLength, expCEs[i], errorCode);
            }
            cesMaxIndex = cesLength - 1;
            return;
        } else if(tag == Collation::CONTRACTION_TAG) {
            const UChar *p = d->contexts + ((int32_t)(ce32 >> 2) & 0x3ffff);
            uint32_t defaultCE32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
            p += 2;
            UChar32 nextCp = nextSkippedCodePoint(errorCode);
            if(nextCp < 0) {
                // No more text.
                ce32 = defaultCE32;
            } else if(nextCp < 0x300 && (ce32 & 2) != 0) {
                // The next code point is below U+0300
                // but all contraction suffixes start with characters >=U+0300.
                backwardNumSkipped(1, errorCode);
                ce32 = defaultCE32;
            } else {
                ce32 = nextCE32FromContraction(d, c, p, defaultCE32,
                                               (ce32 & 1) != 0, nextCp, errorCode);
                U_ASSERT(ce32 != 0x100);
            }
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
        // TODO: case Collation::BUILDER_CONTEXT_TAG:
        // case Collation::DIGIT_TAG:
        // case Collation::HIRAGANA_TAG:
        // case Collation::HANGUL_TAG:
        // case Collation::LEAD_SURROGATE_TAG:
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
    uint32_t zeroPrimary = data->zeroPrimary;
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
    // TODO: The documentation for UCOL_NUMERIC_COLLATION says
    // "Note that the longest digit substring that can be treated as a single collation element
    // is 254 digits (not counting leading zeros). If a digit substring is longer than that,
    // the digits beyond the limit will be treated as a separate digit substring
    // associated with a separate collation element."
    // -> 1. Review whether we should keep the limit (modify this code)
    //       or extend it (to what this code supports).
    //    2. Review whether we handle an overflow as documented or by returning an overflow value.
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
    const int64_t *jamoCEs = data->jamoCEs;
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
    const uint32_t *ce32s = d->ce32s + expIndex;
    if(length == 0) { length = (int32_t)*ce32s++; }
    cesMaxIndex = length - 1;
    for(int32_t i = 0; i < length; ++i) {
        forwardCEs[i] = Collation::ceFromCE32(ce32s[i]);
    }
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(CollationIterator)

// TODO: TwoWayCollationIterator:
// So far, this is just the initial code collection moved out of the base CollationIterator.
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
    UChar32 c = fwd.previousCodePoint(errorCode);
    if(c < 0) { return Collation::NO_CE; }
    if(fwd.data->isUnsafeBackward(c)) {
        return previousCEUnsafe(c, errorCode);
    }
    uint32_t ce32 = fwd.data->getCE32(c);
    // Simple, safe-backwards iteration:
    // Get a CE going backwards, handle prefixes but no contractions.
    int64_t ce;
    if(ce32 < Collation::MIN_SPECIAL_CE32) {  // Forced-inline of isSpecialCE32(ce32).
        // Normal CE from the main data.
        ce = Collation::ceFromCE32(ce32);
    }
    const CollationData *d;
    if(ce32 == Collation::MIN_SPECIAL_CE32) {
        d = fwd.data->base;
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
            fwd.ces = d->ces + ((ce32 >> 3) & 0x1ffff);
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
#if 0  // TODO
        case Collation::BUILDER_CONTEXT_TAG:
            // Backward iteration does not support builder data.
            if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
            return 0;
#endif
        case Collation::DIGIT_TAG:
            if(fwd.data->options & CollationData::CODAN) {
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
                        ce32 = fwd.data->base->getCE32(c);
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
                ce32 = d->ce32s[(ce32 >> 4) & 0xffff];
                break;
            }
        case Collation::HIRAGANA_TAG:
            // TODO: Do we need to handle hiragana going backward? (I.e., do string search or the CollationElementIterator need it?)
            // Fetch the normal CE32 and continue.
            ce32 = d->ce32s[ce32 & 0xfffff];
            break;
        case Collation::HANGUL_TAG:
            fwd.setHangulExpansion(c);
            fwd.cesIndex = fwd.cesMaxIndex;
            return fwd.ces[fwd.cesMaxIndex];
        case Collation::LEAD_SURROGATE_TAG:
            // Must not occur. Backward iteration should never see lead surrogate code _unit_ data.
            if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
            return 0;
        case Collation::OFFSET_TAG:
            return CollationIterator::getCEFromOffsetCE32(d, c, ce32);
        case Collation::IMPLICIT_TAG:
            if((ce32 & 1) == 0) {
                U_ASSERT(c == 0);
                // Fetch the normal ce32 for U+0000 and continue.
                ce32 = d->ce32s[0];
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
    const void *savedLimit = fwd.saveLimitAndSetAfter(c);
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
