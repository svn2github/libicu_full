/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  udicttrie.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010dec17
*   created by: Markus W. Scherer
*/

#ifndef __UDICTTRIE_H__
#define __UDICTTRIE_H__

/**
 * \file
 * \brief C API: Helper definitions for dictionary trie APIs.
 */

#include "unicode/utypes.h"

/**
  * Return values for ByteTrie::next(), UCharTrie::next() and similar methods.
  *
  * The UDICTTRIE_NO_MATCH constant's numeric value is 0, so in C/C++,
  * if checking for UDICTTRIE_HAS_VALUE is not needed,
  * a UDictTrieResult can be treated like a boolean "matches",
  * as in "if(!trie.next(c)) ..."
  */
enum UDictTrieResult {
    /**
      * The input unit(s) did not continue a matching string.
      */
    UDICTTRIE_NO_MATCH,
    /**
      * The input unit(s) continued a matching string
      * but there is no value for the string so far.
      * (It is a prefix of a longer string.)
      */
    UDICTTRIE_NO_VALUE,
    /**
      * The input unit(s) continued a matching string
      * and there is a value for the string so far.
      * This value will be returned by getValue().
      */
    UDICTTRIE_HAS_VALUE
};

#endif  /* __UDICTTRIE_H__ */
