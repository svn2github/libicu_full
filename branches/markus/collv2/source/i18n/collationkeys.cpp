/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationkeys.cpp
*
* created on: 2012sep02
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/bytestream.h"
#include "charstr.h"
#include "collation.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "collationkeys.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

CollationKeys::LevelCallback::~LevelCallback() {}

UBool
CollationKeys::LevelCallback::needToWrite(Collation::Level /*level*/) { return TRUE; }

/**
 * Map from collation strength (UColAttributeValue)
 * to a mask of Collation::Level bits up to that strength,
 * excluding the CASE_LEVEL.
 */
static const uint32_t levelMasks[UCOL_STRENGTH_LIMIT] = {
    2,          // UCOL_PRIMARY -> PRIMARY_LEVEL
    6,          // UCOL_SECONDARY -> up to SECONDARY_LEVEL
    0x16,       // UCOL_TERTIARY -> up to TERTIARY_LEVEL
    0x36,       // UCOL_QUATERNARY -> up to QUATERNARY_LEVEL
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0,
    0x76        // UCOL_IDENTICAL -> up to IDENTICAL_LEVEL
};

void
CollationKeys::writeSortKeyUpToQuaternary(CollationIterator &iter, ByteSink &sink,
                                          Collation::Level minLevel, LevelCallback &callback,
                                          UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }

    const CollationData *data = iter.getData();
    int32_t options = data->options;
    // Set of levels to process and write.
    uint32_t levels = levelMasks[CollationData::getStrength(options)];
    if((options & CollationData::CASE_LEVEL) != 0) {
        levels |= Collation::CASE_LEVEL_FLAG;
    }
    // Minus the levels below minLevel.
    levels &= ~(((uint32_t)1 << minLevel) - 1);

    uint32_t variableTop;
    if((options & CollationData::ALTERNATE_MASK) == 0) {
        variableTop = 0;
    } else {
        // +1 so that we can use "<" and primary ignorables test out early.
        variableTop = data->variableTop + 1;
    }
    const uint8_t *reorderTable = data->reorderTable;

    uint32_t tertiaryMask = CollationData::getTertiaryMask(options);

    CharString cases;
    CharString secondaries;
    CharString tertiaries;
    CharString quaternaries;

    uint32_t compressedP1 = 0;
    int32_t commonCases = 0;
    int32_t commonSecondaries = 0;
    int32_t commonTertiaries = 0;
    int32_t commonQuaternaries = 0;

    uint32_t prevSecondary = 0;
    UBool anyMergeSeparators = FALSE;

    for(;;) {
        int64_t ce = iter.nextCE(errorCode);
        uint32_t p = (uint32_t)(ce >> 32);
        uint32_t p1 = p >> 24;
        if(reorderTable != NULL) { p1 = reorderTable[p1]; }
        if(p < variableTop && p > Collation::MERGE_SEPARATOR_PRIMARY) {
            // Variable CE, shift it to quaternary level.
            // Ignore all following primary ignorables, and shift further variable CEs.
            if(commonQuaternaries != 0) {
                --commonQuaternaries;
                while(commonQuaternaries >= QUAT_COMMON_MAX_COUNT) {
                    appendByte(QUAT_COMMON_MIDDLE, quaternaries, errorCode);
                    commonQuaternaries -= QUAT_COMMON_MAX_COUNT;
                }
                // Shifted primary weights are lower than the common weight.
                appendByte(QUAT_COMMON_LOW + commonQuaternaries, quaternaries, errorCode);
                commonQuaternaries = 0;
            }
            do {
                if((levels & Collation::QUATERNARY_LEVEL_FLAG) != 0) {
                    if(p1 >= QUAT_SHIFTED_LIMIT_BYTE) {
                        // Prevent shifted primary lead bytes from
                        // overlapping with the common compression range.
                        appendByte(QUAT_SHIFTED_LIMIT_BYTE, quaternaries, errorCode);
                    }
                    appendWeight32((p1 << 24) | (p & 0xffffff), quaternaries, errorCode);
                }
                do {
                    ce = iter.nextCE(errorCode);
                    p = (uint32_t)(ce >> 32);
                } while(p == 0);
                p1 = p >> 24;
                if(reorderTable != NULL) { p1 = reorderTable[p1]; }
            } while(p < variableTop && p > Collation::MERGE_SEPARATOR_PRIMARY);
        }
        // ce could be primary ignorable, or NO_CE, or the merge separator,
        // or a regular primary CE, but it is not variable.
        // If ce==NO_CE, then write nothing for the primary level but
        // terminate compression on all levels and then exit the loop.
        if(p > Collation::NO_CE_PRIMARY && (levels & Collation::PRIMARY_LEVEL_FLAG) != 0) {
            if(p1 != compressedP1) {
                if(compressedP1 != 0) {
                    if(p1 < compressedP1) {
                        // No primary compression terminator
                        // at the end of the level or merged segment.
                        if(p1 > Collation::MERGE_SEPARATOR_BYTE) {
                            appendByte(Collation::PRIMARY_COMPRESSION_LOW_BYTE, sink);
                        }
                    } else {
                        appendByte(Collation::PRIMARY_COMPRESSION_HIGH_BYTE, sink);
                    }
                }
                appendByte(p1, sink);
                if(data->isCompressibleLeadByte(p1)) {
                    compressedP1 = p1;
                } else {
                    compressedP1 = 0;
                }
            }
            appendWeight24(p, sink);
        }

        uint32_t lower32 = (uint32_t)ce;
        if((levels & Collation::SECONDARY_LEVEL_FLAG) != 0) {
            uint32_t s = lower32 >> 16;
            if(s == 0) {
                // secondary ignorable
            } else if(s == Collation::COMMON_WEIGHT16) {
                ++commonSecondaries;
            } else if((options & CollationData::BACKWARD_SECONDARY) == 0) {
                if(commonSecondaries != 0) {
                    --commonSecondaries;
                    while(commonSecondaries >= SEC_COMMON_MAX_COUNT) {
                        appendByte(SEC_COMMON_MIDDLE, secondaries, errorCode);
                        commonSecondaries -= SEC_COMMON_MAX_COUNT;
                    }
                    uint32_t b;
                    if(s < Collation::COMMON_WEIGHT16) {
                        b = SEC_COMMON_LOW + commonSecondaries;
                    } else {
                        b = SEC_COMMON_HIGH - commonSecondaries;
                    }
                    appendByte(b, secondaries, errorCode);
                    commonSecondaries = 0;
                }
                appendWeight16(s, secondaries, errorCode);
            } else {
                if(commonSecondaries != 0) {
                    --commonSecondaries;
                    // Append reverse weights. The level will be re-reversed later.
                    int32_t remainder = commonSecondaries % SEC_COMMON_MAX_COUNT;
                    uint32_t b;
                    if(prevSecondary < Collation::COMMON_WEIGHT16) {
                        b = SEC_COMMON_LOW + remainder;
                    } else {
                        b = SEC_COMMON_HIGH - remainder;
                    }
                    appendByte(b, secondaries, errorCode);
                    commonSecondaries -= remainder;
                    // commonSecondaries is now a multiple of SEC_COMMON_MAX_COUNT.
                    while(commonSecondaries > 0) {  // same as >= SEC_COMMON_MAX_COUNT
                        appendByte(SEC_COMMON_MIDDLE, secondaries, errorCode);
                        commonSecondaries -= SEC_COMMON_MAX_COUNT;
                    }
                    // commonSecondaries == 0
                }
                // Reduce separators so that we can look for byte<=1 later.
                if(s <= Collation::MERGE_SEPARATOR_WEIGHT16) {
                    if(s == Collation::MERGE_SEPARATOR_WEIGHT16) {
                        anyMergeSeparators = TRUE;
                    }
                    s -= 0x100;
                }
                appendReverseWeight16(s, secondaries, errorCode);
                prevSecondary = s;
            }
        }

        if(lower32 == 0) { continue; }  // completely ignorable, no case/tertiary/quaternary

        if((levels & Collation::CASE_LEVEL_FLAG) != 0) {
            if((CollationData::getStrength(options) == UCOL_PRIMARY) ?
                    p == 0 : lower32 <= 0xffff) {
                // Primary+case: Ignore case level weights of primary ignorables.
                // Otherwise: Ignore case level weights of secondary ignorables.
                // For details see the comments in the CollationCompare class.
            } else {
                uint32_t c = (lower32 >> 8) & 0xff;  // case bits & tertiary lead byte
                U_ASSERT((c & 0xc0) != 0xc0);
                if((c & 0xc0) == 0 && c > Collation::MERGE_SEPARATOR_BYTE) {
                    ++commonCases;
                } else if((options & CollationData::UPPER_FIRST) == 0) {
                    // lowerFirst: Compress common weights to nibbles 1..7..13, mixed=14, upper=15.
                    if(commonCases != 0) {
                        --commonCases;
                        while(commonCases >= CASE_LOWER_FIRST_COMMON_MAX_COUNT) {
                            appendByte(CASE_LOWER_FIRST_COMMON_MIDDLE << 4, cases, errorCode);
                            commonCases -= CASE_LOWER_FIRST_COMMON_MAX_COUNT;
                        }
                        uint32_t b;
                        if(c <= Collation::MERGE_SEPARATOR_BYTE) {
                            b = CASE_LOWER_FIRST_COMMON_LOW + commonCases;
                        } else {
                            b = CASE_LOWER_FIRST_COMMON_HIGH - commonCases;
                        }
                        appendByte(b << 4, cases, errorCode);
                        commonCases = 0;
                    }
                    if(c > Collation::MERGE_SEPARATOR_BYTE) {
                        c = (CASE_LOWER_FIRST_COMMON_HIGH + (c >> 6)) << 4;  // 14 or 15
                    }
                } else {
                    // upperFirst: Compress common weights to nibbles 3..15, mixed=2, upper=1.
                    // The compressed common case weights only go down from the "high" value
                    // because with upperFirst the common weight is the highest one.
                    if(commonCases != 0) {
                        --commonCases;
                        while(commonCases >= CASE_UPPER_FIRST_COMMON_MAX_COUNT) {
                            appendByte(CASE_UPPER_FIRST_COMMON_LOW << 4, cases, errorCode);
                            commonCases -= CASE_UPPER_FIRST_COMMON_MAX_COUNT;
                        }
                        appendByte((CASE_UPPER_FIRST_COMMON_HIGH - commonCases) << 4, cases, errorCode);
                        commonCases = 0;
                    }
                    if(c > Collation::MERGE_SEPARATOR_BYTE) {
                        c = (CASE_UPPER_FIRST_COMMON_LOW - (c >> 6)) << 4;  // 2 or 1
                    }
                }
                // c is a separator byte 01 or 02,
                // or a left-shifted nibble 0x10, 0x20, ... 0xf0.
                appendByte(c, cases, errorCode);
            }
        }

        if((levels & Collation::TERTIARY_LEVEL_FLAG) != 0) {
            uint32_t t = lower32 & tertiaryMask;
            U_ASSERT((lower32 & 0xc000) != 0xc000);
            if(t == Collation::COMMON_WEIGHT16) {
                ++commonTertiaries;
            } else if((tertiaryMask & 0x8000) == 0) {
                // Tertiary weights without case bits.
                // Move lead bytes 06..3F to C6..FF for a large common-weight range.
                if(commonTertiaries != 0) {
                    --commonTertiaries;
                    while(commonTertiaries >= TER_ONLY_COMMON_MAX_COUNT) {
                        appendByte(TER_ONLY_COMMON_MIDDLE, tertiaries, errorCode);
                        commonTertiaries -= TER_ONLY_COMMON_MAX_COUNT;
                    }
                    uint32_t b;
                    if(t < Collation::COMMON_WEIGHT16) {
                        b = TER_ONLY_COMMON_LOW + commonTertiaries;
                    } else {
                        b = TER_ONLY_COMMON_HIGH - commonTertiaries;
                    }
                    appendByte(b, tertiaries, errorCode);
                    commonTertiaries = 0;
                }
                if(t > Collation::COMMON_WEIGHT16) { t += 0xc000; }
                appendWeight16(t, tertiaries, errorCode);
            } else if((options & CollationData::UPPER_FIRST) == 0) {
                // Tertiary weights with caseFirst=lowerFirst.
                // Move lead bytes 06..BF to 46..FF for the common-weight range.
                if(commonTertiaries != 0) {
                    --commonTertiaries;
                    while(commonTertiaries >= TER_LOWER_FIRST_COMMON_MAX_COUNT) {
                        appendByte(TER_LOWER_FIRST_COMMON_MIDDLE, tertiaries, errorCode);
                        commonTertiaries -= TER_LOWER_FIRST_COMMON_MAX_COUNT;
                    }
                    uint32_t b;
                    if(t < Collation::COMMON_WEIGHT16) {
                        b = TER_LOWER_FIRST_COMMON_LOW + commonTertiaries;
                    } else {
                        b = TER_LOWER_FIRST_COMMON_HIGH - commonTertiaries;
                    }
                    appendByte(b, tertiaries, errorCode);
                    commonTertiaries = 0;
                }
                if(t > Collation::COMMON_WEIGHT16) { t += 0x4000; }
                appendWeight16(t, tertiaries, errorCode);
            } else {
                // Tertiary weights with caseFirst=upperFirst.
                // Lowercase     01..04 -> 81..84  (also uncased/separators)
                // Common weight     05 -> 85..C5
                // Lowercase     06..3F -> C6..FF
                // Mixed case    43..7F -> 43..7F
                // Uppercase     83..BF -> 03..3F
                t ^= 0xc000;
                if(t < (TER_UPPER_FIRST_COMMON_HIGH << 8)) {
                    t -= 0x4000;
                }
                // TODO: does v1 handle 03 & 04 tertiary weights properly? 02?
                //       try &[before 3]a<<<x and U+FFFE with all case settings
                if(commonTertiaries != 0) {
                    --commonTertiaries;
                    while(commonTertiaries >= TER_UPPER_FIRST_COMMON_MAX_COUNT) {
                        appendByte(TER_UPPER_FIRST_COMMON_MIDDLE, tertiaries, errorCode);
                        commonTertiaries -= TER_UPPER_FIRST_COMMON_MAX_COUNT;
                    }
                    uint32_t b;
                    if(t < (TER_UPPER_FIRST_COMMON_LOW << 8)) {
                        b = TER_UPPER_FIRST_COMMON_LOW + commonTertiaries;
                    } else {
                        b = TER_UPPER_FIRST_COMMON_HIGH - commonTertiaries;
                    }
                    appendByte(b, tertiaries, errorCode);
                    commonTertiaries = 0;
                }
                appendWeight16(t, tertiaries, errorCode);
            }
        }

        if((levels & Collation::QUATERNARY_LEVEL_FLAG) != 0) {
            uint32_t q = lower32 & 0xffff;
            if((q & 0xc000) == 0 && q > Collation::MERGE_SEPARATOR_WEIGHT16) {
                ++commonQuaternaries;
            } else if(q <= Collation::MERGE_SEPARATOR_WEIGHT16 &&
                    (options & CollationData::ALTERNATE_MASK) == 0 &&
                    (quaternaries.isEmpty() ||
                        quaternaries[quaternaries.length() - 1] == Collation::MERGE_SEPARATOR_BYTE)) {
                // If alternate=non-ignorable and there are only
                // common quaternary weights between two separators,
                // then we need not write anything between these separators.
                // The only weights greater than the merge separator and less than the common weight
                // are shifted primary weights, which are not generated for alternate=non-ignorable.
                // There are also exactly as many quaternary weights as tertiary weights,
                // so level length differences are handled already on tertiary level.
                // Any above-common quaternary weight will compare greater regardless.
                appendByte(q >> 8, quaternaries, errorCode);
            } else {
                if(q <= Collation::MERGE_SEPARATOR_WEIGHT16) {
                    q >>= 8;
                } else {
                    q = 0xfc + ((q >> 6) & 3);
                }
                if(commonQuaternaries != 0) {
                    --commonQuaternaries;
                    while(commonQuaternaries >= QUAT_COMMON_MAX_COUNT) {
                        appendByte(QUAT_COMMON_MIDDLE, quaternaries, errorCode);
                        commonQuaternaries -= QUAT_COMMON_MAX_COUNT;
                    }
                    uint32_t b;
                    if(q < QUAT_COMMON_LOW) {
                        b = QUAT_COMMON_LOW + commonQuaternaries;
                    } else {
                        b = QUAT_COMMON_HIGH - commonQuaternaries;
                    }
                    appendByte(b, quaternaries, errorCode);
                    commonQuaternaries = 0;
                }
                appendByte(q, quaternaries, errorCode);
            }
        }

        if((lower32 >> 24) == Collation::LEVEL_SEPARATOR_BYTE) { break; }  // ce == NO_CE
    }

    // Append the beyond-primary levels.
    if((levels & Collation::SECONDARY_LEVEL_FLAG) != 0) {
        if(!callback.needToWrite(Collation::SECONDARY_LEVEL)) { return; }
        appendByte(Collation::LEVEL_SEPARATOR_BYTE, sink);
        char *secs = secondaries.data();
        int32_t length = secondaries.length() - 1;  // Ignore the trailing NO_CE.
        if((options & CollationData::BACKWARD_SECONDARY) != 0) {
            // The backwards secondary level compares secondary weights backwards
            // within segments separated by the merge separator (U+FFFE, weight 02).
            // The separator weights 01 & 02 were reduced to 00 & 01 so that
            // we do not accidentally separate at a _second_ weight byte of 02.
            int32_t start = 0;
            for(;;) {
                // Find the merge separator or the NO_CE terminator.
                int32_t limit;
                if(anyMergeSeparators) {
                    limit = start;
                    while((uint8_t)secs[limit] > 1) { ++limit; }
                } else {
                    limit = length;
                }
                // Reverse this segment.
                if(start < limit) {
                    char *p = secs + start;
                    char *q = secs + limit - 1;
                    while(p < q) {
                        char s = *p;
                        *p++ = *q;
                        *q-- = s;
                    }
                }
                // Did we reach the end of the string?
                if(secs[limit] == 0) { break; }
                // Restore the merge separator.
                secs[limit] = 2;
                // Skip the merge separator and continue.
                start = limit + 1;
            }
        }
        sink.Append(secs, length);
    }

    if((levels & Collation::CASE_LEVEL_FLAG) != 0) {
        if(!callback.needToWrite(Collation::CASE_LEVEL)) { return; }
        appendByte(Collation::LEVEL_SEPARATOR_BYTE, sink);
        // Write pairs of nibbles as bytes, except separator bytes as themselves.
        int32_t length = cases.length() - 1;  // Ignore the trailing NO_CE.
        uint8_t b = 0;
        for(int32_t i = 0; i < length; ++i) {
            uint8_t c = (uint8_t)cases[i];
            if(c <= Collation::MERGE_SEPARATOR_BYTE) {
                if(b != 0) {
                    appendByte(b, sink);
                    b = 0;
                }
                appendByte(c, sink);
            } else {
                if(b == 0) {
                    b = c;
                } else {
                    appendByte(b | (c >> 4), sink);
                    b = 0;
                }
            }
        }
        if(b != 0) {
            appendByte(b, sink);
        }
    }

    if((levels & Collation::TERTIARY_LEVEL_FLAG) != 0) {
        if(!callback.needToWrite(Collation::TERTIARY_LEVEL)) { return; }
        appendByte(Collation::LEVEL_SEPARATOR_BYTE, sink);
        sink.Append(tertiaries.data(), tertiaries.length() - 1);
    }

    if((levels & Collation::QUATERNARY_LEVEL_FLAG) != 0) {
        if(!callback.needToWrite(Collation::QUATERNARY_LEVEL)) { return; }
        appendByte(Collation::LEVEL_SEPARATOR_BYTE, sink);
        sink.Append(quaternaries.data(), quaternaries.length() - 1);
    }
}

void
CollationKeys::appendByte(uint32_t b, ByteSink &sink) {
    char buffer[1] = { (char)b };
    sink.Append(buffer, 1);
}

void
CollationKeys::appendWeight16(uint32_t w, ByteSink &sink) {
    if(w != 0) {
        char buffer[2] = { (char)(w >> 8), (char)w };
        sink.Append(buffer, (buffer[1] == 0) ? 1 : 2);
    }
}

void
CollationKeys::appendWeight24(uint32_t w, ByteSink &sink) {
    if(w != 0) {
        char buffer[3] = { (char)(w >> 16), (char)(w >> 8), (char)w };
        sink.Append(buffer, (buffer[1] == 0) ? 1 : (buffer[2] == 0) ? 2 : 3);
    }
}

void
CollationKeys::appendWeight32(uint32_t w, ByteSink &sink) {
    if(w != 0) {
        char buffer[4] = { (char)(w >> 24), (char)(w >> 16), (char)(w >> 8), (char)w };
        sink.Append(buffer,
                    (buffer[1] == 0) ? 1 : (buffer[2] == 0) ? 2 : (buffer[3] == 0) ? 3 : 4);
    }
}

void
CollationKeys::appendWeight16(uint32_t w, CharString &level, UErrorCode &errorCode) {
    if(w != 0) {
        char buffer[2] = { (char)(w >> 8), (char)w };
        level.append(buffer, (buffer[1] == 0) ? 1 : 2, errorCode);
    }
}

void
CollationKeys::appendWeight32(uint32_t w, CharString &level, UErrorCode &errorCode) {
    if(w != 0) {
        char buffer[4] = { (char)(w >> 24), (char)(w >> 16), (char)(w >> 8), (char)w };
        level.append(buffer,
                     (buffer[1] == 0) ? 1 : (buffer[2] == 0) ? 2 : (buffer[3] == 0) ? 3 : 4,
                     errorCode);
    }
}

void
CollationKeys::appendReverseWeight16(uint32_t w, CharString &level, UErrorCode &errorCode) {
    if(w != 0) {
        char buffer[2] = { (char)w, (char)(w >> 8) };
        if(buffer[0] == 0) {
            level.append(buffer + 1, 1, errorCode);
        } else {
            level.append(buffer, 2, errorCode);
        }
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
