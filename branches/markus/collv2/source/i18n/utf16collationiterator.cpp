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

UTF16CollationIterator::~UTF16CollationIterator() {}

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

const void *
UTF16CollationIterator::saveLimitAndSetAfter(UChar32 c) {
    const UChar *savedLimit = limit;
    limit = pos + U16_LENGTH(c);
    return savedLimit;
}

void
UTF16CollationIterator::restoreLimit(const void *savedLimit) {
    limit = static_cast<const UChar *>(savedLimit);
}

FCDUTF16CollationIterator::FCDUTF16CollationIterator(
        const CollationData *data,
        const UChar *s, const UChar *lim,
        UErrorCode & /*errorCode*/)
        : UTF16CollationIterator(data, s, lim),
          rawStart(s), segmentStart(NULL), segmentLimit(NULL), rawLimit(lim),
          lengthBeforeLimit(0),
          nfcImpl(data->nfcImpl),
          inFastPath(TRUE) {
}

void
FCDUTF16CollationIterator::resetToStart() {
    if(segmentStart != rawStart) {
        start = rawStart;
        limit = rawLimit;
        segmentStart = segmentLimit = NULL;
        inFastPath = TRUE;
    }
    lengthBeforeLimit = 0;
    UTF16CollationIterator::resetToStart();
}

uint32_t
FCDUTF16CollationIterator::handleNextCE32(UChar32 &c, UErrorCode &errorCode) {
    if(pos == limit) {
        // When an iteration limit is set we always use the FCD check slow path
        // to track when we reach that limit.
        // Otherwise we check if we reached the end of the input text,
        // and if not then switch back to the FCD check fast path.
        if(lengthBeforeLimit != 0) {
            if(!nextSegment(errorCode)) {
                c = U_SENTINEL;
                return Collation::MIN_SPECIAL_CE32;
            }
        } else if(inFastPath || segmentLimit == rawLimit) {
            c = U_SENTINEL;
            return Collation::MIN_SPECIAL_CE32;
        } else {
            start = pos = segmentLimit;
            limit = rawLimit;
            inFastPath = TRUE;
        }
    }
    // Fetch one code unit.
    c = *pos++;
    // Fast path: Do a quick boundary check.
    if(inFastPath && CollationFCD::hasTccc(c) &&
            (CollationFCD::maybeTibetanCompositeVowel(c) ||
                (pos != limit && CollationFCD::hasLccc(*pos)))) {
        // Code unit c has a non-zero trailing ccc (tccc!=0) and
        // the next unit has a non-zero leading ccc (lccc!=0).
        // Do a full FCD check and normalize if necessary.
        --pos;
        if(!nextSegment(errorCode)) {
            c = U_SENTINEL;
            return Collation::MIN_SPECIAL_CE32;
        }
        c = *pos++;
    }
    return UTRIE2_GET32_FROM_U16_SINGLE_LEAD(trie, c);
}

// TODO: Figure out a way to *extend* a region of input-passes-FCD-check
// rather than moving a tiny segment.
// Especially when going forward after having gone backward.

UChar32
FCDUTF16CollationIterator::nextCodePoint(UErrorCode &errorCode) {
    // Fast path, for contractions. Same as in handleNextCE32().
    if(pos == limit) {
        if(lengthBeforeLimit != 0) {
            if(!nextSegment(errorCode)) {
                return U_SENTINEL;
            }
        } else if(inFastPath || segmentLimit == rawLimit) {
            return U_SENTINEL;
        } else {
            start = pos = segmentLimit;
            limit = rawLimit;
            inFastPath = TRUE;
        }
    }
    UChar32 c = *pos++;
    if(inFastPath) {
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
            // We hit the NUL terminator; remember its pointer.
            limit = rawLimit = --pos;
            return U_SENTINEL;
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
    // Slow path should be ok for prefix matching and backward CE iteration.
    if((inFastPath || pos == start) && !previousSegment(errorCode)) {
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
FCDUTF16CollationIterator::forwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
    if(inFastPath && !nextSegment(errorCode)) { return; }
    while(num > 0 && (pos != limit || nextSegment(errorCode))) {
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
FCDUTF16CollationIterator::backwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
    if(inFastPath && !previousSegment(errorCode)) { return; }
    while(num > 0 && (pos != start || previousSegment(errorCode))) {
        UChar32 c = *--pos;
        --num;
        if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(*(pos-1))) {
            --pos;
        }
    }
}

UBool
FCDUTF16CollationIterator::nextSegment(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    if(inFastPath) {
        if(pos == rawLimit) { return FALSE; }
        segmentStart = pos;
    } else {
        U_ASSERT(pos == limit);
        if(segmentLimit == rawLimit) { return FALSE; }
        if(lengthBeforeLimit != 0) {
            int32_t length = (int32_t)(limit - start);
            if(lengthBeforeLimit <= length) {
                // We have reached the end of the saveLimitAndSetAfter() range.
                return FALSE;
            }
            lengthBeforeLimit -= length;
        }
        segmentStart = segmentLimit;
    }
    const UChar *p = segmentStart;
    if(rawLimit == NULL && *p == 0) {
        // We hit the NUL terminator; remember its pointer.
        start = pos = limit = segmentStart = segmentLimit = rawLimit = p;
        return FALSE;
    }
    uint16_t fcd16 = nfcImpl.nextFCD16(p, rawLimit);
    uint8_t leadCC = (uint8_t)(fcd16 >> 8);
    uint8_t prevCC = 0;
    for(;;) {
        if(leadCC != 0 && (prevCC > leadCC || isFCD16OfTibetanCompositeVowel(fcd16))) {
            // Fails FCD check. Find the next FCD boundary and normalize.
            do {
                segmentLimit = p;
            } while(p != rawLimit && (fcd16 = nfcImpl.nextFCD16(p, rawLimit)) > 0xff);
            if(!normalize(errorCode)) { return FALSE; }
            break;
        }
        prevCC = (uint8_t)fcd16;
        if(p == rawLimit || prevCC == 0) {
            // FCD boundary after the last character.
            start = segmentStart;
            limit = segmentLimit = p;
            break;
        }
        // Fetch the next character's fcd16 value.
        const UChar *q = p;
        fcd16 = nfcImpl.nextFCD16(p, rawLimit);
        leadCC = (uint8_t)(fcd16 >> 8);
        if(leadCC == 0) {
            // FCD boundary before the [q, p[ character.
            start = segmentStart;
            limit = segmentLimit = q;
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
    inFastPath = FALSE;
    return TRUE;
}

UBool
FCDUTF16CollationIterator::previousSegment(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return U_SENTINEL; }
    if(inFastPath) {
        if(pos == rawStart) { return FALSE; }
        segmentLimit = pos;
    } else {
        U_ASSERT(pos == start);
        if(segmentStart == rawStart) { return FALSE; }
        segmentLimit = segmentStart;
    }
    const UChar *p = segmentLimit;
    uint16_t fcd16 = nfcImpl.previousFCD16(rawStart, p);
    uint8_t trailCC = (uint8_t)fcd16;
    uint8_t nextCC = 0;
    for(;;) {
        if(trailCC != 0 && ((nextCC != 0 && trailCC > nextCC) || isFCD16OfTibetanCompositeVowel(fcd16))) {
            // Fails FCD check. Find the previous FCD boundary and normalize.
            while(p != rawStart && (fcd16 = nfcImpl.previousFCD16(rawStart, p)) > 0xff) {}
            segmentStart = p;
            if(!normalize(errorCode)) { return FALSE; }
            break;
        }
        nextCC = (uint8_t)(fcd16 >> 8);
        if(p == rawStart || nextCC == 0) {
            // FCD boundary before the following character.
            start = segmentStart = p;
            limit = segmentLimit;
            break;
        }
        // Fetch the previous character's fcd16 value.
        const UChar *q = p;
        fcd16 = nfcImpl.previousFCD16(rawStart, p);
        trailCC = (uint8_t)fcd16;
        if(trailCC == 0) {
            // FCD boundary after the [p, q[ character.
            start = segmentStart = q;
            limit = segmentLimit;
            break;
        }
    }
    U_ASSERT(start < limit);
    if(lengthBeforeLimit != 0) {
        lengthBeforeLimit += (int32_t)(limit - start);
    }
    pos = limit;
    inFastPath = FALSE;
    return TRUE;
}

const void *
FCDUTF16CollationIterator::saveLimitAndSetAfter(UChar32 c) {
    U_ASSERT(!inFastPath);
    limit = pos + U16_LENGTH(c);
    lengthBeforeLimit = (int32_t)(limit - start);
    return NULL;
}

void
FCDUTF16CollationIterator::restoreLimit(const void * /* savedLimit */) {
    if(inFastPath) {
        limit = rawLimit;
    } else if(start == segmentStart) {
        limit = segmentLimit;
    } else {
        limit = normalized.getBuffer() + normalized.length();
    }
}

UBool
FCDUTF16CollationIterator::normalize(UErrorCode &errorCode) {
    // NFD without argument checking.
    U_ASSERT(U_SUCCESS(errorCode));
    normalized.remove();
    {
        ReorderingBuffer buffer(nfcImpl, normalized);
        if(!buffer.init((int32_t)(segmentLimit - segmentStart), errorCode)) { return FALSE; }
        nfcImpl.decompose(segmentStart, segmentLimit, &buffer, errorCode);
        // The ReorderingBuffer destructor releases the "normalized" string.
    }
    if(U_FAILURE(errorCode)) { return FALSE; }
    // Switch collation processing into the FCD buffer
    // with the result of normalizing [segmentStart, segmentLimit[.
    start = normalized.getBuffer();
    limit = start + normalized.length();
    return TRUE;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
