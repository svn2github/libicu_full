/*
*******************************************************************************
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*
* File ucoleitr.cpp
*
* Modification History:
*
* Date        Name        Description
* 02/15/2001  synwee      Modified all methods to process its own function 
*                         instead of calling the equivalent c++ api (coleitr.h)
*******************************************************************************/

#ifndef UCOLEITR_H
#define UCOLEITR_H

/**  
 * This indicates an error has occured during processing or if no more CEs is 
 * to be returned.
 */
#define UCOL_NULLORDER        0xFFFFFFFF

#include "unicode/ucol.h"

/** 
 * The UCollationElements struct.
 * For usage in C programs.
 */
typedef struct UCollationElements UCollationElements;

/**
 * The UCollationElements  is used as an iterator to walk through each 
 * character of an international string. Use the iterator to return the
 * ordering priority of the positioned character. The ordering priority of a 
 * character, which we refer to as a key, defines how a character is collated 
 * in the given collation object.
 * For example, consider the following in Spanish:
 * <pre>
 * .       "ca" -> the first key is key('c') and second key is key('a').
 * .       "cha" -> the first key is key('ch') and second key is key('a').
 * </pre>
 * And in German,
 * <pre>
 * .       "�b"-> the first key is key('a'), the second key is key('e'), and
 * .       the third key is key('b').
 * </pre>
 * <p>Example of the iterator usage: (without error checking)
 * <pre>
 * .  void CollationElementIterator_Example()
 * .  {
 * .      UChar *s;
 * .      t_int32 order, primaryOrder;
 * .      UCollationElements *c;
 * .      UCollatorOld *coll;
 * .      UErrorCode success = U_ZERO_ERROR;
 * .      s=(UChar*)malloc(sizeof(UChar) * (strlen("This is a test")+1) );
 * .      u_uastrcpy(s, "This is a test");
 * .      coll = ucol_open(NULL, &success);
 * .      c = ucol_openElements(coll, str, u_strlen(str), &status);
 * .      order = ucol_next(c, &success);
 * .      ucol_reset(c);
 * .      order = ucol_prev(c, &success);
 * .      free(s);
 * .      ucol_close(coll);
 * .      ucol_closeElements(c);
 * .  }
 * </pre>
 * <p>
 * ucol_next() returns the collation order of the next.
 * ucol_prev() returns the collation order of the previous character.
 * The Collation Element Iterator moves only in one direction between calls to
 * ucol_reset. That is, ucol_next() and ucol_prev can not be inter-used. 
 * Whenever ucol_prev is to be called after ucol_next() or vice versa, 
 * ucol_reset has to be called first to reset the status, shifting pointers to 
 * either the end or the start of the string. Hence at the next call of 
 * ucol_prev or ucol_next, the first or last collation order will be returned. 
 * If a change of direction is done without a ucol_reset, the result is 
 * undefined.
 * The result of a forward iterate (ucol_next) and reversed result of the  
 * backward iterate (ucol_prev) on the same string are equivalent, if 
 * collation orders with the value UCOL_IGNORABLE are ignored.
 * Character based on the comparison level of the collator.  A collation order 
 * consists of primary order, secondary order and tertiary order.  The data 
 * type of the collation order is <strong>t_int32</strong>. 
 *
 * @see UCollator
 */

/**
 * Open the collation elements for a string.
 *
 * @param coll The collator containing the desired collation rules.
 * @param text The text to iterate over.
 * @param textLength The number of characters in text, or -1 if null-terminated
 * @param status A pointer to an UErrorCode to receive any errors.
 * @return a struct containing collation element information
 */
U_CAPI UCollationElements*
ucol_openElements(const UCollator  *coll,
                  const UChar      *text,
                        int32_t    textLength,
                        UErrorCode *status);

/**
 * get a hash code for a key... Not very useful!
 * @deprecated
 */
U_CAPI int32_t
ucol_keyHashCode(const uint8_t* key, int32_t length);

/**
 * Close a UCollationElements.
 * Once closed, a UCollationElements may no longer be used.
 * @param elems The UCollationElements to close.
 */
U_CAPI void
ucol_closeElements(UCollationElements *elems);

/**
 * Reset the collation elements to their initial state.
 * This will move the 'cursor' to the beginning of the text.
 * @param elems The UCollationElements to reset.
 * @see ucol_next
 * @see ucol_previous
 */
U_CAPI void
ucol_reset(UCollationElements *elems);

/**
 * Get the ordering priority of the next collation element in the text.
 * A single character may contain more than one collation element.
 * @param elems The UCollationElements containing the text.
 * @param status A pointer to an UErrorCode to receive any errors.
 * @return The next collation elements ordering, otherwise returns NULLORDER 
 *         if an error has occured or if the end of string has been reached
 */
U_CAPI int32_t
ucol_next(UCollationElements *elems, UErrorCode *status);

/**
 * Get the ordering priority of the previous collation element in the text.
 * A single character may contain more than one collation element.
 * @param elems The UCollationElements containing the text.
 * @param status A pointer to an UErrorCode to receive any errors.
 * @return The previous collation elements ordering, otherwise returns 
 *         NULLORDER if an error has occured or if the start of string has 
 *         been reached
 */
U_CAPI int32_t
ucol_previous(UCollationElements *elems, UErrorCode *status);

/**
 * Get the maximum length of any expansion sequences that end with the 
 * specified comparison order.
 * This is useful for .... ?
 * @param elems The UCollationElements containing the text.
 * @param order A collation order returned by previous or next.
 * @return maximum size of the expansion sequences ending with the collation 
 *         element or 1 if collation element does not occur at the end of any 
 *         expansion sequence
 */
U_CAPI int32_t
ucol_getMaxExpansion(const UCollationElements *elems, int32_t order);

/**
 * Set the text containing the collation elements.
 * @param elems The UCollationElements to set.
 * @param text The source text containing the collation elements.
 * @param textLength The length of text, or -1 if null-terminated.
 * @param status A pointer to an UErrorCode to receive any errors.
 * @see ucol_getText
 */
U_CAPI void
ucol_setText(      UCollationElements *elems, 
             const UChar              *text,
                   int32_t            textLength,
                   UErrorCode         *status);

/**
 * Get the offset of the current source character.
 * This is an offset into the text of the character containing the current
 * collation elements.
 * @param elems The UCollationElements to query.
 * @return The offset of the current source character.
 * @see ucol_setOffset
 */
U_CAPI UTextOffset
ucol_getOffset(const UCollationElements *elems);

/**
 * Set the offset of the current source character.
 * This is an offset into the text of the character to be processed.
 * @param elems The UCollationElements to set.
 * @param offset The desired character offset.
 * @param status A pointer to an UErrorCode to receive any errors.
 * @see ucol_getOffset
 */
U_CAPI void
ucol_setOffset(UCollationElements *elems,
               UTextOffset        offset,
               UErrorCode         *status);

#endif
