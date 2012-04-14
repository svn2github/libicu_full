/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatabuilder.cpp
*
* created on: 2012apr01
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"

U_NAMESPACE_BEGIN

CollationDataBuilder::CollationDataBuilder(UErrorCode &errorCode)
        : nfcImpl(*Normalizer2Factory::getNFCImpl(errorCode)),
          base(NULL), trie(NULL),
          ce32s(errorCode), ce64s(errorCode) {
    // Reserve the first CE32 for U+0000.
    ce32s.addElement(0, errorCode);
}

CollationDataBuilder::~CollationDataBuilder() {
    utrie2_close(trie);
}

void
CollationDataBuilder::initBase(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    // For a base, the default is to compute an unassigned-character implicit CE.
    trie = utrie2_open(Collation::UNASSIGNED_CE32, 0, &errorCode);

    initHanRanges(errorCode);
    // TODO
}

void
CollationDataBuilder::initTailoring(const CollationData *b, UErrorCode &errorCode) {
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
    trie = utrie2_open(Collation::MIN_SPECIAL_CE32, 0, &errorCode);
    if(U_FAILURE(errorCode)) { return; }
    // TODO
}

void
CollationDataBuilder::initHanRanges(UErrorCode &errorCode) {
    // Preset the Han ranges as ranges of offset CE32s.
    // TODO: Is this the right set?
    UnicodeSet han(UNICODE_STRING_SIMPLE("[:Unified_Ideograph:]"), errorCode);
    if(U_FAILURE(errorCode)) { return; }
    // Multiply the number of code points by (gap+1).
    // Add one for tailoring after the last Han character.
    int32_t gap = 1;
    int32_t step = gap + 1;
    int32_t numHan = han.size() * step + 1;
    // Numbers of Han primaries per lead byte determined by
    // numbers of 2nd (not compressible) times 3rd primary byte values.
    int32_t numHanPerLeadByte = 253 * 253;
    int32_t numHanLeadBytes = (numHan + numHanPerLeadByte - 1) / numHanPerLeadByte;
    uint32_t hanPrimary = (uint32_t)(Collation::UNASSIGNED_IMPLICIT_BYTE - numHanLeadBytes) << 24;
    hanPrimary |= 0x30300;
    // TODO: Save [fixed first implicit byte xx] and [first implicit [hanPrimary, 05, 05]]
    // TODO: Set the range in invuca.
    UnicodeSetIterator hanIter(han);
    // Unihan extension A sorts after the other BMP ranges.
    UChar32 endOfExtA;
    if(!hanIter.nextRange() ||
        hanIter.getCodepoint() != 0x3400 || (endOfExtA = hanIter.getCodepointEnd()) < 0x4d00
    ) {
        // The first range does not look like Unihan extension A.
        errorCode = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    while(hanIter.nextRange()) {
        UChar32 start = hanIter.getCodepoint();
        UChar32 end = hanIter.getCodepointEnd();
        if(start > 0xffff && endOfExtA >= 0) {
            // Insert extension A.
            hanPrimary = setThreeByteOffsetRange(0x3400, endOfExtA,
                                                 hanPrimary, FALSE, step, errorCode);
            endOfExtA = -1;
        }
        hanPrimary = setThreeByteOffsetRange(start, end, hanPrimary, FALSE, step, errorCode);
    }
}

uint32_t
CollationDataBuilder::setThreeByteOffsetRange(UChar32 start, UChar32 end,
                                              uint32_t primary, UBool isCompressible,
                                              int32_t step,
                                              UErrorCode &errorCode) {
    U_ASSERT(start <= end);
    U_ASSERT((start >> 16) == (end >> 16));  // Range does not span a Unicode plane boundary.
    U_ASSERT(1 <= step && step <= 16);
    // TODO: Do we need to check what values are currently set for start..end?
    // We always set a normal CE32 for the start code point.
    utrie2_set32(trie, start, primary + 1, &errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    // An offset range is worth it only if we can achieve an overlap between
    // adjacent UTrie2 blocks of 32 code points each.
    // An offset CE is also a little more expensive to look up and compute
    // than a simple CE.
    // We could calculate how many code points are in each block,
    // but a simple minimum range length check should suffice.
    if((end - start) >= 39) {  // >= 40 code points in the range
        uint32_t offsetCE32 =
            Collation::MIN_SPECIAL_CE32 |
            ((uint32_t)Collation::OFFSET_TAG << 20) |
            ((uint32_t)(start & 0xffff) << 4) |
            (uint32_t)(step - 1);
        utrie2_setRange32(trie, start + 1, end, offsetCE32, TRUE, &errorCode);
        return Collation::incThreeBytePrimaryByOffset(primary, isCompressible,
                                                      ((end - start) + 1) * step);
    } else {
        // Short range: Set individual CE32s.
        for(;;) {
            ++start;
            primary = Collation::incThreeBytePrimaryByOffset(primary, isCompressible, step);
            if(start > end) { return primary; }
            utrie2_set32(trie, start, primary + 1, &errorCode);
        }
    }
}

void
CollationDataBuilder::add(const UnicodeString &prefix, const UnicodeString &s,
                          const int64_t ces[], int32_t cesLength,
                          UErrorCode &errorCode) {
    // TODO
}

CollationData *
CollationDataBuilder::build(UErrorCode &errorCode) {
    return NULL;  // TODO
    // TODO: Copy Latin-1 into each tailoring, but not 0..ff, rather 0..7f && c0..ff.

    // TODO: For U+0000, move its normal ce32 into CE32s[0] and set IMPLICIT_TAG with 0 data bits.

    // TODO: Not compressible:
    // - digits
    // - Latin
    // - Hani
    // - trail weights
}

// TODO: In CollationWeights allocator,
// try to treat secondary & tertiary weights as 3/4-byte weights with bytes 1 & 2 == 0.
// Natural secondary limit of 0x10000.

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
