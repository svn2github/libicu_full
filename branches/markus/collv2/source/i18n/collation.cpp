/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collation.cpp
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "collation.h"

U_NAMESPACE_BEGIN

uint32_t
Collation::incThreeBytePrimaryByOffset(uint32_t basePrimary, UBool isCompressible, int32_t offset) {
    // Extract the third byte, minus the minimum byte value,
    // plus the offset, modulo the number of usable byte values, plus the minimum.
    offset += ((int32_t)(basePrimary >> 8) & 0xff) - 3;
    uint32_t primary = (uint32_t)((offset % 253) + 3) << 8;
    offset /= 253;
    // Same with the second byte,
    // but reserve the PRIMARY_COMPRESSION_LOW_BYTE and high byte if necessary.
    if(isCompressible) {
        offset += ((int32_t)(basePrimary >> 16) & 0xff) - 4;
        primary |= (uint32_t)((offset % 251) + 4) << 16;
        offset /= 251;
    } else {
        offset += ((int32_t)(basePrimary >> 16) & 0xff) - 3;
        primary |= (uint32_t)((offset % 253) + 3) << 16;
        offset /= 253;
    }
    // First byte, assume no further overflow.
    return primary | ((basePrimary & 0xff000000) + (uint32_t)(offset << 24));
}

int64_t
Collation::getCEFromThreeByteOffset(uint32_t basePrimary, UBool isCompressible, int32_t offset) {
    uint32_t primary = incThreeBytePrimaryByOffset(basePrimary, isCompressible, offset);
    return ((int64_t)primary << 32) | COMMON_SEC_AND_TER_CE;
}

uint32_t
Collation::unassignedPrimaryFromCodePoint(UChar32 c) {
    // Create a gap before U+0000. Use c=-1 for [first unassigned].
    ++c;
    // Fourth byte: 18 values, every 14th byte value (gap of 13).
    uint32_t primary = 3 + (c % 18) * 14;
    c /= 18;
    // Third byte: 253 values.
    primary |= (3 + (c % 253)) << 8;
    c /= 253;
    // Second byte: 251 values 04..FE excluding the primary compression bytes.
    primary |= (4 + (c % 251)) << 16;
    // One lead byte covers all code points (c < 0x11710E = 1*251*253*18).
    return primary | (UNASSIGNED_IMPLICIT_BYTE << 24);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
