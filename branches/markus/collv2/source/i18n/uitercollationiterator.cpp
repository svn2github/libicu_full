/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* uitercollationiterator.cpp
*
* created on: 2012sep23 (from utf16collationiterator.cpp)
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uiter.h"
#include "charstr.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationfcd.h"
#include "collationiterator.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "uitercollationiterator.h"

// TODO: Do we still need unorm_it.h / unorm_openIter()?

U_NAMESPACE_BEGIN

void
UIterCollationIterator::resetToStart() {
    iter.move(&iter, 0, UITER_START);
    CollationIterator::resetToStart();
}

uint32_t
UIterCollationIterator::handleNextCE32(UChar32 &c, UErrorCode & /*errorCode*/) {
    c = iter.next(&iter);
    if(c < 0) {
        return Collation::MIN_SPECIAL_CE32;
    }
    return UTRIE2_GET32_FROM_U16_SINGLE_LEAD(trie, c);
}

UChar
UIterCollationIterator::handleGetTrailSurrogate() {
    UChar32 trail = iter.next(&iter);
    if(!U16_IS_TRAIL(trail) && trail >= 0) { iter.previous(&iter); }
    return (UChar)trail;
}

UChar32
UIterCollationIterator::nextCodePoint(UErrorCode & /*errorCode*/) {
    return uiter_next32(&iter);
}

UChar32
UIterCollationIterator::previousCodePoint(UErrorCode & /*errorCode*/) {
    return uiter_previous32(&iter);
}

void
UIterCollationIterator::forwardNumCodePoints(int32_t num, UErrorCode & /*errorCode*/) {
    while(num > 0 && (uiter_next32(&iter)) >= 0) {
        --num;
    }
}

void
UIterCollationIterator::backwardNumCodePoints(int32_t num, UErrorCode & /*errorCode*/) {
    while(num > 0 && (uiter_previous32(&iter)) >= 0) {
        --num;
    }
}

// FCDUIterCollationIterator ----------------------------------------------- ***

FCDUIterCollationIterator::FCDUIterCollationIterator(
        const CollationData *data, UCharIterator &ui, int32_t startIndex)
        : UIterCollationIterator(data, ui),
          state(ITER_CHECK_FWD), start(startIndex),
          nfcImpl(data->nfcImpl) {
}

void
FCDUIterCollationIterator::resetToStart() {
    if(state <= ITER_IN_FCD_SEGMENT || start != 0) {
        iter.move(&iter, 0, UITER_START);
        start = 0;
        if(state == ITER_CHECK_BWD) {
            state = ITER_CHECK_FWD;
        }
    } else {
        // We are in the first text segment which got normalized.
        pos = 0;
    }
    // Skip the UIterCollationIterator::resetToStart() code
    // and go directly to the base class.
    CollationIterator::resetToStart();
}

uint32_t
FCDUIterCollationIterator::handleNextCE32(UChar32 &c, UErrorCode &errorCode) {
    for(;;) {
        if(state == ITER_CHECK_FWD) {
            c = iter.next(&iter);
            if(c < 0) {
                return Collation::MIN_SPECIAL_CE32;
            }
            if(CollationFCD::hasTccc(c)) {
                if(CollationFCD::maybeTibetanCompositeVowel(c) ||
                        CollationFCD::hasLccc(iter.current(&iter))) {
                    iter.previous(&iter);
                    if(!nextSegment(errorCode)) {
                        c = U_SENTINEL;
                        return Collation::MIN_SPECIAL_CE32;
                    }
                    continue;
                }
            }
            break;
        } else if(state == ITER_IN_FCD_SEGMENT && pos != limit) {
            c = iter.next(&iter);
            ++pos;
            U_ASSERT(c >= 0);
            break;
        } else if(state >= IN_NORM_ITER_AT_LIMIT && pos != normalized.length()) {
            c = normalized[pos++];
            break;
        } else {
            switchToForward();
        }
    }
    return UTRIE2_GET32_FROM_U16_SINGLE_LEAD(trie, c);
}

UChar
FCDUIterCollationIterator::handleGetTrailSurrogate() {
    if(state <= ITER_IN_FCD_SEGMENT) {
        UChar32 trail = iter.next(&iter);
        if(!U16_IS_TRAIL(trail) && trail >= 0) { iter.previous(&iter); }
        return (UChar)trail;
    } else {
        if(pos == normalized.length()) { return 0; }
        UChar trail;
        if(U16_IS_TRAIL(trail = normalized[pos])) { ++pos; }
        return trail;
    }
}

UChar32
FCDUIterCollationIterator::nextCodePoint(UErrorCode &errorCode) {
    UChar32 c;
    for(;;) {
        if(state == ITER_CHECK_FWD) {
            c = iter.next(&iter);
            if(c < 0) {
                return c;
            }
            if(CollationFCD::hasTccc(c)) {
                if(CollationFCD::maybeTibetanCompositeVowel(c) ||
                        CollationFCD::hasLccc(iter.current(&iter))) {
                    iter.previous(&iter);
                    if(!nextSegment(errorCode)) {
                        return U_SENTINEL;
                    }
                    continue;
                }
            }
            if(U16_IS_LEAD(c)) {
                UChar32 trail = iter.next(&iter);
                if(U16_IS_TRAIL(trail)) {
                    return U16_GET_SUPPLEMENTARY(c, trail);
                } else if(trail >= 0) {
                    iter.previous(&iter);
                }
            }
            return c;
        } else if(state == ITER_IN_FCD_SEGMENT && pos != limit) {
            c = uiter_next32(&iter);
            pos += U16_LENGTH(c);
            U_ASSERT(c >= 0);
            return c;
        } else if(state >= IN_NORM_ITER_AT_LIMIT && pos != normalized.length()) {
            c = normalized.char32At(pos);
            pos += U16_LENGTH(c);
            return c;
        } else {
            switchToForward();
        }
    }
}

UChar32
FCDUIterCollationIterator::previousCodePoint(UErrorCode &errorCode) {
    UChar32 c;
    for(;;) {
        if(state == ITER_CHECK_BWD) {
            c = iter.previous(&iter);
            if(c < 0) {
                start = pos = 0;
                state = ITER_IN_FCD_SEGMENT;
                return U_SENTINEL;
            }
            if(CollationFCD::hasLccc(c)) {
                UChar32 prev = U_SENTINEL;
                if(CollationFCD::maybeTibetanCompositeVowel(c) ||
                        CollationFCD::hasTccc(prev = iter.previous(&iter))) {
                    iter.next(&iter);
                    if(prev >= 0) {
                        iter.next(&iter);
                    }
                    if(!previousSegment(errorCode)) {
                        return U_SENTINEL;
                    }
                    continue;
                }
                // hasLccc(trail)=true for all trail surrogates
                if(U16_IS_TRAIL(c)) {
                    if(prev < 0) {
                        prev = iter.previous(&iter);
                    }
                    if(U16_IS_LEAD(prev)) {
                        return U16_GET_SUPPLEMENTARY(prev, c);
                    }
                }
                if(prev >= 0) {
                    iter.next(&iter);
                }
            }
            return c;
        } else if(state == ITER_IN_FCD_SEGMENT && pos != start) {
            c = uiter_previous32(&iter);
            pos -= U16_LENGTH(c);
            U_ASSERT(c >= 0);
            return c;
        } else if(state >= IN_NORM_ITER_AT_LIMIT && pos != 0) {
            c = normalized.char32At(pos - 1);
            pos -= U16_LENGTH(c);
            return c;
        } else {
            switchToBackward();
        }
    }
}

void
FCDUIterCollationIterator::forwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
    // Specify the class to avoid a virtual-function indirection.
    // In Java, we would declare this class final.
    while(num > 0 && FCDUIterCollationIterator::nextCodePoint(errorCode) >= 0) {
        --num;
    }
}

void
FCDUIterCollationIterator::backwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
    // Specify the class to avoid a virtual-function indirection.
    // In Java, we would declare this class final.
    while(num > 0 && FCDUIterCollationIterator::previousCodePoint(errorCode) >= 0) {
        --num;
    }
}

void
FCDUIterCollationIterator::switchToForward() {
    U_ASSERT(state == ITER_CHECK_BWD ||
             (state == ITER_IN_FCD_SEGMENT && pos == limit) ||
             (state >= IN_NORM_ITER_AT_LIMIT && pos == normalized.length()));
    if(state == ITER_CHECK_BWD) {
        // Turn around from backward checking.
        start = pos = iter.getIndex(&iter, UITER_CURRENT);
        if(pos == limit) {
            state = ITER_CHECK_FWD;  // Check forward.
        } else {  // pos < limit
            state = ITER_IN_FCD_SEGMENT;  // Stay in FCD segment.
        }
    } else {
        // Reached the end of the FCD segment.
        if(state == ITER_IN_FCD_SEGMENT) {
            // The input text segment is FCD, extend it forward.
        } else {
            // The input text segment needed to be normalized.
            // Switch to checking forward from it.
            if(state == IN_NORM_ITER_AT_START) {
                iter.move(&iter, limit - start, UITER_CURRENT);
            }
            start = limit;
        }
        state = ITER_CHECK_FWD;
    }
}

UBool
FCDUIterCollationIterator::nextSegment(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    U_ASSERT(state == ITER_CHECK_FWD);
    // The input text [start..(iter index)[ passes the FCD check.
    pos = iter.getIndex(&iter, UITER_CURRENT);
    // Collect the characters being checked, in case they need to be normalized.
    UnicodeString s;
    UChar32 c = uiter_next32(&iter);
    U_ASSERT(c >= 0);
    s.append(c);
    uint16_t fcd16 = nfcImpl.getFCD16(c);
    uint8_t leadCC = (uint8_t)(fcd16 >> 8);
    uint8_t prevCC = 0;
    for(;;) {
        if(leadCC != 0 && (prevCC > leadCC || CollationFCD::isFCD16OfTibetanCompositeVowel(fcd16))) {
            // Fails FCD check. Find the next FCD boundary and normalize.
            for(;;) {
                c = uiter_next32(&iter);
                if(c < 0) { break; }
                if(nfcImpl.getFCD16(c) <= 0xff) {
                    uiter_previous32(&iter);
                    break;
                }
                s.append(c);
            }
            if(!normalize(s, errorCode)) { return FALSE; }
            start = pos;
            limit = pos + s.length();
            state = IN_NORM_ITER_AT_LIMIT;
            pos = 0;
            return TRUE;
        }
        prevCC = (uint8_t)fcd16;
        if(prevCC == 0 || (c = uiter_next32(&iter)) < 0) {
            // FCD boundary after the last character.
            break;
        }
        // Fetch the next character's fcd16 value.
        fcd16 = nfcImpl.getFCD16(c);
        leadCC = (uint8_t)(fcd16 >> 8);
        if(leadCC == 0) {
            // FCD boundary before this character.
            uiter_previous32(&iter);
            break;
        }
        s.append(c);
    }
    limit = pos + s.length();
    U_ASSERT(pos != limit);
    iter.move(&iter, -s.length(), UITER_CURRENT);
    state = ITER_IN_FCD_SEGMENT;
    return TRUE;
}

void
FCDUIterCollationIterator::switchToBackward() {
    U_ASSERT(state == ITER_CHECK_FWD ||
             (state == ITER_IN_FCD_SEGMENT && pos == start) ||
             (state >= IN_NORM_ITER_AT_LIMIT && pos == 0));
    if(state == ITER_CHECK_FWD) {
        // Turn around from forward checking.
        limit = pos = iter.getIndex(&iter, UITER_CURRENT);
        if(pos == start) {
            state = ITER_CHECK_BWD;  // Check backward.
        } else {  // pos > start
            state = ITER_IN_FCD_SEGMENT;  // Stay in FCD segment.
        }
    } else {
        // Reached the start of the FCD segment.
        if(state == ITER_IN_FCD_SEGMENT) {
            // The input text segment is FCD, extend it backward.
        } else {
            // The input text segment needed to be normalized.
            // Switch to checking backward from it.
            if(state == IN_NORM_ITER_AT_LIMIT) {
                iter.move(&iter, start - limit, UITER_CURRENT);
            }
            limit = start;
        }
        state = ITER_CHECK_BWD;
    }
}

UBool
FCDUIterCollationIterator::previousSegment(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return U_SENTINEL; }
    U_ASSERT(state == ITER_CHECK_BWD);
    // The input text [(iter index)..limit[ passes the FCD check.
    pos = iter.getIndex(&iter, UITER_CURRENT);
    // Collect the characters being checked, in case they need to be normalized.
    UnicodeString s;
    UChar32 c = uiter_previous32(&iter);
    U_ASSERT(c >= 0);
    s.append(c);
    uint16_t fcd16 = nfcImpl.getFCD16(c);
    uint8_t trailCC = (uint8_t)fcd16;
    uint8_t nextCC = 0;
    for(;;) {
        if(trailCC != 0 && ((nextCC != 0 && trailCC > nextCC) ||
                            CollationFCD::isFCD16OfTibetanCompositeVowel(fcd16))) {
            // Fails FCD check. Find the previous FCD boundary and normalize.
            for(;;) {
                c = uiter_previous32(&iter);
                if(c < 0) { break; }
                s.append(c);
                if(nfcImpl.getFCD16(c) <= 0xff) {
                    break;
                }
            }
            s.reverse();
            if(!normalize(s, errorCode)) { return FALSE; }
            limit = pos;
            start = pos - s.length();
            state = IN_NORM_ITER_AT_START;
            pos = normalized.length();
            return TRUE;
        }
        nextCC = (uint8_t)(fcd16 >> 8);
        if(nextCC == 0 || (c = uiter_previous32(&iter)) < 0) {
            // FCD boundary before the following character.
            break;
        }
        // Fetch the previous character's fcd16 value.
        fcd16 = nfcImpl.getFCD16(c);
        trailCC = (uint8_t)fcd16;
        if(trailCC == 0) {
            // FCD boundary after this character.
            uiter_next32(&iter);
            break;
        }
        s.append(c);
    }
    start = pos - s.length();
    U_ASSERT(pos != start);
    iter.move(&iter, s.length(), UITER_CURRENT);
    state = ITER_IN_FCD_SEGMENT;
    return TRUE;
}

UBool
FCDUIterCollationIterator::normalize(const UnicodeString &s, UErrorCode &errorCode) {
    // NFD without argument checking.
    U_ASSERT(U_SUCCESS(errorCode));
    nfcImpl.decompose(s, normalized, errorCode);
    return U_SUCCESS(errorCode);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
