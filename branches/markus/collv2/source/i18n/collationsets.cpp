/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationsets.cpp
*
* created on: 2013feb09
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucharstrie.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/ustringtrie.h"
#include "collation.h"
#include "collationdata.h"
#include "collationsets.h"
#include "uassert.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

U_CDECL_BEGIN

static UBool U_CALLCONV
enumCnERange(const void *context, UChar32 start, UChar32 end, uint32_t ce32) {
    ContractionsAndExpansions *cne = (ContractionsAndExpansions *)context;
    if(cne->checkTailored == 0) {
        // There is no tailoring.
        // No need to collect nor check the tailored set.
    } else if(cne->checkTailored < 0) {
        // Collect the set of code points with mappings in the tailoring data.
        if(ce32 == Collation::MIN_SPECIAL_CE32) {
            return TRUE;  // fallback to base, not tailored
        } else {
            cne->tailored.add(start, end);
        }
        // checkTailored > 0: Exclude tailored ranges from the base data enumeration.
    } else if(start == end) {
        if(cne->tailored.contains(start)) {
            return TRUE;
        }
    } else if(cne->tailored.containsSome(start, end)) {
        cne->ranges.set(start, end).removeAll(cne->tailored);
        int32_t count = cne->ranges.getRangeCount();
        for(int32_t i = 0; i < count; ++i) {
            cne->handleCE32(cne->ranges.getRangeStart(i), cne->ranges.getRangeEnd(i), ce32);
        }
        return U_SUCCESS(cne->errorCode);
    }
    cne->handleCE32(start, end, ce32);
    return U_SUCCESS(cne->errorCode);
}

U_CDECL_END

void
ContractionsAndExpansions::get(const CollationData *d, UErrorCode &ec) {
    if(U_FAILURE(ec)) { return; }
    errorCode = ec;  // Preserve info & warning codes.
    // Add all from the data, can be tailoring or base.
    if(d->base != NULL) {
        checkTailored = -1;
    }
    data = d;
    utrie2_enum(data->trie, NULL, enumCnERange, this);
    if(d->base == NULL || U_FAILURE(errorCode)) {
        ec = errorCode;
        return;
    }
    // Add all from the base data but only for un-tailored code points.
    tailored.freeze();
    checkTailored = 1;
    tailoring = d;
    data = d->base;
    utrie2_enum(data->trie, NULL, enumCnERange, this);
    ec = errorCode;
}

void
ContractionsAndExpansions::handleCE32(UChar32 start, UChar32 end, uint32_t ce32) {
    for(;;) {
        if(ce32 <= Collation::MIN_SPECIAL_CE32) {
            // !isSpecialCE32(), or fallback to the base
            return;
        }
        // Loop while ce32 is special.
        int32_t tag = Collation::getSpecialCE32Tag(ce32);
        if(tag <= Collation::EXPANSION_TAG || tag == Collation::HANGUL_TAG) {
            // Optimization: If we have a prefix,
            // then the relevant strings have been added already.
            if(prefix == NULL) {
                addExpansions(start, end);
            }
            return;
        } else if(tag == Collation::PREFIX_TAG) {
            handlePrefixes(start, end, ce32);
            return;
        } else if(tag == Collation::CONTRACTION_TAG) {
            handleContractions(start, end, ce32);
            return;
        } else if(tag == Collation::DIGIT_TAG) {
            // Fetch the non-numeric-collation CE32 and continue.
            ce32 = data->ce32s[(ce32 >> 4) & 0xffff];
        } else if(tag == Collation::RESERVED_TAG_11 || tag == Collation::LEAD_SURROGATE_TAG) {
            if(U_SUCCESS(errorCode)) { errorCode = U_INTERNAL_PROGRAM_ERROR; }
            return;
        } else if(tag == Collation::IMPLICIT_TAG && (ce32 & 1) == 0) {
            U_ASSERT(start == 0 && end == 0);
            // Fetch the normal ce32 for U+0000 and continue.
            ce32 = data->ce32s[0];
        } else {
            return;
        }
    }
}

void
ContractionsAndExpansions::handlePrefixes(
        UChar32 start, UChar32 end, uint32_t ce32) {
    const UChar *p = data->contexts + (ce32 & 0xfffff);
    uint32_t defaultCE32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no prefix match.
    handleCE32(start, end, defaultCE32);
    if(!addPrefixes) { return; }
    UCharsTrie::Iterator prefixes(p + 2, 0, errorCode);
    while(prefixes.next(errorCode)) {
        prefix = &prefixes.getString();
        // Prefix/pre-context mappings are special kinds of contractions
        // that always yield expansions.
        addStrings(start, end, contractions);
        addStrings(start, end, expansions);
        handleCE32(start, end, (uint32_t)prefixes.getValue());
    }
    prefix = NULL;
}

void
ContractionsAndExpansions::handleContractions(
        UChar32 start, UChar32 end, uint32_t ce32) {
    const UChar *p = data->contexts + ((int32_t)(ce32 >> 2) & 0x3ffff);
    uint32_t defaultCE32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
    handleCE32(start, end, defaultCE32);
    UCharsTrie::Iterator suffixes(p + 2, 0, errorCode);
    while(suffixes.next(errorCode)) {
        suffix = &suffixes.getString();
        addStrings(start, end, contractions);
        if(prefix != NULL) {
            addStrings(start, end, expansions);
        }
        handleCE32(start, end, (uint32_t)suffixes.getValue());
    }
    suffix = NULL;
}

void
ContractionsAndExpansions::addExpansions(UChar32 start, UChar32 end) {
    if(prefix == NULL && suffix == NULL) {
        if(expansions != NULL) {
            // TODO: verify that UnicodeSet takes a fastpath if start==end
            expansions->add(start, end);
        }
    } else {
        addStrings(start, end, expansions);
    }
}

void
ContractionsAndExpansions::addStrings(UChar32 start, UChar32 end, UnicodeSet *set) {
    if(set == NULL) { return; }
    UnicodeString s;
    int32_t prefixLength;
    if(prefix != NULL) {
        s = *prefix;
        prefixLength = prefix->length();
    } else {
        prefixLength = 0;
    }
    do {
        s.append(start);
        if(suffix != NULL) {
            s.append(*suffix);
        }
        expansions->add(s);
        s.truncate(prefixLength);
    } while(++start <= end);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
