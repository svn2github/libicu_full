/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoringdatabuilder.cpp
*
* created on: 2013mar05
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/localpointer.h"
#include "unicode/ucharstrie.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/ustringtrie.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationtailoringdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

CollationTailoringDataBuilder::CollationTailoringDataBuilder(UErrorCode &errorCode)
        : CollationDataBuilder(errorCode) {}

CollationTailoringDataBuilder::~CollationTailoringDataBuilder() {
}

void
CollationTailoringDataBuilder::init(const CollationData *b, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    if(b == NULL) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    base = b;

    // For a tailoring, the default is to fall back to the base.
    trie = utrie2_open(Collation::MIN_SPECIAL_CE32, Collation::FFFD_CE32, &errorCode);

    unsafeBackwardSet = *b->unsafeBackwardSet;

    if(U_FAILURE(errorCode)) { return; }
    // TODO
}

void
CollationTailoringDataBuilder::build(CollationData &data, UErrorCode &errorCode) {
    buildMappings(data, errorCode);
    data.numericPrimary = base->numericPrimary;
    data.compressibleBytes = base->compressibleBytes;
}

int32_t
CollationTailoringDataBuilder::getCEs(const UnicodeString &s,
                                      int64_t ces[], int32_t cesLength) const {
    // Modified copy of CollationIterator::nextCE() and CollationIterator::nextCEFromSpecialCE32().

    // Track which input code points have been consumed,
    // rather than the UCA spec's modifying the input string
    // or the runtime code's more complicated state management.
    // (In Java we may use a BitSet.)
    UnicodeSet consumed;
    for(int32_t i = 0; i < s.length();) {
        UChar32 c = s.char32At(i);
        int32_t nextIndex = i + U16_LENGTH(c);
        if(consumed.contains(i)) {
            i = nextIndex;
            continue;
        }
        consumed.add(i);
        i = nextIndex;
        UBool fromBase = FALSE;
        uint32_t ce32 = utrie2_get32(trie, c);
        if(ce32 == Collation::MIN_SPECIAL_CE32) {
            ce32 = base->getCE32(c);
            fromBase = TRUE;
        }
        for(;;) {  // Loop while ce32 is special.
            if(!Collation::isSpecialCE32(ce32)) {
                cesLength = appendCE(ces, cesLength, Collation::ceFromCE32(ce32));
                break;
            }
            int32_t tag = Collation::getSpecialCE32Tag(ce32);
            if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
                U_ASSERT(ce32 != Collation::MIN_SPECIAL_CE32);
                cesLength = appendCE(ces, cesLength, Collation::getLatinCE0(ce32));
                cesLength = appendCE(ces, cesLength, Collation::getLatinCE1(ce32));
                break;
            }
            switch(tag) {
            case Collation::EXPANSION32_TAG: {
                const uint32_t *rawCE32s =
                    fromBase ? base->ce32s : reinterpret_cast<uint32_t *>(ce32s.getBuffer());
                rawCE32s += Collation::getExpansionIndex(ce32);
                int32_t length = Collation::getExpansionLength(ce32);
                if(length == 0) { length = (int32_t)*rawCE32s++; }
                do {
                    cesLength = appendCE(ces, cesLength, Collation::ceFromCE32(*rawCE32s++));
                } while(--length > 0);
                break;
            }
            case Collation::EXPANSION_TAG: {
                const int64_t *rawCEs = fromBase ? base->ces : ce64s.getBuffer();
                rawCEs += Collation::getExpansionIndex(ce32);
                int32_t length = Collation::getExpansionLength(ce32);
                if(length == 0) { length = (int32_t)*rawCEs++; }
                do {
                    cesLength = appendCE(ces, cesLength, *rawCEs++);
                } while(--length > 0);
                break;
            }
            case Collation::PREFIX_TAG:
                U_ASSERT(fromBase);
                ce32 = getCE32FromBasePrefix(s, ce32, i - U16_LENGTH(c));
                continue;
            case Collation::CONTRACTION_TAG:
                if(fromBase) {
                    ce32 = getCE32FromBaseContraction(s, ce32, i, consumed);
                } else {
                    ce32 = getCE32FromContext(s, ce32, i, consumed);
                }
                continue;
            case Collation::DIGIT_TAG:
                U_ASSERT(fromBase);
                // Fetch the non-numeric-collation CE32 and continue.
                ce32 = base->ce32s[Collation::getDigitIndex(ce32)];
                continue;
            case Collation::RESERVED_TAG_11:
            case Collation::HANGUL_TAG:
            case Collation::LEAD_SURROGATE_TAG:
                U_ASSERT(FALSE);
                break;
            case Collation::OFFSET_TAG: {
                U_ASSERT(fromBase);
                int64_t dataCE = base->ces[Collation::getOffsetIndex(ce32)];
                int64_t ce = Collation::makeCE(Collation::getThreeBytePrimaryForOffsetData(c, dataCE));
                cesLength = appendCE(ces, cesLength, ce);
                break;
            }
            case Collation::IMPLICIT_TAG:
                U_ASSERT(fromBase);
                if((ce32 & 1) == 0) {
                    U_ASSERT(c == 0);
                    // Fetch the normal ce32 for U+0000 and continue.
                    ce32 = base->ce32s[0];
                    continue;
                } else {
                    return Collation::unassignedCEFromCodePoint(c);
                }
            }
            break;
        }
    }
    return cesLength;
}

uint32_t
CollationTailoringDataBuilder::getCE32FromBasePrefix(const UnicodeString &s,
                                                     uint32_t ce32, int32_t i) const {
    const UChar *p = base->contexts + Collation::getPrefixIndex(ce32);
    ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no prefix match.
    UCharsTrie prefixes(p + 2);
    while(i > 0) {
        UChar32 c = s.char32At(i - 1);
        i -= U16_LENGTH(c);
        UStringTrieResult match = prefixes.nextForCodePoint(c);
        if(USTRINGTRIE_HAS_VALUE(match)) {
            ce32 = (uint32_t)prefixes.getValue();
        }
        if(!USTRINGTRIE_HAS_NEXT(match)) { break; }
    }
    return ce32;
}

uint32_t
CollationTailoringDataBuilder::getCE32FromBaseContraction(const UnicodeString &s,
                                                          uint32_t ce32, int32_t sIndex,
                                                          UnicodeSet &consumed) const {
    UBool maybeDiscontiguous = (ce32 & 1) != 0;
    const UChar *p = base->contexts + Collation::getContractionIndex(ce32);
    ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
    UCharsTrie suffixes(p + 2);
    UCharsTrie::State state;
    suffixes.saveState(state);

    // Find the longest contiguous match.
    int32_t i = sIndex;
    while(i < s.length()) {
        UChar32 c = s.char32At(i);
        int32_t nextIndex = i + U16_LENGTH(c);
        if(consumed.contains(i)) {
            i = nextIndex;
            continue;
        }
        UStringTrieResult match = suffixes.nextForCodePoint(c);
        if(USTRINGTRIE_HAS_VALUE(match)) {
            // contiguous match
            consumed.add(sIndex, nextIndex - 1);
            sIndex = nextIndex;
            ce32 = (uint32_t)suffixes.getValue();
            suffixes.saveState(state);
        } else if(!USTRINGTRIE_HAS_NEXT(match)) {
            // not even a partial match
            break;
        }
        // partial match, continue
        i = nextIndex;
    }
    if(!maybeDiscontiguous) { return ce32; }
    suffixes.resetToState(state);

    // Look for a discontiguous match.
    // Mark matching combining marks as consumed, rather than removing them,
    // so that we do not disturb later prefix matching.
    uint8_t prevCC = 0;
    i = sIndex;
    while(i < s.length() && USTRINGTRIE_HAS_NEXT(suffixes.current())) {
        UChar32 c = s.char32At(i);
        int32_t nextIndex = i + U16_LENGTH(c);
        if(consumed.contains(i)) {
            i = nextIndex;
            continue;
        }
        uint8_t cc = nfcImpl.getCC(nfcImpl.getNorm16(c));
        if(cc == 0) { break; }
        if(cc == prevCC) {
            // c is blocked
            i = nextIndex;
            continue;
        }
        prevCC = cc;
        UStringTrieResult match = suffixes.nextForCodePoint(c);
        if(USTRINGTRIE_HAS_VALUE(match)) {
            // extend the match by c
            consumed.add(i);
            ce32 = (uint32_t)suffixes.getValue();
            suffixes.saveState(state);
        } else {
            // no match for c
            suffixes.resetToState(state);
        }
        i = nextIndex;
    }

    return ce32;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
