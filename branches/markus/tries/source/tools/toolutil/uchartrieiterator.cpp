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

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "uchartrie.h"
#include "uchartrieiterator.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

UCharTrieIterator::UCharTrieIterator(const UChar *trieUChars, int32_t maxStringLength,
                                     UErrorCode &errorCode)
        : trie(trieUChars), maxLength(maxStringLength), value(0), stack(errorCode) {
    trie.saveState(initialState);
}

UCharTrieIterator::UCharTrieIterator(const UCharTrie &otherTrie, int32_t maxStringLength,
                                     UErrorCode &errorCode)
        : trie(otherTrie), maxLength(maxStringLength), value(0), stack(errorCode) {
    trie.saveState(initialState);
    int32_t length=trie.remainingMatchLength_;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Pending linear-match node, append remaining UChars to str.
        ++length;
        if(maxLength>0 && length>maxLength) {
            length=maxLength;  // This will leave remainingMatchLength>=0 as a signal.
        }
        str.append(trie.pos_, length);
        trie.pos_+=length;
        trie.remainingMatchLength_-=length;
    }
}

UCharTrieIterator &UCharTrieIterator::reset() {
    trie.resetToState(initialState);
    int32_t length=trie.remainingMatchLength_+1;  // Remaining match length.
    if(maxLength>0 && length>maxLength) {
        length=maxLength;
    }
    str.truncate(length);
    trie.pos_+=length;
    trie.remainingMatchLength_-=length;
    stack.setSize(0);
    return *this;
}

UBool
UCharTrieIterator::next(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
#if 0
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
        if(state==kSplitBranchGreaterOrEqual) {
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
    if(trie.remainingMatchLength>=0) {
        // We only get here if we started in a pending linear-match node
        // with more than maxLength remaining UChars.
        return truncateAndStop();
    }
    for(;;) {
        int32_t node=*trie.pos++;
        if(node>=UCharTrie::kMinValueLead) {
            // Deliver value for the string so far.
            if(trie.readCompactInt(node) || (maxLength>0 && str.length()==maxLength)) {
                trie.stop();
            }
            value=trie.value;
            return TRUE;
        }
        if(maxLength>0 && str.length()==maxLength) {
            return truncateAndStop();
        }
        if(node<UCharTrie::kMinLinearMatch) {
            // Branch node, needs to take the first outbound edge and push state for the rest.
            if(node>=UCharTrie::kMinSplitBranch) {
                // Branching on a unit value,
                // with a jump delta for less-than, and continuing for greater-or-equal.
                // Both edges must lead to branch nodes again.
                ++trie.pos;  // ignore the comparison unit
                // Jump.
                int32_t delta=trie.readFixedInt(node);
                stack.addElement((int32_t)(trie.pos-trie.uchars), errorCode);
                stack.addElement((kSplitBranchGreaterOrEqual<<28)|str.length(), errorCode);
                trie.pos+=delta;
            } else {
                // We will need to re-read the node lead unit.
                stack.addElement((int32_t)(trie.pos-1-trie.uchars), errorCode);
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
            if(maxLength>0 && str.length()+length>maxLength) {
                str.append(trie.pos, maxLength-str.length());
                return truncateAndStop();
            }
            str.append(trie.pos, length);
            trie.pos+=length;
        }
    }
#endif
    return FALSE;
}

U_NAMESPACE_END
