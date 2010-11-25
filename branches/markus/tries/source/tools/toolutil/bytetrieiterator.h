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
 * Iterator for all of the (byte sequence, value) pairs in a a ByteTrie.
 */
class U_TOOLUTIL_API ByteTrieIterator : public UMemory {
public:
    ByteTrieIterator(const void *trieBytes, UErrorCode &errorCode)
            : trie(trieBytes), value(0), stack(errorCode) {}

    /**
     * Finds the next (byte sequence, value) pair if there is one.
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

    // TODO: We could add a constructor that takes a ByteTrie object
    // and copies its current state, for iterating over all byte sequences and their
    // values reachable from that state.

private:
    // The stack stores pairs of integers for backtracking to another
    // outbound edge of a branch node.
    // The first integer is an offset from ByteTrie.bytes.
    // The second integer has the str.length() from before the node in bits 27..0,
    // and the state in bits 31..28.
    // Except for the following values for a three-way-branch node,
    // the lower values indicate how many branches of a list-branch node
    // are left to be visited.
    static const int32_t kThreeWayBranchEquals=0xe;
    static const int32_t kThreeWayBranchGreaterThan=0xf;

    ByteTrie trie;

    CharString str;
    StringPiece sp;
    int32_t value;

    UVector32 stack;
};

U_NAMESPACE_END

#endif  // __BYTETRIEITERATOR_H__
