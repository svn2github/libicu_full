/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* utf16collationiterator.cpp
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
#include "collationfcd.h"
#include "collationiterator.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "utf16collationiterator.h"

U_NAMESPACE_BEGIN

void
UTF16CollationIterator::resetToStart() {
    pos = start;
    CollationIterator::resetToStart();
}

uint32_t
UTF16CollationIterator::handleNextCE32(UChar32 &c, UErrorCode & /*errorCode*/) {
    if(pos == limit) {
        c = U_SENTINEL;
        return Collation::MIN_SPECIAL_CE32;
    }
    c = *pos++;
    return UTRIE2_GET32_FROM_U16_SINGLE_LEAD(trie, c);
}

UChar
UTF16CollationIterator::handleGetTrailSurrogate() {
    if(pos == limit) { return 0; }
    UChar trail;
    if(U16_IS_TRAIL(trail = *pos)) { ++pos; }
    return trail;
}

UBool
UTF16CollationIterator::foundNULTerminator() {
    if(limit == NULL) {
        limit = --pos;
        return TRUE;
    } else {
        return FALSE;
    }
}

UChar32
UTF16CollationIterator::nextCodePoint(UErrorCode & /*errorCode*/) {
    if(pos == limit) {
        return U_SENTINEL;
    }
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

UChar32
UTF16CollationIterator::previousCodePoint(UErrorCode & /*errorCode*/) {
    if(pos == start) {
        return U_SENTINEL;
    }
    UChar32 c = *--pos;
    UChar lead;
    if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(lead = *(pos - 1))) {
        --pos;
        return U16_GET_SUPPLEMENTARY(lead, c);
    } else {
        return c;
    }
}

void
UTF16CollationIterator::forwardNumCodePoints(int32_t num, UErrorCode & /*errorCode*/) {
    while(num > 0 && pos != limit) {
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
    }
}

void
UTF16CollationIterator::backwardNumCodePoints(int32_t num, UErrorCode & /*errorCode*/) {
    while(num > 0 && pos != start) {
        UChar32 c = *--pos;
        --num;
        if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(*(pos-1))) {
            --pos;
        }
    }
}

// FCDUTF16CollationIterator ----------------------------------------------- ***

FCDUTF16CollationIterator::FCDUTF16CollationIterator(
        const CollationData *data,
        const UChar *s, const UChar *lim,
        UErrorCode & /*errorCode*/)
        : UTF16CollationIterator(data, s, lim),
          rawStart(s), segmentStart(s), segmentLimit(NULL), rawLimit(lim),
          nfcImpl(data->nfcImpl),
          checkDir(1) {
}

void
FCDUTF16CollationIterator::resetToStart() {
    if(checkDir < 0 || segmentStart != rawStart) {
        start = segmentStart = rawStart;
        limit = rawLimit;
        checkDir = 1;
    }
    UTF16CollationIterator::resetToStart();
}

uint32_t
FCDUTF16CollationIterator::handleNextCE32(UChar32 &c, UErrorCode &errorCode) {
    for(;;) {
        if(checkDir > 0) {
            if(pos == limit) {
                c = U_SENTINEL;
                return Collation::MIN_SPECIAL_CE32;
            }
            c = *pos++;
            if(CollationFCD::hasTccc(c)) {
                if(CollationFCD::maybeTibetanCompositeVowel(c) ||
                        (pos != limit && CollationFCD::hasLccc(*pos))) {
                    --pos;
                    if(!nextSegment(errorCode)) {
                        c = U_SENTINEL;
                        return Collation::MIN_SPECIAL_CE32;
                    }
                    c = *pos++;
                }
            }
            break;
        } else if(checkDir == 0 && pos != limit) {
            c = *pos++;
            break;
        } else {
            switchToForward();
        }
    }
    return UTRIE2_GET32_FROM_U16_SINGLE_LEAD(trie, c);
}

UBool
FCDUTF16CollationIterator::foundNULTerminator() {
    if(limit == NULL) {
        limit = rawLimit = --pos;
        return TRUE;
    } else {
        return FALSE;
    }
}

UChar32
FCDUTF16CollationIterator::nextCodePoint(UErrorCode &errorCode) {
    UChar32 c;
    for(;;) {
        if(checkDir > 0) {
            if(pos == limit) {
                return U_SENTINEL;
            }
            c = *pos++;
            if(CollationFCD::hasTccc(c)) {
                if(CollationFCD::maybeTibetanCompositeVowel(c) ||
                        (pos != limit && CollationFCD::hasLccc(*pos))) {
                    --pos;
                    if(!nextSegment(errorCode)) {
                        return U_SENTINEL;
                    }
                    c = *pos++;
                }
            } else if(c == 0 && limit == NULL) {
                limit = rawLimit = --pos;
                return U_SENTINEL;
            }
            break;
        } else if(checkDir == 0 && pos != limit) {
            c = *pos++;
            break;
        } else {
            switchToForward();
        }
    }
    UChar trail;
    if(U16_IS_LEAD(c) && pos != limit && U16_IS_TRAIL(trail = *pos)) {
        ++pos;
        return U16_GET_SUPPLEMENTARY(c, trail);
    } else {
        return c;
    }
}

UChar32
FCDUTF16CollationIterator::previousCodePoint(UErrorCode &errorCode) {
    UChar32 c;
    for(;;) {
        if(checkDir < 0) {
            if(pos == start) {
                return U_SENTINEL;
            }
            c = *--pos;
            if(CollationFCD::hasLccc(c)) {
                if(CollationFCD::maybeTibetanCompositeVowel(c) ||
                        (pos != start && CollationFCD::hasTccc(*(pos - 1)))) {
                    ++pos;
                    if(!previousSegment(errorCode)) {
                        return U_SENTINEL;
                    }
                    c = *--pos;
                }
            }
            break;
        } else if(checkDir == 0 && pos != start) {
            c = *--pos;
            break;
        } else {
            switchToBackward();
        }
    }
    UChar lead;
    if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(lead = *(pos - 1))) {
        --pos;
        return U16_GET_SUPPLEMENTARY(lead, c);
    } else {
        return c;
    }
}

void
FCDUTF16CollationIterator::forwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
    // Specify the class to avoid a virtual-function indirection.
    // In Java, we would declare this class final.
    while(num > 0 && FCDUTF16CollationIterator::nextCodePoint(errorCode) >= 0) {
        --num;
    }
}

void
FCDUTF16CollationIterator::backwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
    // Specify the class to avoid a virtual-function indirection.
    // In Java, we would declare this class final.
    while(num > 0 && FCDUTF16CollationIterator::previousCodePoint(errorCode) >= 0) {
        --num;
    }
}

void
FCDUTF16CollationIterator::switchToForward() {
    U_ASSERT(checkDir < 0 || (checkDir == 0 && pos == limit));
    if(checkDir < 0) {
        // Turn around from backward checking.
        start = segmentStart = pos;
        if(pos == segmentLimit) {
            limit = rawLimit;
            checkDir = 1;  // Check forward.
        } else {  // pos < segmentLimit
            checkDir = 0;  // Stay in FCD segment.
        }
    } else {
        // Reached the end of the FCD segment.
        if(start == segmentStart) {
            // The input text segment is FCD, extend it forward.
        } else {
            // The input text segment needed to be normalized.
            // Switch to checking forward from it.
            pos = start = segmentStart = segmentLimit;
        }
        limit = rawLimit;
        checkDir = 1;
    }
}

UBool
FCDUTF16CollationIterator::nextSegment(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    U_ASSERT(checkDir > 0 && pos != limit);
    // The input text [segmentStart..pos[ passes the FCD check.
    const UChar *p = pos;
    uint16_t fcd16 = nfcImpl.nextFCD16(p, rawLimit);
    uint8_t leadCC = (uint8_t)(fcd16 >> 8);
    uint8_t prevCC = 0;
    for(;;) {
        if(leadCC != 0 && (prevCC > leadCC || isFCD16OfTibetanCompositeVowel(fcd16))) {
            // Fails FCD check. Find the next FCD boundary and normalize.
            const UChar *q;
            do {
                q = p;
            } while(p != rawLimit && (fcd16 = nfcImpl.nextFCD16(p, rawLimit)) > 0xff);
            if(!normalize(pos, q, errorCode)) { return FALSE; }
            pos = start;
            break;
        }
        prevCC = (uint8_t)fcd16;
        if(p == rawLimit || prevCC == 0) {
            // FCD boundary after the last character.
            limit = segmentLimit = p;
            break;
        }
        // Fetch the next character's fcd16 value.
        const UChar *q = p;
        fcd16 = nfcImpl.nextFCD16(p, rawLimit);
        leadCC = (uint8_t)(fcd16 >> 8);
        if(leadCC == 0) {
            // FCD boundary before the [q, p[ character.
            limit = segmentLimit = q;
            break;
        }
    }
    U_ASSERT(pos != limit);
    checkDir = 0;
    return TRUE;
}

void
FCDUTF16CollationIterator::switchToBackward() {
    U_ASSERT(checkDir > 0 || (checkDir == 0 && pos == start));
    if(checkDir > 0) {
        // Turn around from forward checking.
        limit = segmentLimit = pos;
        if(pos == segmentStart) {
            start = rawStart;
            checkDir = -1;  // Check backward.
        } else {  // pos > segmentStart
            checkDir = 0;  // Stay in FCD segment.
        }
    } else {
        // Reached the start of the FCD segment.
        if(start == segmentStart) {
            // The input text segment is FCD, extend it backward.
        } else {
            // The input text segment needed to be normalized.
            // Switch to checking backward from it.
            pos = limit = segmentLimit = segmentStart;
        }
        start = rawStart;
        checkDir = -1;
    }
}

UBool
FCDUTF16CollationIterator::previousSegment(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return U_SENTINEL; }
    U_ASSERT(checkDir < 0 && pos != start);
    // The input text [pos..segmentLimit[ passes the FCD check.
    const UChar *p = pos;
    uint16_t fcd16 = nfcImpl.previousFCD16(rawStart, p);
    uint8_t trailCC = (uint8_t)fcd16;
    uint8_t nextCC = 0;
    for(;;) {
        if(trailCC != 0 && ((nextCC != 0 && trailCC > nextCC) || isFCD16OfTibetanCompositeVowel(fcd16))) {
            // Fails FCD check. Find the previous FCD boundary and normalize.
            while(p != rawStart && (fcd16 = nfcImpl.previousFCD16(rawStart, p)) > 0xff) {}
            if(!normalize(p, pos, errorCode)) { return FALSE; }
            pos = limit;
            break;
        }
        nextCC = (uint8_t)(fcd16 >> 8);
        if(p == rawStart || nextCC == 0) {
            // FCD boundary before the following character.
            start = segmentStart = p;
            break;
        }
        // Fetch the previous character's fcd16 value.
        const UChar *q = p;
        fcd16 = nfcImpl.previousFCD16(rawStart, p);
        trailCC = (uint8_t)fcd16;
        if(trailCC == 0) {
            // FCD boundary after the [p, q[ character.
            start = segmentStart = q;
            break;
        }
    }
    U_ASSERT(pos != start);
    checkDir = 0;
    return TRUE;
}

UBool
FCDUTF16CollationIterator::normalize(const UChar *from, const UChar *to, UErrorCode &errorCode) {
    // NFD without argument checking.
    U_ASSERT(U_SUCCESS(errorCode));
    normalized.remove();
    {
        ReorderingBuffer buffer(nfcImpl, normalized);
        if(!buffer.init((int32_t)(to - from), errorCode)) { return FALSE; }
        nfcImpl.decompose(from, to, &buffer, errorCode);
        // The ReorderingBuffer destructor releases the "normalized" string.
    }
    if(U_FAILURE(errorCode)) { return FALSE; }
    // Switch collation processing into the FCD buffer
    // with the result of normalizing [segmentStart, segmentLimit[.
    segmentStart = from;
    segmentLimit = to;
    start = normalized.getBuffer();
    limit = start + normalized.length();
    return TRUE;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
