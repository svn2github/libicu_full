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


//----------------------------------------------------------------------------
//
//  ScriptSet implementation
//
//----------------------------------------------------------------------------
ScriptSet::ScriptSet() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
    }
}

ScriptSet::~ScriptSet() {
}

UBool ScriptSet::operator == (const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        if (bits[i] != other.bits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

void ScriptSet::Union(UScriptCode script) {
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    U_ASSERT(index < sizeof(bits)*4);
    bits[index] |= bit;
}


void ScriptSet::Union(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] |= other.bits[i];
    }
}

void ScriptSet::intersect(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] &= other.bits[i];
    }
}

void ScriptSet::intersect(UScriptCode script) {
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
}


ScriptSet & ScriptSet::operator =(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = other.bits[i];
    }
    return *this;
}


void ScriptSet::setAll() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0xffffffffu;
    }
}


void ScriptSet::resetAll() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
    }
}

int32_t ScriptSet::countMembers() {
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

U_NAMESPACE_END

