/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetrieiterator.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov03
*   created by: Markus W. Scherer
*/

#ifndef __BYTETRIEITERATOR_H__
#define __BYTETRIEITERATOR_H__

/**
 * \file
 * \brief C++ API: ByteTrie iterator for all of its (byte sequence, value) pairs.
 */

// Needed if and when we change the .dat package index to a ByteTrie,
// so that icupkg can work with an input package.

#include "unicode/utypes.h"
#include "unicode/stringpiece.h"
#include "bytetrie.h"
#include "charstr.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

/**
 * Iterator for all of the (byte sequence, value) pairs in a ByteTrie.
 */
class U_TOOLUTIL_API ByteTrieIterator : public UMemory {
public:
    /**
     * Iterates from the root of a byte-serialized ByteTrie.
     * @param trieBytes The trie bytes.
     * @param maxStringLength If 0, the iterator returns full strings/byte sequences.
     *                        Otherwise, the iterator returns strings with this maximum length.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     */
    ByteTrieIterator(const void *trieBytes, int32_t maxStringLength, UErrorCode &errorCode);

    /**
     * Iterates from the current state of the specified ByteTrie.
     * @param otherTrie The trie whose state will be copied for iteration.
     * @param maxStringLength If 0, the iterator returns full strings/byte sequences.
     *                        Otherwise, the iterator returns strings with this maximum length.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     */
    ByteTrieIterator(const ByteTrie &otherTrie, int32_t maxStringLength, UErrorCode &errorCode);

    /**
     * Resets this iterator to its initial state.
     */
    ByteTrieIterator &reset();

    /**
     * Finds the next (byte sequence, value) pair if there is one.
     *
     * If the byte sequence is truncated to the maximum length and does not
     * have a real value, then the value is set to -1.
     * In this case, this "not a real value" is indistinguishable from
     * a real value of -1.
     * @return TRUE if there is another element.
     */
    UBool next(UErrorCode &errorCode);

    /**
     * @return TRUE if there are more elements.
     */
    UBool hasNext() const { return trie.pos!=NULL || !stack.isEmpty(); }

    /**
     * @return the NUL-terminated byte sequence for the last successful next()
     */
    const StringPiece &getString() const { return sp; }
    /**
     * @return the value for the last successful next()
     */
    int32_t getValue() const { return value; }

private:
    UBool truncateAndStop() {
        trie.stop();
        value=-1;  // no real value for str
        sp.set(str.data(), str.length());
        return TRUE;
    }

    // The stack stores pairs of integers for backtracking to another
    // outbound edge of a branch node.
    // The first integer is an offset from ByteTrie.bytes.
    // The second integer has the str.length() from before the node in bits 27..0,
    // and the state in bits 31..28.
    // Except for the following value for a split-branch node,
    // the lower values indicate how many branches of a list-branch node
    // are left to be visited.
    static const int32_t kSplitBranchGreaterOrEqual=0xf;

    ByteTrie trie;
    ByteTrie::State initialState;

    CharString str;
    StringPiece sp;
    int32_t maxLength;
    int32_t value;

    UVector32 stack;
};

U_NAMESPACE_END

#endif  // __BYTETRIEITERATOR_H__
