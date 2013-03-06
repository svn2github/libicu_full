/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationrootelements.cpp
*
* created on: 2013mar05
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "collation.h"
#include "collationrootelements.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

int32_t
CollationRootElements::findPrimary(uint32_t p) {
    U_ASSERT((p & 0xff) == 0);  // at most a 3-byte primary
    // modified binary search
    int32_t start = (int32_t)elements[IX_FIRST_PRIMARY_INDEX];
    U_ASSERT(p >= elements[start]);
    int32_t limit = length - 1;
    U_ASSERT(elements[limit] >= ((uint32_t)Collation::SPECIAL_BYTE << 24));
    U_ASSERT(p < elements[limit]);
    while((start + 1) < limit) {
        // Invariant: elements[start] and elements[limit] are primaries,
        // and elements[start]<=p<=elements[limit].
        int32_t i = (start + limit) / 2;
        uint32_t q = elements[i];
        if((q & SEC_TER_DELTA_FLAG) != 0) {
            // Find the next primary.
            int32_t j = i + 1;
            for(;;) {
                if(j == limit) { break; }
                q = elements[j];
                if((q & SEC_TER_DELTA_FLAG) == 0) {
                    i = j;
                    break;
                }
                ++j;
            }
            if((q & SEC_TER_DELTA_FLAG) != 0) {
                // Find the preceding primary.
                j = i - 1;
                for(;;) {
                    if(j == start) { break; }
                    q = elements[j];
                    if((q & SEC_TER_DELTA_FLAG) == 0) {
                        i = j;
                        break;
                    }
                    --j;
                }
                if((q & SEC_TER_DELTA_FLAG) != 0) {
                    // No primary between start and limit.
                    break;
                }
            }
        }
        if(p < (q & 0xffffff00)) {  // reset the "step" bits of a range end primary
            limit = i;
        } else {
            start = i;
        }
    }
    return start;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
