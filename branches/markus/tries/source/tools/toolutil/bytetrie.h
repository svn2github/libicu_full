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
class /*U_COMMON_API*/ ByteTrie : public UMemory {
public:
    ByteTrie(const void *trieBytes)
            : bytes(reinterpret_cast<const uint8_t *>(trieBytes)),
              pos(bytes), remainingMatchLength(-1), value(0) {}

    ByteTrie &reset() {
        pos=bytes;
        remainingMatchLength=-1;
        return *this;
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
     */
    UBool contains();

    /**
     * Traverses the trie from the current state for this byte sequence,
     * calls next(b) for each byte b in the sequence,
     * and calls contains() at the end.
     */
    UBool containsNext(const char *s, int32_t length);

    /**
     * Returns a byte sequence's value if called immediately after contains()
     * returned TRUE. Otherwise undefined.
     */
    int32_t getValue() const { return value; }

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
    inline UBool readCompactInt(int32_t leadByte);
    // pos is on the leadByte.
    inline UBool readCompactInt() {
        int32_t leadByte=*pos++;
        return readCompactInt(leadByte);
    }

    // Reads a fixed-width integer and post-increments pos.
    inline int32_t readFixedInt(int32_t bytesPerValue);

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
    // Value for a match, after contains() returned TRUE.
    int32_t value;
};

const int8_t ByteTrie::bytesPerLead[kFiveByteLead+1]={
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5
};

UBool
ByteTrie::readCompactInt(int32_t leadByte) {
    UBool isFinal=(UBool)(leadByte&kValueIsFinal);
    leadByte>>=1;
    int numBytes=bytesPerLead[leadByte]-1;  // -1: lead byte was already consumed.
    switch(numBytes) {
    case 0:
        value=leadByte-kMinOneByteLead;
        break;
    case 1:
        value=((leadByte-kMinTwoByteLead)<<8)|*pos;
        break;
    case 2:
        value=((leadByte-kMinThreeByteLead)<<16)|(pos[0]<<8)|pos[1];
        break;
    case 3:
        value=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        break;
    case 4:
        value=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        break;
    }
    pos+=numBytes;
    return isFinal;
}

int32_t
ByteTrie::readFixedInt(int32_t bytesPerValue) {
    int32_t fixedInt;
    switch(bytesPerValue) {  // Actually number of bytes minus 1.
    case 0:
        fixedInt=*pos;
        break;
    case 1:
        fixedInt=(pos[0]<<8)|pos[1];
        break;
    case 2:
        fixedInt=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        break;
    case 3:
        fixedInt=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        break;
    }
    pos+=bytesPerValue+1;
    return fixedInt;
}

UBool
ByteTrie::next(int inByte) {
    if(pos==NULL) {
        return FALSE;
    }
    int32_t length=remainingMatchLength;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Remaining part of a linear-match node.
        if(inByte==*pos) {
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
            // No further matching bytes.
            stop();
            return FALSE;
        } else {
            // Skip intermediate value.
            pos+=bytesPerLead[node>>1];
            // The next node must not also be a value node.
            node=*pos;
            U_ASSERT(node<kMinValueLead);
        }
    }
    ++pos;
    if(node<kMinLinearMatch) {
        // Branch according to the current byte.
        while(node<kMinListBranch) {
            // Branching on a byte value,
            // with a jump delta for less-than, a compact int for equals,
            // and continuing for greater-than.
            // The less-than and greater-than branches must lead to branch nodes again.
            uint8_t trieByte=*pos++;
            if(inByte<trieByte) {
                int32_t delta=readFixedInt(node);
                pos+=delta;
            } else {
                pos+=node+1;  // Skip fixed-width integer.
                node=*pos;
                U_ASSERT(node>=kMinValueLead);
                if(inByte==trieByte) {
                    if(node&kValueIsFinal) {
                        // Leave the final value for contains() to read.
                    } else {
                        // Use the non-final value as the jump delta.
                        ++pos;
                        readCompactInt(node);
                        pos+=value;
                    }
                    return TRUE;
                } else {  // inByte>trieByte
                    pos+=bytesPerLead[node>>1];
                }
            }
            node=*pos++;
            U_ASSERT(node<kMinLinearMatch);
        }
        // Branch node with a list of key-value pairs where
        // values are compact integers: either final values or jump deltas.
        // If the last key byte matches, just continue after it rather
        // than jumping.
        length=node-(kMinListBranch-1);  // Actual list length minus 1.
        for(;;) {
            uint8_t trieByte=*pos++;
            U_ASSERT(length==0 || *pos>=kMinValueLead);
            if(inByte==trieByte) {
                if(length>0) {
                    node=*pos;
                    if(node&kValueIsFinal) {
                        // Leave the final value for contains() to read.
                    } else {
                        // Use the non-final value as the jump delta.
                        ++pos;
                        readCompactInt(node);
                        pos+=value;
                    }
                }
                return TRUE;
            }
            if(inByte<trieByte || length--==0) {
                stop();
                return FALSE;
            }
            pos+=bytesPerLead[*pos>>1];
        }
    } else {
        // Match the first of length+1 bytes.
        length=node-kMinLinearMatch;  // Actual match length minus 1.
        if(inByte==*pos) {
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
ByteTrie::contains() {
    int32_t node;
    if(pos!=NULL && remainingMatchLength<0 && (node=*pos)>=kMinValueLead) {
        // Deliver value for the matching bytes.
        ++pos;
        if(readCompactInt(node)) {
            stop();
        }
        return TRUE;
    }
    return FALSE;
}

UBool
ByteTrie::containsNext(const char *s, int32_t length) {
    if(length<0) {
        // NUL-terminated
        int b;
        while((b=(uint8_t)*s++)!=0) {
            if(!next(b)) {
                return FALSE;
            }
        }
    } else {
        while(length>0) {
            if(!next((uint8_t)*s++)) {
                return FALSE;
            }
            --length;
        }
    }
    return contains();
}

U_NAMESPACE_END

#endif  // __BYTETRIE_H__
