/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* template.cpp
*/
#include "template.h"
#include "cstring.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

typedef enum TemplateCompileState {
    INIT,
    APOSTROPHE,
    PLACEHOLDER
} TemplateCompileState;

class TemplateIdBuilder {
public:
    TemplateIdBuilder() : id(0), idLen(0) { }
    ~TemplateIdBuilder() { }
    void reset() { id = 0; idLen = 0; }
    int32_t getId() const { return id; }
    void appendTo(UChar *buffer, int32_t *len) const;
    UBool isValid() const { return (idLen > 0); }
    void add(UChar ch);
private:
    int32_t id;
    int32_t idLen;
    TemplateIdBuilder(const TemplateIdBuilder &other);
    TemplateIdBuilder &operator=(const TemplateIdBuilder &other);
};

void TemplateIdBuilder::appendTo(UChar *buffer, int32_t *len) const {
    int32_t origLen = *len;
    int32_t kId = id;
    for (int32_t i = origLen + idLen - 1; i >= origLen; i--) {
        int32_t digit = kId % 10;
        buffer[i] = digit + 0x30;
        kId /= 10;
    }
    *len = origLen + idLen;
}

void TemplateIdBuilder::add(UChar ch) {
    id = id * 10 + (ch - 0x30);
    idLen++;
}

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
    const UChar *patternBuffer = pattern.getBuffer();
    int32_t patternLength = pattern.length();
    UChar *buffer = noPlaceholders.getBuffer(patternLength);
    int32_t len = 0;
    placeholderSize = 0;
    placeholderCount = 0;
    TemplateCompileState state = INIT;
    TemplateIdBuilder idBuilder;
    for (int32_t i = 0; i < patternLength; ++i) {
        UChar ch = patternBuffer[i];
        switch (state) {
        case INIT:
            if (ch == 0x27) {
                state = APOSTROPHE;
            } else if (ch == 0x7B) {
                state = PLACEHOLDER;
                idBuilder.reset();
            } else {
               buffer[len++] = ch;
            }
            break;
        case APOSTROPHE:
            if (ch == 0x27) {
                buffer[len++] = 0x27;
            } else if (ch == 0x7B) {
                buffer[len++] = 0x7B;
            } else {
                buffer[len++] = 0x27;
                buffer[len++] = ch;
            }
            state = INIT;
            break;
        case PLACEHOLDER:
            if (ch >= 0x30 && ch <= 0x39) {
                idBuilder.add(ch);
            } else if (ch == 0x7D && idBuilder.isValid()) {
                if (!addPlaceholder(idBuilder.getId(), len)) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return FALSE;
                }
                state = INIT;
            } else {
                buffer[len++] = 0x7B;
                idBuilder.appendTo(buffer, &len);
                buffer[len++] = ch;
                state = INIT;
            }
            break;
        default:
            U_ASSERT(FALSE);
        }
    }
    switch (state) {
    case INIT:
        break;
    case APOSTROPHE:
        buffer[len++] = 0x27;
        break;
    case PLACEHOLDER:
        buffer[len++] = 0X7B;
        idBuilder.appendTo(buffer, &len);
        break;
    default:
        U_ASSERT(false);
    }
    noPlaceholders.releaseBuffer(len);
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
    if (id >= placeholderCount) {
        placeholderCount = id + 1;
    }
    return TRUE;
}
    
U_NAMESPACE_END
