/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationsets.h
*
* created on: 2013feb09
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONSETS_H__
#define __COLLATIONKEYS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "collation.h"

U_NAMESPACE_BEGIN

struct CollationData;

/**
 * Finds the set of characters and strings that sort differently in the tailoring
 * from the base data.
 *
 * Every mapping in the tailoring needs to be compared to the base,
 * because some mappings are copied for optimization, and
 * all contractions for a character are copied if any contractions for that character
 * are added, modified or removed.
 *
 * It might be simpler to re-parse the rule string, but:
 * - That would require duplicating some of the from-rules builder code.
 * - That would make the runtime code depend on the builder.
 * - That would only work if we have the rule string, and we allow users to
 *   omit the rule string from data files.
 */
class TailoredSet : public UMemory {
public:
    TailoredSet(UnicodeSet *t)
            : data(NULL), baseData(NULL),
              tailored(t),
              prefix(NULL), suffix(NULL),
              errorCode(U_ZERO_ERROR) {}

    void forData(const CollationData *d, UErrorCode &errorCode);

    // all following: @internal, only public for access by callback

    void handleCE32(UChar32 start, UChar32 end, uint32_t ce32);

    uint32_t getIndirectCE32(const CollationData *d, uint32_t ce32);
    uint32_t getFinalCE32(const CollationData *d, uint32_t ce32);

    void compare(UChar32 c, uint32_t ce32, uint32_t baseCE32);
    void comparePrefixes(UChar32 c, const UChar *p, const UChar *q);
    void compareContractions(UChar32 c, const UChar *p, const UChar *q);

    void addPrefixes(const CollationData *d, UChar32 c, const UChar *p);
    void addPrefix(const CollationData *d, const UnicodeString &pfx, UChar32 c, uint32_t ce32);
    void addContractions(UChar32 c, const UChar *p);
    void addSuffix(UChar32 c, const UnicodeString &sfx);
    void add(UChar32 c);

    const CollationData *data;
    const CollationData *baseData;
    UnicodeSet *tailored;
    const UnicodeString *prefix;
    const UnicodeString *suffix;
    UErrorCode errorCode;
};

class ContractionsAndExpansions : public UMemory {
public:
    ContractionsAndExpansions(UnicodeSet *con, UnicodeSet *exp, UBool prefixes)
            : data(NULL), tailoring(NULL),
              contractions(con), expansions(exp),
              addPrefixes(prefixes),
              checkTailored(0),
              prefix(NULL), suffix(NULL),
              errorCode(U_ZERO_ERROR) {}

    void forData(const CollationData *d, UErrorCode &errorCode);

    // all following: @internal, only public for access by callback

    void handleCE32(UChar32 start, UChar32 end, uint32_t ce32);

    void handlePrefixes(UChar32 start, UChar32 end, uint32_t ce32);
    void handleContractions(UChar32 start, UChar32 end, uint32_t ce32);

    void addExpansions(UChar32 start, UChar32 end);
    void addStrings(UChar32 start, UChar32 end, UnicodeSet *set);

    const CollationData *data;
    const CollationData *tailoring;
    UnicodeSet *contractions;
    UnicodeSet *expansions;
    UBool addPrefixes;
    int8_t checkTailored;  // -1: collected tailored  +1: exclude tailored
    UnicodeSet tailored;
    UnicodeSet ranges;
    const UnicodeString *prefix;
    const UnicodeString *suffix;
    UErrorCode errorCode;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONKEYS_H__
