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
 * Base class for objects to which Unicode characters and strings can be appended.
 * Combines elements of Java Appendable and ICU4C ByteSink.
 * TODO: Should live in separate files, could be public API.
 */
class U_TOOLUTIL_API Appendable : public UObject {
public:
    /**
     * Appends a 16-bit code unit.
     * @param c code unit
     * @return *this
     */
    virtual Appendable &append(UChar c) = 0;
    /**
     * Appends a code point; has a default implementation.
     * @param c code point
     * @return *this
     */
    virtual Appendable &appendCodePoint(UChar32 c);
    /**
     * Appends a string; has a default implementation.
     * @param s string
     * @param length string length, or -1 if NUL-terminated
     * @return *this
     */
    virtual Appendable &append(const UChar *s, int32_t length);

    // TODO: getAppendBuffer(), see ByteSink
    // TODO: flush() (?) see ByteSink

private:
    // No ICU "poor man's RTTI" for this class nor its subclasses.
    virtual UClassID getDynamicClassID() const;
};

/**
 * Light-weight, non-const reader class for a UCharTrie.
 * Traverses a UChar-serialized data structure with minimal state,
 * for mapping strings (16-bit-unit sequences) to non-negative integer values.
 */
class U_TOOLUTIL_API UCharTrie : public UMemory {
public:
    UCharTrie(const UChar *trieUChars)
            : uchars_(trieUChars),
              pos_(uchars_), remainingMatchLength_(-1), value_(0), haveValue_(FALSE),
              uniqueValue_(0), haveUniqueValue_(FALSE) {}

    /**
     * Resets this trie to its initial state.
     */
    UCharTrie &reset() {
        pos_=uchars_;
        remainingMatchLength_=-1;
        haveValue_=FALSE;
        return *this;
    }

    /**
     * UCharTrie state object, for saving a trie's current state
     * and resetting the trie back to this state later.
     */
    class State : public UMemory {
    public:
        State() { uchars=NULL; }
    private:
        friend class UCharTrie;

        const UChar *uchars;
        const UChar *pos;
        int32_t remainingMatchLength;
        int32_t value;
        UBool haveValue;
    };

    /**
     * Saves the state of this trie.
     * @see resetToState
     */
    const UCharTrie &saveState(State &state) const {
        state.uchars=uchars_;
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
    UCharTrie &resetToState(const State &state) {
        if(uchars_==state.uchars && uchars_!=NULL) {
            pos_=state.pos;
            remainingMatchLength_=state.remainingMatchLength;
            value_=state.value;
            haveValue_=state.haveValue;
        }
        return *this;
    }

    /**
     * Return values for next() and similar methods.
     *
     * The NO_MATCH constant's numeric value is 0, so in C++,
     * if checking for HAS_VALUE is not needed,
     * a Result can be treated like a boolean "matches", as in "if(!trie.next(c)) ..."
     */
    enum Result {
        /**
         * The input unit(s) did not continue a matching string.
         */
        NO_MATCH,
        /**
         * The input unit(s) continued a matching string
         * but there is no value for the string so far.
         * (It is a prefix of a longer string.)
         */
        NO_VALUE,
        /**
         * The input unit(s) continued a matching string
         * and there is a value for the string so far.
         * This value will be returned by getValue().
         */
        HAS_VALUE
    };

    /**
     * Traverses the trie from the initial state for this input UChar.
     * Equivalent to reset().next(uchar).
     * @return The match/value Result.
     */
    inline Result first(int32_t uchar) {
        remainingMatchLength_=-1;
        haveValue_=FALSE;
        return nextImpl(uchars_, uchar);
    }

    /**
     * Traverses the trie from the initial state for the
     * one or two UTF-16 code units for this input code point.
     * Equivalent to reset().nextForCodePoint(cp).
     * @return The match/value Result.
     */
    inline Result firstForCodePoint(UChar32 cp) {
        return cp<=0xffff ?
            first(cp) :
            (first(U16_LEAD(cp))!=NO_MATCH ?
                next(U16_TRAIL(cp)) :
                NO_MATCH);
    }

    /**
     * Tests whether some input UChar can continue a matching string.
     * In other words, this is TRUE when next(u)!=NO_MATCH for some UChar u.
     * @return TRUE if some UChar can continue a matching string.
     */
    inline UBool hasNext() const {
        int32_t node;
        const UChar *pos=pos_;
        return pos!=NULL &&  // more input, and
            (remainingMatchLength_>=0 ||  // more linear-match bytes or
                // the next node is not a final-value node
                (node=*pos)<kMinValueLead || (node&kValueIsFinal)==0);
    }

    /**
     * Traverses the trie from the current state for this input UChar.
     * @return The match/value Result.
     */
    Result next(int32_t uchar);

    /**
     * Traverses the trie from the current state for the
     * one or two UTF-16 code units for this input code point.
     * @return The match/value Result.
     */
    inline Result nextForCodePoint(UChar32 cp) {
        return cp<=0xffff ?
            next(cp) :
            (next(U16_LEAD(cp))!=NO_MATCH ?
                next(U16_TRAIL(cp)) :
                NO_MATCH);
    }

    /**
     * Traverses the trie from the current state for this string.
     * Equivalent to
     * \code
     * Result result;
     * if(length==0)
     *   result=hasValue() ? HAS_VALUE : NO_VALUE;
     * else
     *   for(each c in s)
     *     if((result=next(c))==NO_MATCH) return NO_MATCH;
     * return result;
     * \endcode
     * @return The match/value Result.
     */
    Result next(const UChar *s, int32_t length);

    /**
     * Tests whether the trie contains the string so far.
     * If next() has been called with some input, then hasValue() is TRUE
     * if next() returned HAS_VALUE.
     * If the trie is in the initial state, then hasValue() returns HAS_VALUE or NO_VALUE
     * depending on whether the trie contains the empty string.
     * @return TRUE if the trie contains the string so far.
     */
    inline UBool hasValue() const {
        return haveValue_ || (pos_!=NULL && remainingMatchLength_<0 && *pos_>=kMinValueLead);
    }

    /**
     * Returns a matching string's value if called immediately after
     * next() returned HAS_VALUE or hasValue() returned TRUE.
     * Must not be called otherwise!
     */
    int32_t getValue() {
        if(!haveValue_) {
            readValueAndFinal();
        }
        return value_;
    }

    /**
     * Determines whether all strings reachable from the current state
     * map to the same value.
     * Sets hasValue() to the return value of this function, and if there is
     * a unique value, then a following getValue() will return that unique value.
     *
     * Aside from hasValue()/getValue(),
     * after this function returns the trie will be in the same state as before.
     *
     * @return TRUE if all strings reachable from the current state
     *         map to the same value.
     */
    UBool hasUniqueValue();

    /**
     * Finds each UChar which continues the string from the current state.
     * That is, each UChar c for which next(c) would be TRUE now.
     * After this function returns the trie will be in the same state as before.
     * @param out Each next UChar is appended to this object.
     *            (Only uses the out.append(c) method.)
     * @return the number of UChars which continue the string from here
     */
    int32_t getNextUChars(Appendable &out);

private:
    friend class UCharTrieBuilder;
    friend class UCharTrieIterator;

    inline void stop() {
        pos_=NULL;
    }

    // Reads a compact 32-bit integer and post-increments pos.
    // pos is already after the leadUnit, and the lead unit is already shifted right by 1.
    inline const UChar *readValue(const UChar *pos, int32_t leadUnit) {
        int32_t value;
        if(leadUnit<kMinTwoUnitValueLead) {
            value=leadUnit-kMinOneUnitValueLead;
        } else if(leadUnit<kThreeUnitValueLead) {
            value=((leadUnit-kMinTwoUnitValueLead)<<16)|*pos++;
        } else {
            value=(pos[0]<<16)|pos[1];
            pos+=2;
        }
        value_=value;
        return pos;
    }
    // Reads a compact 32-bit integer and post-increments pos.
    // pos is already after the leadUnit.
    inline void readValueAndFinal(const UChar *pos, int32_t leadUnit) {
        U_ASSERT(leadUnit>=kMinValueLead);
        UBool isFinal=(UBool)(leadUnit&kValueIsFinal);
        pos=readValue(pos, leadUnit>>1);
        if(isFinal) {
            stop();
        } else {
            // The next node must not also be a value node.
            U_ASSERT(*pos<kMinValueLead);
            pos_=pos;
        }
        haveValue_=TRUE;
    }
    inline void readValueAndFinal() {
        const UChar *pos=pos_;
        int32_t leadUnit=*pos++;
        readValueAndFinal(pos, leadUnit);
    }
    static inline const UChar *skipValue(const UChar *pos, int32_t leadUnit) {
        U_ASSERT(leadUnit>=kMinValueLead);
        if(leadUnit>=(kMinTwoUnitValueLead<<1)) {
            if(leadUnit<(kThreeUnitValueLead<<1)) {
                ++pos;
            } else {
                pos+=2;
            }
        }
        return pos;
    }
    static inline const UChar *skipValue(const UChar *pos) {
        int32_t leadUnit=*pos++;
        return skipValue(pos, leadUnit);
    }

/*
    inline int32_t readDelta() {
        const UChar *pos=pos_;
        int32_t delta=*pos++;
        if(delta>=kMinTwoUnitDeltaLead) {
            if(delta==kThreeUnitDeltaLead) {
                delta=(pos[0]<<16)|pos[1];
                pos+=2;
            } else {
                delta=((delta-kMinTwoUnitDeltaLead)<<16)|*pos++;
            }
        }
        pos_=pos;
        return delta;
    }
*/
    static const UChar *skipDelta(const UChar *pos) {
        int32_t delta=*pos++;
        if(delta>=kMinTwoUnitDeltaLead) {
            if(delta==kThreeUnitDeltaLead) {
                pos+=2;
            } else {
                ++pos;
            }
        }
        return pos;
    }

    // Handles a branch node for both next(uchar) and next(string).
    Result branchNext(const UChar *pos, int32_t length, int32_t uchar);

    // Requires remainingLength_<0.
    Result nextImpl(const UChar *pos, int32_t uchar);

    // Helper functions for hasUniqueValue().
    // Compares the latest value with the previous one, or saves the latest one.
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
    // Recurses into a branch edge and returns to the current position.
    inline UBool findUniqueValueAt(int32_t delta) {
        const UChar *currentPos=pos_;
        pos_+=delta;
        if(!findUniqueValue()) {
            return FALSE;
        }
        pos_=currentPos;
        return TRUE;
    }
    // Handles a branch node entry (final value or jump delta).
    UBool findUniqueValueFromBranchEntry(int32_t node);
    // Recursively finds a unique value (or whether there is not a unique one)
    // starting from a position on a node lead unit.
    UBool findUniqueValue();

    // Helper functions for getNextUChars().
    // getNextUChars() when pos is on a branch node.
    int32_t getNextBranchUChars(Appendable &out);

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
    //    The node unit is the length of the branch (number of units to select from)
    //    minus 1. It is followed by a sub-node:
    //    - If the length is at most kMaxBranchLinearSubNodeLength, then
    //      there are length-1 (key, value) pairs and then one more comparison unit.
    //      If one of the key units matches, then the value is either a final value for
    //      the string so far, or a "jump" delta to the next node.
    //      If the last unit matches, then matching continues with the next node.
    //      (Values have the same encoding as value nodes.)
    //    - If the length is greater than kMaxBranchLinearSubNodeLength, then
    //      there is one unit and one "jump" delta.
    //      If the input unit is less than the sub-node unit, then "jump" by delta to
    //      the next sub-node which will have a length of length/2.
    //      (The delta has its own compact encoding.)
    //      Otherwise, skip the "jump" delta to the next sub-node
    //      which will have a length of length-length/2.

    // Node lead unit values.

    // 0000..00ff: Branch node. If node!=0 then the length is node+1, otherwise
    // the length is one more than the next unit.

    // For a branch sub-node with at most this many entries, we drop down
    // to a linear search.
    static const int32_t kMaxBranchLinearSubNodeLength=4;

    // 0100..01ff: Linear-match node, match 1..256 units and continue reading the next node.
    static const int32_t kMinLinearMatch=0x100;
    static const int32_t kMaxLinearMatchLength=0x100;

    // 0200..ffff: Variable-length value node.
    // If odd, the value is final. (Otherwise, intermediate value or jump delta.)
    // Then shift-right by 1 bit.
    // The remaining lead unit value indicates the number of following units (0..2)
    // and contains the value's top bits.
    static const int32_t kMinValueLead=kMinLinearMatch+kMaxLinearMatchLength;  // 0x0200
    // It is a final value if bit 0 is set.
    static const int32_t kValueIsFinal=1;

    // Compact value: After testing bit 0, shift right by 1 and then use the following thresholds.
    static const int32_t kMinOneUnitValueLead=kMinValueLead/2;  // 0x0100
    static const int32_t kMaxOneUnitValue=0x5fff;

    static const int32_t kMinTwoUnitValueLead=kMinOneUnitValueLead+kMaxOneUnitValue+1;  // 0x6100
    static const int32_t kThreeUnitValueLead=0x7fff;

    static const int32_t kMaxTwoUnitValue=((kThreeUnitValueLead-kMinTwoUnitValueLead)<<16)-1;  // 0x1efeffff

    // Compact delta integers.
    static const int32_t kMaxOneUnitDelta=0xfbff;
    static const int32_t kMinTwoUnitDeltaLead=kMaxOneUnitDelta+1;  // 0xfc00
    static const int32_t kThreeUnitDeltaLead=0xffff;

    static const int32_t kMaxTwoUnitDelta=((kThreeUnitDeltaLead-kMinTwoUnitDeltaLead)<<16)-1;  // 0x03feffff

    // Fixed value referencing the UCharTrie words.
    const UChar *uchars_;

    // Iterator variables.

    // Pointer to next trie unit to read. NULL if no more matches.
    const UChar *pos_;
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

#endif  // __UCHARTRIE_H__
