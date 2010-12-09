/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetrieiterator.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov03
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/stringpiece.h"
#include "bytetrie.h"
#include "bytetrieiterator.h"
#include "charstr.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

ByteTrieIterator::ByteTrieIterator(const void *trieBytes, int32_t maxStringLength,
                                   UErrorCode &errorCode)
        : trie(trieBytes), maxLength(maxStringLength), value(0), stack(errorCode) {
    trie.saveState(initialState);
}

ByteTrieIterator::ByteTrieIterator(const ByteTrie &otherTrie, int32_t maxStringLength,
                                   UErrorCode &errorCode)
        : trie(otherTrie), maxLength(maxStringLength), value(0), stack(errorCode) {
    trie.saveState(initialState);
    int32_t length=trie.remainingMatchLength;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Pending linear-match node, append remaining bytes to str.
        ++length;
        if(maxLength>0 && length>maxLength) {
            length=maxLength;  // This will leave remainingMatchLength>=0 as a signal.
        }
        str.append(reinterpret_cast<const char *>(trie.pos), length, errorCode);
        trie.pos+=length;
        trie.remainingMatchLength-=length;
    }
}

ByteTrieIterator &ByteTrieIterator::reset() {
    trie.resetToState(initialState);
    int32_t length=trie.remainingMatchLength+1;  // Remaining match length.
    if(maxLength>0 && length>maxLength) {
        length=maxLength;
    }
    str.truncate(length);
    trie.pos+=length;
    trie.remainingMatchLength-=length;
    stack.setSize(0);
    return *this;
}

UBool
ByteTrieIterator::next(UErrorCode &errorCode) {
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
        trie.pos=trie.bytes+stack.elementAti(stackSize-2);
        str.truncate(state&0xfffffff);
        state=(state>>28)&0xf;
        if(state==kThreeWayBranchEquals) {
            // Known to be a three-way-branch node.
            int32_t node=*trie.pos;
            uint8_t trieByte=trie.pos[1];
            // Skip node, trie byte and fixed-width integer.
            trie.pos+=node-ByteTrie::kMinThreeWayBranch+3;
            UBool isFinal=trie.readCompactInt();
            // Rewrite the top of the stack for the greater-than branch.
            stack.setElementAt((int32_t)(trie.pos-trie.bytes), stackSize-2);
            stack.setElementAt((kThreeWayBranchGreaterThan<<28)|str.length(), stackSize-1);
            str.append((char)trieByte, errorCode);
            if(isFinal) {
                value=trie.value;
                trie.stop();
                sp.set(str.data(), str.length());
                return TRUE;
            } else {
                trie.pos+=trie.value;
            }
        } else if(state==kThreeWayBranchGreaterThan) {
            // Pop the state.
            stack.setSize(stackSize-2);
        } else {
            // Remainder of a list-branch node.
            // Read the next key byte.
            str.append((char)*trie.pos++, errorCode);
            if(state>0) {
                UBool isFinal=trie.readCompactInt();
                // Rewrite the top of the stack for the next branch.
                stack.setElementAt((int32_t)(trie.pos-trie.bytes), stackSize-2);
                stack.setElementAt(((state-1)<<28)|(str.length()-1), stackSize-1);
                if(isFinal) {
                    value=trie.value;
                    trie.stop();
                    sp.set(str.data(), str.length());
                    return TRUE;
                } else {
                    trie.pos+=trie.value;
                }
            } else {
                // Pop the state.
                stack.setSize(stackSize-2);
            }
        }
    }
    if(trie.remainingMatchLength>=0) {
        // We only get here if we started in a pending linear-match node
        // with more than maxLength remaining bytes.
        return truncateAndStop();
    }
    for(;;) {
        int32_t node=*trie.pos++;
        if(node>=ByteTrie::kMinValueLead) {
            // Deliver value for the byte sequence so far.
            if(trie.readCompactInt(node) || (maxLength>0 && str.length()==maxLength)) {
                trie.stop();
            }
            value=trie.value;
            sp.set(str.data(), str.length());
            return TRUE;
        }
        if(maxLength>0 && str.length()==maxLength) {
            return truncateAndStop();
        }
        if(node<ByteTrie::kMinLinearMatch) {
            // Branch node, needs to take the first outbound edge and push state for the rest.
            if(node>=ByteTrie::kMinThreeWayBranch) {
                // Branching on a byte value,
                // with a jump delta for less-than, a compact int for equals,
                // and continuing for greater-than.
                node-=ByteTrie::kMinThreeWayBranch;
                stack.addElement((int32_t)(trie.pos-1-trie.bytes), errorCode);
                stack.addElement((kThreeWayBranchEquals<<28)|str.length(), errorCode);
                // For the less-than branch, ignore the trie byte.
                ++trie.pos;
                // Jump.
                int32_t delta=trie.readFixedInt(node);
                trie.pos+=delta;
            } else {
                // Branch node with a list of key-value pairs where
                // values are compact integers: either final values or jump deltas.
                int32_t length=node;  // Actual list length minus 2.
                // Read the first (key, value) pair.
                uint8_t trieByte=*trie.pos++;
                UBool isFinal=trie.readCompactInt();
                stack.addElement((int32_t)(trie.pos-trie.bytes), errorCode);
                stack.addElement((length<<28)|str.length(), errorCode);
                str.append((char)trieByte, errorCode);
                if(isFinal) {
                    value=trie.value;
                    trie.stop();
                    sp.set(str.data(), str.length());
                    return TRUE;
                } else {
                    trie.pos+=trie.value;
                }
            }
        } else {
            // Linear-match node, append length bytes to str.
            int32_t length=node-ByteTrie::kMinLinearMatch+1;
            if(maxLength>0 && str.length()+length>maxLength) {
                str.append(reinterpret_cast<const char *>(trie.pos),
                           maxLength-str.length(), errorCode);
                return truncateAndStop();
            }
            str.append(reinterpret_cast<const char *>(trie.pos), length, errorCode);
            trie.pos+=length;
        }
    }
}

U_NAMESPACE_END
