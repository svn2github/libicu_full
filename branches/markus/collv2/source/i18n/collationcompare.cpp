/*
*******************************************************************************
* Copyright (C) 1996-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationcompare.cpp
*
* created on: 2012feb14 with new and old collation code
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "cmemory.h"
#include "collation.h"
#include "collationcompare.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

// Buffer for secondary & tertiary weights.
class STBuffer {
public:
    STBuffer() : length(0) {}

    inline void append(uint32_t st, UErrorCode &errorCode) {
        if(length < buffer.getCapacity()) {
            buffer[length++] = st;
        } else {
            doAppend(st, errorCode);
        }
    }

    inline uint32_t operator[](ptrdiff_t i) const { return buffer[i]; }

    int32_t length;

private:
    STBuffer(const STBuffer &);
    void operator=(const STBuffer &);

    void doAppend(uint32_t st, UErrorCode &errorCode);

    MaybeStackArray<uint32_t, 40> buffer;
};

void
STBuffer::doAppend(uint32_t st, UErrorCode &errorCode) {
    // length == buffer.getCapacity()
    if(U_FAILURE(errorCode)) { return; }
    int32_t capacity = buffer.getCapacity();
    int32_t newCapacity;
    if(capacity < 1000) {
        newCapacity = 4 * capacity;
    } else {
        newCapacity = 2 * capacity;
    }
    uint32_t *p = buffer.resize(newCapacity, length);
    if(p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    p[length++] = st;
}

UCollationResult
CollationCompare::compareUpToQuaternary(CollationIterator &left, CollationIterator &right,
                                        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }

    // Buffers for secondary & tertiary weights.
    STBuffer leftSTBuffer;
    STBuffer rightSTBuffer;

    const CollationData *data = left.getData();
    int32_t options = data->options;
    uint32_t variableTop;
    if((options & CollationData::ALTERNATE_MASK) == 0) {
        variableTop = 0;
    } else {
        variableTop = data->variableTop;
    }

    // Fetch CEs, compare primaries, store secondary & tertiary weights.
    U_ALIGN_CODE(16);
    for(;;) {
        // We fetch CEs until we get a non-ignorable primary or reach the end.
        uint32_t leftPrimary;
        do {
            int64_t ce = left.nextCE(errorCode);
            leftPrimary = (uint32_t)(ce >> 32);
            if(leftPrimary <= variableTop && leftPrimary > Collation::MERGE_SEPARATOR_PRIMARY) {
                // Ignore this CE, all following primary ignorables, and further variable CEs.
                do {
                    ce = left.nextCE(errorCode);
                    leftPrimary = (uint32_t)(ce >> 32);
                } while(leftPrimary <= variableTop &&
                        (leftPrimary > Collation::MERGE_SEPARATOR_PRIMARY || leftPrimary == 0));
            }
            leftSTBuffer.append((uint32_t)ce, errorCode);
        } while(leftPrimary == 0);

        uint32_t rightPrimary;
        do {
            int64_t ce = right.nextCE(errorCode);
            rightPrimary = (uint32_t)(ce >> 32);
            if(rightPrimary <= variableTop && rightPrimary > Collation::MERGE_SEPARATOR_PRIMARY) {
                // Ignore this CE, all following primary ignorables, and further variable CEs.
                do {
                    ce = right.nextCE(errorCode);
                    rightPrimary = (uint32_t)(ce >> 32);
                } while(rightPrimary <= variableTop &&
                        (rightPrimary > Collation::MERGE_SEPARATOR_PRIMARY || rightPrimary == 0));
            }
            rightSTBuffer.append((uint32_t)ce, errorCode);
        } while(rightPrimary == 0);

        if(leftPrimary != rightPrimary) {
            // Return the primary difference, with script reordering.
            const uint8_t *reorderTable = data->reorderTable;
            if (reorderTable != NULL) {
                leftPrimary = Collation::reorder(reorderTable, leftPrimary);
                rightPrimary = Collation::reorder(reorderTable, rightPrimary);
            }
            return (leftPrimary < rightPrimary) ? UCOL_LESS : UCOL_GREATER;
        }
        if(leftPrimary == Collation::NO_CE_WEIGHT) { break; }
    }
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }

    // Compare the buffered secondary & tertiary weights.
    // We might skip the secondary level but continue with the case level
    // which is turned on separately.
    if(CollationData::getStrength(options) >= UCOL_SECONDARY) {
        if((options & CollationData::BACKWARD_SECONDARY) == 0) {
            int32_t leftSTIndex = 0;
            int32_t rightSTIndex = 0;
            for(;;) {
                uint32_t leftSecondary;
                do {
                    leftSecondary = leftSTBuffer[leftSTIndex++] >> 16;
                } while(leftSecondary == 0);

                uint32_t rightSecondary;
                do {
                    rightSecondary = rightSTBuffer[rightSTIndex++] >> 16;
                } while(rightSecondary == 0);

                if(leftSecondary != rightSecondary) {
                    return (leftSecondary < rightSecondary) ? UCOL_LESS : UCOL_GREATER;
                }
                if(leftSecondary == Collation::NO_CE_WEIGHT) { break; }
            }
        } else {
            // French secondary level compares backwards.
            // Ignore the last item, the NO_CE terminator.
            int32_t leftSTIndex = leftSTBuffer.length - 1;
            int32_t rightSTIndex = rightSTBuffer.length - 1;
            for(;;) {
                uint32_t leftSecondary = 0;
                while(leftSecondary == 0 && leftSTIndex > 0) {
                    leftSecondary = leftSTBuffer[--leftSTIndex] >> 16;
                }

                uint32_t rightSecondary = 0;
                while(rightSecondary == 0 && rightSTIndex > 0) {
                    rightSecondary = rightSTBuffer[--rightSTIndex] >> 16;
                }

                if(leftSecondary != rightSecondary) {
                    return (leftSecondary < rightSecondary) ? UCOL_LESS : UCOL_GREATER;
                }
                if(leftSecondary == 0) { break; }
            }
        }
    }

    if((options & CollationData::CASE_LEVEL) != 0) {
        int32_t leftSTIndex = 0;
        int32_t rightSTIndex = 0;
        for(;;) {
            // Case bits cannot distinguish between 0=ignorable and 0=uncased/lowercase.
            // Therefore, ignore the case "weight" when the non-case tertiary weight is 0.
            // The data has non-zero case bits only when the non-case tertiary bits are non-zero,
            // so the "while" condition need not mask with 0x3fff for the "ignorable" test.
            uint32_t leftTertiary;
            do {
                leftTertiary = leftSTBuffer[leftSTIndex++] & 0xffff;
                U_ASSERT((leftTertiary & 0x3fff) != 0 || (leftTertiary & 0xc000) == 0);
            } while(leftTertiary == 0);

            uint32_t rightTertiary;
            do {
                rightTertiary = rightSTBuffer[rightSTIndex++] & 0xffff;
                U_ASSERT((rightTertiary & 0x3fff) != 0 || (rightTertiary & 0xc000) == 0);
            } while(rightTertiary == 0);

            uint32_t leftCase = leftTertiary & 0xc000;
            uint32_t rightCase = rightTertiary & 0xc000;
            if(leftCase != rightCase) {
                if((options & CollationData::UPPER_FIRST) == 0) {
                    return (leftCase < rightCase) ? UCOL_LESS : UCOL_GREATER;
                } else {
                    // Check for end-of-string before inverting the case bits or result.
                    // Otherwise NO_CE_WEIGHT would sort greater than mixed/upper case.
                    if(leftTertiary == Collation::NO_CE_WEIGHT) { return UCOL_LESS; }
                    if(rightTertiary == Collation::NO_CE_WEIGHT) { return UCOL_GREATER; }
                    return (leftCase < rightCase) ? UCOL_GREATER : UCOL_LESS;
                }
            }

            if(leftTertiary == Collation::NO_CE_WEIGHT) {
                if(rightTertiary == Collation::NO_CE_WEIGHT) { break; }
                return UCOL_LESS;
            } else if(rightTertiary == Collation::NO_CE_WEIGHT) {
                return UCOL_GREATER;
            }
        }
    }
    if(CollationData::getStrength(options) <= UCOL_SECONDARY) { return UCOL_EQUAL; }

    uint32_t tertiaryMask = CollationData::getTertiaryMask(options);

    int32_t leftSTIndex = 0;
    int32_t rightSTIndex = 0;
    for(;;) {
        uint32_t leftTertiary;
        do {
            leftTertiary = leftSTBuffer[leftSTIndex++] & tertiaryMask;
            U_ASSERT((leftTertiary & 0x3fff) != 0 || (leftTertiary & 0xc000) == 0);
        } while(leftTertiary == 0);

        uint32_t rightTertiary;
        do {
            rightTertiary = rightSTBuffer[rightSTIndex++] & tertiaryMask;
            U_ASSERT((rightTertiary & 0x3fff) != 0 || (rightTertiary & 0xc000) == 0);
        } while(rightTertiary == 0);

        if(leftTertiary != rightTertiary) {
            if(CollationData::sortsTertiaryUpperCaseFirst(options)) {
                // Check for end-of-string before inverting the case bits or result.
                // Otherwise NO_CE_WEIGHT would sort greater than mixed/upper case.
                if(leftTertiary == Collation::NO_CE_WEIGHT) { return UCOL_LESS; }
                if(rightTertiary == Collation::NO_CE_WEIGHT) { return UCOL_GREATER; }
                return (leftTertiary < rightTertiary) ? UCOL_GREATER : UCOL_LESS;
            } else {
                return (leftTertiary < rightTertiary) ? UCOL_LESS : UCOL_GREATER;
            }
        }
        if(leftTertiary == Collation::NO_CE_WEIGHT) { break; }
    }
    if(CollationData::getStrength(options) <= UCOL_TERTIARY) { return UCOL_EQUAL; }

    if(variableTop == 0 &&
            ((options & CollationData::HIRAGANA_QUATERNARY) == 0 ||
                (!left.getAnyHiragana() && !right.getAnyHiragana()))) {
        // If "shifted" mode is off and hiraganaQuaternary is off
        // or neither string contains Hiragana characters,
        // then there are no quaternary differences.
        return UCOL_EQUAL;
    }
    left.resetToStart();
    right.resetToStart();
    return compareQuaternary(left, right, errorCode);
}

class QuaternaryIterator {
public:
    QuaternaryIterator(CollationIterator &iter)
            : source(iter),
              variableTop(0),
              withHiraganaQuaternary((iter.getData()->options & CollationData::HIRAGANA_QUATERNARY) != 0),
              shifted(FALSE), hiragana(0) {
        const CollationData *data = iter.getData();
        if((data->options & CollationData::ALTERNATE_MASK) != 0) {
            variableTop = data->variableTop;
        }
    }
    uint32_t next(UErrorCode &errorCode);
private:
    CollationIterator &source;
    uint32_t variableTop;
    UBool withHiraganaQuaternary;
    UBool shifted;
    int8_t hiragana;
};

uint32_t
QuaternaryIterator::next(UErrorCode &errorCode) {
    for(;;) {
        int64_t ce = source.nextCE(errorCode);
        if(withHiraganaQuaternary) {
            int8_t hira = source.getHiragana();
            if(hira >= 0) { hiragana = hira; }
        }
        // Extract the primary weight and use it as a quaternary unless we override it.
        uint32_t q = (uint32_t)(ce >> 32);
        if(q == 0) {
            // Skip completely-ignorables, and primary ignorables after a shifted primary.
            if(ce == 0 || shifted) { continue; }
            // Primary ignorable but not completely ignorable.
            q = 0xffffffff - hiragana;
        } else if(q <= Collation::MERGE_SEPARATOR_PRIMARY) {
            // Leave NO_CE_WEIGHT or MERGE_SEPARATOR_PRIMARY as is.
            shifted = FALSE;
        } else if(q <= variableTop) {
            // Shift this variable primary weight into the quaternary level.
            shifted = TRUE;
        } else {
            // Regular CE.
            shifted = FALSE;
            q = 0xffffffff - hiragana;
        }
        return q;
    }
}

UCollationResult
CollationCompare::compareQuaternary(CollationIterator &left, CollationIterator &right,
                                    UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }

    QuaternaryIterator leftQI(left);
    QuaternaryIterator rightQI(right);

    for(;;) {
        uint32_t leftQuaternary = leftQI.next(errorCode);
        uint32_t rightQuaternary = rightQI.next(errorCode);

        if(leftQuaternary != rightQuaternary) {
            // TODO: Script reordering?! v1 does not perform it for shifted primaries.
            // Return the difference, with script reordering.
            const uint8_t *reorderTable = left.getData()->reorderTable;
            if (reorderTable != NULL) {
                leftQuaternary = Collation::reorder(reorderTable, leftQuaternary);
                rightQuaternary = Collation::reorder(reorderTable, rightQuaternary);
            }
            return (leftQuaternary < rightQuaternary) ? UCOL_LESS : UCOL_GREATER;
        }
        if(leftQuaternary == Collation::NO_CE_WEIGHT) { break; }
    }
    return UCOL_EQUAL;
}

// TODO: The quaternary sort key level needs at least
// primary lead byte 0xff for non-shifted, non-Hiragana CEs,
// and at least 0xfe for Hiragana, therefore we must limit variableTop
// to a maximum lead byte of 0xfd (i.e., no trail weights).

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
