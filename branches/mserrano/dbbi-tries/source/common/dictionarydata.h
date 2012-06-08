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

#ifndef __DICTIONARYDATA_H__
#define __DICTIONARYDATA_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/udata.h"
#include "udataswp.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class U_COMMON_API DictionaryData : public UMemory {
public:
    DictionaryData()
            : memory(NULL), bytes(NULL), uchars(NULL),
              trieType(TRIE_TYPE_BYTES), transform(TRANSFORM_NONE) {
    }
    ~DictionaryData();

    /** Loads the name.dict file from the packageName. */
    void load(const char *packageName, const char *name, UErrorCode &errorCode);

    /**
     * Checks that the the trie type in the data is the expected type.
     * Sets an error code if the types differ.
     * @param type TRIE_HAS_VALUES | TRIE_TYPE_BYTES etc.
     * @return TRUE if the data has the expected type
     */
    UBool assertTrieType(int32_t type, UErrorCode &errorCode) const;

    static const int32_t TRIE_TYPE_BYTES = 0;
    static const int32_t TRIE_TYPE_UCHARS = 1;
    static const int32_t TRIE_TYPE_MASK = 7;
    static const int32_t TRIE_HAS_VALUES = 8;

    static const int32_t TRANSFORM_NONE = 0;
    static const int32_t TRANSFORM_TYPE_OFFSET = 0x1000000;
    static const int32_t TRANSFORM_TYPE_MASK = 0x7f000000;
    static const int32_t TRANSFORM_OFFSET_MASK = 0x1fffff;


    /**
     * Returns the transformation offset.
     * Sets an error code if the data does not specify an offset transformation.
     * @return the transformation offset
     */
    UChar32 getTransformOffset(UErrorCode &errorCode) const;

    const char *getBytesTrie() const { return bytes; }
    const UChar *getUCharsTrie() const { return uchars; }

    enum {
        // Byte offsets from the start of the data, after the generic header.
        IX_STRING_TRIE_OFFSET,
        IX_RESERVED1_OFFSET,
        IX_RESERVED2_OFFSET,
        IX_TOTAL_SIZE,

        // Trie type: TRIE_HAS_VALUES | TRIE_TYPE_BYTES etc.
        IX_TRIE_TYPE,
        // Transform specification: TRANSFORM_TYPE_OFFSET | 0xe00 etc.
        IX_TRANSFORM,

        IX_RESERVED6,
        IX_RESERVED7,
        IX_COUNT
    };

private:
    static UBool U_CALLCONV
    isAcceptable(void *context, const char *type, const char *name, const UDataInfo *pInfo);

    UDataMemory *memory;
    UVersionInfo dataVersion;

    // At most one of the following pointers can be non-NULL.
    const char *bytes;
    const UChar *uchars;

    int32_t trieType;  // indexes[IX_TRIE_TYPE]
    int32_t transform;  // indexes[IX_TRANSFORM]
};

U_NAMESPACE_END

U_CAPI int32_t U_EXPORT2
udict_swap(const UDataSwapper *ds, const void *inData, int32_t length, void *outData, UErrorCode *pErrorCode);

/**
 * Format of dictionary .dict data files.
 * Format version 1.0.
 *
 * A dictionary .dict data file contains a byte-serialized BytesTrie or
 * a UChars-serialized UCharsTrie.
 * Such files are used in dictionary-based break iteration (DBBI).
 *
 * For a BytesTrie, a transformation type is specified for
 * transforming Unicode strings into byte sequences.
 *
 * A .dict file begins with a standard ICU data file header
 * (DataHeader, see ucmndata.h and unicode/udata.h).
 * The UDataInfo.dataVersion field is currently unused (set to 0.0.0.0).
 *
 * After the header, the file contains the following parts.
 * Constants are defined in the DictionaryData class.
 *
 * For the data structure of BytesTrie & UCharsTrie see
 * http://site.icu-project.org/design/struct/tries
 * and the bytestrie.h and ucharstrie.h header files.
 *
 * int32_t indexes[indexesLength]; -- indexesLength=indexes[IX_STRING_TRIE_OFFSET]/4;
 *
 *      The first four indexes are byte offsets in ascending order.
 *      Each byte offset marks the start of the next part in the data file,
 *      and the end of the previous one.
 *      When two consecutive byte offsets are the same, then the corresponding part is empty.
 *      Byte offsets are offsets from after the header,
 *      that is, from the beginning of the indexes[].
 *      Each part starts at an offset with proper alignment for its data.
 *      If necessary, the previous part may include padding bytes to achieve this alignment.
 *
 *      trieType=indexes[IX_TRIE_TYPE] defines the trie type.
 *      transform=indexes[IX_TRANSFORM] defines the Unicode-to-bytes transformation.
 *          If the transformation type is TRANSFORM_TYPE_OFFSET,
 *          then the lower 21 bits contain the offset code point.
 *          Each code point c is mapped to byte b = (c - offset).
 *          Code points outside the range offset..(offset+0xff) cannot be mapped
 *          and do not occur in the dictionary.
 *
 * stringTrie; -- a serialized BytesTrie or UCharsTrie
 *
 *      The dictionary maps strings to specific values (TRIE_HAS_VALUES bit set in trieType),
 *      or it maps all strings to 0 (TRIE_HAS_VALUES bit not set).
 */

#endif  /* !UCONFIG_NO_BREAK_ITERATION */
#endif  /* __DICTIONARYDATA_H__ */
