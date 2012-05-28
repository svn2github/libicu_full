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
    UCollationResult result = compareUpToTertiary(left, right, errorCode);
    if(result != UCOL_EQUAL || left.getData()->getStrength() <= UCOL_TERTIARY || U_FAILURE(errorCode)) {
        return result;
    }

    if(left.getData()->variableTop == 0 && !left.getAnyHiragana() && !right.getAnyHiragana()) {
        // If "shifted" mode is off and neither string contains Hiragana characters,
        // then there are no quaternary differences.
        return UCOL_EQUAL;
    }
    left.resetToStart();
    right.resetToStart();
    return compareQuaternary(left, right, errorCode);
}

UCollationResult
CollationCompare::compareUpToTertiary(CollationIterator &left, CollationIterator &right,
                                      UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }

    // Buffers for secondary & tertiary weights.
    STBuffer leftSTBuffer;
    STBuffer rightSTBuffer;

    const CollationData *data = left.getData();
    uint32_t variableTop = data->variableTop;

    // Fetch CEs, compare primaries, store secondary & tertiary weights.
    U_ALIGN_CODE(16);
    for(;;) {
        // We fetch CEs until we get a non-ignorable, non-variable primary or reach the end.
        uint32_t leftPrimary;
        do {
            int64_t ce = left.nextCE(errorCode);
            leftPrimary = (uint32_t)(ce >> 32);
            // TODO: Should [02, 02, 02] be variable?! It is variable in v1 but that seems wrong.
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
            // TODO: This version keeps [02, 02, 02] variable.
            if(rightPrimary <= variableTop && rightPrimary > Collation::NO_CE_WEIGHT) {
                // Ignore this CE, all following primary ignorables, and further variable CEs.
                do {
                    ce = right.nextCE(errorCode);
                    rightPrimary = (uint32_t)(ce >> 32);
                } while(rightPrimary <= variableTop && rightPrimary != Collation::NO_CE_WEIGHT);
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
    int32_t options = data->options;
    if(CollationData::getStrength(options) == UCOL_PRIMARY || U_FAILURE(errorCode)) { return UCOL_EQUAL; }
    // TODO: If we decide against comparePrimaryAndCase(),
    // then we need to continue if strength == UCOL_PRIMARY && caseLevel == UCOL_ON.
    // In that case, check for if(strength >= UCOL_SECONDARY) for the secondary level, etc.

    // Compare the buffered secondary & tertiary weights.

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

    if((options & CollationData::CASE_LEVEL) != 0) {
        int32_t leftSTIndex = 0;
        int32_t rightSTIndex = 0;
        for(;;) {
            uint32_t leftTertiary;
            do {
                leftTertiary = leftSTBuffer[leftSTIndex++] & 0xffff;
            } while((leftTertiary & 0x3fff) == 0);
            // TODO: Should we construct CEs such that case bits are set only if
            // the CE is not tertiary-ignorable?
            // Then we should not need the & 0x3fff in the loop condition.
            // It seems like otherwise the CE would be malformed
            // because the higher case level would be non-zero
            // while the lower tertiary level would be 0.

            uint32_t rightTertiary;
            do {
                rightTertiary = rightSTBuffer[rightSTIndex++] & 0xffff;
            } while((rightTertiary & 0x3fff) == 0);

            uint32_t leftCase = leftTertiary & 0xc000;
            uint32_t rightCase = rightTertiary & 0xc000;
            if(leftCase != rightCase) {
                return
                    ((leftCase < rightCase) ^ ((options & CollationData::UPPER_FIRST) != 0)) ?
                    UCOL_LESS : UCOL_GREATER;
            }

            if(leftTertiary == Collation::NO_CE_WEIGHT) {
                if(rightTertiary == Collation::NO_CE_WEIGHT) { break; }
                return UCOL_LESS;
            } else if(rightTertiary == Collation::NO_CE_WEIGHT) {
                return UCOL_GREATER;
            }
        }
    }
    // TODO: Bug in old ucol_strcollRegular(collIterate *sColl, collIterate *tColl, UErrorCode *status):
    // In case level comparison, it compares equal when reaching the end of either input
    // even when one is shorter than the other.
    if(CollationData::getStrength(options) == UCOL_SECONDARY) { return UCOL_EQUAL; }

    uint32_t tertiaryMask = ((options & CollationData::CASE_FIRST) == 0) ? 0x3fff : 0xffff;

    int32_t leftSTIndex = 0;
    int32_t rightSTIndex = 0;
    for(;;) {
        uint32_t leftTertiary;
        do {
            leftTertiary = leftSTBuffer[leftSTIndex++] & tertiaryMask;
        } while((leftTertiary & 0x3fff) == 0);
        // TODO: See TODO about 0x3fff in case level processing.

        uint32_t rightTertiary;
        do {
            rightTertiary = rightSTBuffer[rightSTIndex++] & tertiaryMask;
        } while((rightTertiary & 0x3fff) == 0);

        if(leftTertiary != rightTertiary) {
            if(data->sortsTertiaryUpperCaseFirst()) {
                leftTertiary ^= 0xc000;
                rightTertiary ^= 0xc000;
            }
            return (leftTertiary < rightTertiary) ? UCOL_LESS : UCOL_GREATER;
        }
        if(leftTertiary == Collation::NO_CE_WEIGHT) { break; }
    }

    return UCOL_EQUAL;
}

UCollationResult
CollationCompare::comparePrimaryAndCase(CollationIterator &/*left*/, CollationIterator &/*right*/,
                                        UErrorCode &/*errorCode*/) {
    // TODO: Why?? <quote from v1 implementation>
    // if(((secS & UCOL_PRIMARYMASK) != 0) || strength > UCOL_PRIMARY)
    //   primary ignorables should not be considered on the case level when the strength is primary
    //   otherwise, the CEs stop being well-formed
    // </quote>
    // We do consider secondary weights for primary ignorables,
    // and case bits for primary and secondary ignorables when strength>PRIMARY,
    // why not case bits for primary ignorables when strengh==PRIMARY?
    return UCOL_EQUAL;
}

class QuaternaryIterator {
public:
    QuaternaryIterator(CollationIterator &iter)
            : source(iter),
              variableTop(iter.getData()->variableTop),
              withHiraganaQuaternary((iter.getData()->options & CollationData::HIRAGANA_QUATERNARY) != 0),
              shifted(FALSE), hiragana(0) {}
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
        int8_t hira = source.getHiragana();
        if(hira >= 0) { hiragana = hira; }
        if(ce == 0) { continue; }
        // Extract the primary weight and use it as a quaternary unless we override it.
        uint32_t q = (uint32_t)(ce >> 32);
        if(q == 0) {
            // Skip primary ignorables after a shifted primary.
            if(shifted) { continue; }
            // Primary ignorable but not tertiary ignorable.
            q = 0xffffffff;
            if(withHiraganaQuaternary) { q -= hiragana; }
        } else if(q <= Collation::MERGE_SEPARATOR_PRIMARY) {
            // Leave NO_CE_WEIGHT or MERGE_SEPARATOR_PRIMARY as is.
            shifted = FALSE;
        } else if(q <= variableTop) {
            // Shift this variable primary weight into the quaternary level.
            shifted = TRUE;
        } else {
            // Regular CE.
            shifted = FALSE;
            q = 0xffffffff;
            if(withHiraganaQuaternary) { q -= hiragana; }
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
