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
#include "collationdata.h"
#include "collationsettings.h"

U_NAMESPACE_BEGIN

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
 */
struct U_I18N_API CollationTailoring : public UMemory {
    CollationTailoring(const CollationSettings &baseSettings);
    ~CollationTailoring();

    /**
     * Increments the number of references to this object. Thread-safe.
     */
    void addRef();
    /**
     * Decrements the number of references to this object,
     * and auto-deletes "this" if the number becomes 0. Thread-safe.
     */
    void removeRef();

    const CollationData *data;
    CollationSettings settings;
    UnicodeString rules;
    UVersionInfo version;
    int32_t refCount;
    /**
     * TRUE if this object owns the data and its values.
     * Otherwise the data is an alias to the base data.
     */
    UBool isDataOwned;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONTAILORING_H__
