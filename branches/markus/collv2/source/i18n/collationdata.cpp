/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdata.cpp
*
* created on: 2012jul28
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/udata.h"
#include "unicode/uscript.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "uassert.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

CollationData *
CollationData::clone() const {
    LocalPointer<CollationData> newData(new CollationData(*this));
    if(newData.isNull()) {
        return NULL;
    }
    // The copy constructor does a shallow clone, copying all fields.
    // We cannot clone the UDataMemory.
    newData->memory = NULL;
    if(base == NULL) {
        // The root collator singleton is cached and owns its loaded data.
        // Clones need not clone that data.
        newData->ownedTrie = NULL;
        newData->ownedUnsafeBackwardSet = NULL;
    } else {
        // The CollationData object for a loaded or runtime-built tailoring
        // is not cached, and we need to deep-clone owned objects.
        if(ownedTrie != NULL) {
            UErrorCode errorCode = U_ZERO_ERROR;
            newData->ownedTrie = utrie2_clone(ownedTrie, &errorCode);
            if(newData->ownedTrie == NULL || U_FAILURE(errorCode)) {
                return NULL;
            }
            newData->trie = newData->ownedTrie;
        }
        if(ownedUnsafeBackwardSet != NULL) {
            newData->ownedUnsafeBackwardSet = new UnicodeSet(*ownedUnsafeBackwardSet);
            if(newData->ownedUnsafeBackwardSet == NULL) {
                return NULL;
            }
            newData->unsafeBackwardSet = newData->ownedUnsafeBackwardSet;
        }
    }
    return newData.orphan();
}

CollationData::~CollationData() {
    udata_close(memory);
    utrie2_close(ownedTrie);
    delete ownedUnsafeBackwardSet;
}

void
CollationData::setStrength(int32_t value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noStrength = options & ~STRENGTH_MASK;
    switch(value) {
    case UCOL_PRIMARY:
    case UCOL_SECONDARY:
    case UCOL_TERTIARY:
    case UCOL_QUATERNARY:
    case UCOL_IDENTICAL:
        options = noStrength | (value << STRENGTH_SHIFT);
        break;
    case UCOL_DEFAULT:
        options = noStrength | (defaultOptions & STRENGTH_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setFlag(int32_t bit, UColAttributeValue value,
                       int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    switch(value) {
    case UCOL_ON:
        options |= bit;
        break;
    case UCOL_OFF:
        options &= ~bit;
        break;
    case UCOL_DEFAULT:
        options = (options & ~bit) | (defaultOptions & bit);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setCaseFirst(UColAttributeValue value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noCaseFirst = options & ~CASE_FIRST_AND_UPPER_MASK;
    switch(value) {
    case UCOL_OFF:
        options = noCaseFirst;
        break;
    case UCOL_LOWER_FIRST:
        options = noCaseFirst | CASE_FIRST;
        break;
    case UCOL_UPPER_FIRST:
        options = noCaseFirst | CASE_FIRST_AND_UPPER_MASK;
        break;
    case UCOL_DEFAULT:
        options = noCaseFirst | (defaultOptions & CASE_FIRST_AND_UPPER_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setAlternateHandling(UColAttributeValue value,
                                    int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noAlternate = options & ~ALTERNATE_MASK;
    switch(value) {
    case UCOL_NON_IGNORABLE:
        options = noAlternate;
        break;
    case UCOL_SHIFTED:
        options = noAlternate | SHIFTED;
        break;
    case UCOL_DEFAULT:
        options = noAlternate | (defaultOptions & ALTERNATE_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

uint32_t
CollationData::getVariableTopForMaxVariable(MaxVariable maxVariable) const {
    int32_t index = findScript(UCOL_REORDER_CODE_FIRST + maxVariable);
    if(index < 0) {
        return 0;
    }
    int32_t head = scripts[index];
    int32_t lastByte = head & 0xff;
    return (((uint32_t)lastByte + 1) << 24) - 1;
}

int32_t
CollationData::findScript(int32_t script) const {
    if(script < 0 || 0xffff < script) { return -1; }
    for(int32_t i = 0; i < scriptsLength;) {
        int32_t limit = i + 2 + scripts[i + 1];
        for(int32_t j = i + 2; j < limit; ++j) {
            if(script == scripts[j]) { return i; }
        }
        i = limit;
    }
    return -1;
}

int32_t
CollationData::getEquivalentScripts(int32_t script,
                                    int32_t dest[], int32_t capacity,
                                    UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    int32_t i = findScript(script);
    if(i < 0) { return 0; }
    int32_t length = scripts[i + 1];
    U_ASSERT(length != 0);
    if(length > capacity) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return length;
    }
    i += 2;
    dest[0] = scripts[i++];
    for(int32_t j = 1; j < length; ++j) {
        script = scripts[i++];
        // Sorted insertion.
        for(int32_t k = j;; --k) {
            // Invariant: dest[k] is free to receive either script or dest[k - 1].
            if(k > 0 && script < dest[k - 1]) {
                dest[k] = dest[k - 1];
            } else {
                dest[k] = script;
                break;
            }
        }
    }
    return length;
}

void
CollationData::makeReorderTable(const int32_t *reorder, int32_t length,
                                uint8_t table[256], UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return; }

    // Initialize the table.
    // Never reorder special low and high primary lead bytes.
    int32_t lowByte;
    for(lowByte = 0; lowByte <= Collation::MERGE_SEPARATOR_BYTE; ++lowByte) {
        table[lowByte] = lowByte;
    }
    // lowByte == 03

    int32_t highByte;
    for(highByte = 0xff; highByte >= Collation::UNASSIGNED_IMPLICIT_BYTE; --highByte) {
        table[highByte] = highByte;
    }
    // highByte == FC

    // Set intermediate bytes to 0 to indicate that they have not been set yet.
    for(int32_t i = lowByte; i <= highByte; ++i) {
        table[i] = 0;
    }

    // Get the set of special reorder codes in the input list.
    // This supports up to 32 special reorder codes;
    // it works for data with codes beyond UCOL_REORDER_CODE_LIMIT.
    uint32_t specials = 0;
    for(int32_t i = 0; i < length; ++i) {
        int32_t reorderCode = reorder[i] - UCOL_REORDER_CODE_FIRST;
        if(0 <= reorderCode && reorderCode <= 31) {
            specials |= (uint32_t)1 << reorderCode;
        }
    }

    // Start the reordering with the special low reorder codes that do not occur in the input.
    for(int32_t i = 0;; i += 3) {
        if(scripts[i + 1] != 1) { break; }  // Went beyond special single-code reorder codes.
        int32_t reorderCode = (int32_t)scripts[i + 2] - UCOL_REORDER_CODE_FIRST;
        if(reorderCode < 0) { break; }  // Went beyond special reorder codes.
        if((specials & ((uint32_t)1 << reorderCode)) == 0) {
            int32_t head = scripts[i];
            int32_t firstByte = head >> 8;
            int32_t lastByte = head & 0xff;
            do { table[firstByte++] = lowByte++; } while(firstByte <= lastByte);
        }
    }

    // Reorder according to the input scripts, continuing from the bottom of the bytes range.
    for(int32_t i = 0; i < length;) {
        int32_t script = reorder[i++];
        if(script == USCRIPT_UNKNOWN) {
            // Put the remaining scripts at the top.
            while(i < length) {
                script = reorder[--length];
                if(script == USCRIPT_UNKNOWN ||  // Must occur at most once.
                        script == UCOL_REORDER_CODE_DEFAULT) {
                    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                    return;
                }
                int32_t index = findScript(script);
                if(index < 0) { continue; }
                int32_t head = scripts[index];
                int32_t firstByte = head >> 8;
                int32_t lastByte = head & 0xff;
                if(table[firstByte] != 0) {  // Duplicate or equivalent script.
                    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                    return;
                }
                do { table[lastByte--] = highByte--; } while(firstByte <= lastByte);
            }
            break;
        }
        if(script == UCOL_REORDER_CODE_DEFAULT) {
            // The default code must be the only one in the list, and that is handled by the caller.
            // Otherwise it must not be used.
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        int32_t index = findScript(script);
        if(index < 0) { continue; }
        int32_t head = scripts[index];
        int32_t firstByte = head >> 8;
        int32_t lastByte = head & 0xff;
        if(table[firstByte] != 0) {  // Duplicate or equivalent script.
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        do { table[firstByte++] = lowByte++; } while(firstByte <= lastByte);
    }

    // Put all remaining scripts into the middle.
    // Avoid table[0] which must remain 0.
    for(int32_t i = 1; i <= 0xff; ++i) {
        if(table[i] == 0) { table[i] = lowByte++; }
    }
    U_ASSERT(lowByte == highByte + 1);
}

namespace {

int32_t getIndex(const int32_t *indexes, int32_t length, int32_t i) {
    return (i < length) ? indexes[i] : 0;
}

};  // namespace

void
CollationData::setData(const CollationData *baseData, const uint8_t *inBytes,
                       UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    const int32_t *inIndexes = reinterpret_cast<const int32_t *>(inBytes);
    int32_t indexesLength = inIndexes[IX_INDEXES_LENGTH];
    if(indexesLength < 2) {
        errorCode = U_INVALID_FORMAT_ERROR;  // Not enough indexes.
        return;
    }

    base = baseData;
    options = inIndexes[IX_OPTIONS];
    numericPrimary = options & 0xff000000;
    options &= 0xffffff;

    // Set pointers to non-empty data parts.
    // Do this in order of their byte offsets. (Should help porting to Java.)

    int32_t index;  // one of the indexes[] slots
    int32_t offset;  // byte offset for the index part
    int32_t length;  // number of bytes in the index part

    index = IX_REORDER_CODES_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 4) {
        reorderCodes = reinterpret_cast<const int32_t *>(inBytes + offset);
        reorderCodesLength = length / 4;
    } else {
        reorderCodes = NULL;
        reorderCodesLength = 0;
    }

    // There should be a reorder table only if there are reorder codes.
    // However, when there are reorder codes the reorder table may be omitted to reduce
    // the data size, and then the caller needs to allocate and build the reorder table.
    index = IX_REORDER_TABLE_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 256) {
        reorderTable = inBytes + offset;
    } else {
        reorderTable = NULL;
    }

    index = IX_TRIE_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 8) {
        trie = ownedTrie = utrie2_openFromSerialized(
            UTRIE2_32_VALUE_BITS, inBytes + offset, length, NULL,
            &errorCode);
        if(U_FAILURE(errorCode)) { return; }
    } else if(base != NULL) {
        trie = base->trie;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No mappings.
        return;
    }

    index = IX_CES_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 8) {
        ces = reinterpret_cast<const int64_t *>(inBytes + offset);
    } else {
        ces = NULL;
    }

    int32_t jamoCEsStart = getIndex(inIndexes, indexesLength, IX_JAMO_CES_START);
    if(jamoCEsStart >= 0) {
        if(ces == NULL) {
            errorCode = U_INVALID_FORMAT_ERROR;  // Index into non-existent CEs[].
            return;
        }
        jamoCEs = ces + jamoCEsStart;
    } else if(base != NULL) {
        jamoCEs = base->jamoCEs;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No Jamo CEs for Hangul processing.
        return;
    }

    index = IX_CE32S_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 4) {
        ce32s = reinterpret_cast<const uint32_t *>(inBytes + offset);
    } else {
        ce32s = NULL;
    }

    index = IX_CONTEXTS_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 2) {
        contexts = reinterpret_cast<const UChar *>(inBytes + offset);
    } else {
        contexts = NULL;
    }

    index = IX_UNSAFE_BWD_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 2) {
        if(base == NULL) {
            // Create the unsafe-backward set for the root collator.
            // Include all non-zero combining marks and trail surrogates.
            // We do this at load time, rather than at build time,
            // to simplify Unicode version bootstrapping:
            // The root data builder only needs the new FractionalUCA.txt data,
            // but it need not be built with a version of ICU already updated to
            // the corresponding new Unicode Character Database.
            // TODO: Optimize, and reduce dependencies,
            // by enumerating the Normalizer2Impl data more directly.
            ownedUnsafeBackwardSet = new UnicodeSet(
                UNICODE_STRING_SIMPLE("[[:^lccc=0:][\\udc00-\\udfff]]"), errorCode);
            if(U_FAILURE(errorCode)) { return; }
        } else {
            // Clone the root collator's set.
            ownedUnsafeBackwardSet = new UnicodeSet(*base->unsafeBackwardSet);
        }
        if(ownedUnsafeBackwardSet == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        // Add the ranges from the data file to the unsafe-backward set.
        USerializedSet sset;
        const uint16_t *unsafeData = reinterpret_cast<const uint16_t *>(inBytes + offset);
        if(!uset_getSerializedSet(&sset, unsafeData, length / 2)) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        int32_t count = uset_getSerializedRangeCount(&sset);
        for(int32_t i = 0; i < count; ++i) {
            UChar32 start, end;
            uset_getSerializedRange(&sset, i, &start, &end);
            ownedUnsafeBackwardSet->add(start, end);
        }
        // Mark each lead surrogate as "unsafe"
        // if any of its 1024 associated supplementary code points is "unsafe".
        UChar32 c = 0x10000;
        for(UChar lead = 0xd800; lead < 0xdc00; ++lead, c += 0x400) {
            if(!ownedUnsafeBackwardSet->containsNone(c, c + 0x3ff)) {
                ownedUnsafeBackwardSet->add(lead);
            }
        }
        ownedUnsafeBackwardSet->freeze();
        unsafeBackwardSet = ownedUnsafeBackwardSet;
    } else if(base != NULL) {
        // No tailoring-specific data: Alias the root collator's set.
        unsafeBackwardSet = base->unsafeBackwardSet;
    } else {
        unsafeBackwardSet = NULL;
    }

    index = IX_SCRIPTS_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 2) {
        scripts = reinterpret_cast<const uint16_t *>(inBytes + offset);
        scriptsLength = length / 2;
    } else if(base != NULL) {
        scripts = base->scripts;
        scriptsLength = base->scriptsLength;
    } else {
        scripts = NULL;
        scriptsLength = 0;
    }

    index = IX_COMPRESSIBLE_BYTES_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 256) {
        compressibleBytes = reinterpret_cast<const UBool *>(inBytes + offset);
    } else if(base != NULL) {
        compressibleBytes = base->compressibleBytes;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No compressibleBytes[].
        return;
    }

    // Set variableTop from options & ALTERNATE_MASK + MAX_VARIABLE_MASK and scripts data.
    int32_t maxVariable = (options & MAX_VARIABLE_MASK) >> MAX_VARIABLE_SHIFT;
    variableTop = getVariableTopForMaxVariable((MaxVariable)maxVariable);
    if(variableTop == 0) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return;
    }
}

UBool U_CALLCONV
CollationData::isAcceptable(void *context,
                            const char * /* type */, const char * /*name*/,
                            const UDataInfo *pInfo) {
    if(
        pInfo->size >= 20 &&
        pInfo->isBigEndian == U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily == U_CHARSET_FAMILY &&
        pInfo->dataFormat[0] == 0x55 &&  // dataFormat="UCol"
        pInfo->dataFormat[1] == 0x43 &&
        pInfo->dataFormat[2] == 0x6f &&
        pInfo->dataFormat[3] == 0x6c &&
        pInfo->formatVersion[0] == 4
    ) {
        UVersionInfo *version = reinterpret_cast<UVersionInfo *>(context);
        if(version != NULL) {
            uprv_memcpy(version, pInfo->dataVersion, 4);
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
