/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedobject.h
*/

#ifndef __TEMPLATE_H__
#define __TEMPLATE_H__

#define EXPECTED_PLACEHOLDER_COUNT 3

#include "unicode/utypes.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

/**
 * Handles templates like "{0} has {1} friends".
 */
class U_COMMON_API Template : public UMemory {
public:
    Template();
    Template(const Template& other);
    Template &operator=(const Template& other);
    ~Template();
    UBool compile(const UnicodeString &pattern, UErrorCode &status);
    int32_t getPlaceholderCount() const {
        return placeholderCount;
     }
    UnicodeString &evaluate(
            const UnicodeString *placeholderValues,
            int32_t placeholderValueCount,
            UnicodeString appendTo,
            UErrorCode &status) const;
    UnicodeString &evaluate(
            const UnicodeString *placeholderValues,
            int32_t placeholderValueCount,
            UnicodeString appendTo,
            int32_t *offsetArray,
            int32_t offsetArrayLength,
            UErrorCode &status) const;
private:
    UnicodeString noPlaceholders;
    int32_t placeholderBuffer[EXPECTED_PLACEHOLDER_COUNT * 2];
    int32_t *placeholdersByOffset;
    int32_t placeholderSize;
    int32_t placeholderCapacity;
    int32_t placeholderCount;
    int32_t ensureCapacity(int32_t size);
    UBool addPlaceholder(int32_t id, int32_t offset);
};

U_NAMESPACE_END

#endif
