/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* dictionarydata.h
*
* created on: 2012may31
* created by: Markus W. Scherer & Maxime Serrano
*/

#include "unicode/udata.h"
#include "cmemory.h"
#include "dictionarydata.h"

U_NAMESPACE_BEGIN

UBool DictionaryData::assertTrieType(int32_t type, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return FALSE; }
    if(type == trieType) { return TRUE; }
    errorCode = U_INVALID_FORMAT_ERROR;
    return FALSE;
}


/**
 * Returns the transformation offset.
 * Sets an error code if the data does not specify an offset transformation.
 * @return the transformation offset
 */
UChar32 DictionaryData::getTransformOffset(UErrorCode &errorCode) const { 
    if(U_FAILURE(errorCode)) { return U_SENTINEL; }
    if((transform & TRANSFORM_TYPE_MASK) != TRANSFORM_TYPE_OFFSET) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return U_SENTINEL;
    }
    return transform & TRANSFORM_OFFSET_MASK;
}


U_NAMESPACE_END

U_NAMESPACE_USE

U_CAPI int32_t U_EXPORT2
udict_swap(const UDataSwapper *ds, const void *inData, int32_t length,
           void *outData, UErrorCode *pErrorCode) {
    const UDataInfo *pInfo;
    int32_t headerSize;
    const uint8_t *inBytes;
    uint8_t *outBytes;
    const int32_t *inIndexes;
    int32_t indexes[DictionaryData::IX_COUNT];
    int32_t i, offset, size;

    headerSize = udata_swapDataHeader(ds, inData, length, outData, pErrorCode);
    if (pErrorCode == NULL || U_FAILURE(*pErrorCode)) return 0;
    pInfo = (const UDataInfo *)((const char *)inData + 4);
    if (!(pInfo->dataFormat[0] == 0x44 && 
          pInfo->dataFormat[1] == 0x69 && 
          pInfo->dataFormat[2] == 0x63 && 
          pInfo->dataFormat[3] == 0x74 && 
          pInfo->formatVersion[0] == 1)) {
        udata_printError(ds, "udict_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as dictionary data\n",
                         pInfo->dataFormat[0], pInfo->dataFormat[1], pInfo->dataFormat[2], pInfo->dataFormat[3], pInfo->formatVersion[0]);
        *pErrorCode = U_UNSUPPORTED_ERROR;
        return 0;
    }

    inBytes = (const uint8_t *)inData + headerSize;
    outBytes = (uint8_t *)outData + headerSize;

    inIndexes = (const int32_t *)inBytes;
    if (length >= 0) {
        length -= headerSize;
        if (length < (int32_t)(sizeof(indexes))) {
            udata_printError(ds, "udict_swap(): too few bytes (%d after header) for dictionary data\n", length);
            *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }
    }

    for (i = 0; i < DictionaryData::IX_COUNT; i++) {
       indexes[i] = udata_readInt32(ds, inIndexes[i]);
    }

    size = indexes[DictionaryData::IX_TOTAL_SIZE];

    if (length >= 0) {
        if (length < size) {
            udata_printError(ds, "udict_swap(): too few bytes (%d after header) for all of dictionary data\n", length);
            *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }

        if (inBytes != outBytes) {
            uprv_memcpy(outBytes, inBytes, size);
        }

        offset = 0;
        ds->swapArray32(ds, inBytes, sizeof(indexes), outBytes, pErrorCode);
        offset = (int32_t)sizeof(indexes);
        int32_t trieType = indexes[DictionaryData::IX_TRIE_TYPE] & DictionaryData::TRIE_TYPE_MASK;
        int32_t nextOffset = indexes[DictionaryData::IX_RESERVED1_OFFSET];

        if (trieType == DictionaryData::TRIE_TYPE_BYTES) {
           // we're done!
           offset = nextOffset;
        }
        else if (trieType == DictionaryData::TRIE_TYPE_UCHARS) {
            ds->swapArray16(ds, inBytes + offset, nextOffset - offset, outBytes + offset, pErrorCode);
            offset = nextOffset;
        } else {
            udata_printError(ds, "udict_swap(): unknown trie type!\n");
            *pErrorCode = U_UNSUPPORTED_ERROR;
            return 0;
        }
        nextOffset = indexes[DictionaryData::IX_RESERVED2_OFFSET];
        // nothing to do, as this section is empty in the current format
        offset = nextOffset;
        nextOffset = indexes[DictionaryData::IX_TOTAL_SIZE];
        offset = nextOffset;
    }
    return headerSize + size;
}


