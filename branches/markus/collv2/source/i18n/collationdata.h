/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdata.h
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATA_H__
#define __COLLATIONDATA_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

/**
 * Data container with access functions which walk the data structures.
 */
class U_I18N_API CollationData : public UMemory {
public:
    uint32_t getCE32(UChar32 c) const {
        return UTRIE2_GET32(trie, c);
    }

    UBool isUnsafeBackward(UChar32 c) {
        if(U_IS_TRAIL(c)) {
            return TRUE;
        }
        return FALSE;  // TODO
        // TODO: Are all cc!=0 marked as unsafe for prevCE() (because of discontiguous contractions)?
        // TODO: Use a frozen UnicodeSet rather than an imprecise bit set, at least initially.
    }

    /**
     * Returns this data object's main UTrie2.
     * To be used only by the CollationIterator constructor.
     */
    const UTrie2 *getTrie() { return trie; }

protected:
    // Main lookup trie.
    const UTrie2 *trie;

private:
    UBool isFinalData;  // TODO: needed?
    const CollationData *base;  // TODO: probably needed?
};

/**
 * Low-level CollationData builder.
 * Takes (character, CE) pairs and builds them into runtime data structures.
 * Supports characters with context prefixes and contraction suffixes.
 */
class CollationDataBuilder : public CollationData {
};

// TODO: In CollationWeights allocator,
// try to treat secondary & tertiary weights as 3/4-byte weights with bytes 1 & 2 == 0.
// Natural secondary limit of 0x10000.

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATA_H__
