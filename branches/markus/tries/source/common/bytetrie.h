/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetrie.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010sep25
*   created by: Markus W. Scherer
*/

#ifndef __BYTETRIE_H__
#define __BYTETRIE_H__

/**
 * \file
 * \brief C++ API: Dictionary trie for mapping arbitrary byte sequences
 *                 to integer values.
 */

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

class ByteTrieBuilder;
class ByteTrieIterator;

/**
 * Light-weight, non-const reader class for a ByteTrie.
 * Traverses a byte-serialized data structure with minimal state,
 * for mapping byte sequences to non-negative integer values.
 */
class U_COMMON_API ByteTrie : public UMemory {
public:
    ByteTrie(const void *trieBytes)
            : bytes(reinterpret_cast<const uint8_t *>(trieBytes)),
              pos(bytes), remainingMatchLength(-1), value(0), haveValue(FALSE),
              markedPos(pos), markedRemainingMatchLength(remainingMatchLength),
              markedValue(0), markedHaveValue(FALSE) {}

    /**
     * Resets this trie (and its marked state) to its initial state.
     */
    ByteTrie &reset() {
        pos=markedPos=bytes;
        remainingMatchLength=markedRemainingMatchLength=-1;
        haveValue=markedHaveValue=FALSE;
        return *this;
    }

    /**
     * Marks the state of this trie.
     * @see resetToMark
     */
    ByteTrie &mark() {
        markedPos=pos;
        markedRemainingMatchLength=remainingMatchLength;
        markedValue=value;
        markedHaveValue=haveValue;
        return *this;
    }

    /**
     * Resets this trie to the state at the time mark() was last called.
     * If mark() has not been called since the last reset()
     * then this is equivalent to reset() itself.
     * @see mark
     * @see reset
     */
    ByteTrie &resetToMark() {
        pos=markedPos;
        remainingMatchLength=markedRemainingMatchLength;
        value=markedValue;
        haveValue=markedHaveValue;
        return *this;
    }

    /**
     * Tests whether some input byte can continue a matching byte sequence.
     * In other words, this is TRUE when next(b) for some byte would return TRUE.
     * @return TRUE if some byte can continue a matching byte sequence.
     */
    UBool hasNext() const {
        int32_t node;
        return pos!=NULL &&  // more input, and
            (remainingMatchLength>=0 ||  // more linear-match bytes or
                // the next node is not a final-value node
                (node=*pos)<kMinValueLead || (node&kValueIsFinal)==0);
    }

    /**
     * Traverses the trie from the current state for this input byte.
     * @return TRUE if the byte continues a matching byte sequence.
     */
    UBool next(int inByte);

    /**
     * @return TRUE if the trie contains the byte sequence so far.
     *         In this case, an immediately following call to getValue()
     *         returns the byte sequence's value.
     *         hasValue() is only defined if called from the initial state
     *         or immediately after next() returns TRUE.
     */
    UBool hasValue();

    /**
     * Traverses the trie from the current state for this byte sequence,
     * calls next(b) for each byte b in the sequence,
     * and calls hasValue() at the end.
     */
    UBool hasValue(const char *s, int32_t length);

    /**
     * Returns a byte sequence's value if called immediately after hasValue()
     * returned TRUE. Otherwise undefined.
     */
    int32_t getValue() const { return value; }

    /**
     * Determines whether all byte sequences reachable from the current state
     * map to the same value.
     * Sets hasValue() to the return value of this function, and if there is
     * a unique value, then a following getValue() will return that unique value.
     *
     * Aside from hasValue()/getValue(),
     * after this function returns the trie will be in the same state as before.
     *
     * @return TRUE if all byte sequences reachable from the current state
     *         map to the same value.
     */
    UBool hasUniqueValue();

    // TODO: For startsWith() functionality, add
    //   UBool getRemainder(ByteSink *remainingBytes, &value);
    // Returns TRUE if exactly one byte sequence can be reached from the current iterator state.
    // The remainingBytes sink will receive the remaining bytes of that one sequence.
    // It might receive some bytes even when the function returns FALSE.

private:
    friend class ByteTrieBuilder;
    friend class ByteTrieIterator;

    inline void stop() {
        pos=NULL;
    }

    // Reads a compact 32-bit integer and post-increments pos.
    // pos is already after the leadByte.
    // Returns TRUE if the integer is a final value.
    UBool readCompactInt(int32_t leadByte);
    // pos is on the leadByte.
    inline UBool readCompactInt() {
        int32_t leadByte=*pos++;
        return readCompactInt(leadByte);
    }

    // Reads a fixed-width integer and post-increments pos.
    int32_t readFixedInt(int32_t bytesPerValue);

    // Helper functions for hasUniqueValue().
    // Compare the latest value with the previous one, or save the latest one.
    inline UBool isUniqueValue() {
        if(markedHaveValue) {
            if(value!=markedValue) {
                return FALSE;
            }
        } else {
            markedValue=value;
            markedHaveValue=TRUE;
        }
        return TRUE;
    }
    // Recurse into a branch edge and return to the current position.
    inline UBool findUniqueValueAt(int32_t delta) {
        const uint8_t *currentPos=pos;
        pos+=delta;
        if(!findUniqueValue()) {
            return FALSE;
        }
        pos=currentPos;
        return TRUE;
    }
    // Handle a branch node entry (final value or jump delta).
    UBool findUniqueValueFromBranchEntry();
    // Recursively find a unique value (or whether there is not a unique one)
    // starting from a position on a node lead unit.
    UBool findUniqueValue();

    // ByteTrie data structure
    //
    // The trie consists of a series of byte-serialized nodes for incremental
    // string/byte sequence matching. The root node is at the beginning of the trie data.
    //
    // Types of nodes are distinguished by their node lead byte ranges.
    // After each node, except a final-value node, another node follows to
    // encode match values or continue matching further bytes.
    //
    // Node types:
    //  - Value node: Stores a 32-bit integer in a compact, variable-length format.
    //    The value is for the string/byte sequence so far.
    //  - Linear-match node: Matches a number of bytes.
    //  - Branch node: Branches to other nodes according to the current input byte.
    //    - List-branch node: If the input byte is in the list, a "jump"
    //        leads to another node for further matching.
    //        Instead of a jump, a final value may be stored.
    //        For the last byte listed there is no "jump" or value directly in
    //        the branch node: Instead, matching continues with the next node.
    //    - Three-way-branch node: Compares the input byte with one included byte.
    //        If less-than, "jumps" to another node which is a branch node.
    //        If equals, "jumps" to another node (any type) or stores a final value.
    //        If greater-than, matching continues with the next node which is a branch node.

    // Node lead byte values.

    // 0..3: Three-way-branch node with less/equal/greater outbound edges.
    // The 2 lower bits indicate the length of the less-than "jump" (1..4 bytes).
    // Followed by the comparison byte, the equals value (compact int) and
    // continue reading the next node from there for the "greater" edge.

    // 04..0b: Branch node with a list of 2..9 comparison bytes.
    // Followed by the (key, value) pairs except that the last byte's value is omitted
    // (just continue reading the next node from there).
    // Values are compact ints: Final values or jump deltas.
    static const int32_t kMinListBranch=4;
    static const int32_t kMaxListBranchLength=9;

    // 0c..1f: Linear-match node, match 1..24 bytes and continue reading the next node.
    static const int32_t kMinLinearMatch=kMinListBranch+kMaxListBranchLength-1;  // 0xc
    static const int32_t kMaxLinearMatchLength=20;

    // 20..ff: Variable-length value node.
    // If odd, the value is final. (Otherwise, intermediate value or jump delta.)
    // Then shift-right by 1 bit.
    // The remaining lead byte value indicates the number of following bytes (0..4)
    // and contains the value's top bits.
    static const int32_t kMinValueLead=kMinLinearMatch+kMaxLinearMatchLength;  // 0x20
    // It is a final value if bit 0 is set.
    static const int32_t kValueIsFinal=1;

    // Compact int: After testing bit 0, shift right by 1 and then use the following thresholds.
    static const int32_t kMinOneByteLead=kMinValueLead/2;  // 0x10
    static const int32_t kMaxOneByteValue=0x40;  // At least 6 bits in the first byte.

    static const int32_t kMinTwoByteLead=kMinOneByteLead+kMaxOneByteValue+1;  // 0x51
    static const int32_t kMaxTwoByteValue=0x1aff;

    static const int32_t kMinThreeByteLead=kMinTwoByteLead+(kMaxTwoByteValue>>8)+1;  // 0x6c
    static const int32_t kFourByteLead=0x7e;

    // A little more than Unicode code points.
    static const int32_t kMaxThreeByteValue=((kFourByteLead-kMinThreeByteLead)<<16)-1;  // 0x11ffff;

    static const int32_t kFiveByteLead=0x7f;

    // Map a shifted-right compact-int lead byte to its number of bytes.
    static const int8_t bytesPerLead[kFiveByteLead+1];

    // Fixed value referencing the ByteTrie bytes.
    const uint8_t *bytes;

    // Iterator variables.

    // Pointer to next trie byte to read. NULL if no more matches.
    const uint8_t *pos;
    // Remaining length of a linear-match node, minus 1. Negative if not in such a node.
    int32_t remainingMatchLength;
    // Value for a match, after hasValue() returned TRUE.
    int32_t value;
    UBool haveValue;

    // mark() and resetToMark() variables
    const uint8_t *markedPos;
    int32_t markedRemainingMatchLength;
    int32_t markedValue;
    UBool markedHaveValue;
    // Note: If it turns out that constructor and reset() are too slow because
    // of the extra mark() variables, then we could move them out into
    // a separate state object which is passed into mark() and resetToMark().
    // Usage of those functions would be a little more clunky,
    // especially in Java where the state object would have to be heap-allocated.
};

U_NAMESPACE_END

#endif  // __BYTETRIE_H__
