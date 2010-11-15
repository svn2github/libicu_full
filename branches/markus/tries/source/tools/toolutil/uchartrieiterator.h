/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uchartrieiterator.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov15
*   created by: Markus W. Scherer
*/

#ifndef __UCHARTRIEITERATOR_H__
#define __UCHARTRIEITERATOR_H__

/**
 * \file
 * \brief C++ API: UCharTrie iterator for all of its (string, value) pairs.
 */

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "uchartrie.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

/**
 * Iterator for all of the (string, value) pairs in a a UCharTrie.
 */
class /*U_TOOLUTIL_API*/ UCharTrieIterator : public UMemory {
public:
    UCharTrieIterator(const void *trieUChars, UErrorCode &errorCode)
            : trie(reinterpret_cast<const UChar *>(trieUChars)), value(0), stack(errorCode) {}

    /**
     * Finds the next (string, value) pair if there is one.
     * @return TRUE if there is another element.
     */
    UBool next(UErrorCode &errorCode);

    /**
     * @return TRUE if there are more elements.
     */
    UBool hasNext() const { return trie.pos!=NULL || !stack.isEmpty(); }

    /**
     * @return the NUL-terminated string for the last successful next()
     */
    const UnicodeString &getString() const { return str; }
    /**
     * @return the value for the last successful next()
     */
    const int32_t getValue() const { return value; }

private:
    // The stack stores pairs of integers for backtracking to another
    // outbound edge of a branch node.
    // The first integer is an offset from UCharTrie.uchars.
    // The second integer has the str.length() from before the node in bits 27..0,
    // and the state in bits 31..28.
    // Except for the following values for a three-way-branch node,
    // the lower values indicate how many branches of a list-branch node
    // have been visited so far.
    static const int32_t kThreeWayBranchEquals=0xe;
    static const int32_t kThreeWayBranchGreaterThan=0xf;

    UCharTrie trie;

    UnicodeString str;
    int32_t value;

    UVector32 stack;
};

UBool
UCharTrieIterator::next(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
    if(trie.pos==NULL) {
        if(stack.isEmpty()) {
            return FALSE;
        }
        // Read the top of the stack and continue with the next outbound edge of
        // the branch node.
        // The last outbound edge causes the branch node to be popped off the stack
        // and the iteration to continue from the trie.pos there.
        int32_t stackSize=stack.size();
        int32_t state=stack.elementAti(stackSize-1);
        trie.pos=trie.uchars+stack.elementAti(stackSize-2);
        str.truncate(state&0xfffffff);
        state=(state>>28)&0xf;
        if(state==kThreeWayBranchEquals) {
            int32_t node=*trie.pos;  // Known to be a three-way-branch node.
            UChar trieUnit=trie.pos[1];
            trie.pos+=(node&1)+3;  // Skip node, trie unit and fixed-width integer.
            node>>=1;
            value=trie.readFixedInt(node);
            // Rewrite the top of the stack for the greater-than branch.
            stack.setElementAt((int32_t)(trie.pos-trie.uchars), stackSize-2);
            stack.setElementAt((kThreeWayBranchGreaterThan<<28)|str.length(), stackSize-1);
            str.append(trieUnit);
            if(node&UCharTrie::kFixedIntIsFinal) {
                trie.stop();
                return TRUE;
            } else {
                trie.pos+=value;
            }
        } else if(state==kThreeWayBranchGreaterThan) {
            // Pop the state.
            stack.setSize(stackSize-2);
        } else {
            // Remainder of a list-branch node.
            // Until we are done, we keep our state on the node lead unit
            // and loop forward to the next key unit.
            // This is because we do not have enough state bits for the
            // bit pairs for the (key, value) pairs.
            int32_t node=*trie.pos++;  // Known to be a list-branch node.
            int32_t length=(node>>UCharTrie::kMaxListBranchLengthShift)+1;
            // Actual list length minus 1.
            if(length>=UCharTrie::kMaxListBranchSmallLength) {
                // For 7..14 pairs, read the next unit as well.
                node=(node<<16)|*trie.pos++;
            }
            // Skip (key, value) pairs already visited.
            int32_t i=state;
            do {
                trie.pos+=(node&UCharTrie::kFixedInt32)+2;
                node>>=2;
            } while(i--!=0);
            // Read the next key unit.
            str.append(*trie.pos++);
            if(++state!=length) {
                value=trie.readFixedInt(node);
                // Rewrite the top of the stack for the next branch.
                stack.setElementAt((state<<28)|(str.length()-1), stackSize-1);
                if(node&UCharTrie::kFixedIntIsFinal) {
                    trie.stop();
                    return TRUE;
                } else {
                    trie.pos+=value;
                }
            } else {
                // Pop the state.
                stack.setSize(stackSize-2);
            }
        }
    }
    for(;;) {
        int32_t node=*trie.pos++;
        if(node>=UCharTrie::kMinValueLead) {
            // Deliver value for the string so far.
            if(trie.readCompactInt(node)) {
                trie.stop();
            }
            value=trie.value;
            return TRUE;
        } else if(node<UCharTrie::kMinLinearMatch) {
            // Branch node, needs to take the first outbound edge and push state for the rest.
            // We will need to re-read the node lead unit.
            stack.addElement((int32_t)(trie.pos-1-trie.uchars), errorCode);
            if(node>=UCharTrie::kMinThreeWayBranch) {
                // Branching on a unit value,
                // with a jump delta for less-than, a fixed-width int for equals,
                // and continuing for greater-than.
                stack.addElement((kThreeWayBranchEquals<<28)|str.length(), errorCode);
                // For the less-than branch, ignore the trie unit.
                ++trie.pos;
                // Jump.
                int32_t delta=trie.readFixedInt(node);
                trie.pos+=delta;
            } else {
                // Branch node with a list of key-value pairs where
                // values are fixed-width integers: either final values or jump deltas.
                int32_t length=(node>>UCharTrie::kMaxListBranchLengthShift)+1;
                // Actual list length minus 1.
                if(length>=UCharTrie::kMaxListBranchSmallLength) {
                    // For 7..14 pairs, read the next unit as well.
                    node=(node<<16)|*trie.pos++;
                }
                // Read the first (key, value) pair.
                UChar trieUnit=*trie.pos++;
                value=trie.readFixedInt(node);
                stack.addElement(str.length(), errorCode);  // state=0
                str.append(trieUnit);
                if(node&UCharTrie::kFixedIntIsFinal) {
                    trie.stop();
                    return TRUE;
                } else {
                    trie.pos+=value;
                }
            }
        } else {
            // Linear-match node, append length UChars to str.
            int32_t length=node-UCharTrie::kMinLinearMatch+1;
            str.append(trie.pos, length);
            trie.pos+=length;
        }
    }
}

U_NAMESPACE_END

#endif  // __UCHARTRIEITERATOR_H__
