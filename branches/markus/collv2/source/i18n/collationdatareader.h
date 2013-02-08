/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatareader.h
*
* created on: 2013feb07
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATAREADER_H__
#define __COLLATIONDATAREADER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collation.h"
#include "collationdata.h"
#include "collationsettings.h"
#include "normalizer2impl.h"
#include "utrie2.h"

struct UDataMemory;

U_NAMESPACE_BEGIN

/**
 * Collation data reader.
 * Reads bindary data and stores collation data and settings.
 * Owns deserialized, heap-allocated objects.
 */
struct U_I18N_API CollationDataReader : public UMemory {
    CollationDataReader(const Normalizer2Impl &nfc)
            : data(nfc),
              memory(NULL),
              trie(NULL), unsafeBackwardSet(NULL) {
        version[0] = version[1] = version[2] = version[3] = 0;
    }

    CollationDataReader *clone() const;

    ~CollationDataReader();

    enum {
        /**
         * Number of int32_t indexes.
         *
         * Can be 2 if there are only options.
         * Can be 7 or 8 if there are only options and a script reordering.
         * The loader treats any index>=indexes[IX_INDEXES_LENGTH] as 0.
         */
        IX_INDEXES_LENGTH,  // 0
        /**
         * Bits 31..24: numericPrimary, for numeric collation
         *      23.. 0: options bit set
         */
        IX_OPTIONS,
        IX_RESERVED2,
        IX_RESERVED3,

        /** Array offset to Jamo CEs in ces[], or <0 if none. */
        IX_JAMO_CES_START,  // 4

        // Byte offsets from the start of the data, after the generic header.
        // The indexes[] are at byte offset 0, other data follows.
        // Each data item is aligned properly.
        // The data items should be in descending order of unit size,
        // to minimize the need for padding.
        // Each item's byte length is given by the difference between its offset and
        // the next index/offset value.
        /** Byte offset to int32_t reorderCodes[]. */
        IX_REORDER_CODES_OFFSET,
        /**
         * Byte offset to uint8_t reorderTable[].
         * Empty table if <256 bytes (padding only).
         * Otherwise 256 bytes or more (with padding).
         */
        IX_REORDER_TABLE_OFFSET,
        /** Byte offset to the collation trie. Its length is a multiple of 8 bytes. */
        IX_TRIE_OFFSET,

        IX_RESERVED8_OFFSET,  // 8
        /** Byte offset to int64_t ces[]. */
        IX_CES_OFFSET,
        IX_RESERVED10_OFFSET,
        /** Byte offset to uint32_t ce32s[]. */
        IX_CE32S_OFFSET,

        IX_RESERVED12_OFFSET,  // 12
        /** Byte offset to UChar *contexts[]. */
        IX_CONTEXTS_OFFSET,
        /** Byte offset to uint16_t [] with serialized unsafeBackwardSet. */
        IX_UNSAFE_BWD_OFFSET,
        IX_RESERVED15_OFFSET,

        /** Byte offset to uint16_t scripts[]. */
        IX_SCRIPTS_OFFSET,  // 16
        /**
         * Byte offset to UBool compressibleBytes[].
         * Empty table if <256 bytes (padding only).
         * Otherwise 256 bytes or more (with padding).
         */
        IX_COMPRESSIBLE_BYTES_OFFSET,
        IX_RESERVED18_OFFSET,
        IX_TOTAL_SIZE
    };

    void setData(const CollationData *baseData, const uint8_t *inBytes, UErrorCode &errorCode);

    static UBool U_CALLCONV
    isAcceptable(void *context, const char *type, const char *name, const UDataInfo *pInfo);

    CollationData data;
    CollationSettings settings;
    UnicodeString rules;
    UVersionInfo version;

    UDataMemory *memory;
    UTrie2 *trie;
    UnicodeSet *unsafeBackwardSet;
};

/**
 * Format of collation data (ucadata.icu, binary data in coll/ *.res files).
 * Format version 4.0.
 *
 * TODO: document format, see normalizer2impl.h
 */

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATAREADER_H__
