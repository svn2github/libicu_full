/*
**********************************************************************
*   Copyright (C) 2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* scriptset.cpp
*
* created on: 2013 Jan 7
* created by: Andy Heninger
*/

#include "unicode/utypes.h"
#include "scriptset.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

//----------------------------------------------------------------------------
//
//  ScriptSet implementation
//
//----------------------------------------------------------------------------
ScriptSet::ScriptSet() {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        bits[i] = 0;
    }
}

ScriptSet::~ScriptSet() {
}

ScriptSet::ScriptSet(const ScriptSet &other) {
    *this = other;
}
    

ScriptSet & ScriptSet::operator =(const ScriptSet &other) {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        bits[i] = other.bits[i];
    }
    return *this;
}


UBool ScriptSet::operator == (const ScriptSet &other) const {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        if (bits[i] != other.bits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

UBool ScriptSet::test(UScriptCode script, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (script < 0 || script >= (int32_t)sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    return ((bits[index] & bit) != 0);
}


ScriptSet &ScriptSet::set(UScriptCode script, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (script < 0 || script >= (int32_t)sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    bits[index] |= bit;
    return *this;
}

ScriptSet &ScriptSet::reset(UScriptCode script, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (script < 0 || script >= (int32_t)sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    bits[index] &= ~bit;
    return *this;
}



ScriptSet &ScriptSet::Union(const ScriptSet &other) {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        bits[i] |= other.bits[i];
    }
    return *this;
}

ScriptSet &ScriptSet::intersect(const ScriptSet &other) {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        bits[i] &= other.bits[i];
    }
    return *this;
}

ScriptSet &ScriptSet::intersect(UScriptCode script, UErrorCode &status) {
    ScriptSet t;
    t.set(script, status);
    if (U_SUCCESS(status)) {
        this->intersect(t);
    }
    return *this;
}
    
UBool ScriptSet::intersects(const ScriptSet &other) const {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        if ((bits[i] & other.bits[i]) != 0) {
            return true;
        }
    }
    return false;
}

UBool ScriptSet::contains(const ScriptSet &other) const {
    ScriptSet t(*this);
    t.intersect(other);
    return (t == other);
}


ScriptSet &ScriptSet::setAll() {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        bits[i] = 0xffffffffu;
    }
    return *this;
}


ScriptSet &ScriptSet::resetAll() {
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        bits[i] = 0;
    }
    return *this;
}

int32_t ScriptSet::countMembers() const {
    // This bit counter is good for sparse numbers of '1's, which is
    //  very much the case that we will usually have.
    int32_t count = 0;
    for (uint32_t i=0; i<LENGTHOF(bits); i++) {
        uint32_t x = bits[i];
        while (x > 0) {
            count++;
            x &= (x - 1);    // and off the least significant one bit.
        }
    }
    return count;
}

int32_t ScriptSet::hashCode() const {
    int32_t hash = 0;
    for (int32_t i=0; i<LENGTHOF(bits); i++) {
        hash ^= bits[i];
    }
    return hash;
}

int32_t ScriptSet::nextSetBit(int32_t fromIndex) const {
    // TODO: Wants a better implementation.
    if (fromIndex < 0) {
        return -1;
    }
    UErrorCode status = U_ZERO_ERROR;
    for (int32_t scriptIndex = fromIndex; scriptIndex < (int32_t)sizeof(bits)*8; scriptIndex++) {
        if (test((UScriptCode)scriptIndex, status)) {
            return scriptIndex;
        }
    }
    return -1;
}



U_NAMESPACE_END

U_CAPI UBool U_EXPORT2
uhash_compareScriptSet(const UElement key1, const UElement key2) {
    icu::ScriptSet *s1 = static_cast<icu::ScriptSet *>(key1.pointer);
    icu::ScriptSet *s2 = static_cast<icu::ScriptSet *>(key2.pointer);
    return (*s1 == *s2);
}

U_CAPI int32_t U_EXPORT2
uhash_hashScriptSet(const UElement key) {
    icu::ScriptSet *s = static_cast<icu::ScriptSet *>(key.pointer);
    return s->hashCode();
}

U_CAPI void U_EXPORT2
uhash_deleteScriptSet(void *obj) {
    icu::ScriptSet *s = static_cast<icu::ScriptSet *>(obj);
    delete s;
}
