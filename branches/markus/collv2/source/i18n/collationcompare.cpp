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

namespace {

class CEBuffer {
public:
    CEBuffer() : length(0) {}

    inline void append(int64_t ce, UErrorCode &errorCode) {
        if(length < buffer.getCapacity()) {
            buffer[length++] = ce;
        } else {
            doAppend(ce, errorCode);
        }
    }

    inline int64_t operator[](ptrdiff_t i) const { return buffer[i]; }

    int32_t length;

private:
    CEBuffer(const CEBuffer &);
    void operator=(const CEBuffer &);

    void doAppend(int64_t ce, UErrorCode &errorCode);

    MaybeStackArray<int64_t, 40> buffer;
};

void
CEBuffer::doAppend(int64_t ce, UErrorCode &errorCode) {
    // length == buffer.getCapacity()
    if(U_FAILURE(errorCode)) { return; }
    int32_t capacity = buffer.getCapacity();
    int32_t newCapacity;
    if(capacity < 1000) {
        newCapacity = 4 * capacity;
    } else {
        newCapacity = 2 * capacity;
    }
    int64_t *p = buffer.resize(newCapacity, length);
    if(p == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    p[length++] = ce;
}

}  // namespace

UCollationResult
CollationCompare::compareUpToQuaternary(CollationIterator &left, CollationIterator &right,
                                        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }

    CEBuffer leftBuffer;
    CEBuffer rightBuffer;

    const CollationData *data = left.getData();
    int32_t options = data->options;
    uint32_t variableTop;
    if((options & CollationData::ALTERNATE_MASK) == 0) {
        variableTop = 0;
    } else {
        // +1 so that we can use "<" and primary ignorables test out early.
        variableTop = data->variableTop + 1;
    }
    UBool anyVariable = FALSE;

    // Fetch CEs, compare primaries, store secondary & tertiary weights.
    U_ALIGN_CODE(16);
    for(;;) {
        // We fetch CEs until we get a non-ignorable primary or reach the end.
        uint32_t leftPrimary;
        do {
            int64_t ce = left.nextCE(errorCode);
            leftPrimary = (uint32_t)(ce >> 32);
            if(leftPrimary < variableTop && leftPrimary > Collation::MERGE_SEPARATOR_PRIMARY) {
                // Variable CE, shift it to quaternary level.
                // Ignore all following primary ignorables, and shift further variable CEs.
                anyVariable = TRUE;
                do {
                    // Store the primary of the variable CE.
                    leftBuffer.append(ce & 0xffffffff00000000, errorCode);
                    do {
                        ce = left.nextCE(errorCode);
                        leftPrimary = (uint32_t)(ce >> 32);
                    } while(leftPrimary == 0);
                } while(leftPrimary < variableTop &&
                        leftPrimary > Collation::MERGE_SEPARATOR_PRIMARY);
            }
            leftBuffer.append(ce, errorCode);
        } while(leftPrimary == 0);

        uint32_t rightPrimary;
        do {
            int64_t ce = right.nextCE(errorCode);
            rightPrimary = (uint32_t)(ce >> 32);
            if(rightPrimary < variableTop && rightPrimary > Collation::MERGE_SEPARATOR_PRIMARY) {
                // Variable CE, shift it to quaternary level.
                // Ignore all following primary ignorables, and shift further variable CEs.
                anyVariable = TRUE;
                do {
                    // Store the primary of the variable CE.
                    rightBuffer.append(ce & 0xffffffff00000000, errorCode);
                    do {
                        ce = right.nextCE(errorCode);
                        rightPrimary = (uint32_t)(ce >> 32);
                    } while(rightPrimary == 0);
                } while(rightPrimary < variableTop &&
                        rightPrimary > Collation::MERGE_SEPARATOR_PRIMARY);
            }
            rightBuffer.append(ce, errorCode);
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
        if(leftPrimary == Collation::NO_CE_PRIMARY) { break; }
    }
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }

    // Compare the buffered secondary & tertiary weights.
    // We might skip the secondary level but continue with the case level
    // which is turned on separately.
    if(CollationData::getStrength(options) >= UCOL_SECONDARY) {
        if((options & CollationData::BACKWARD_SECONDARY) == 0) {
            int32_t leftIndex = 0;
            int32_t rightIndex = 0;
            for(;;) {
                uint32_t leftSecondary;
                do {
                    leftSecondary = ((uint32_t)leftBuffer[leftIndex++]) >> 16;
                } while(leftSecondary == 0);

                uint32_t rightSecondary;
                do {
                    rightSecondary = ((uint32_t)rightBuffer[rightIndex++]) >> 16;
                } while(rightSecondary == 0);

                if(leftSecondary != rightSecondary) {
                    return (leftSecondary < rightSecondary) ? UCOL_LESS : UCOL_GREATER;
                }
                if(leftSecondary == Collation::NO_CE_WEIGHT16) { break; }
            }
        } else {
            // The backwards secondary level compares secondary weights backwards
            // within segments separated by the merge separator (U+FFFE, weight 02).
            int32_t leftStart = 0;
            int32_t rightStart = 0;
            for(;;) {
                // Find the merge separator or the NO_CE terminator.
                int32_t leftLimit = leftStart;
                uint32_t leftLower32;
                while((leftLower32 = (uint32_t)leftBuffer[leftLimit]) >
                            Collation::MERGE_SEPARATOR_LOWER32 ||
                        leftLower32 == 0) {
                    ++leftLimit;
                }
                int32_t rightLimit = rightStart;
                uint32_t rightLower32;
                while((rightLower32 = (uint32_t)rightBuffer[rightLimit]) >
                            Collation::MERGE_SEPARATOR_LOWER32 ||
                        rightLower32 == 0) {
                    ++rightLimit;
                }

                // Compare the segments.
                int32_t leftIndex = leftLimit;
                int32_t rightIndex = rightLimit;
                for(;;) {
                    int32_t leftSecondary = 0;
                    while(leftSecondary == 0 && leftIndex > leftStart) {
                        leftSecondary = ((uint32_t)leftBuffer[--leftIndex]) >> 16;
                    }

                    int32_t rightSecondary = 0;
                    while(rightSecondary == 0 && rightIndex > rightStart) {
                        rightSecondary = ((uint32_t)rightBuffer[--rightIndex]) >> 16;
                    }

                    if(leftSecondary != rightSecondary) {
                        return (leftSecondary < rightSecondary) ? UCOL_LESS : UCOL_GREATER;
                    }
                    if(leftSecondary == 0) { break; }
                }

                // Did we reach the end of either string?
                // Both strings have the same number of merge separators,
                // or else there would have been a primary-level difference.
                U_ASSERT(leftBuffer[leftLimit] == rightBuffer[rightLimit]);
                if(leftBuffer[leftLimit] == Collation::NO_CE) { break; }
                // Skip both merge separators and continue.
                leftStart = leftLimit + 1;
                rightStart = rightLimit + 1;
            }
        }
    }

    if((options & CollationData::CASE_LEVEL) != 0) {
        int32_t strength = CollationData::getStrength(options);
        int32_t leftIndex = 0;
        int32_t rightIndex = 0;
        for(;;) {
            uint32_t leftCase, leftLower32, rightCase;
            if(strength == UCOL_PRIMARY) {
                // Primary+case: Ignore case level weights of primary ignorables.
                // Otherwise we would get a-umlaut > a
                // which is not desirable for accent-insensitive sorting.
                // Check for (lower 32 bits) == 0 as well because variable CEs are stored
                // with only primary weights.
                int64_t ce;
                do {
                    ce = leftBuffer[leftIndex++];
                    leftCase = (uint32_t)ce;
                } while((uint32_t)(ce >> 32) == 0 || leftCase == 0);
                leftLower32 = leftCase;
                leftCase &= 0xc000;

                do {
                    ce = rightBuffer[rightIndex++];
                    rightCase = (uint32_t)ce;
                } while((uint32_t)(ce >> 32) == 0 || rightCase == 0);
                rightCase &= 0xc000;
            } else {
                // Secondary+case: By analogy with the above,
                // ignore case level weights of secondary ignorables.
                // Tertiary+case: If we turned 0.0.ct into 0.0.c.t (c=case weight)
                // then 0.0.c.t would be ill-formed because c<upper would be less than
                // uppercase weights on primary and secondary CEs.
                // (See UCA well-formedness condition 2.)
                // We could construct a special case weight higher than uppercase,
                // but it's simpler to always ignore case weights of secondary ignorables,
                // turning 0.0.ct into 0.0.0.t.
                do {
                    leftCase = (uint32_t)leftBuffer[leftIndex++];
                } while(leftCase <= 0xffff);
                leftLower32 = leftCase;
                leftCase &= 0xc000;

                do {
                    rightCase = (uint32_t)rightBuffer[rightIndex++];
                } while(rightCase <= 0xffff);
                rightCase &= 0xc000;
            }

            // No need to handle NO_CE and MERGE_SEPARATOR specially:
            // There is one case weight for each previous-level weight,
            // so level length differences were handled there.
            if(leftCase != rightCase) {
                if((options & CollationData::UPPER_FIRST) == 0) {
                    return (leftCase < rightCase) ? UCOL_LESS : UCOL_GREATER;
                } else {
                    return (leftCase < rightCase) ? UCOL_GREATER : UCOL_LESS;
                }
            }
            if((leftLower32 >> 16) == Collation::NO_CE_WEIGHT16) { break; }
        }
    }
    if(CollationData::getStrength(options) <= UCOL_SECONDARY) { return UCOL_EQUAL; }

    uint32_t tertiaryMask = CollationData::getTertiaryMask(options);

    int32_t leftIndex = 0;
    int32_t rightIndex = 0;
    uint32_t anyQuaternaries = 0;
    for(;;) {
        uint32_t leftLower32, leftTertiary;
        do {
            leftLower32 = (uint32_t)leftBuffer[leftIndex++];
            anyQuaternaries |= leftLower32;
            U_ASSERT((leftLower32 & Collation::ONLY_TERTIARY_MASK) != 0 ||
                     (leftLower32 & 0xc0c0) == 0);
            leftTertiary = leftLower32 & tertiaryMask;
        } while(leftTertiary == 0);

        uint32_t rightLower32, rightTertiary;
        do {
            rightLower32 = (uint32_t)rightBuffer[rightIndex++];
            anyQuaternaries |= rightLower32;
            U_ASSERT((rightLower32 & Collation::ONLY_TERTIARY_MASK) != 0 ||
                     (rightLower32 & 0xc0c0) == 0);
            rightTertiary = rightLower32 & tertiaryMask;
        } while(rightTertiary == 0);

        if(leftTertiary != rightTertiary) {
            if(CollationData::sortsTertiaryUpperCaseFirst(options)) {
                // Pass through NO_CE and MERGE_SEPARATOR
                // and keep real tertiary weights larger than the MERGE_SEPARATOR.
                // Do not change the artificial uppercase weight of a tertiary CE (0.0.ut),
                // to keep tertiary CEs well-formed.
                // Their case+tertiary weights must be greater than those of
                // primary and secondary CEs.
                if(leftTertiary > Collation::MERGE_SEPARATOR_WEIGHT16 && leftLower32 > 0xffff) {
                    leftTertiary ^= 0x8000;
                }
                if(rightTertiary > Collation::MERGE_SEPARATOR_WEIGHT16 && rightLower32 > 0xffff) {
                    rightTertiary ^= 0x8000;
                }
            }
            return (leftTertiary < rightTertiary) ? UCOL_LESS : UCOL_GREATER;
        }
        if(leftTertiary == Collation::NO_CE_WEIGHT16) { break; }
    }
    if(CollationData::getStrength(options) <= UCOL_TERTIARY) { return UCOL_EQUAL; }

    if(!anyVariable && (anyQuaternaries & 0xc0) == 0) {
        // If there are no "variable" CEs and no non-zero quaternary weights,
        // then there are no quaternary differences.
        return UCOL_EQUAL;
    }

    leftIndex = 0;
    rightIndex = 0;
    for(;;) {
        uint32_t leftQuaternary;
        do {
            int64_t ce = leftBuffer[leftIndex++];
            uint32_t low16 = (uint32_t)ce & 0xffff;
            if(low16 == 0) {
                // Variable primary or completely ignorable.
                leftQuaternary = (uint32_t)(ce >> 32);
            } else if(low16 <= Collation::MERGE_SEPARATOR_WEIGHT16) {
                // Leave NO_CE or MERGE_SEPARATOR as is.
                leftQuaternary = low16;
            } else {
                // Regular CE, not tertiary ignorable.
                // Preserve the quaternary weight in bits 7..6.
                leftQuaternary = low16 | 0xffffff3f;
            }
        } while(leftQuaternary == 0);

        uint32_t rightQuaternary;
        do {
            int64_t ce = rightBuffer[rightIndex++];
            uint32_t low16 = (uint32_t)ce & 0xffff;
            if(low16 == 0) {
                // Variable primary or completely ignorable.
                rightQuaternary = (uint32_t)(ce >> 32);
            } else if(low16 <= Collation::MERGE_SEPARATOR_WEIGHT16) {
                // Leave NO_CE or MERGE_SEPARATOR as is.
                rightQuaternary = low16;
            } else {
                // Regular CE, not tertiary ignorable.
                // Preserve the quaternary weight in bits 7..6.
                rightQuaternary = low16 | 0xffffff3f;
            }
        } while(rightQuaternary == 0);

        if(leftQuaternary != rightQuaternary) {
            // Return the difference, with script reordering.
            const uint8_t *reorderTable = left.getData()->reorderTable;
            if (reorderTable != NULL) {
                leftQuaternary = Collation::reorder(reorderTable, leftQuaternary);
                rightQuaternary = Collation::reorder(reorderTable, rightQuaternary);
            }
            return (leftQuaternary < rightQuaternary) ? UCOL_LESS : UCOL_GREATER;
        }
        if(leftQuaternary == Collation::NO_CE_WEIGHT16) { break; }
    }
    return UCOL_EQUAL;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
