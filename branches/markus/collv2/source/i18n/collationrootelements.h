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

class CollationRootElements : public UMemory {
public:
    CollationRootElements(const uint32_t *rootElements, int32_t rootElementsLength)
            : elements(rootElements), length(rootElementsLength) {}

    static const uint32_t SEC_TER_DELTA_FLAG = 0x80;
    static const uint32_t PRIMARY_STEP_MASK = 0x7f;
    static const uint32_t NO_CASE_NO_QUAT_MASK = 0xffff3f3f;

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
        // TODO: check [fixed last secondary common byte] <= CollationKeys::SEC_COMMON_HIGH
        /**
         * The current number of indexes.
         * Currently the same as elements[IX_FIRST_TERTIARY_INDEX].
         */
        IX_COUNT
    };

    uint32_t getFirstPrimary() const {
        return elements[elements[IX_FIRST_PRIMARY_INDEX]];
    }

private:
    /**
     * Finds the largest index i where elements[i]<=p.
     * Requires getFirstPrimary()<=p<0xff000000 (SPECIAL_BYTE<<24).
     */
    int32_t findPrimary(uint32_t p);

    const uint32_t *elements;
    int32_t length;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONROOTELEMENTS_H__
