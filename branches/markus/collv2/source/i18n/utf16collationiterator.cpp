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
#include "collationiterator.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "utf16collationiterator.h"

U_NAMESPACE_BEGIN

UTF16CollationIterator::~UTF16CollationIterator() {}

uint32_t
UTF16CollationIterator::handleNextCE32(UChar32 &c, UErrorCode &errorCode) {
    if(pos != limit) {
        uint32_t ce32;
        UTRIE2_U16_NEXT32(trie, pos, limit, c, ce32);
        return ce32;
    } else {
        c = handleNextCodePoint(errorCode);
        return (c < 0) ? Collation::MIN_SPECIAL_CE32 : data->getCE32(c);
    }
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
        const CollationData *data, int8_t iterFlags,
        const UChar *s, const UChar *lim,
        UErrorCode &errorCode)
        : UTF16CollationIterator(data, iterFlags, s, s),
          rawStart(s), segmentStart(s), segmentLimit(s), rawLimit(lim),
          lengthBeforeLimit(0),
          smallSteps(TRUE),
          nfcImpl(data->nfcImpl),
          buffer(nfcImpl, normalized) {
    if(U_SUCCESS(errorCode)) {
        buffer.init(2, errorCode);
    }
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
FCDUTF16CollationIterator::nextCodePointDecompHangul(UErrorCode &errorCode) {
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
FCDUTF16CollationIterator::handlePreviousCodePoint(UErrorCode &errorCode) {
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
FCDUTF16CollationIterator::previousCodePointDecompHangul(UErrorCode &errorCode) {
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
