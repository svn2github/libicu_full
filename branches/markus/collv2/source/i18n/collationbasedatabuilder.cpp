/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationbasedatabuilder.cpp
*
* created on: 2012aug11
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/localpointer.h"
#include "unicode/ucharstriebuilder.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationbasedatabuilder.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

CollationBaseDataBuilder::CollationBaseDataBuilder(UErrorCode &errorCode)
        : CollationDataBuilder(errorCode),
          scripts(errorCode) {
}

CollationBaseDataBuilder::~CollationBaseDataBuilder() {
}

void
CollationBaseDataBuilder::initBase(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }

    for(UChar32 c = 0; c < 0xf00; ++c) {
        fcd16_F00[c] = nfcImpl.getFCD16(c);
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
    // This includes surrogate code points; see the last option in
    // UCA section 7.1.1 Handling Ill-Formed Code Unit Sequences.
    trie = utrie2_open(Collation::UNASSIGNED_CE32, 0, &errorCode);

    utrie2_set32(trie, 0xfffe, Collation::MERGE_SEPARATOR_CE32, &errorCode);
    utrie2_set32(trie, 0xffff, Collation::MAX_REGULAR_CE32, &errorCode);

    initHanRanges(errorCode);
    initHanCompat(errorCode);

    uint32_t hangulCE32 = makeSpecialCE32(Collation::HANGUL_TAG, 0u);
    utrie2_setRange32(trie, 0xac00, 0xd7a3, hangulCE32, TRUE, &errorCode);

    // Initialize the unsafe-backwards set:
    // All combining marks and trail surrogates.
    unsafeBackwardSet.applyPattern(UNICODE_STRING_SIMPLE("[[:^lccc=0:][\\udc00-\\udfff]]"), errorCode);
    // TODO
}

void
CollationBaseDataBuilder::initTailoring(const CollationData *, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
}

void
CollationBaseDataBuilder::initHanRanges(UErrorCode &errorCode) {
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
    int32_t numHanPerLeadByte = 254 * 254;
    int32_t numHanLeadBytes = (numHan + numHanPerLeadByte - 1) / numHanPerLeadByte;
    uint32_t hanPrimary = (uint32_t)(Collation::UNASSIGNED_IMPLICIT_BYTE - numHanLeadBytes) << 24;
    hanPrimary |= 0x20200;
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
            hanPrimary = setPrimaryRangeAndReturnNext(0x3400, endOfExtA, hanPrimary, step, errorCode);
            endOfExtA = -1;
        }
        hanPrimary = setPrimaryRangeAndReturnNext(start, end, hanPrimary, step, errorCode);
    }
}

void
CollationBaseDataBuilder::initHanCompat(UErrorCode &errorCode) {
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

UBool
CollationBaseDataBuilder::isCompressibleLeadByte(uint32_t b) const {
    return compressibleBytes[b];
}

void
CollationBaseDataBuilder::setCompressibleLeadByte(uint32_t b) {
    compressibleBytes[b] = TRUE;
}

void
CollationBaseDataBuilder::addFirstPrimary(int32_t script, UBool firstInGroup, uint32_t primary,
                                          UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(script == USCRIPT_UNKNOWN) {
        // We use this impossible value (Zzzz) while building the scripts data.
        // It is also the code used in the API to separate low and high scripts.
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    script = CollationBaseData::scriptByteFromInt(script);
    // The script code must be encodeable in our data structure,
    // and the primary weight must at most be three bytes long.
    if(script < 0 || (primary & 0xff) != 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(scripts.isEmpty()) {
        if(!firstInGroup) {
            errorCode = U_INVALID_STATE_ERROR;
            return;
        }
    } else {
        uint32_t prevPrimary = (uint32_t)scripts.elementAti(scripts.size() - 1);
        if(firstInGroup) {
            if((prevPrimary & 0xff000000) >= (primary & 0xff000000)) {
                // The new group shares a lead byte with the previous group.
                errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                return;
            }
            finishPreviousReorderingGroup((primary >> 24) - 1);
        } else {
            if((prevPrimary & 0xffffff00) > primary) {
                // Script/group first primaries must be added in ascending order.
                // Two scripts can share a range, as with Merc=Mero.
                errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                return;
            }
        }
    }
    if(firstInGroup) {
        scripts.addElement((int32_t)primary | USCRIPT_UNKNOWN, errorCode);
    }
    scripts.addElement((int32_t)primary | script, errorCode);
}

void
CollationBaseDataBuilder::finishPreviousReorderingGroup(uint32_t lastByte) {
    if(scripts.isEmpty()) { return; }
    int32_t i = scripts.size() - 1;
    int32_t x;
    while(i > 0 && ((x = scripts.elementAti(--i)) & 0xff) != USCRIPT_UNKNOWN) {}
    x &= 0xff000000;  // first byte
    x |= (int32_t)lastByte << 16;  // last byte
    x |= scripts.size() - i - 1;  // group length (number of script codes)
    scripts.setElementAt(x, i);
}

CollationData *
CollationBaseDataBuilder::buildTailoring(UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
    return NULL;
}

CollationBaseData *
CollationBaseDataBuilder::buildBaseData(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }

    // Create a CollationBaseData container of aliases to this builder's finalized data.
    LocalPointer<CollationBaseData> cd(new CollationBaseData(nfcImpl));
    if(cd.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    buildMappings(*cd, errorCode);
    finishPreviousReorderingGroup(Collation::UNASSIGNED_IMPLICIT_BYTE - 1);
    if(U_FAILURE(errorCode)) { return NULL; }

    cd->fcd16_F00 = fcd16_F00;
    // TODO: cd->variableTop = variableTop;
    cd->compressibleBytes = compressibleBytes;
    cd->scripts = reinterpret_cast<const uint32_t *>(scripts.getBuffer());
    cd->scriptsLength = scripts.size();
    return cd.orphan();
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
