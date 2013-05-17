/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationrootelements.h
*
* created on: 2013mar01
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONROOTELEMENTS_H__
#define __COLLATIONROOTELEMENTS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class U_I18N_API CollationRootElements : public UMemory {
public:
    CollationRootElements(const uint32_t *rootElements, int32_t rootElementsLength)
            : elements(rootElements), length(rootElementsLength) {}

    static const uint32_t SEC_TER_DELTA_FLAG = 0x80;
    static const uint32_t PRIMARY_STEP_MASK = 0x7f;

    enum {
        /**
         * Index of the first CE with a non-zero tertiary weight.
         * Same as the start of the compact root elements table.
         */
        IX_FIRST_TERTIARY_INDEX,
        /**
         * Index of the first CE with a non-zero secondary weight.
         */
        IX_FIRST_SECONDARY_INDEX,
        /**
         * Index of the first CE with a non-zero primary weight.
         */
        IX_FIRST_PRIMARY_INDEX,
        /**
         * Must match Collation::COMMON_SEC_AND_TER_CE.
         */
        IX_COMMON_SEC_AND_TER_CE,
        /**
         * Secondary & tertiary boundaries.
         * Bits 31..24: [fixed last secondary common byte 45]
         * Bits 23..16: [fixed first ignorable secondary byte 80]
         * Bits 15.. 8: reserved, 0
         * Bits  7.. 0: [fixed first ignorable tertiary byte 3C]
         */
        IX_SEC_TER_BOUNDARIES,
        /**
         * The current number of indexes.
         * Currently the same as elements[IX_FIRST_TERTIARY_INDEX].
         */
        IX_COUNT
    };

    /**
     * Returns the boundary between tertiary weights of primary/secondary CEs
     * and those of tertiary CEs.
     * This is the upper limit for tertiaries of primary/secondary CEs.
     * This minus one is the lower limit for tertiaries of tertiary CEs.
     */
    uint32_t getTertiaryBoundary() const {
        return (elements[IX_SEC_TER_BOUNDARIES] << 8) & 0xff00;
    }

    /**
     * Returns the first assigned tertiary CE.
     */
    uint32_t getFirstTertiaryCE() const {
        return elements[elements[IX_FIRST_TERTIARY_INDEX]];
    }

    /**
     * Returns the last assigned tertiary CE.
     */
    uint32_t getLastTertiaryCE() const {
        return elements[elements[IX_FIRST_SECONDARY_INDEX] - 1];
    }

    /**
     * Returns the last common secondary weight.
     * This is the lower limit for secondaries of primary CEs.
     */
    uint32_t getLastCommonSecondaryWeight() const {
        return (elements[IX_SEC_TER_BOUNDARIES] >> 16) & 0xff00;
    }

    /**
     * Returns the boundary between secondary weights of primary CEs
     * and those of secondary CEs.
     * This is the upper limit for secondaries of primary CEs.
     * This minus one is the lower limit for secondaries of secondary CEs.
     */
    uint32_t getSecondaryBoundary() const {
        return (elements[IX_SEC_TER_BOUNDARIES] >> 8) & 0xff00;
    }

    /**
     * Returns the first assigned secondary CE.
     */
    uint32_t getFirstSecondaryCE() const {
        return elements[elements[IX_FIRST_SECONDARY_INDEX]];
    }

    /**
     * Returns the last assigned secondary CE.
     */
    uint32_t getLastSecondaryCE() const {
        return elements[elements[IX_FIRST_PRIMARY_INDEX] - 1];
    }

    /**
     * Returns the first assigned primary weight.
     */
    uint32_t getFirstPrimary() const {
        return elements[elements[IX_FIRST_PRIMARY_INDEX]];
    }

    /**
     * Returns the first assigned primary CE.
     */
    int64_t getFirstPrimaryCE() const {
        return Collation::makeCE(elements[elements[IX_FIRST_PRIMARY_INDEX]]);
    }

    /**
     * Returns the last root CE with a primary weight before p.
     * Intended only for reordering group boundaries.
     */
    int64_t lastCEWithPrimaryBefore(uint32_t p) const;

    /**
     * Returns the first root CE with a primary weight of at least p.
     * Intended only for reordering group boundaries.
     */
    int64_t firstCEWithPrimaryAtLeast(uint32_t p) const;

    /**
     * Returns the primary weight before p.
     * p must be greater than the first root primary.
     */
    uint32_t getPrimaryBefore(uint32_t p, UBool isCompressible) const;

    /** Returns the secondary weight before [p, s]. */
    uint32_t getSecondaryBefore(uint32_t p, uint32_t s) const;

    /** Returns the tertiary weight before [p, s, t]. */
    uint32_t getTertiaryBefore(uint32_t p, uint32_t s, uint32_t t) const;

    /**
     * Returns lower and upper primary weight limits for the input ce.
     * @return ce's primary in upper 32 bits, next primary in lower 32
     */
    int64_t getPrimaryLimitsAt(int64_t ce, UBool isCompressible) const;
    /**
     * Returns lower and upper secondary weight limits for the input ce.
     * @return ce's secondary in upper 32 bits, next secondary in lower 32
     */
    int64_t getSecondaryLimitsAt(int64_t ce) const;
    /**
     * Returns lower and upper tertiary weight limits for the input ce.
     * @return ce's tertiary in upper 32 bits, next tertiary in lower 32
     */
    int64_t getTertiaryLimitsAt(int64_t ce) const;

private:
    /**
     * Finds the index i for the input CE. The CE must occur as a root CE.
     * It must not be 0.
     *
     * If the CE has non-common secondary/tertiary weights, then the index is on the sec/ter delta.
     * Otherwise, if the primary element is followed by a range end,
     * then the non-zero step value is included in the return value.
     *
     * @return (i<<8) | step
     */
    int32_t findCE(int64_t ce) const;
    /**
     * Finds the largest index i where elements[i]<=p.
     * Requires first primary<=p<0xff000000 (SPECIAL_PRIMARY).
     */
    int32_t findPrimary(uint32_t p) const;

    /**
     * Data structure:
     *
     * The first few entries are indexes, up to elements[IX_FIRST_TERTIARY_INDEX].
     * See the comments on the IX_ constants.
     *
     * All other elements are a compact form of the root collator CEs
     * in collation order.
     *
     * Primary weights have the SEC_TER_DELTA_FLAG flag not set.
     * A primary-weight element by itself represents a root CE
     * with Collation::COMMON_SEC_AND_TER_CE.
     *
     * If there are root CEs with the same primary but other secondary/tertiary weights,
     * then for each such CE there is an element with those secondary and tertiary weights,
     * and with the SEC_TER_DELTA_FLAG flag set.
     *
     * A range of only-primary CEs with a consistent "step" increment
     * from each primary to the next may be stored as a range.
     * Only the first and last primary are stored, and the last has the step
     * value in the low bits (PRIMARY_STEP_MASK).
     *
     * An range-end element may also either start a new range or be followed by
     * elements with secondary/tertiary deltas.
     *
     * A primary element that is not a range end has zero step bits.
     *
     * There is no element for the completely ignorable CE (all weights 0).
     *
     * Before elements[IX_FIRST_PRIMARY_INDEX], all elements are secondary/tertiary deltas,
     * for all of the ignorable root CEs.
     *
     * There are no elements for unassigned-implicit primary CEs.
     * All primaries stored here are at most 3 bytes long.
     */
    const uint32_t *elements;
    int32_t length;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONROOTELEMENTS_H__
