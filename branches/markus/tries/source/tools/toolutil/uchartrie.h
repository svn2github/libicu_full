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
     * Tests whether some input UChar can continue a matching string.
     * In other words, this is TRUE when next(u) for some UChar would return TRUE.
     * @return TRUE if some UChar can continue a matching string.
     */
    UBool hasNext() const {
        int32_t node;
        const UChar *pos=pos_;
        return pos!=NULL &&  // more input, and
            (remainingMatchLength_>=0 ||  // more linear-match bytes or
                // the next node is not a final-value node
                (node=*pos)<kMinValueLead || (node&kValueIsFinal)==0);
    }

    /**
     * Traverses the trie from the current state for this input UChar.
     * @return TRUE if the UChar continues a matching string.
     */
    UBool next(int32_t uchar);

    /**
     * Traverses the trie from the current state for the
     * one or two UTF-16 code units for this input code point.
     * @return TRUE if the code point continues a matching string.
     */
    UBool nextForCodePoint(UChar32 cp) {
        return cp<=0xffff ? next(cp) : next(U16_LEAD(cp)) && next(U16_TRAIL(cp));
    }

    /**
     * Traverses the trie from the current state for this string.
     * Equivalent to
     * \code
     * for(each c in s)
     *   if(!next(c)) return FALSE;
     * return TRUE;
     * \endcode
     * @return TRUE if the string is empty, or if it continues a matching string.
     */
    UBool next(const UChar *s, int32_t length);

    /**
     * @return TRUE if the trie contains the string so far.
     *         In this case, an immediately following call to getValue()
     *         returns the string's value.
     *         hasValue() is only defined if called from the initial state
     *         or immediately after next() returns TRUE.
     */
    UBool hasValue();

    /**
     * Returns a string's value if called immediately after hasValue()
     * returned TRUE. Otherwise undefined.
     */
    int32_t getValue() const { return value_; }

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

    // lead unit already shifted right by 1.
    inline int32_t readValue(const UChar *pos, int32_t leadUnit) {
        int32_t value;
        if(leadUnit<kMinTwoUnitValueLead) {
            value=leadUnit-kMinOneUnitValueLead;
        } else if(leadUnit<kThreeUnitValueLead) {
            value=((leadUnit-kMinTwoUnitValueLead)<<16)|*pos++;
        } else {
            value=(pos[0]<<16)|pos[1];
            pos+=2;
        }
        pos_=pos;
        return value;
    }
    // Reads a compact 32-bit integer and post-increments pos.
    // pos is already after the leadUnit.
    // Returns TRUE if the integer is a final value.
    inline UBool readValueAndFinal(const UChar *pos, int32_t leadUnit) {
        UBool isFinal=(UBool)(leadUnit&kValueIsFinal);
        value_=readValue(pos, leadUnit>>1);
        return isFinal;
    }
    inline UBool readValueAndFinal() {
        const UChar *pos=pos_;
        int32_t leadUnit=*pos++;
        return readValueAndFinal(pos, leadUnit);
    }
    static inline const UChar *skipValueAndFinal(const UChar *pos, int32_t leadUnit) {
        if(leadUnit>=(kMinTwoUnitValueLead<<1)) {
            if(leadUnit<(kThreeUnitValueLead<<1)) {
                ++pos;
            } else {
                pos+=2;
            }
        }
        return pos;
    }
    static inline const UChar *skipValueAndFinal(const UChar *pos) {
        int32_t leadUnit=*pos++;
        return skipValueAndFinal(pos, leadUnit);
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
    static const UChar *branchNext(const UChar *pos, int32_t node, int32_t uchar);

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
        const UChar *currentPos=pos_;
        pos_+=delta;
        if(!findUniqueValue()) {
            return FALSE;
        }
        pos_=currentPos;
        return TRUE;
    }
    // Handle a branch node entry (final value or jump delta).
    UBool findUniqueValueFromBranchEntry(int32_t node);
    // Recursively find a unique value (or whether there is not a unique one)
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
    //    TODO

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
