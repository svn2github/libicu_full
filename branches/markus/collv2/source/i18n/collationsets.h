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

U_NAMESPACE_BEGIN

struct CollationData;

class ContractionsAndExpansions : public UMemory {
public:
    ContractionsAndExpansions(UnicodeSet *con, UnicodeSet *exp, UBool prefixes)
            : data(NULL), tailoring(NULL),
              contractions(con), expansions(exp),
              addPrefixes(prefixes),
              checkTailored(0),
              prefix(NULL), suffix(NULL),
              errorCode(U_ZERO_ERROR) {}

    void get(const CollationData *d, UErrorCode &errorCode);

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
