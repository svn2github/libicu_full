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
 * Iterator for all of the (string, value) pairs in a UCharTrie.
 */
class U_TOOLUTIL_API UCharTrieIterator : public UMemory {
public:
    /**
     * Iterates from the root of a UChar-serialized UCharTrie.
     * @param trieUChars The trie UChars.
     * @param maxStringLength If 0, the iterator returns full strings.
     *                        Otherwise, the iterator returns strings with this maximum length.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     */
    UCharTrieIterator(const UChar *trieUChars, int32_t maxStringLength, UErrorCode &errorCode);

    /**
     * Iterates from the current state of the specified UCharTrie.
     * @param otherTrie The trie whose state will be copied for iteration.
     * @param maxStringLength If 0, the iterator returns full strings.
     *                        Otherwise, the iterator returns strings with this maximum length.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     */
    UCharTrieIterator(const UCharTrie &otherTrie, int32_t maxStringLength, UErrorCode &errorCode);

    /**
     * Resets this iterator to its initial state.
     */
    UCharTrieIterator &reset();

    /**
     * Finds the next (string, value) pair if there is one.
     *
     * If the string is truncated to the maximum length and does not
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
     * @return the NUL-terminated string for the last successful next()
     */
    const UnicodeString &getString() const { return str; }
    /**
     * @return the value for the last successful next()
     */
    int32_t getValue() const { return value; }

private:
    UBool truncateAndStop() {
        trie.stop();
        value=-1;  // no real value for str
        return TRUE;
    }

    // The stack stores pairs of integers for backtracking to another
    // outbound edge of a branch node.
    // The first integer is an offset from UCharTrie.uchars.
    // The second integer has the str.length() from before the node in bits 27..0,
    // and the state in bits 31..28.
    // Except for the following value for a split-branch node,
    // the lower values indicate how many branches of a list-branch node
    // have been visited so far.
    static const int32_t kSplitBranchGreaterOrEqual=0xf;

    UCharTrie trie;
    UCharTrie::State initialState;

    UnicodeString str;
    int32_t maxLength;
    int32_t value;

    UVector32 stack;
};

U_NAMESPACE_END

#endif  // __UCHARTRIEITERATOR_H__
