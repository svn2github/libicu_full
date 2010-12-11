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

class ByteSink;
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
              uniqueValue(0), haveUniqueValue(FALSE) {}

    /**
     * Resets this trie to its initial state.
     */
    ByteTrie &reset() {
        pos=bytes;
        remainingMatchLength=-1;
        haveValue=FALSE;
        return *this;
    }

    /**
     * ByteTrie state object, for saving a trie's current state
     * and resetting the trie back to this state later.
     */
    class State : public UMemory {
    public:
        State() { bytes=NULL; }
    private:
        friend class ByteTrie;

        const uint8_t *bytes;
        const uint8_t *pos;
        int32_t remainingMatchLength;
        int32_t value;
        UBool haveValue;
    };

    /**
     * Saves the state of this trie.
     * @see resetToState
     */
    const ByteTrie &saveState(State &state) const {
        state.bytes=bytes;
        state.pos=pos;
        state.remainingMatchLength=remainingMatchLength;
        state.value=value;
        state.haveValue=haveValue;
        return *this;
    }

    /**
     * Resets this trie to the saved state.
     * If the state object contains no state, or the state of a different trie,
     * then this trie remains unchanged.
     * @see saveState
     * @see reset
     */
    ByteTrie &resetToState(const State &state) {
        if(bytes==state.bytes && bytes!=NULL) {
            pos=state.pos;
            remainingMatchLength=state.remainingMatchLength;
            value=state.value;
            haveValue=state.haveValue;
        }
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
    UBool next(int32_t inByte);

    /**
     * Traverses the trie from the current state for this byte sequence.
     * Equivalent to
     * \code
     * for(each b in s)
     *   if(!next(b)) return FALSE;
     * return TRUE;
     * \endcode
     * @return TRUE if the byte sequence is empty, or if it continues a matching byte sequence.
     */
    UBool next(const char *s, int32_t length);

    /**
     * @return TRUE if the trie contains the byte sequence so far.
     *         In this case, an immediately following call to getValue()
     *         returns the byte sequence's value.
     *         hasValue() is only defined if called from the initial state
     *         or immediately after next() returns TRUE.
     */
    UBool hasValue();

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

    /**
     * Finds each byte which continues the byte sequence from the current state.
     * That is, each byte b for which next(b) would be TRUE now.
     * After this function returns the trie will be in the same state as before.
     * @param out Each next byte is appended to this object.
     *            (Only uses the out.Append(s, length) method.)
     * @return the number of bytes which continue the byte sequence from here
     */
    int32_t getNextBytes(ByteSink &out);

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

    // Handles a branch node for both next(byte) and next(string).
    UBool branchNext(int32_t node, int32_t inByte);

    // Helper functions for hasUniqueValue().
    // Compare the latest value with the previous one, or save the latest one.
    inline UBool isUniqueValue() {
        if(haveUniqueValue) {
            if(value!=uniqueValue) {
                return FALSE;
            }
        } else {
            uniqueValue=value;
            haveUniqueValue=TRUE;
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

    // Helper functions for getNextBytes().
    // getNextBytes() when pos is on a branch node.
    int32_t getNextBranchBytes(ByteSink &out);
    void append(ByteSink &out, int c);

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
    //    - Split-branch node: Compares the input byte with one included byte.
    //        If less-than, "jumps" to another node which is a branch node.
    //        Otherwise, matching continues with the next node which is a branch node.

    // Node lead byte values.

    // 00..07: Branch node with a list of 2..9 comparison bytes.
    // Followed by the (key, value) pairs except that the last byte's value is omitted
    // (just continue reading the next node from there).
    // Values are compact ints: Final values or jump deltas.
    static const int32_t kMaxListBranchLength=9;

    // 08..0b: Split-branch node with less/greater-or-equal outbound edges.
    // The 2 lower bits indicate the length of the less-than "jump" (1..4 bytes).
    // Followed by the comparison byte, and
    // continue reading the next node from there for the "greater-or-equal" edge.
    static const int32_t kMinSplitBranch=kMaxListBranchLength-1;  // 8

    // 0c..1f: Linear-match node, match 1..24 bytes and continue reading the next node.
    static const int32_t kMinLinearMatch=kMinSplitBranch+4;  // 0xc
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
    // Unique value, only used in hasUniqueValue().
    int32_t uniqueValue;
    UBool haveUniqueValue;
};

U_NAMESPACE_END

#endif  // __BYTETRIE_H__
