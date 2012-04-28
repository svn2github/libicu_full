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

#include "unicode/localpointer.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"

// TODO: Move to utrie2.h.
// TODO: Used here?
U_DEFINE_LOCAL_OPEN_POINTER(LocalUTrie2Pointer, UTrie2, utrie2_close);

U_NAMESPACE_BEGIN

CollationDataBuilder::CollationDataBuilder(UErrorCode &errorCode)
        : nfcImpl(*Normalizer2Factory::getNFCImpl(errorCode)),
          base(NULL), trie(NULL),
          ce32s(errorCode), ce64s(errorCode),
          fcd16_F00(NULL), compressibleBytes(NULL) {
    // Reserve the first CE32 for U+0000.
    ce32s.addElement(0, errorCode);
}

CollationDataBuilder::~CollationDataBuilder() {
    utrie2_close(trie);
    uprv_free(fcd16_F00);
    uprv_free(compressibleBytes);
}

void
CollationDataBuilder::initBase(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }

    fcd16_F00 = (uint16_t *)uprv_malloc(0xf00 * 2);
    if(fcd16_F00 == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    for(UChar32 c = 0; c < 0xf00; ++c) {
        fcd16_F00[c] = nfcImpl.getFCD16(c);
    }

    compressibleBytes = (UBool *)uprv_malloc(256);
    if(compressibleBytes == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    // Not compressible:
    // - digits
    // - Latin
    // - Hani
    // - trail weights
    // Some scripts are compressible, some are not.
    uprv_memset(compressibleBytes, FALSE, 256);
    compressibleBytes[Collation::UNASSIGNED_IMPLICIT_BYTE] = TRUE;

    // For a base, the default is to compute an unassigned-character implicit CE.
    trie = utrie2_open(Collation::UNASSIGNED_CE32, 0, &errorCode);

    utrie2_set32(trie, 0xfffe, Collation::MERGE_SEPARATOR_CE32, &errorCode);
    utrie2_set32(trie, 0xffff, Collation::MAX_REGULAR_CE32, &errorCode);

    initHanRanges(errorCode);
    initHanCompat(errorCode);
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
    // See http://www.unicode.org/reports/tr10/#Implicit_Weights
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
            hanPrimary = setThreeByteOffsetRange(0x3400, endOfExtA, hanPrimary, step, errorCode);
            endOfExtA = -1;
        }
        hanPrimary = setThreeByteOffsetRange(start, end, hanPrimary, step, errorCode);
    }
}

void
CollationDataBuilder::initHanCompat(UErrorCode &errorCode) {
    // Set the compatibility ideographs which decompose to regular ones.
    UnicodeSet compat(UNICODE_STRING_SIMPLE("[[:Hani:]&[:L:]&[:NFD_QC=No:]]"), errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UnicodeSetIterator iter(compat);
    UChar buffer[4];
    int32_t length;
    while(iter.next()) {
        // Get the singleton decomposition Han character.
        UChar32 c = iter.getCodepoint();
        const UChar *decomp = nfcImpl.getDecomposition(c, buffer, length);
        U_ASSERT(length > 0);
        int32_t i = 0;
        UChar32 han;
        U16_NEXT_UNSAFE(decomp, i, han);
        U_ASSERT(i == length);  // Expect a singleton decomposition.
        // Give c its decomposition's regular CE32.
        uint32_t ce32 = utrie2_get32(trie, han);
        if(Collation::isSpecialCE32(ce32)) {
            if(Collation::getSpecialCE32Tag(ce32) != Collation::OFFSET_TAG) {
                errorCode = U_INTERNAL_PROGRAM_ERROR;
                return;
            }
            ce32 = getCE32FromOffsetCE32(han, ce32);
        } else {
            if(!Collation::isLongPrimaryCE32(ce32)) {
                errorCode = U_INTERNAL_PROGRAM_ERROR;
                return;
            }
        }
        utrie2_set32(trie, c, ce32, &errorCode);
    }
}

uint32_t
CollationDataBuilder::setThreeByteOffsetRange(UChar32 start, UChar32 end,
                                              uint32_t primary, int32_t step,
                                              UErrorCode &errorCode) {
    U_ASSERT(start <= end);
    U_ASSERT((start >> 16) == (end >> 16));  // Range does not span a Unicode plane boundary.
    U_ASSERT(1 <= step && step <= 16);
    // TODO: Do we need to check what values are currently set for start..end?
    // We always set a normal CE32 for the start code point.
    utrie2_set32(trie, start, makeLongPrimaryCE32(primary), &errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    // An offset range is worth it only if we can achieve an overlap between
    // adjacent UTrie2 blocks of 32 code points each.
    // An offset CE is also a little more expensive to look up and compute
    // than a simple CE.
    // We could calculate how many code points are in each block,
    // but a simple minimum range length check should suffice.
    UChar32 limit = end + 1;
    UBool isCompressible = isCompressiblePrimary(primary);
    if((limit - start) >= 40) {  // >= 40 code points in the range
        uint32_t offsetCE32 = makeSpecialCE32(
            Collation::OFFSET_TAG,
            ((uint32_t)(start & 0xffff) << 4) | (uint32_t)(step - 1));
        utrie2_setRange32(trie, start + 1, end, offsetCE32, TRUE, &errorCode);
        return Collation::incThreeBytePrimaryByOffset(primary, isCompressible,
                                                      (limit - start) * step);
    } else {
        // Short range: Set individual CE32s.
        for(;;) {
            ++start;
            primary = Collation::incThreeBytePrimaryByOffset(primary, isCompressible, step);
            if(start > end) { return primary; }
            utrie2_set32(trie, start, makeLongPrimaryCE32(primary), &errorCode);
        }
    }
}

uint32_t
CollationDataBuilder::getCE32FromOffsetCE32(UChar32 c, uint32_t ce32) const {
    UChar32 baseCp = (c & 0x1f0000) | (((UChar32)ce32 >> 4) & 0xffff);
    int32_t offset = (c - baseCp) * ((ce32 & 0xf) + 1);  // delta * increment
    ce32 = utrie2_get32(trie, baseCp);
    // ce32 must be a long-primary pppppp01.
    U_ASSERT(Collation::isLongPrimaryCE32(ce32));
    uint32_t p = Collation::primaryFromLongPrimaryCE32(ce32);
    return makeLongPrimaryCE32(
        Collation::incThreeBytePrimaryByOffset(p, isCompressiblePrimary(p), offset));
}

void
CollationDataBuilder::add(const UnicodeString &prefix, const UnicodeString &s,
                          const int64_t ces[], int32_t cesLength,
                          UErrorCode &errorCode) {
    // TODO
}

CollationData *
CollationDataBuilder::build(UErrorCode &errorCode) {
    // TODO: Copy Latin-1 into each tailoring, but not 0..ff, rather 0..7f && c0..ff.

    // TODO: For U+0000, move its normal ce32 into CE32s[0] and set IMPLICIT_TAG with 0 data bits.

    utrie2_freeze(trie, UTRIE2_32_VALUE_BITS, &errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }

    // Create a CollationData container of aliases to this builder's finalized data.
    LocalPointer<CollationData> cd(new CollationData(nfcImpl));
    if(cd.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    cd->trie = trie;
    cd->base = base;
    cd->fcd16_F00 = (fcd16_F00 != NULL) ? fcd16_F00 : base->fcd16_F00;
    cd->compressibleBytes = compressibleBytes;
    return cd.orphan();
}

// TODO: In CollationWeights allocator,
// try to treat secondary & tertiary weights as 3/4-byte weights with bytes 1 & 2 == 0.
// Natural secondary limit of 0x10000.

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
