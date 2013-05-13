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

int64_t
CollationRootElements::lastCEWithPrimaryBefore(uint32_t p) const {
    if(p == 0) { return 0; }
    U_ASSERT(p > elements[elements[IX_FIRST_PRIMARY_INDEX]]);
    int32_t index = findPrimary(p);
    uint32_t q = elements[index];
    uint32_t secTer;
    if(p == (q & 0xffffff00)) {
        // p == elements[index] is a root primary. Find the CE before it.
        // We must not be in a primary range.
        U_ASSERT((q & PRIMARY_STEP_MASK) == 0);
        secTer = elements[index - 1];
        if((secTer & SEC_TER_DELTA_FLAG) == 0) {
            // Primary CE just before p.
            p = secTer & 0xffffff00;
            secTer = Collation::COMMON_SEC_AND_TER_CE;
        } else {
            // secTer = last secondary & tertiary for the previous primary
            index -= 2;
            for(;;) {
                p = elements[index];
                if((p & SEC_TER_DELTA_FLAG) == 0) {
                    p &= 0xffffff00;
                    break;
                }
                --index;
            }
        }
    } else {
        // p > elements[index] which is the previous primary.
        // Find the last secondary & tertiary weights for it.
        p = q & 0xffffff00;
        secTer = Collation::COMMON_SEC_AND_TER_CE;
        for(;;) {
            q = elements[++index];
            if((q & SEC_TER_DELTA_FLAG) == 0) {
                // We must not be in a primary range.
                U_ASSERT((q & PRIMARY_STEP_MASK) == 0);
                break;
            }
            secTer = q;
        }
    }
    return ((int64_t)p << 32) | secTer;
}

int64_t
CollationRootElements::firstCEWithPrimaryAtLeast(uint32_t p) const {
    if(p == 0) { return 0; }
    int32_t index = findPrimary(p);
    if(p != (elements[index] & 0xffffff00)) {
        for(;;) {
            p = elements[++index];
            if((p & SEC_TER_DELTA_FLAG) == 0) {
                // First primary after p. We must not be in a primary range.
                U_ASSERT((p & PRIMARY_STEP_MASK) == 0);
                break;
            }
        }
    }
    return ((int64_t)p << 32) | Collation::COMMON_SEC_AND_TER_CE;
}

#if 0  // TODO
uint32_t
CollationRootElements::getPrimaryBefore(int64_t ce, UBool isCompressible) const {
    uint32_t p = (uint32_t)(ce >> 32);
    if(p <= elements[elements[IX_FIRST_PRIMARY_INDEX]]) {
        // No primary before [first space].
        return 0;
    }
    int32_t indexAndStep = findCE(ce);
    int32_t step = indexAndStep & 0xff;
    uint32_t q;  // next primary
    if(step != 0) {
        if((p & 0xffff) == 0) {
            q = Collation::incTwoBytePrimaryByOffset(p, isCompressible, step);
        } else {
            q = Collation::incThreeBytePrimaryByOffset(p, isCompressible, step);
        }
    } else {
        int32_t index = indexAndStep >> 8;
        for(;;) {
            q = elements[++index];
            if((q & SEC_TER_DELTA_FLAG) == 0) {
                U_ASSERT((q & PRIMARY_STEP_MASK) == 0);
                break;
            }
        }
    }
    return ((int64_t)p << 32) | q;
}
#endif

int64_t
CollationRootElements::getPrimaryLimitsAt(int64_t ce, UBool isCompressible) const {
    uint32_t p = (uint32_t)(ce >> 32);
    if(p == 0) {
        // No primary before [first space].
        return 0;
    }
    int32_t indexAndStep = findCE(ce);
    int32_t step = indexAndStep & 0xff;
    uint32_t q;  // next primary
    if(step != 0) {
        if((p & 0xffff) == 0) {
            q = Collation::incTwoBytePrimaryByOffset(p, isCompressible, step);
        } else {
            q = Collation::incThreeBytePrimaryByOffset(p, isCompressible, step);
        }
    } else {
        int32_t index = indexAndStep >> 8;
        for(;;) {
            q = elements[++index];
            if((q & SEC_TER_DELTA_FLAG) == 0) {
                U_ASSERT((q & PRIMARY_STEP_MASK) == 0);
                break;
            }
        }
    }
    return ((int64_t)p << 32) | q;
}

int64_t
CollationRootElements::getSecondaryLimitsAt(int64_t ce) const {
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t secTer = (uint32_t)ce & Collation::ONLY_SEC_TER_MASK;
    uint32_t s = secTer >> 16;
    uint32_t u;  // next secondary
    if(p == 0 && s == 0) {
        // Gap at the beginning of the secondary CE range.
        s = getSecondaryBoundary() - 0x100;
        u = getFirstSecondaryCE() >> 16;
    } else {
        int32_t index = findCE(ce) >> 8;
        for(;;) {
            u = elements[++index];
            if((u & SEC_TER_DELTA_FLAG) == 0) {
                // No secondary greater than s for this primary.
                if(p == 0) {
                    // Gap at the end of the secondary CE range.
                    u = 0x10000;
                } else {
                    // Gap for secondaries of primary CEs.
                    u = getSecondaryBoundary();
                }
                break;
            }
            u >>= 16;
            if(s < u) { break; }
            U_ASSERT(s == u);
        }
        if(s == Collation::COMMON_WEIGHT16) {
            // Protect the range of compressed secondary bytes for sort keys.
            s = getLastCommonSecondaryWeight();
        }
    }
    return ((int64_t)s << 32) | u;
}

int64_t
CollationRootElements::getTertiaryLimitsAt(int64_t ce) const {
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t secTer = (uint32_t)ce & Collation::ONLY_SEC_TER_MASK;
    uint32_t t = secTer & 0xffff;
    uint32_t u;  // next tertiary
    if(p == 0 && secTer == 0) {
        // Gap at the beginning of the tertiary CE range.
        t = getTertiaryBoundary() - 0x100;
        u = getFirstTertiaryCE() & Collation::ONLY_TERTIARY_MASK;
    } else {
        int32_t index = findCE(ce) >> 8;
        u = elements[++index];
        if((u & SEC_TER_DELTA_FLAG) == 0) {
            // No tertiary greater than t for this primary+secondary.
            if((secTer >> 16) == 0) {
                // Gap at the end of the tertiary CE range.
                u = 0x4000;
            } else {
                // Gap for tertiaries of primary/secondary CEs.
                u = getTertiaryBoundary();
            }
        } else {
            u &= Collation::ONLY_TERTIARY_MASK;
        }
    }
    return ((int64_t)t << 32) | u;
}

int32_t
CollationRootElements::findCE(int64_t ce) const {
    // Requirement: ce must occur as a root CE.
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t secTer = (uint32_t)ce & Collation::ONLY_SEC_TER_MASK;
    int32_t index;
    if(p == 0) {
        index = (int32_t)elements[IX_FIRST_TERTIARY_INDEX];
    } else {
        U_ASSERT((p & 0xff) == 0);  // at most a 3-byte primary
        index = findPrimary(p);
        uint32_t nextElement = elements[index + 1];
        if((nextElement & SEC_TER_DELTA_FLAG) != 0) {
            U_ASSERT(p == (elements[index] & 0xffffff00));
            if(secTer == Collation::COMMON_SEC_AND_TER_CE) {
                return index << 8;
            }
            ++index;
        } else {
            // No following sec/ter delta.
            U_ASSERT(secTer == Collation::COMMON_SEC_AND_TER_CE);
            int32_t step = (int32_t)nextElement & 0xff;
            if(step == 0) {
                U_ASSERT(p == (elements[index] & 0xffffff00));
            } else {
                // We just assume that p is an actual primary in this range.
            }
            return (index << 8) | step;
        }
    }
    U_ASSERT(secTer > Collation::COMMON_SEC_AND_TER_CE);
    for(;;) {
        uint32_t st = elements[index];
        U_ASSERT((st & SEC_TER_DELTA_FLAG) != 0);
        st &= Collation::ONLY_SEC_TER_MASK;
        if(st == secTer) {
            return index << 8;
        }
        U_ASSERT(st < secTer);
        ++index;
    }
}

int32_t
CollationRootElements::findPrimary(uint32_t p) const {
    U_ASSERT((p >> 24) != Collation::UNASSIGNED_IMPLICIT_BYTE);
    // modified binary search
    int32_t start = (int32_t)elements[IX_FIRST_PRIMARY_INDEX];
    U_ASSERT(p >= elements[start]);
    int32_t limit = length - 1;
    U_ASSERT(elements[limit] >= Collation::SPECIAL_PRIMARY);
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
        if(p < (q & 0xffffff00)) {  // Reset the "step" bits of a range end primary.
            limit = i;
        } else {
            start = i;
        }
    }
    return start;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
