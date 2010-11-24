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
class U_TOOLUTIL_API UCharTrieIterator : public UMemory {
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
    int32_t getValue() const { return value; }

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

U_NAMESPACE_END

#endif  // __UCHARTRIEITERATOR_H__
