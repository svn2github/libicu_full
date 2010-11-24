/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uchartrie.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov14
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "uassert.h"
#include "uchartrie.h"

U_NAMESPACE_BEGIN

UBool
UCharTrie::readCompactInt(int32_t leadUnit) {
    UBool isFinal=(UBool)(leadUnit&kValueIsFinal);
    leadUnit>>=1;
    if(leadUnit<kMinTwoUnitLead) {
        value=leadUnit-kMinOneUnitLead;
    } else if(leadUnit<kThreeUnitLead) {
        value=((leadUnit-kMinTwoUnitLead)<<16)|*pos++;
    } else {
        value=(pos[0]<<16)|pos[1];
        pos+=2;
    }
    return isFinal;
}

int32_t
UCharTrie::readFixedInt(int32_t node) {
    int32_t fixedInt=*pos++;
    if(node&kFixedInt32) {
        fixedInt=(fixedInt<<16)|*pos++;
    }
    return fixedInt;
}

UBool
UCharTrie::next(int uchar) {
    if(pos==NULL) {
        return FALSE;
    }
    haveValue=FALSE;
    int32_t length=remainingMatchLength;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Remaining part of a linear-match node.
        if(uchar==*pos) {
            remainingMatchLength=length-1;
            ++pos;
            return TRUE;
        } else {
            // No match.
            stop();
            return FALSE;
        }
    }
    int32_t node=*pos;
    if(node>=kMinValueLead) {
        if(node&kValueIsFinal) {
            // No further matching units.
            stop();
            return FALSE;
        } else {
            // Skip intermediate value.
            node>>=1;
            if(node<kMinTwoUnitLead) {
                ++pos;
            } else if(node<kThreeUnitLead) {
                pos+=2;
            } else {
                pos+=3;
            }
            // The next node must not also be a value node.
            node=*pos;
            U_ASSERT(node<kMinValueLead);
        }
    }
    ++pos;
    if(node<kMinLinearMatch) {
        // Branch according to the current unit.
        while(node>=kMinThreeWayBranch) {
            // Branching on a unit value,
            // with a jump delta for less-than, a compact int for equals,
            // and continuing for greater-than.
            // The less-than and greater-than branches must lead to branch nodes again.
            UChar trieUnit=*pos++;
            if(uchar<trieUnit) {
                int32_t delta=readFixedInt(node);
                pos+=delta;
            } else {
                pos+=(node&kFixedInt32)+1;  // Skip fixed-width integer.
                node>>=1;
                if(uchar==trieUnit) {
                    value=readFixedInt(node);
                    if(node&kFixedIntIsFinal) {
                        haveValue=TRUE;
                        stop();
                    } else {
                        // Use the non-final value as the jump delta.
                        pos+=value;
                    }
                    return TRUE;
                } else {  // uchar>trieUnit
                    // Skip the equals value.
                    pos+=(node&kFixedInt32)+1;
                }
            }
            node=*pos++;
            U_ASSERT(node<kMinLinearMatch);
        }
        // Branch node with a list of key-value pairs where
        // values are either final values or jump deltas.
        // If the last key unit matches, just continue after it rather
        // than jumping.
        length=(node>>kMaxListBranchLengthShift)+1;  // Actual list length minus 1.
        if(length>=kMaxListBranchSmallLength) {
            // For 7..14 pairs, read the next unit as well.
            node=(node<<16)|*pos++;
        }
        // The lower node bits now contain a bit pair for each (key, value) pair:
        // - whether the value is final
        // - whether the value is 1 or 2 units long
        for(;;) {
            UChar trieUnit=*pos++;
            if(uchar==trieUnit) {
                if(length>0) {
                    value=readFixedInt(node);
                    if(node&kFixedIntIsFinal) {
                        haveValue=TRUE;
                        stop();
                    } else {
                        // Use the non-final value as the jump delta.
                        pos+=value;
                    }
                }
                return TRUE;
            }
            if(uchar<trieUnit || length--==0) {
                stop();
                return FALSE;
            }
            // Skip the value.
            pos+=(node&kFixedInt32)+1;
            node>>=2;
        }
    } else {
        // Match the first of length+1 units.
        length=node-kMinLinearMatch;  // Actual match length minus 1.
        if(uchar==*pos) {
            remainingMatchLength=length-1;
            ++pos;
            return TRUE;
        } else {
            // No match.
            stop();
            return FALSE;
        }
    }
}

UBool
UCharTrie::contains() {
    int32_t node;
    if(haveValue) {
        return TRUE;
    } else if(pos!=NULL && remainingMatchLength<0 && (node=*pos)>=kMinValueLead) {
        // Deliver value for the matching units.
        ++pos;
        if(readCompactInt(node)) {
            stop();
        }
        return TRUE;
    }
    return FALSE;
}

UBool
UCharTrie::containsNext(const UChar *s, int32_t length) {
    if(length<0) {
        // NUL-terminated
        int c;
        while((c=*s++)!=0) {
            if(!next(c)) {
                return FALSE;
            }
        }
    } else {
        while(length>0) {
            if(!next(*s++)) {
                return FALSE;
            }
            --length;
        }
    }
    return contains();
}

U_NAMESPACE_END
