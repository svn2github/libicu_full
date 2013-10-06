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

#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collationsettings.h"
#include "uhash.h"
#include "umutex.h"

struct UDataMemory;
struct UResourceBundle;
struct UTrie2;

U_NAMESPACE_BEGIN

struct CollationData;

class UnicodeSet;

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
    CollationTailoring(const CollationSettings *baseSettings);
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
    void deleteIfZeroRefCount() const;

    UBool ensureOwnedData(UErrorCode &errorCode);

    static void makeBaseVersion(const UVersionInfo ucaVersion, UVersionInfo version);
    void setVersion(const UVersionInfo baseVersion, const UVersionInfo rulesVersion);
    int32_t getUCAVersion() const;

    // data for sorting etc.
    const CollationData *data;  // == base data or ownedData
    CollationSettings settings;
    UnicodeString rules;
    Locale actualLocale, validLocale;  // bogus when built from rules or constructed from a binary blob
    // UCA version u.v.w & rules version r.s.t.q:
    // version[0]: builder version (runtime version is mixed in at runtime)
    // version[1]: bits 7..3=u, bits 2..0=v
    // version[2]: bits 7..6=w, bits 5..0=r
    // version[3]= (s<<5)+(s>>3)+t+(q<<4)+(q>>4)
    UVersionInfo version;

    // owned objects
    CollationData *ownedData;
    UObject *builder;
    UDataMemory *memory;
    UResourceBundle *bundle;
    UTrie2 *trie;
    UnicodeSet *unsafeBackwardSet;
    int32_t *reorderCodes;
    uint8_t reorderTable[256];
    mutable UHashtable *maxExpansions;
    mutable UInitOnce maxExpansionsInitOnce;

    mutable u_atomic_int32_t refCount;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONTAILORING_H__
