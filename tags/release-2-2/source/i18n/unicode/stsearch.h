/*
**********************************************************************
*   Copyright (C) 2001 IBM and others. All rights reserved.
**********************************************************************
*   Date        Name        Description
*  03/22/2000   helena      Creation.
**********************************************************************
*/

#ifndef STSEARCH_H
#define STSEARCH_H

#include "unicode/tblcoll.h"
#include "unicode/coleitr.h"
#include "unicode/search.h"

U_NAMESPACE_BEGIN

/**
 * <tt>StringSearch</tt> is a <tt>SearchIterator</tt> that provides
 * language-sensitive text searching based on the comparison rules defined
 * in a {@link RuleBasedCollator} object.
 * StringSearch ensures that language eccentricity can be 
 * handled, e.g. for the German collator, characters � and SS will be matched 
 * if case is chosen to be ignored.
 * See the <a href=http://oss.software.ibm.com/icu/develop/collation/ICU_collation_design.htm>
 * "ICU Collation Design Document"</a> for more information.
 * <p> 
 * The algorithm implemented is a modified form of the Boyer Moore's search.
 * For more information  see 
 * <a href=http://oss.software.ibm.com/icu/docs/papers/text-search.html>
 * "Efficient Text Searching in Java"</a>, published in <i>Java Report</i> 
 * in February, 1999, for further information on the algorithm.
 * <p>
 * There are 2 match options for selection:<br>
 * Let S' be the sub-string of a text string S between the offsets start and 
 * end <start, end>.
 * <br>
 * A pattern string P matches a text string S at the offsets <start, end> 
 * if
 * <pre> 
 * option 1. Some canonical equivalent of P matches some canonical equivalent 
 *           of S'
 * option 2. P matches S' and if P starts or ends with a combining mark, 
 *           there exists no non-ignorable combining mark before or after S? 
 *           in S respectively. 
 * </pre>
 * Option 2. will be the default�
 * <p>
 * This search has APIs similar to that of other text iteration mechanisms 
 * such as the break iterators in <tt>BreakIterator</tt>. Using these 
 * APIs, it is easy to scan through text looking for all occurances of 
 * a given pattern. This search iterator allows changing of direction by 
 * calling a <tt>reset</tt> followed by a <tt>next</tt> or <tt>previous</tt>. 
 * Though a direction change can occur without calling <tt>reset</tt> first,  
 * this operation comes with some speed penalty.
 * Match results in the forward direction will match the result matches in 
 * the backwards direction in the reverse order
 * <p>
 * <tt>SearchIterator</tt> provides APIs to specify the starting position 
 * within the text string to be searched, e.g. <tt>setOffset</tt>,
 * <tt>preceding</tt> and <tt>following</tt>. Since the 
 * starting position will be set as it is specified, please take note that 
 * there are some danger points which the search may render incorrect 
 * results:
 * <ul>
 * <li> The midst of a substring that requires normalization.
 * <li> If the following match is to be found, the position should not be the
 *      second character which requires to be swapped with the preceding 
 *      character. Vice versa, if the preceding match is to be found, 
 *      position to search from should not be the first character which 
 *      requires to be swapped with the next character. E.g certain Thai and
 *      Lao characters require swapping.
 * <li> If a following pattern match is to be found, any position within a 
 *      contracting sequence except the first will fail. Vice versa if a 
 *      preceding pattern match is to be found, a invalid starting point 
 *      would be any character within a contracting sequence except the last.
 * </ul>
 * <p>
 * A breakiterator can be used if only matches at logical breaks are desired.
 * Using a breakiterator will only give you results that exactly matches the
 * boundaries given by the breakiterator. For instance the pattern "e" will
 * not be found in the string "\u00e9" if a character break iterator is used.
 * <p>
 * Options are provided to handle overlapping matches. 
 * E.g. In English, overlapping matches produces the result 0 and 2 
 * for the pattern "abab" in the text "ababab", where else mutually 
 * exclusive matches only produce the result of 0.
 * <p>
 * Though collator attributes will be taken into consideration while 
 * performing matches, there are no APIs here for setting and getting the 
 * attributes. These attributes can be set by getting the collator
 * from <tt>getCollator</tt> and using the APIs in <tt>coll.h</tt>.
 * Lastly to update StringSearch to the new collator attributes, 
 * reset() has to be called.
 * <p> 
 * Restriction: <br>
 * Currently there are no composite characters that consists of a
 * character with combining class > 0 before a character with combining 
 * class == 0. However, if such a character exists in the future,  
 * StringSearch does not guarantee the results for option 1.
 * <p>
 * Consult the <tt>SearchIterator</tt> documentation for information on
 * and examples of how to use instances of this class to implement text
 * searching.
 * <pre><code>
 * UnicodeString target("The quick brown fox jumped over the lazy fox");
 * UnicodeString pattern("fox");
 *
 * SearchIterator *iter  = new StringSearch(pattern, target);
 * UErrorCode      error = U_ZERO_ERROR;
 * for (int pos = iter->first(error); pos != USEARCH_DONE; 
 *                               pos = iter->next(error)) {
 *     printf("Found match at %d pos, length is %d\n", pos, 
 *                                             iter.getMatchLength());
 * }
 * </code></pre>
 * @see SearchIterator
 * @see RuleBasedCollator
 * @since ICU 2.0
 */

class U_I18N_API StringSearch : public SearchIterator
{
public:

    // public constructors and destructors --------------------------------

    /**
     * Creating a <tt>StringSearch</tt> instance using the argument locale 
     * language rule set. A collator will be created in the process, which 
     * will be owned by this instance and will be deleted in during 
     * destruction
     * @param pattern The text for which this object will search.
     * @param text    The text in which to search for the pattern.
     * @param locale  A locale which defines the language-sensitive 
     *                comparison rules used to determine whether text in the 
     *                pattern and target matches. 
     * @param breakiter A <tt>BreakIterator</tt> object used to constrain 
     *                the matches that are found. Matches whose start and end 
     *                indices in the target text are not boundaries as 
     *                determined by the <tt>BreakIterator</tt> are 
     *                ignored. If this behavior is not desired, 
     *                <tt>NULL</tt> can be passed in instead.
     * @param status  for errors if any. If pattern or text is NULL, or if
     *               either the length of pattern or text is 0 then an 
     *               U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    StringSearch(const UnicodeString &pattern, const UnicodeString &text,
                 const Locale        &locale,       
                       BreakIterator *breakiter,
                       UErrorCode    &status);

    /**
     * Creating a <tt>StringSearch</tt> instance using the argument collator 
     * language rule set. Note, user retains the ownership of this collator, 
     * it does not get destroyed during this instance's destruction.
     * @param pattern The text for which this object will search.
     * @param text    The text in which to search for the pattern.
     * @param coll    A <tt>RuleBasedCollator</tt> object which defines 
     *                the language-sensitive comparison rules used to 
     *                determine whether text in the pattern and target 
     *                matches. User is responsible for the clearing of this
     *                object.
     * @param breakiter A <tt>BreakIterator</tt> object used to constrain 
     *                the matches that are found. Matches whose start and end 
     *                indices in the target text are not boundaries as 
     *                determined by the <tt>BreakIterator</tt> are 
     *                ignored. If this behavior is not desired, 
     *                <tt>NULL</tt> can be passed in instead.
     * @param status for errors if any. If either the length of pattern or 
     *               text is 0 then an U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    StringSearch(const UnicodeString     &pattern, 
                 const UnicodeString     &text,
                       RuleBasedCollator *coll,       
                       BreakIterator     *breakiter,
                       UErrorCode        &status);

    /**
     * Creating a <tt>StringSearch</tt> instance using the argument locale 
     * language rule set. A collator will be created in the process, which 
     * will be owned by this instance and will be deleted in during 
     * destruction
     * <p>
     * Note: No parsing of the text within the <tt>CharacterIterator</tt> 
     * will be done during searching for this version. The block of text 
     * in <tt>CharacterIterator</tt> will be used as it is.
     * @param pattern The text for which this object will search.
     * @param text    The text iterator in which to search for the pattern.
     * @param locale  A locale which defines the language-sensitive 
     *                comparison rules used to determine whether text in the 
     *                pattern and target matches. User is responsible for 
     *                the clearing of this object.
     * @param breakiter A <tt>BreakIterator</tt> object used to constrain 
     *                the matches that are found. Matches whose start and end 
     *                indices in the target text are not boundaries as 
     *                determined by the <tt>BreakIterator</tt> are 
     *                ignored. If this behavior is not desired, 
     *                <tt>NULL</tt> can be passed in instead.
     * @param status for errors if any. If either the length of pattern or 
     *               text is 0 then an U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    StringSearch(const UnicodeString &pattern, CharacterIterator &text,
                 const Locale        &locale, 
                       BreakIterator *breakiter,
                       UErrorCode    &status);

    /**
     * Creating a <tt>StringSearch</tt> instance using the argument collator 
     * language rule set. Note, user retains the ownership of this collator, 
     * it does not get destroyed during this instance's destruction.
     * <p>
     * Note: No parsing of the text within the <tt>CharacterIterator</tt> 
     * will be done during searching for this version. The block of text 
     * in <tt>CharacterIterator</tt> will be used as it is.
     * @param pattern The text for which this object will search.
     * @param text    The text in which to search for the pattern.
     * @param coll    A <tt>RuleBasedCollator</tt> object which defines 
     *                the language-sensitive comparison rules used to 
     *                determine whether text in the pattern and target 
     *                matches. User is responsible for the clearing of this
     *                object.
     * @param breakiter A <tt>BreakIterator</tt> object used to constrain 
     *                the matches that are found. Matches whose start and end 
     *                indices in the target text are not boundaries as 
     *                determined by the <tt>BreakIterator</tt> are 
     *                ignored. If this behavior is not desired, 
     *                <tt>NULL</tt> can be passed in instead.
     * @param status for errors if any. If either the length of pattern or 
     *               text is 0 then an U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    StringSearch(const UnicodeString     &pattern, CharacterIterator &text,
                       RuleBasedCollator *coll, 
                       BreakIterator     *breakiter,
                       UErrorCode        &status);

    /**
     * Copy constructor that creates a StringSearch instance with the same 
     * behavior, and iterating over the same text.
     * @param that StringSearch instance to be copied.
     * @draft ICU 2.0
     */
    StringSearch(const StringSearch &that);

    /**
    * Destructor. Cleans up the search iterator data struct.
    * If a collator is created in the constructor, it will be destroyed here.
    * @draft ICU 2.0
    */
    virtual ~StringSearch(void);

    // operator overloading ---------------------------------------------

    /**
     * Assignment operator. Sets this iterator to have the same behavior,
     * and iterate over the same text, as the one passed in.
     * @param that instance to be copied.
     * @draft ICU 2.0
     */
    StringSearch & operator=(const StringSearch &that);

    /**
     * Equality operator. 
     * @param that instance to be compared.
     * @return TRUE if both instances have the same attributes, 
     *         breakiterators, collators and iterate over the same text 
     *         while looking for the same pattern.
     * @draft ICU 2.0
     */
    virtual UBool operator==(const SearchIterator &that) const;

    // public get and set methods ----------------------------------------

    /**
     * Sets the index to point to the given position, and clears any state 
     * that's affected.
     * <p>
     * This method takes the argument index and sets the position in the text 
     * string accordingly without checking if the index is pointing to a 
     * valid starting point to begin searching. 
     * @param position within the text to be set. If position is less
     * 			than or greater than the text range for searching, 
     *          an U_INDEX_OUTOFBOUNDS_ERROR will be returned
     * @param status for errors if it occurs
     * @draft ICU 2.0
     */
    virtual void setOffset(int32_t position, UErrorCode &status);

    /**
     * Return the current index in the text being searched.
     * If the iteration has gone past the end of the text
     * (or past the beginning for a backwards search), USEARCH_DONE
     * is returned.
     * @return current index in the text being searched.
     * @draft ICU 2.0
     */
    virtual int32_t getOffset(void) const;

    /**
     * Set the target text to be searched.
     * Text iteration will hence begin at the start of the text string. 
     * This method is 
     * useful if you want to re-use an iterator to search for the same 
     * pattern within a different body of text.
     * @param text text string to be searched
     * @param status for errors if any. If the text length is 0 then an 
     *        U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    virtual void setText(const UnicodeString &text, UErrorCode &status);
    
    /**
     * Set the target text to be searched.
     * Text iteration will hence begin at the start of the text string. 
     * This method is 
     * useful if you want to re-use an iterator to search for the same 
     * pattern within a different body of text.
     * Note: No parsing of the text within the <tt>CharacterIterator</tt> 
     * will be done during searching for this version. The block of text 
     * in <tt>CharacterIterator</tt> will be used as it is.
     * @param text text string to be searched
     * @param status for errors if any. If the text length is 0 then an 
     *        U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    virtual void setText(CharacterIterator &text, UErrorCode &status);

    /**
     * Gets the collator used for the language rules. 
     * <p>
     * Deleting the returned <tt>RuleBasedCollator</tt> before calling 
     * the destructor would cause the string search to fail.
     * The destructor will delete the collator if this instance owns it
     * @return collator used for string search
     * @draft ICU 2.0
     */
    RuleBasedCollator * getCollator() const;
    
    /**
     * Sets the collator used for the language rules. User retains the 
     * ownership of this collator, thus the responsibility of deletion lies 
     * with the user. This method causes internal data such as Boyer-Moore 
     * shift tables to be recalculated, but the iterator's position is 
     * unchanged.
     * @param coll    collator 
     * @param status  for errors if any
     * @draft ICU 2.0
     */
    void setCollator(RuleBasedCollator *coll, UErrorCode &status);
    
    /**
     * Sets the pattern used for matching.
     * Internal data like the Boyer Moore table will be recalculated, but 
     * the iterator's position is unchanged.
     * @param pattern search pattern to be found
     * @param status for errors if any. If the pattern length is 0 then an 
     *               U_ILLEGAL_ARGUMENT_ERROR is returned.
     * @draft ICU 2.0
     */
    void setPattern(const UnicodeString &pattern, UErrorCode &status);
    
    /**
     * Gets the search pattern.
     * @return pattern used for matching
     * @draft ICU 2.0
     */
    const UnicodeString & getPattern() const;

    // public methods ----------------------------------------------------

    /** 
     * Reset the iteration.
     * Search will begin at the start of the text string if a forward 
     * iteration is initiated before a backwards iteration. Otherwise if 
     * a backwards iteration is initiated before a forwards iteration, the 
     * search will begin at the end of the text string.
     * @draft ICU 2.0
     */
    virtual void reset();

    /**
     * Returns a copy of StringSearch with the same behavior, and 
     * iterating over the same text, as this one. Note that all data will be
     * replicated, except for the user-specified collator and the
     * breakiterator.
     * @return cloned object
     * @draft ICU 2.0
     */
    virtual SearchIterator * safeClone(void) const;
    
    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 2.2
     */
    virtual inline UClassID getDynamicClassID() const { return getStaticClassID(); }

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 2.2
     */
    static inline UClassID getStaticClassID() { return (UClassID)&fgClassID; }

protected:

    // protected method -------------------------------------------------

    /**
     * Search forward for matching text, starting at a given location.
     * Clients should not call this method directly; instead they should 
     * call {@link SearchIterator#next}.
     * <p>
     * If a match is found, this method returns the index at which the match
     * starts and calls {@link SearchIterator#setMatchLength} with the number 
     * of characters in the target text that make up the match. If no match 
     * is found, the method returns <tt>USEARCH_DONE</tt>.
     * <p>
     * The <tt>StringSearch</tt> is adjusted so that its current index 
     * (as returned by {@link #getOffset}) is the match position if one was 
     * found.
     * If a match is not found, <tt>USEARCH_DONE</tt> will be returned and
     * the <tt>StringSearch</tt> will be adjusted to the index USEARCH_DONE.
     * @param position The index in the target text at which the search 
     *                 starts
     * @param status for errors if any occurs
     * @return The index at which the matched text in the target starts, or 
     *         USEARCH_DONE if no match was found.
	 * @draft ICU 2.0.
     */
    virtual int32_t handleNext(int32_t position, UErrorCode &status);

    /**
     * Search backward for matching text, starting at a given location.
     * Clients should not call this method directly; instead they should call
     * <tt>SearchIterator.previous()</tt>, which this method overrides.
     * <p>
     * If a match is found, this method returns the index at which the match
     * starts and calls {@link SearchIterator#setMatchLength} with the number 
     * of characters in the target text that make up the match. If no match 
     * is found, the method returns <tt>USEARCH_DONE</tt>.
     * <p>
     * The <tt>StringSearch</tt> is adjusted so that its current index 
     * (as returned by {@link #getOffset}) is the match position if one was 
     * found.
     * If a match is not found, <tt>USEARCH_DONE</tt> will be returned and
     * the <tt>StringSearch</tt> will be adjusted to the index USEARCH_DONE.
     * @param position The index in the target text at which the search 
     *                 starts.
     * @param status for errors if any occurs
     * @return The index at which the matched text in the target starts, or 
     *         USEARCH_DONE if no match was found.
	 * @draft ICU 2.0.
     */
    virtual int32_t handlePrev(int32_t position, UErrorCode &status);
    
private :

    // private data members ----------------------------------------------

    /**
    * RuleBasedCollator, contains exactly the same UCollator * in m_strsrch_
	* @draft ICU 2.0.
    */
    RuleBasedCollator  m_collator_;
    /**
    * Pattern text
	* @draft ICU 2.0.
    */
    UnicodeString      m_pattern_;
    /**
    * Corresponding collation rules
	* @draft ICU 2.0.
    */
    UnicodeString      m_collation_rules_;
    /**
    * String search struct data
	* @draft ICU 2.0.
    */
    UStringSearch     *m_strsrch_;

    /**
     * The address of this static class variable serves as this class's ID
     * for ICU "poor man's RTTI".
     */
    static const char fgClassID;
};

U_NAMESPACE_END

#endif

