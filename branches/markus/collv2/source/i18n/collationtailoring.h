/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoring.h
*
* created on: 2013mar12
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONTAILORING_H__
#define __COLLATIONTAILORING_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collationsettings.h"

U_NAMESPACE_BEGIN

struct CollationData;

/**
 * Collation tailoring data & settings.
 * This is a container of values for a collation tailoring
 * built from rules or deserialized from binary data.
 *
 * It is logically immutable: Do not modify its values.
 * The fields are public for convenience.
 *
 * It is shared, reference-counted, and auto-deleted.
 * Use either LocalPointer or addRef()/removeRef().
 * Reference-counting avoids having to be able to clone every field,
 * and saves memory, when a Collator is cloned.
 * The constructors initialize refCount to 0.
 */
struct U_I18N_API CollationTailoring : public UMemory {
    CollationTailoring();
    CollationTailoring(const CollationSettings &baseSettings);
    ~CollationTailoring();

    /**
     * Increments the number of references to this object. Thread-safe.
     */
    void addRef() const;
    /**
     * Decrements the number of references to this object,
     * and auto-deletes "this" if the number becomes 0. Thread-safe.
     */
    void removeRef() const;

    UBool ensureOwnedData(UErrorCode &errorCode);

    // data for sorting etc.
    const CollationData *data;  // == base data or ownedData
    CollationSettings settings;
    UnicodeString rules;
    UVersionInfo version;

    // owned objects
    CollationData *ownedData;
    UObject *builder;
    UDataMemory *memory;
    UTrie2 *trie;
    UnicodeSet *unsafeBackwardSet;
    int32_t *reorderCodes;
    uint8_t reorderTable[256];

    mutable int32_t refCount;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONTAILORING_H__
