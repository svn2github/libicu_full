/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
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
    trie = utrie2_open(Collation::UNASSIGNED_CE32, Collation::FFFD_CE32, &errorCode);

    utrie2_set32(trie, 0xfffd, Collation::FFFD_CE32, &errorCode);
    utrie2_set32(trie, 0xfffe, Collation::MERGE_SEPARATOR_CE32, &errorCode);
    utrie2_set32(trie, 0xffff, Collation::MAX_REGULAR_CE32, &errorCode);

    uint32_t hangulCE32 = makeSpecialCE32(Collation::HANGUL_TAG, 0u);
    utrie2_setRange32(trie, 0xac00, 0xd7a3, hangulCE32, TRUE, &errorCode);
}

void
CollationBaseDataBuilder::initTailoring(const CollationData *, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
}

void
CollationBaseDataBuilder::initHanRanges(const UChar32 ranges[], int32_t length, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || length == 0) { return; }
    if((length & 1) != 0) {  // incomplete start/end pairs
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(isAssigned(0x4e00)) {  // already set
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    int32_t numHanCodePoints = 0;
    for(int32_t i = 0; i < length; i += 2) {
        UChar32 start = ranges[i];
        UChar32 end = ranges[i + 1];
        numHanCodePoints += end - start + 1;
    }
    // Multiply the number of code points by (gap+1).
    // Add one for tailoring after the last Han character.
    int32_t gap = 1;
    int32_t step = gap + 1;
    int32_t numHan = numHanCodePoints * step + 1;
    // Numbers of Han primaries per lead byte determined by
    // numbers of 2nd (not compressible) times 3rd primary byte values.
    int32_t numHanPerLeadByte = 254 * 254;
    int32_t numHanLeadBytes = (numHan + numHanPerLeadByte - 1) / numHanPerLeadByte;
    uint32_t hanPrimary = (uint32_t)(Collation::UNASSIGNED_IMPLICIT_BYTE - numHanLeadBytes) << 24;
    hanPrimary |= 0x20200;
    // TODO: Save [fixed first implicit byte xx] and [first implicit [hanPrimary, 05, 05]]
    // TODO: Set the range in invuca.
    for(int32_t i = 0; i < length; i += 2) {
        UChar32 start = ranges[i];
        UChar32 end = ranges[i + 1];
        hanPrimary = setPrimaryRangeAndReturnNext(start, end, hanPrimary, step, errorCode);
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
CollationBaseDataBuilder::addReorderingGroup(uint32_t firstByte, uint32_t lastByte,
                                             const UnicodeString &groupScripts,
                                             UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(groupScripts.isEmpty()) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(groupScripts.indexOf((UChar)USCRIPT_UNKNOWN) >= 0) {
        // Zzzz must not occur.
        // It is the code used in the API to separate low and high scripts.
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    // Note: We are mostly trusting the input data,
    // rather than verifying that reordering groups do not intersect
    // with their lead byte ranges nor their sets of scripts,
    // and that all script codes are valid.
    scripts.append((UChar)((firstByte << 8) | lastByte));
    scripts.append((UChar)groupScripts.length());
    scripts.append(groupScripts);
}

CollationData *
CollationBaseDataBuilder::buildTailoring(UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
    return NULL;
}

CollationData *
CollationBaseDataBuilder::buildBaseData(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }

    // Create a CollationData container of aliases to this builder's finalized data.
    LocalPointer<CollationData> cd(new CollationData(nfcImpl));
    if(cd.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    buildMappings(*cd, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }

    // TODO: cd->variableTop = variableTop;
    cd->compressibleBytes = compressibleBytes;
    cd->scripts = reinterpret_cast<const uint16_t *>(scripts.getBuffer());
    cd->scriptsLength = scripts.length();
    return cd.orphan();
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
