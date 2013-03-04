/*
*******************************************************************************
* Copyright (C) 2010-2013, International Business Machines
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

const uint32_t Collation::ONLY_TERTIARY_MASK;
const uint32_t Collation::CASE_AND_TERTIARY_MASK;

uint32_t
Collation::incTwoBytePrimaryByOffset(uint32_t basePrimary, UBool isCompressible, int32_t offset) {
    // Extract the second byte, minus the minimum byte value,
    // plus the offset, modulo the number of usable byte values, plus the minimum.
    // Reserve the PRIMARY_COMPRESSION_LOW_BYTE and high byte if necessary.
    uint32_t primary;
    if(isCompressible) {
        offset += ((int32_t)(basePrimary >> 16) & 0xff) - 4;
        primary = (uint32_t)((offset % 251) + 4) << 16;
        offset /= 251;
    } else {
        offset += ((int32_t)(basePrimary >> 16) & 0xff) - 2;
        primary = (uint32_t)((offset % 254) + 2) << 16;
        offset /= 254;
    }
    // First byte, assume no further overflow.
    return primary | ((basePrimary & 0xff000000) + (uint32_t)(offset << 24));
}

uint32_t
Collation::incThreeBytePrimaryByOffset(uint32_t basePrimary, UBool isCompressible, int32_t offset) {
    // Extract the third byte, minus the minimum byte value,
    // plus the offset, modulo the number of usable byte values, plus the minimum.
    offset += ((int32_t)(basePrimary >> 8) & 0xff) - 2;
    uint32_t primary = (uint32_t)((offset % 254) + 2) << 8;
    offset /= 254;
    // Same with the second byte,
    // but reserve the PRIMARY_COMPRESSION_LOW_BYTE and high byte if necessary.
    if(isCompressible) {
        offset += ((int32_t)(basePrimary >> 16) & 0xff) - 4;
        primary |= (uint32_t)((offset % 251) + 4) << 16;
        offset /= 251;
    } else {
        offset += ((int32_t)(basePrimary >> 16) & 0xff) - 2;
        primary |= (uint32_t)((offset % 254) + 2) << 16;
        offset /= 254;
    }
    // First byte, assume no further overflow.
    return primary | ((basePrimary & 0xff000000) + (uint32_t)(offset << 24));
}

uint32_t
Collation::getThreeBytePrimaryForOffsetData(UChar32 c, int64_t dataCE) {
    uint32_t p = (uint32_t)(dataCE >> 32);  // three-byte primary pppppp00
    int32_t lower32 = (int32_t)dataCE;  // base code point b & step s: bbbbbbss (bit 7: isCompressible)
    int32_t offset = (c - (lower32 >> 8)) * (lower32 & 0x7f);  // delta * increment
    UBool isCompressible = (lower32 & 0x80) != 0;
    return Collation::incThreeBytePrimaryByOffset(p, isCompressible, offset);
}

uint32_t
Collation::unassignedPrimaryFromCodePoint(UChar32 c) {
    // Create a gap before U+0000. Use c=-1 for [first unassigned].
    ++c;
    // Fourth byte: 18 values, every 14th byte value (gap of 13).
    uint32_t primary = 2 + (c % 18) * 14;
    c /= 18;
    // Third byte: 254 values.
    primary |= (2 + (c % 254)) << 8;
    c /= 254;
    // Second byte: 251 values 04..FE excluding the primary compression bytes.
    primary |= (4 + (c % 251)) << 16;
    // One lead byte covers all code points (c < 0x1182B4 = 1*251*254*18).
    return primary | (UNASSIGNED_IMPLICIT_BYTE << 24);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
