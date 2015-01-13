/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitformatter.cpp
 */

#include "unicode/utypes.h"

#include "digitaffix.h"
#include "fphdlimp.h"


U_NAMESPACE_BEGIN

void
DigitAffix::remove() {
    fAffix.remove();
    fAnnotations.remove();
}

void
DigitAffix::append(const UnicodeString &value, int32_t fieldId) {
    fAffix.append(value);
    int32_t len = value.length();
    for (int32_t i = 0; i < len; ++i) {
        fAnnotations.append((UChar) fieldId);
    }
}

UnicodeString &
DigitAffix::format(FieldPositionHandler &handler, UnicodeString &appendTo) const {
    int32_t len = fAffix.length();
    if (len == 0) {
        return appendTo;
    }
    if (!handler.isRecording()) {
        return appendTo.append(fAffix);
    }
    int32_t appendToStart = appendTo.length();
    int32_t lastId = (int32_t) fAnnotations.charAt(0);
    int32_t lastIdStart = 0;
    for (int32_t i = 1; i < len; ++i) {
        int32_t id = (int32_t) fAnnotations.charAt(i);
        if (id != lastId) {
            if (lastId != UNUM_FIELD_COUNT) {
                handler.addAttribute(lastId, appendToStart + lastIdStart, appendToStart + i);
            }
            lastId = id;
            lastIdStart = i;
        }
    }
    if (lastId != UNUM_FIELD_COUNT) {
        handler.addAttribute(lastId, appendToStart + lastIdStart, appendToStart + len);
    }
    return appendTo.append(fAffix);
}

U_NAMESPACE_END

