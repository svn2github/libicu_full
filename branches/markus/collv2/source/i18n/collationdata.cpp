/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdata.cpp
*
* created on: 2012jul28
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/uscript.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

void
CollationData::setStrength(int32_t value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noStrength = options & ~STRENGTH_MASK;
    switch(value) {
    case UCOL_PRIMARY:
    case UCOL_SECONDARY:
    case UCOL_TERTIARY:
    case UCOL_QUATERNARY:
    case UCOL_IDENTICAL:
        options = noStrength | (value << STRENGTH_SHIFT);
        break;
    case UCOL_DEFAULT:
        options = noStrength | (defaultOptions & STRENGTH_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setFlag(int32_t bit, UColAttributeValue value,
                       int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    switch(value) {
    case UCOL_ON:
        options |= bit;
        break;
    case UCOL_OFF:
        options &= ~bit;
        break;
    case UCOL_DEFAULT:
        options = (options & ~bit) | (defaultOptions & bit);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setCaseFirst(UColAttributeValue value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noCaseFirst = options & ~CASE_FIRST_AND_UPPER_MASK;
    switch(value) {
    case UCOL_OFF:
        options = noCaseFirst;
        break;
    case UCOL_LOWER_FIRST:
        options = noCaseFirst | CASE_FIRST;
        break;
    case UCOL_UPPER_FIRST:
        options = noCaseFirst | CASE_FIRST_AND_UPPER_MASK;
        break;
    case UCOL_DEFAULT:
        options = noCaseFirst | (defaultOptions & CASE_FIRST_AND_UPPER_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationData::setAlternateHandling(UColAttributeValue value,
                                    int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noAlternate = options & ~ALTERNATE_MASK;
    switch(value) {
    case UCOL_NON_IGNORABLE:
        options = noAlternate;
        break;
    case UCOL_SHIFTED:
        options = noAlternate | SHIFTED;
        break;
    case UCOL_DEFAULT:
        options = noAlternate | (defaultOptions & ALTERNATE_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

int32_t
CollationBaseData::findScript(int32_t script) const {
    if(script < 0 || 0xffff < script) { return -1; }
    for(int32_t i = 0; i < scriptsLength;) {
        int32_t limit = i + 2 + scripts[i + 1];
        for(int32_t j = i + 2; j < limit; ++j) {
            if(script == scripts[j]) { return i; }
        }
        i = limit;
    }
    return -1;
}

int32_t
CollationBaseData::getEquivalentScripts(int32_t script,
                                        int32_t dest[], int32_t capacity,
                                        UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    int32_t i = findScript(script);
    if(i < 0) { return 0; }
    int32_t length = scripts[i + 1];
    U_ASSERT(length != 0);
    if(length > capacity) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return length;
    }
    i += 2;
    dest[0] = scripts[i++];
    for(int32_t j = 1; j < length; ++j) {
        script = scripts[i++];
        // Sorted insertion.
        for(int32_t k = j;; --k) {
            // Invariant: dest[k] is free to receive either script or dest[k - 1].
            if(k > 0 && script < dest[k - 1]) {
                dest[k] = dest[k - 1];
            } else {
                dest[k] = script;
                break;
            }
        }
    }
    return length;
}

void
CollationBaseData::makeReorderTable(const int32_t *reorder, int32_t length,
                                    uint8_t table[256], UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return; }

    // Initialize the table.
    // Never reorder special low and high primary lead bytes.
    int32_t lowByte;
    for(lowByte = 0; lowByte <= Collation::MERGE_SEPARATOR_BYTE; ++lowByte) {
        table[lowByte] = lowByte;
    }
    // lowByte == 03

    int32_t highByte;
    for(highByte = 0xff; highByte >= Collation::UNASSIGNED_IMPLICIT_BYTE; --highByte) {
        table[highByte] = highByte;
    }
    // highByte == FC

    // Set intermediate bytes to 0 to indicate that they have not been set yet.
    for(int32_t i = lowByte; i <= highByte; ++i) {
        table[i] = 0;
    }

    // Get the set of special reorder codes in the input list.
    // This supports up to 32 special reorder codes;
    // it works for data with codes beyond UCOL_REORDER_CODE_LIMIT.
    uint32_t specials = 0;
    for(int32_t i = 0; i < length; ++i) {
        int32_t reorderCode = reorder[i] - UCOL_REORDER_CODE_FIRST;
        if(0 <= reorderCode && reorderCode <= 31) {
            specials |= (uint32_t)1 << reorderCode;
        }
    }

    // Start the reordering with the special low reorder codes that do not occur in the input.
    for(int32_t i = 0;; i += 3) {
        if(scripts[i + 1] != 1) { break; }  // Went beyond special single-code reorder codes.
        int32_t reorderCode = (int32_t)scripts[i + 2] - UCOL_REORDER_CODE_FIRST;
        if(reorderCode < 0) { break; }  // Went beyond special reorder codes.
        if((specials & ((uint32_t)1 << reorderCode)) == 0) {
            int32_t head = scripts[i];
            int32_t firstByte = head >> 8;
            int32_t lastByte = head & 0xff;
            do { table[firstByte++] = lowByte++; } while(firstByte <= lastByte);
        }
    }

    // Reorder according to the input scripts, continuing from the bottom of the bytes range.
    for(int32_t i = 0; i < length;) {
        int32_t script = reorder[i++];
        if(script == USCRIPT_UNKNOWN) {
            // Put the remaining scripts at the top.
            while(i < length) {
                script = reorder[--length];
                if(script == USCRIPT_UNKNOWN ||  // Must occur at most once.
                        script == UCOL_REORDER_CODE_DEFAULT) {
                    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                    return;
                }
                int32_t index = findScript(script);
                if(index < 0) { continue; }
                int32_t head = scripts[index];
                int32_t firstByte = head >> 8;
                int32_t lastByte = head & 0xff;
                if(table[firstByte] != 0) {  // Duplicate or equivalent script.
                    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                    return;
                }
                do { table[lastByte--] = highByte--; } while(firstByte <= lastByte);
            }
            break;
        }
        if(script == UCOL_REORDER_CODE_DEFAULT) {
            // The default code must be the only one in the list, and that is handled by the caller.
            // Otherwise it must not be used.
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        int32_t index = findScript(script);
        if(index < 0) { continue; }
        int32_t head = scripts[index];
        int32_t firstByte = head >> 8;
        int32_t lastByte = head & 0xff;
        if(table[firstByte] != 0) {  // Duplicate or equivalent script.
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        do { table[firstByte++] = lowByte++; } while(firstByte <= lastByte);
    }

    // Put all remaining scripts into the middle.
    // Avoid table[0] which must remain 0.
    for(int32_t i = 1; i <= 0xff; ++i) {
        if(table[i] == 0) { table[i] = lowByte++; }
    }
    U_ASSERT(lowByte == highByte + 1);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
