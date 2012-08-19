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
UTF16CollationIterator::handleNextCE32(UChar32 &c, UErrorCode &errorCode) {
    if(pos != limit) {
        c = *pos++;
        return UTRIE2_GET32_FROM_U16_SINGLE_LEAD(trie, c);
    } else {
        c = handleNextCodePoint(errorCode);
        return (c < 0) ? Collation::MIN_SPECIAL_CE32 : data->getCE32(c);
    }
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
UTF16CollationIterator::nextCodePoint(UErrorCode &errorCode) {
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

UChar32
UTF16CollationIterator::previousCodePoint(UErrorCode &errorCode) {
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

UChar32
UTF16CollationIterator::handleNextCodePoint(UErrorCode & /* errorCode */) {
    U_ASSERT(pos == limit);
    return U_SENTINEL;
}

UChar32
UTF16CollationIterator::handlePreviousCodePoint(UErrorCode & /* errorCode */) {
    U_ASSERT(pos == start);
    return U_SENTINEL;
}

void
UTF16CollationIterator::forwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
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
UTF16CollationIterator::backwardNumCodePoints(int32_t num, UErrorCode &errorCode) {
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

const void *
UTF16CollationIterator::saveLimitAndSetAfter(UChar32 c) {
    const UChar *savedLimit = limit;
    limit = pos + U16_LENGTH(c);
    return savedLimit;
}

void
UTF16CollationIterator::restoreLimit(const void *savedLimit) {
    limit = reinterpret_cast<const UChar *>(savedLimit);
}

FCDUTF16CollationIterator::FCDUTF16CollationIterator(
        const CollationData *data,
        const UChar *s, const UChar *lim,
        UErrorCode &errorCode)
        : UTF16CollationIterator(data, s, s),
          rawStart(s), segmentStart(s), segmentLimit(s), rawLimit(lim),
          lengthBeforeLimit(0),
          smallSteps(TRUE),
          nfcImpl(data->nfcImpl),
          buffer(nfcImpl, normalized) {
    if(U_SUCCESS(errorCode)) {
        buffer.init(2, errorCode);
    }
}

void
FCDUTF16CollationIterator::resetToStart() {
    if(segmentStart != rawStart) {
        segmentStart = segmentLimit = start = limit = rawStart;
    }
    lengthBeforeLimit = 0;
    UTF16CollationIterator::resetToStart();
}

UChar32
FCDUTF16CollationIterator::handleNextCodePoint(UErrorCode &errorCode) {
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
    segmentStart = segmentLimit;
    const UChar *p = segmentLimit;
    uint8_t prevCC = 0;
    for(;;) {
        // So far, we have segmentStart<=segmentLimit<=p,
        // [segmentStart, p[ passes the FCD test,
        // and segmentLimit is at the last FCD boundary before or on p.
        // Advance p by one code point, fetch its fcd16 value,
        // and continue the incremental FCD test.
        const UChar *q = p;
        UChar32 c = *p++;
        if(c < 0x300) {
            if(c == 0) {
                if(rawLimit == NULL) {
                    // We hit the NUL terminator; remember its pointer.
                    segmentLimit = rawLimit = q;
                    start = segmentStart;
                    limit = rawLimit;
                    if(segmentStart == rawLimit) {
                        pos = limit;
                        return U_SENTINEL;
                    }
                    break;
                }
                segmentLimit = p;
                prevCC = 0;
            } else {
                prevCC = (uint8_t)data->fcd16_F00[c];  // leadCC == 0
                if(prevCC <= 1) {
                    segmentLimit = p;  // FCD boundary after the [q, p[ code point.
                } else {
                    segmentLimit = q;  // FCD boundary before the [q, p[ code point.
                }
            }
        } else if(!nfcImpl.singleLeadMightHaveNonZeroFCD16(c)) {
            if(U16_IS_LEAD(c) && p != rawLimit && U16_IS_TRAIL(*p)) {
                // c is the lead surrogate of an inert supplementary code point.
                ++p;
            }
            segmentLimit = p;
            prevCC = 0;
        } else {
            uint16_t fcd16;
            if(c < 0xf00) {
                fcd16 = data->fcd16_F00[c];
            } else {
                UChar c2;
                if(U16_IS_LEAD(c) && p != rawLimit && U16_IS_TRAIL(c2 = *p)) {
                    c = U16_GET_SUPPLEMENTARY(c, c2);
                    ++p;
                }
                fcd16 = nfcImpl.getFCD16FromNormData(c);
            }
            uint8_t leadCC = (uint8_t)(fcd16 >> 8);
            if(leadCC != 0 && (prevCC > leadCC || isFCD16OfTibetanCompositeVowel(fcd16))) {
                // Fails FCD test.
                if(segmentStart != segmentLimit) {
                    // Deliver the already-FCD text segment so far.
                    start = segmentStart;
                    limit = segmentLimit;
                    break;
                }
                // Find the next FCD boundary and normalize.
                do {
                    segmentLimit = p;
                } while(p != rawLimit && (fcd16 = nfcImpl.nextFCD16(p, rawLimit)) > 0xff);
                buffer.remove();
                nfcImpl.decompose(segmentStart, segmentLimit, &buffer, errorCode);
                if(U_FAILURE(errorCode)) { return U_SENTINEL; }
                // Switch collation processing into the FCD buffer
                // with the result of normalizing [segmentStart, segmentLimit[.
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
            start = segmentStart;
            limit = segmentLimit = p;
            break;
        }
        if(smallSteps && (segmentLimit - segmentStart) >= 5) {
            // Compromise: During string comparison, where differences are often
            // found after a few characters, we do not want to read ahead too far.
            // However, we do want to go forward several characters
            // which will then be handled in the base class fastpath.
            start = segmentStart;
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
    UChar32 c = *pos++;
    UChar trail;
    if(U16_IS_LEAD(c) && pos != limit && U16_IS_TRAIL(trail = *pos)) {
        ++pos;
        c = U16_GET_SUPPLEMENTARY(c, trail);
    }
    return c;
}

UChar32
FCDUTF16CollationIterator::handlePreviousCodePoint(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || segmentStart == rawStart) { return U_SENTINEL; }
    U_ASSERT(pos == start);
    segmentLimit = segmentStart;
    const UChar *p = segmentStart;
    uint8_t nextCC = 0;
    for(;;) {
        // So far, we have p<=segmentStart<=segmentLimit,
        // [p, segmentLimit[ passes the FCD test,
        // and segmentStart is at the first FCD boundary on or after p.
        // Go back with p by one code point, fetch its fcd16 value,
        // and continue the incremental FCD test.
        UChar32 c = *--p;
        uint16_t fcd16;
        if(c < 0xf00) {
            fcd16 = data->fcd16_F00[c];
        } else if(c < 0xd800) {
            if(!nfcImpl.singleLeadMightHaveNonZeroFCD16(c)) {
                fcd16 = 0;
            } else {
                fcd16 = nfcImpl.getFCD16FromNormData(c);
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
            if((nextCC != 0 && trailCC > nextCC) || isFCD16OfTibetanCompositeVowel(fcd16)) {
                // Fails FCD test.
                if(segmentLimit != segmentStart) {
                    // Deliver the already-FCD text segment so far.
                    start = segmentStart;
                    limit = segmentLimit;
                    break;
                }
                // Find the previous FCD boundary and normalize.
                while(p != rawStart && (fcd16 = nfcImpl.previousFCD16(rawStart, p)) > 0xff) {}
                segmentStart = p;
                buffer.remove();
                nfcImpl.decompose(segmentStart, segmentLimit, &buffer, errorCode);
                if(U_FAILURE(errorCode)) { return U_SENTINEL; }
                // Switch collation processing into the FCD buffer
                // with the result of normalizing [segmentStart, segmentLimit[.
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
            limit = segmentLimit;
            break;
        }
        if((segmentLimit - segmentStart) >= 8) {
            // Go back several characters at a time, for the base class fastpath.
            start = segmentStart;
            limit = segmentLimit;
            break;
        }
    }
    U_ASSERT(start < limit);
    if(lengthBeforeLimit != 0) {
        lengthBeforeLimit += (int32_t)(limit - start);
    }
    pos = limit;
    // Return the previous code point before pos != start.
    UChar32 c = *--pos;
    UChar lead;
    if(U16_IS_TRAIL(c) && pos != start && U16_IS_LEAD(lead = *(pos - 1))) {
        --pos;
        c = U16_GET_SUPPLEMENTARY(lead, c);
    }
    return c;
}

const void *
FCDUTF16CollationIterator::saveLimitAndSetAfter(UChar32 c) {
    limit = pos + U16_LENGTH(c);
    lengthBeforeLimit = (int32_t)(limit - start);
    return NULL;
}

void
FCDUTF16CollationIterator::restoreLimit(const void * /* savedLimit */) {
    if(start == segmentStart) {
        limit = segmentLimit;
    } else {
        limit = buffer.getLimit();
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
