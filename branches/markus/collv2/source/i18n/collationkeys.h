/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationkeys.h
*
* created on: 2012sep02
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONKEYS_H__
#define __COLLATIONKEYS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "charstr.h"
#include "collation.h"

U_NAMESPACE_BEGIN

class ByteSink;
class CollationIterator;

// TODO: CollationRadixKeys

class U_I18N_API CollationKeys /* not : public UObject because all methods are static */ {
public:
    class LevelCallback : public UMemory {
    public:
        virtual ~LevelCallback();
        /**
         * @param level The next level about to be written to the ByteSink.
         * @return TRUE if the level is to be written
         *         (the base class implementation always returns TRUE)
         */
        virtual UBool needToWrite(Collation::Level level);
    };

    /**
     * Writes the sort key bytes for minLevel up to the iterator data's strength.
     * Optionally writes the case level.
     * Stops writing levels when callback.needToWrite(level) returns FALSE.
     * Separates levels with the LEVEL_SEPARATOR_BYTE
     * but does not write a TERMINATOR_BYTE.
     */
    static void writeSortKeyUpToQuaternary(CollationIterator &iter, ByteSink &sink,
                                           Collation::Level minLevel, LevelCallback &callback,
                                           UErrorCode &errorCode);
private:
    CollationKeys();  // no instantiation

    // Secondary level: Compress up to 33 common weights as 05..25 or 25..45.
    static const uint32_t SEC_COMMON_LOW = Collation::COMMON_BYTE;
    static const uint32_t SEC_COMMON_MIDDLE = SEC_COMMON_LOW + 0x20;
    static const uint32_t SEC_COMMON_HIGH = SEC_COMMON_LOW + 0x40;
    static const int32_t SEC_COMMON_MAX_COUNT = 0x21;

    // Case level, lowerFirst: Compress up to 7 common weights as 1..7 or 7..13.
    static const uint32_t CASE_LOWER_FIRST_COMMON_LOW = 1;
    static const uint32_t CASE_LOWER_FIRST_COMMON_MIDDLE = 7;
    static const uint32_t CASE_LOWER_FIRST_COMMON_HIGH = 13;
    static const int32_t CASE_LOWER_FIRST_COMMON_MAX_COUNT = 7;

    // Case level, upperFirst: Compress up to 13 common weights as 3..15.
    static const uint32_t CASE_UPPER_FIRST_COMMON_LOW = 3;
    static const uint32_t CASE_UPPER_FIRST_COMMON_HIGH = 15;
    static const int32_t CASE_UPPER_FIRST_COMMON_MAX_COUNT = 13;

    // Tertiary level only (no case): Compress up to 97 common weights as 05..65 or 65..C5.
    static const uint32_t TER_ONLY_COMMON_LOW = Collation::COMMON_BYTE;
    static const uint32_t TER_ONLY_COMMON_MIDDLE = TER_ONLY_COMMON_LOW + 0x60;
    static const uint32_t TER_ONLY_COMMON_HIGH = TER_ONLY_COMMON_LOW + 0xc0;
    static const int32_t TER_ONLY_COMMON_MAX_COUNT = 0x61;

    // Tertiary with case, lowerFirst: Compress up to 33 common weights as 05..25 or 25..45.
    static const uint32_t TER_LOWER_FIRST_COMMON_LOW = Collation::COMMON_BYTE;
    static const uint32_t TER_LOWER_FIRST_COMMON_MIDDLE = TER_LOWER_FIRST_COMMON_LOW + 0x20;
    static const uint32_t TER_LOWER_FIRST_COMMON_HIGH = TER_LOWER_FIRST_COMMON_LOW + 0x40;
    static const int32_t TER_LOWER_FIRST_COMMON_MAX_COUNT = 0x21;

    // Tertiary with case, upperFirst: Compress up to 33 common weights as 85..A5 or A5..C5.
    static const uint32_t TER_UPPER_FIRST_COMMON_LOW = Collation::COMMON_BYTE + 0x80;
    static const uint32_t TER_UPPER_FIRST_COMMON_MIDDLE = TER_UPPER_FIRST_COMMON_LOW + 0x20;
    static const uint32_t TER_UPPER_FIRST_COMMON_HIGH = TER_UPPER_FIRST_COMMON_LOW + 0x40;
    static const int32_t TER_UPPER_FIRST_COMMON_MAX_COUNT = 0x21;

    // Quaternary level: Compress up to 113 common weights as 1C..8C or 8C..FC.
    static const uint32_t QUAT_COMMON_LOW = 0x1c;
    static const uint32_t QUAT_COMMON_MIDDLE = QUAT_COMMON_LOW + 0x70;
    static const uint32_t QUAT_COMMON_HIGH = QUAT_COMMON_LOW + 0xE0;
    static const int32_t QUAT_COMMON_MAX_COUNT = 0x71;
    // Primary weights shifted to quaternary level must be encoded with
    // a lead byte below the common-weight compression range.
    static const uint32_t QUAT_SHIFTED_LIMIT_BYTE = QUAT_COMMON_LOW - 1;  // 0x1b

    static void appendByte(uint32_t b, ByteSink &sink);
    static void appendWeight16(uint32_t w, ByteSink &sink);
    static void appendWeight24(uint32_t w, ByteSink &sink);
    static void appendWeight32(uint32_t w, ByteSink &sink);

    static void appendByte(uint32_t b, CharString &level, UErrorCode &errorCode) {
        level.append((char)b, errorCode);
    }
    static void appendWeight16(uint32_t w, CharString &level, UErrorCode &errorCode);
    static void appendWeight32(uint32_t w, CharString &level, UErrorCode &errorCode);

    static void appendReverseWeight16(uint32_t w, CharString &level, UErrorCode &errorCode);
};

// TODO: class PartLevelCallback : public LevelCallback {
//   tests overflow of CheckedArrayByteSink, has reference to that
//   has lastLevel, set in needToWrite()
// }
//
// TODO: class SlicingByteSink writes to byte array only after numIgnore bytes,
// and then exactly until the array is full.

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONKEYS_H__
