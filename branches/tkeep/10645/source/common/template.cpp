/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* template.cpp
*/
#include "template.h"
#include "cstring.h"

U_NAMESPACE_BEGIN
Template::Template() :
        noPlaceholders(),
        placeholdersByOffset(placeholderBuffer),
        placeholderSize(0),
        placeholderCapacity(EXPECTED_PLACEHOLDER_COUNT),
        placeholderCount(0) {
}

Template::Template(const Template &other) :
        noPlaceholders(other.noPlaceholders),
        placeholdersByOffset(placeholderBuffer),
        placeholderSize(0),
        placeholderCapacity(EXPECTED_PLACEHOLDER_COUNT),
        placeholderCount(other.placeholderCount) {
    placeholderSize = ensureCapacity(other.placeholderSize);
    uprv_memcpy(
            placeholdersByOffset,
            other.placeholdersByOffset,
            placeholderSize * 2 * sizeof(int32_t));
}

Template &Template::operator=(const Template& other) {
    if (this == &other) {
        return *this;
    }
    noPlaceholders = other.noPlaceholders;
    placeholderCount = other.placeholderCount;
    placeholderSize = ensureCapacity(other.placeholderSize);
    uprv_memcpy(
            placeholdersByOffset,
            other.placeholdersByOffset,
            placeholderSize * 2 * sizeof(int32_t));
    return *this;
}

Template::~Template() {
    if (placeholdersByOffset != placeholderBuffer) {
        uprv_free(placeholdersByOffset);
    }
}

UBool Template::compile(const UnicodeString &pattern, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return TRUE;
}

UnicodeString& Template::evaluate(
        const UnicodeString *placeholderValues,
        int32_t placeholderValueCount,
        UnicodeString appendTo,
        UErrorCode &status) const {
    return evaluate(
            placeholderValues,
            placeholderValueCount,
            appendTo,
            NULL,
            0,
            status);
}

UnicodeString& Template::evaluate(
        const UnicodeString *placeholderValues,
        int32_t placeholderValueCount,
        UnicodeString appendTo,
        int32_t *offsetArray,
        int32_t offsetArrayLength,
        UErrorCode &status) const {
    return appendTo;
}

int32_t Template::ensureCapacity(int32_t atLeast) {
    if (atLeast <= placeholderCapacity) {
        return atLeast;
    }
    // aim to double capacity each time
    int32_t newCapacity = 2*atLeast - 2;

    // allocate new buffer
    int32_t *newBuffer = (int32_t *) uprv_malloc(2 * newCapacity * sizeof(int32_t));
    if (newBuffer == NULL) {
        return placeholderCapacity;
    }

    // Copy contents of old buffer to new buffer
    uprv_memcpy(newBuffer, placeholdersByOffset, 2 * placeholderSize * sizeof(int32_t));

    // free old buffer
    if (placeholdersByOffset != placeholderBuffer) {
        uprv_free(placeholdersByOffset);
    }

    // Use new buffer
    placeholdersByOffset = newBuffer;
    placeholderCapacity = newCapacity;
    return atLeast;
}

UBool Template::addPlaceholder(int32_t id, int32_t offset) {
    if (ensureCapacity(placeholderSize + 1) < placeholderSize + 1) {
        return FALSE;
    }
    ++placeholderSize;
    placeholdersByOffset[2 * placeholderSize - 2] = offset;
    placeholdersByOffset[2 * placeholderSize - 1] = id;
    return TRUE;
}
    
U_NAMESPACE_END
