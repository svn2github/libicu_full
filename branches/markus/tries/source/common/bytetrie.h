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
            : bytes_(reinterpret_cast<const uint8_t *>(trieBytes)),
              pos_(bytes_), remainingMatchLength_(-1), value_(0), haveValue_(FALSE),
              uniqueValue_(0), haveUniqueValue_(FALSE) {}

    /**
     * Resets this trie to its initial state.
     */
    ByteTrie &reset() {
        pos_=bytes_;
        remainingMatchLength_=-1;
        haveValue_=FALSE;
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
        state.bytes=bytes_;
        state.pos=pos_;
        state.remainingMatchLength=remainingMatchLength_;
        state.value=value_;
        state.haveValue=haveValue_;
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
        if(bytes_==state.bytes && bytes_!=NULL) {
            pos_=state.pos;
            remainingMatchLength_=state.remainingMatchLength;
            value_=state.value;
            haveValue_=state.haveValue;
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
        const uint8_t *pos=pos_;
        return pos!=NULL &&  // more input, and
            (remainingMatchLength_>=0 ||  // more linear-match bytes or
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
    int32_t getValue() const { return value_; }

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
        pos_=NULL;
    }

    int32_t readValue(const uint8_t *pos, int32_t leadByte);
    // Reads a compact 32-bit integer and post-increments pos.
    // pos is already after the leadByte.
    // Returns TRUE if the integer is a final value.
    inline UBool readValueAndFinal(const uint8_t *pos, int32_t leadByte) {
        UBool isFinal=(UBool)(leadByte&kValueIsFinal);
        value_=readValue(pos, leadByte>>1);
        return isFinal;
    }
    // pos is on the leadByte.
    inline UBool readValueAndFinal() {
        const uint8_t *pos=pos_;
        int32_t leadByte=*pos++;
        return readValueAndFinal(pos, leadByte);
    }
    static inline const uint8_t *skipValueAndFinal(const uint8_t *pos, int32_t leadByte) {
        if(leadByte>=(kMinTwoByteValueLead<<1)) {
            if(leadByte<(kMinThreeByteValueLead<<1)) {
                ++pos;
            } else if(leadByte<(kFourByteValueLead<<1)) {
                pos+=2;
            } else {
                pos+=3+((leadByte>>1)&1);
            }
        }
        return pos;
    }
    static inline const uint8_t *skipValueAndFinal(const uint8_t *pos) {
        int32_t leadByte=*pos++;
        return skipValueAndFinal(pos, leadByte);
    }
    inline void skipValueAndFinal(int32_t leadByte) {
        pos_=skipValueAndFinal(pos_, leadByte);
    }
    inline void skipValueAndFinal() {
        pos_=skipValueAndFinal(pos_);
    }

    // Reads a jump delta and post-increments pos.
    int32_t readDelta();

    static inline const uint8_t *skipDelta(const uint8_t *pos) {
        int32_t delta=*pos++;
        if(delta>=kMinTwoByteDeltaLead) {
            if(delta<kMinThreeByteDeltaLead) {
                ++pos;
            } else if(delta<kFourByteDeltaLead) {
                pos+=2;
            } else {
                pos+=3+(delta&1);
            }
        }
        return pos;
    }
    inline void skipDelta() { pos_=skipDelta(pos_); }

    // Handles a branch node for both next(byte) and next(string).
    static const uint8_t *branchNext(const uint8_t *pos, int32_t node, int32_t inByte);

    // Helper functions for hasUniqueValue().
    // Compare the latest value with the previous one, or save the latest one.
    inline UBool isUniqueValue() {
        if(haveUniqueValue_) {
            if(value_!=uniqueValue_) {
                return FALSE;
            }
        } else {
            uniqueValue_=value_;
            haveUniqueValue_=TRUE;
        }
        return TRUE;
    }
    // Recurse into a branch edge and return to the current position.
    inline UBool findUniqueValueAt(int32_t delta) {
        const uint8_t *currentPos=pos_;
        pos_+=delta;
        if(!findUniqueValue()) {
            return FALSE;
        }
        pos_=currentPos;
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
    //    One node bit indicates whether the value is final or whether
    //    matching continues with the next node.
    //  - Linear-match node: Matches a number of bytes.
    //  - Branch node: Branches to other nodes according to the current input byte.
    //    The node byte is the length of the branch (number of bytes to select from)
    //    minus 1. It is followed by a sub-node:
    //    - If the length is at most kMaxBranchLinearSubNodeLength, then
    //      there are length-1 (key, value) pairs and then one more comparison byte.
    //      If one of the key bytes matches, then the value is either a final value for
    //      the string/byte sequence so far, or a "jump" delta to the next node.
    //      If the last byte matches, then matching continues with the next node.
    //      (Values have the same encoding as value nodes.)
    //    - If the length is greater than kMaxBranchLinearSubNodeLength, then
    //      there is one byte and one "jump" delta.
    //      If the input byte is less than the sub-node byte, then "jump" by delta to
    //      the next sub-node which will have a length of length/2.
    //      (The delta has its own compact encoding.)
    //      Otherwise, skip the "jump" delta to the next sub-node
    //      which will have a length of length-length/2.

    // Node lead byte values.

    // 00..0f: Branch node. If node!=0 then the length is node+1, otherwise
    // the length is one more than the next byte.

    // For a branch sub-node with at most this many entries, we drop down
    // to a linear search.
    static const int32_t kMaxBranchLinearSubNodeLength=4;

    // 10..1f: Linear-match node, match 1..16 bytes and continue reading the next node.
    static const int32_t kMinLinearMatch=0x10;
    static const int32_t kMaxLinearMatchLength=0x10;

    // 20..ff: Variable-length value node.
    // If odd, the value is final. (Otherwise, intermediate value or jump delta.)
    // Then shift-right by 1 bit.
    // The remaining lead byte value indicates the number of following bytes (0..4)
    // and contains the value's top bits.
    static const int32_t kMinValueLead=kMinLinearMatch+kMaxLinearMatchLength;  // 0x20
    // It is a final value if bit 0 is set.
    static const int32_t kValueIsFinal=1;

    // Compact value: After testing bit 0, shift right by 1 and then use the following thresholds.
    static const int32_t kMinOneByteValueLead=kMinValueLead/2;  // 0x10
    static const int32_t kMaxOneByteValue=0x40;  // At least 6 bits in the first byte.

    static const int32_t kMinTwoByteValueLead=kMinOneByteValueLead+kMaxOneByteValue+1;  // 0x51
    static const int32_t kMaxTwoByteValue=0x1aff;

    static const int32_t kMinThreeByteValueLead=kMinTwoByteValueLead+(kMaxTwoByteValue>>8)+1;  // 0x6c
    static const int32_t kFourByteValueLead=0x7e;

    // A little more than Unicode code points. (0x11ffff)
    static const int32_t kMaxThreeByteValue=((kFourByteValueLead-kMinThreeByteValueLead)<<16)-1;

    static const int32_t kFiveByteValueLead=0x7f;

    // Compact delta integers.
    static const int32_t kMaxOneByteDelta=0xbf;
    static const int32_t kMinTwoByteDeltaLead=kMaxOneByteDelta+1;  // 0xc0
    static const int32_t kMinThreeByteDeltaLead=0xf0;
    static const int32_t kFourByteDeltaLead=0xfe;
    static const int32_t kFiveByteDeltaLead=0xff;

    static const int32_t kMaxTwoByteDelta=((kMinThreeByteDeltaLead-kMinTwoByteDeltaLead)<<8)-1;  // 0x2fff
    static const int32_t kMaxThreeByteDelta=((kFourByteDeltaLead-kMinThreeByteDeltaLead)<<16)-1;  // 0xdffff

    // Fixed value referencing the ByteTrie bytes.
    const uint8_t *bytes_;

    // Iterator variables.

    // Pointer to next trie byte to read. NULL if no more matches.
    const uint8_t *pos_;
    // Remaining length of a linear-match node, minus 1. Negative if not in such a node.
    int32_t remainingMatchLength_;
    // Value for a match, after hasValue() returned TRUE.
    int32_t value_;
    UBool haveValue_;
    // Unique value, only used in hasUniqueValue().
    int32_t uniqueValue_;
    UBool haveUniqueValue_;
};

U_NAMESPACE_END

#endif  // __BYTETRIE_H__
