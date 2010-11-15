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

#ifndef __UCHARTRIE_H__
#define __UCHARTRIE_H__

/**
 * \file
 * \brief C++ API: Dictionary trie for mapping Unicode strings (or 16-bit-unit sequences)
 *                 to integer values.
 */

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class UCharTrieBuilder;
class UCharTrieIterator;

/**
 * Light-weight, non-const reader class for a UCharTrie.
 * Traverses a UChar-serialized data structure with minimal state,
 * for mapping strings (16-bit-unit sequences) to non-negative integer values.
 */
class /*U_COMMON_API*/ UCharTrie : public UMemory {
public:
    UCharTrie(const UChar *trieUChars)
            : uchars(trieUChars),
              pos(uchars), remainingMatchLength(-1), value(0), haveValue(FALSE) {}

    UCharTrie &reset() {
        pos=uchars;
        remainingMatchLength=-1;
        haveValue=FALSE;
        return *this;
    }

    /**
     * Traverses the trie from the current state for this input UChar.
     * @return TRUE if the UChar continues a matching string.
     */
    UBool next(int uchar);

    UBool nextForCodePoint(UChar32 cp) {
        return cp<=0xffff ? next(cp) : next(U16_LEAD(cp)) && next(U16_TRAIL(cp));
    }

    /**
     * @return TRUE if the trie contains the string so far.
     *         In this case, an immediately following call to getValue()
     *         returns the string's value.
     */
    UBool contains();

    /**
     * Traverses the trie from the current state for this string,
     * calls next(u) for each UChar u in the sequence,
     * and calls contains() at the end.
     */
    UBool containsNext(const UChar *s, int32_t length);

    /**
     * Returns a string's value if called immediately after contains()
     * returned TRUE. Otherwise undefined.
     */
    int32_t getValue() const { return value; }

private:
    friend class UCharTrieBuilder;
    friend class UCharTrieIterator;

    inline void stop() {
        pos=NULL;
    }

    // Reads a compact 32-bit integer and post-increments pos.
    // pos is already after the leadUnit.
    // Returns TRUE if the integer is a final value.
    inline UBool readCompactInt(int32_t leadUnit);

    // pos is already after the leadUnit.
    inline void skipCompactInt(int32_t leadUnit);

    // Reads a fixed-width integer and post-increments pos.
    inline int32_t readFixedInt(int32_t node);

    // Node lead unit values.

    // 0..0aff: Branch node with a list of 2..12 comparison UChars.
    // Bits 11..8=0..10 for 2..12 units to match.
    // The lower bits, and if 5..12 units then also the bits from the next unit,
    // indicate whether each key unit has a final value vs. a "jump" (higher bit of a pair),
    // and whether the match unit's value is 1 or 2 units long.
    // Followed by the (key, value) pairs except that the last unit's value is omitted
    // if it would be a "jump" (just continue reading the next node from there).

    // 0b00..0b07: Three-way-branch node with less/equal/greater outbound edges.
    // The 3 lower bits indicate the length of the less-than "jump" (bit 0: 1 or 2 units),
    // the length of the equals value (bit 1: 1 or 2),
    // and whether the equals value is final (bit 2).
    // Followed by the comparison unit, the equals value and
    // continue reading the next node from there for the "greater" edge.
    static const int32_t kMinThreeWayBranch=0xb00;

    // 0b08..0b1f: Linear-match node, match 1..24 units and continue reading the next node.
    static const int32_t kMinLinearMatch=0xb08;

    // 0b20..ffff: Variable-length value node.
    // If odd, the value is final. (Otherwise, intermediate value or jump delta.)
    // Then shift-right by 1 bit.
    // Remaining <=0x458f is a single-unit value,
    // <=0x7a6e combines with the next unit for almost 30 bits (0..0x34deffff),
    // and 0x7a6f indicates that the value is in the next two units.
    static const int32_t kMinValueLead=0xb20;
    // It is a final value if bit 0 is set.
    static const int32_t kValueIsFinal=1;
    // Compact int: After testing bit 0, shift right by 1 and then use the following thresholds.
    static const int32_t kMinOneUnitLead=0x590;
    static const int32_t kMinTwoUnitLead=0x4590;
    static const int32_t kThreeUnitLead=0x7a6f;

    static const int32_t kMaxOneUnitValue=0x3fff;
    static const int32_t kMaxTwoUnitValue=0x34deffff;

    static const int32_t kMaxListBranchLength=12;
    static const int32_t kMaxLinearMatchLength=kMinValueLead-kMinLinearMatch;  // 24

    // A fixed-length integer has its length indicated by a preceding node value.
    static const int32_t kFixedInt32=1;
    static const int32_t kFixedIntIsFinal=2;

    // Fixed value referencing the UCharTrie words.
    const UChar *uchars;

    // Iterator variables.

    // Pointer to next trie unit to read. NULL if no more matches.
    const UChar *pos;
    // Remaining length of a linear-match node, minus 1. Negative if not in such a node.
    int32_t remainingMatchLength;
    // Value for a match, after contains() returned TRUE.
    int32_t value;
    UBool haveValue;
};

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

void
UCharTrie::skipCompactInt(int32_t leadUnit) {
    leadUnit>>=1;
    if(leadUnit<kMinTwoUnitLead) {
        // pos is already after the leadUnit.
    } else if(leadUnit<kThreeUnitLead) {
        ++pos;
    } else {
        pos+=2;
    }
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
    int32_t node=*pos++;
    if(node>=kMinValueLead) {
        if(node&kValueIsFinal) {
            // No further matching units.
            stop();
            return FALSE;
        } else {
            // Skip intermediate value.
            skipCompactInt(node);
            // The next node must not also be a value node.
            node=*pos++;
            // TODO: U_ASSERT(node<kMinValueLead);
        }
    }
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
            // TODO: U_ASSERT(node<kMinLinearMatch);
        }
        // Branch node with a list of key-value pairs where
        // values are either final values or jump deltas.
        // If the last key unit matches, just continue after it rather
        // than jumping.
        length=(node>>8)+1;  // Actual list length minus 1.
        if(length>=5) {
            // For 6..12 pairs, read the next unit as well.
            node=(node<<16)|*pos++;
        }
        // The lower node bits now contain a bit pair for each (key, value) pair:
        // - whether the value is final
        // - whether the value is 1 or 2 units long
        for(;;) {
            UChar trieUnit=*pos++;
            // U_ASSERT(listLength==0);
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

#endif  // __UCHARTRIE_H__
