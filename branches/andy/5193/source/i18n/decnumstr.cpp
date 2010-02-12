/*
******************************************************************************
*
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File decnumstr.cpp
*
*/

#include "unicode/utypes.h"
#include "decnumstr.h"
#include "cmemory.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

DecimalNumberString::DecimalNumberString() {
    fLength  = 0;
    fText[0] = 0;
}

DecimalNumberString::~DecimalNumberString() {
}

DecimalNumberString::DecimalNumberString(const StringPiece &source) {
    fLength = 0;
    append(source);
}

DecimalNumberString & DecimalNumberString::append(char c) {
    if (fText.getCapacity() < fLength+2) {
        fText.resize(fText.getCapacity() * 2, fLength);
    }
    fText[fLength++] = c;
    fText[fLength] = 0;
    return *this;
}

DecimalNumberString &DecimalNumberString::append(const StringPiece &str) {
    int32_t sLength = str.length();
    if (fText.getCapacity() < fLength + sLength + 1) {
        fText.resize(fLength + sLength +1, fLength);
    }
    uprv_memcpy(&fText[fLength], str.data(), sLength);
    fLength += sLength;
    fText[fLength] = 0;
    return *this;
}

char & DecimalNumberString::operator [] (int32_t index) {
    U_ASSERT(index>=0 && index<fLength);
    return fText[index];
}

const char & DecimalNumberString::operator [] (int32_t index) const {
    U_ASSERT(index>=0 && index<fLength);
    return fText[index];
}

int32_t DecimalNumberString::length() const {
    return fLength;
}

void DecimalNumberString::setLength(int32_t length) {
    if (fText.getCapacity() < length+1) {
        fText.resize(length+1, fLength);
    }
    if (length > fLength) {
        uprv_memset(&fText[fLength], length - fLength, 0);
    }
    fLength = length;
    fText[fLength] = 0;
}

DecimalNumberString::operator StringPiece() const {
    return StringPiece(fText, fLength);
}

U_NAMESPACE_END

