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
#include "uassert.h"

U_NAMESPACE_BEGIN

class UCharTrieBuilder;
class UCharTrieIterator;

/**
 * Light-weight, non-const reader class for a UCharTrie.
 * Traverses a UChar-serialized data structure with minimal state,
 * for mapping strings (16-bit-unit sequences) to non-negative integer values.
 */
class U_TOOLUTIL_API UCharTrie : public UMemory {
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
     *         hasValue() is only defined if called from the initial state
     *         or once immediately after next() returns TRUE.
     */
    UBool hasValue();

    /**
     * Traverses the trie from the current state for this string,
     * calls next(u) for each UChar u in the sequence,
     * and calls hasValue() at the end.
     */
    UBool hasValue(const UChar *s, int32_t length);

    /**
     * Returns a string's value if called immediately after hasValue()
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
    UBool readCompactInt(int32_t leadUnit);

    // pos is already after the leadUnit.
    void skipCompactInt(int32_t leadUnit);

    // Reads a fixed-width integer and post-increments pos.
    int32_t readFixedInt(int32_t node);

    // UCharTrie data structure
    //
    // The trie consists of a series of UChar-serialized nodes for incremental
    // Unicode string/UChar sequence matching. (UChar=16-bit unsigned integer)
    // The root node is at the beginning of the trie data.
    //
    // Types of nodes are distinguished by their node lead unit ranges.
    // After each node, except a final-value node, another node follows to
    // encode match values or continue matching further units.
    //
    // Node types:
    //  - Value node: Stores a 32-bit integer in a compact, variable-length format.
    //    The value is for the string/UChar sequence so far.
    //  - Linear-match node: Matches a number of units.
    //  - Branch node: Branches to other nodes according to the current input unit.
    //    - List-branch node: If the input unit is in the list, a "jump"
    //        leads to another node for further matching.
    //        Instead of a jump, a final value may be stored.
    //        For the last unit listed there is no "jump" or value directly in
    //        the branch node: Instead, matching continues with the next node.
    //    - Three-way-branch node: Compares the input unit with one included unit.
    //        If less-than, "jumps" to another node which is a branch node.
    //        If equals, "jumps" to another node (any type) or stores a final value.
    //        If greater-than, matching continues with the next node which is a branch node.

    // Node lead unit values.

    // 0..33ff: Branch node with a list of 2..14 comparison UChars.
    // Bits 13..10=0..12 for 2..14 units to match.
    // The lower bits, and if 7..14 units then also the bits from the next unit,
    // indicate whether each key unit has a final value vs. a "jump" (higher bit of a pair),
    // and whether the match unit's value is 1 or 2 units long.
    // Followed by the (key, value) pairs except that the last unit's value is omitted
    // (just continue reading the next node from there).
    // Thus, for the last key unit there are no (final, length) value bits.

    // The node lead unit has kMaxListBranchSmallLength-1 bit pairs.
    static const int32_t kMaxListBranchSmallLength=6;
    static const int32_t kMaxListBranchLengthShift=(kMaxListBranchSmallLength-1)*2;  // 10
    // 8 more bit pairs in the next unit, for branch length > kMaxListBranchSmallLength.
    static const int32_t kMaxListBranchLength=kMaxListBranchSmallLength+8;  // 14

    // 3400..3407: Three-way-branch node with less/equal/greater outbound edges.
    // The 3 lower bits indicate the length of the less-than "jump" (bit 0: 1 or 2 units),
    // the length of the equals value (bit 1: 1 or 2),
    // and whether the equals value is final (bit 2).
    // Followed by the comparison unit, the equals value and
    // continue reading the next node from there for the "greater" edge.
    static const int32_t kMinThreeWayBranch=
        (kMaxListBranchLength-1)<<kMaxListBranchLengthShift;  // 0x3400

    // 3408..341f: Linear-match node, match 1..24 units and continue reading the next node.
    static const int32_t kMinLinearMatch=kMinThreeWayBranch+8;  // 0x3408
    static const int32_t kMaxLinearMatchLength=24;

    // 3420..ffff: Variable-length value node.
    // If odd, the value is final. (Otherwise, intermediate value or jump delta.)
    // Then shift-right by 1 bit.
    // The remaining lead unit value indicates the number of following units (0..2)
    // and contains the value's top bits.
    static const int32_t kMinValueLead=kMinLinearMatch+kMaxLinearMatchLength;  // 0x3420
    // It is a final value if bit 0 is set.
    static const int32_t kValueIsFinal=1;

    // Compact int: After testing bit 0, shift right by 1 and then use the following thresholds.
    static const int32_t kMinOneUnitLead=kMinValueLead/2;  // 0x1a10
    static const int32_t kMaxOneUnitValue=0x3fff;

    static const int32_t kMinTwoUnitLead=kMinOneUnitLead+kMaxOneUnitValue+1;  // 0x5a10
    static const int32_t kThreeUnitLead=0x7fff;

    static const int32_t kMaxTwoUnitValue=((kThreeUnitLead-kMinTwoUnitLead)<<16)-1;  // 0x25eeffff

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
    // Value for a match, after hasValue() returned TRUE.
    int32_t value;
    UBool haveValue;
};

U_NAMESPACE_END

#endif  // __UCHARTRIE_H__
