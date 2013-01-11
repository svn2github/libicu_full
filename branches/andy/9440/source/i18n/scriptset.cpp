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

UBool ScriptSet::operator == (const ScriptSet &other) const {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        if (bits[i] != other.bits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

ScriptSet &ScriptSet::set(UScriptCode script, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (script < 0 || script >= sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    bits[index] |= bit;
    return *this;
}

ScriptSet &ScriptSet::reset(UScriptCode script, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (script < 0 || script >= sizeof(bits) * 8) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    bits[index] &= ~bit;
    return *this;
}



ScriptSet &ScriptSet::Union(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] |= other.bits[i];
    }
    return *this;
}

ScriptSet &ScriptSet::intersect(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] &= other.bits[i];
    }
    return *this;
}

ScriptSet &ScriptSet::intersect(UScriptCode script) {
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    U_ASSERT(index < sizeof(bits)*4);
    uint32_t i;
    for (i=0; i<index; i++) {
        bits[i] = 0;
    }
    bits[index] &= bit;
    for (i=index+1; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
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

ScriptSet & ScriptSet::operator =(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = other.bits[i];
    }
    return *this;
}


ScriptSet &ScriptSet::setAll() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0xffffffffu;
    }
    return *this;
}


ScriptSet &ScriptSet::resetAll() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
    }
    return *this;
}

int32_t ScriptSet::countMembers() const {
    // This bit counter is good for sparse numbers of '1's, which is
    //  very much the case that we will usually have.
    int32_t count = 0;
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
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

U_NAMESPACE_END

U_CAPI UBool U_EXPORT2
uhash_compareScriptSet(const UElement key1, const UElement key2) {
    icu::ScriptSet *s1 = static_cast<icu::ScriptSet *>(key1.pointer);
    icu::ScriptSet *s2 = static_cast<icu::ScriptSet *>(key2.pointer);
    return (*s1 == *s2);
}

U_CAPI void U_EXPORT2
uhash_deleteScriptSet(void *obj) {
    icu::ScriptSet s = static_cast<icu::ScriptSet *>(obj);
    delete s;
}
