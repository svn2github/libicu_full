/*
*******************************************************************************
*
*   Copyright (C) 2010 International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*/

#ifndef INDEXCHARS_H
#define INDEXCHARS_H

#include "unicode/uobject.h"
#include "unicode/locid.h"

/**
 * \file 
 * \brief C++ API: Index Characters
 */
 
U_NAMESPACE_BEGIN

// Forward Declarations

class Collator;
class StringEnumeration;
class UnicodeSet;
class UVector;

/**
 * A set of characters for use as a UI "index", that is, a
 * list of clickable characters (or character sequences) that allow the user to
 * see a segment of a larger "target" list. That is, each character corresponds
 * to a bucket in the target list, where everything in the bucket is greater
 * than or equal to the character (according to the locale's collation). The
 * intention is to have two main functions; one that produces an index list that
 * is relatively static, and the other is a list that produces roughly
 * equally-sized buckets. Only the first is currently provided.
 * <p>
 * The static list would be presented as something like
 * 
 * <pre>
 *  A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
 * </pre>
 * 
 * In the UI, an index character could be omitted if its bucket is empty. For
 * example, if there is nothing in the bucket for Q, then Q could be omitted.
 * <p>
 * <b>Important Notes:</b>
 * <ul>
 * <li>Although we say "character" above, the index character could be a
 * sequence, like "CH".</li>
 * <li>There could be items in a target list that are less than the first or
 * (much) greater than the last; examples include words from other scripts. The
 * UI could bucket them with the first or last respectively, or have some symbol
 * for those categories.</li>
 * <li>The use of the list requires that the target list be sorted according to
 * the locale that is used to create that list.</li>
 * <li>For languages without widely accepted sorting methods (eg Chinese/Japanese)
 * the results may appear arbitrary, and it may be best not to use these methods.</li>
 * <li>In the initial version, an arbitrary limit of 100 is placed on these lists.</li>
 * </ul>
 * 
 * @draft ICU 4.6
 * @provisional This API might change or be removed in a future release.
 */
class U_I18N_API IndexCharacters: public UObject {

  public:

    /**
     * Construct an IndexCharacters object for the specified locale.  If the locale's
     * data does not include index characters, a set of them will be
     * synthesized based on the locale's exemplar characters.
     *
     * @param locale the desired locale.
     * @param status Error code, will be set with the reason if the construction
     *               of the IndexCharacters object fails.
     * @draft ICU 4.6
     */
     IndexCharacters(const Locale &locale, UErrorCode &status);


    /**
     * Construct an IndexCharacters object for the specified locale, and
     * add an additional set of index characters.  If the locale's
     * data does not include index characters, a set of them will be
     * synthesized based on the locale's exemplar characters.
     * 
     * @param locale
     *            The locale to be passed.
     * @param additions
     *            Additional characters to be added, eg A-Z for non-Latin locales.
     * @param status Error code, will be set with the reason if the construction
     *               of the IndexCharacters object fails.
     * @draft ICU 4.6
     * @provisional This API might change or be removed in a future release.
     */
     IndexCharacters(const Locale &locale, const UnicodeSet &additions, UErrorCode &status);


#if 0
    TODO:  Late addtion to Java.  How best to do this here?

     /**
     * Create the index object.
     * 
     * @param locale
     *            The locale to be passed.
     * @param additions
     *            Additional characters to be added, eg A-Z for non-Latin locales.
     * @draft ICU 4.4
     * @provisional This API might change or be removed in a future release.
     */
    public IndexCharacters(ULocale locale, ULocale... additionalLocales) {
#endif


     /**
      * Copy constructor
      *
      * @param other  The source IndexCharacters object.
      * @param status Error code, will be set with the reason if the construction fails.
      * @draft ICU 4.6
      */
     IndexCharacters(const IndexCharacters &other, UErrorCode &status);

     /**
      * Destructor
      */
     virtual ~IndexCharacters();


    /**
     * Equality operator.
     * @draft ICU 4.6
     */
     virtual UBool operator==(const IndexCharacters& other) const;

    /**
     * Inequality operator.
     * @draft ICU 4.6
     */
     virtual UBool operator!=(const IndexCharacters& other) const;



    /**
     * Get a String Enumeration that will produce the index characters.
     * @return A String Enumeration over the index characters.
     * @draft ICU 4.6
     */
     virtual StringEnumeration *getIndexCharacters() const;

   /**
     * Get the locale.
     * TODO: The one specified?  The one actually used?  For collation or exepmplars?
     * @return The locale.
     * @draft ICU 4.6
     */
    virtual Locale getLocale() const;


    /**
     * Get the Collator that establishes the ordering of the index characters.
     * Ownership of the collator remains with the IndexCharacters instance.
     * @return The collator
     * @draft ICU 4.6
     */
    virtual const Collator &getCollator() const;

   /**
     * Get the default label used for abbreviated buckets <i>between</i> other index characters. 
     * For example, consider the index characters for Latin and Greek are used: 
     *     X Y Z ... &#x0391; &#x0392; &#x0393;.
     * 
     * @param status Error code, will be set with the reason if the operation fails.
     * @return inflow label
     * @draft ICU 4.6
     */
    virtual UnicodeString getInflowLabel(UErrorCode &status) const;


   /**
     * Get the default label used in the IndexCharacters' locale for overflow, eg the first item in:
     *     ... A B C
     * 
     * @param status Error code, will be set with the reason if the operation fails.
     * @return overflow label
     * @draft ICU 4.6
     */
    virtual UnicodeString getOverflowLabel(UErrorCode &status) const;


   /**
     * Get the default label used in the IndexCharacters' locale for underflow, eg the last item in:
     *    X Y Z ...
     * 
     * @param status Error code, will be set with the reason if the operation fails.
     * @return underflow label
     * @draft ICU 4.6
     */
    virtual UnicodeString getUnderflowLabel(UErrorCode &status) const;


    /**
     * Get the Unicode character (or tailored string) that defines an overflow bucket; that is anything greater than or
     * equal to that string should go in that bucket, instead of with the last character. Normally that is the first
     * character of the script after lowerLimit. Thus in X Y Z ... <i>Devanagari-ka</i>, the overflow character for Z
     * would be the <i>Greek-alpha</i>.
     * 
     * @param lowerLimit   The character below the overflow (or inflow) bucket
     * @return string that defines top of the overflow buck for lowerLimit, or null if there is none
     * @draft ICU 4.6
     */
    virtual UnicodeString getOverflowComparisonString(UnicodeString lowerLimit);
    

    /**
     *   Convenience class for building and enumerating an Index.  Bundles together a set of 
     *   index characters and, for each index character, a set of value strings that sort to 
     *   (should be displayed under) that index character.
     *   @draft ICU 4.6
     */
    class Index: public UObject {
      public:
        /**
         * Constructor.  
         * @param indexChars  An IndexCharacters defining the set of labels under which
         *                      entries of this index will be placed.
         * @param status      Error code, will be set with the reason if the operation fails.
         * @draft ICU 4.6
         */
        Index(const IndexCharacters &indexChars, UErrorCode &status);
        virtual ~Index();

        /**
         * Add a value string to the index.  It will be bucketed to the appropriate
         * index character.
         *
         * @param value      a value (string) to be place into the index.
         * @param status      Error code, will be set with the reason if the operation fails.
         * @draft ICU 4.6
         */
        virtual void addValue(UnicodeString value, UErrorCode &status);

        /**
         * Get an enumeration over all of the index labels that have at least
         * one value string associated with them.  The index characters will be returned
         * in their sorted order, except for overflow, underflow or inflow labels, if
         * present.
         *
         * @return  A string enumeration over the labels for this index.
         * @draft ICU 4.6
         */
        virtual StringEnumeration *getLabels() const;

        /**
         * Get an enumeration over the set of value strings associated with the label
         * most recently returned by the supplied labelEnum.
         * The value strings will be produced in sorted order.
         *
         * @param labelEnum  A stringEnumeration produced by getLabels().  The value strings
         *                   produced will be those for the label most recently returned by
         *                   the labelEnum.
         *
         * @param status     Error code, will be set if the operation fails.
         *                   Set to U_ILLEGAL_ARGUEMENT_ERROR if the labelEnum is not
         *                   associated with this Index, or if it is positioned before the
         *                   first label or after the last label (having returned NULL).
         *
         * @return           A StringEnumeration over the values associated with the label
         *                   most recently produced by the labelEnum.
         *
         * @draft ICU 4.6
         */
        virtual StringEnumeration *getValues(const StringEnumeration &labelEnum, UErrorCode &status);
    };

        

  private:
     /**
      *   No assignment.  IndexCharacters objects are const after creation/
      */
     IndexCharacters &operator =(const IndexCharacters & /*other*/) { return *this;};

     // Common initialization, for use from all constructors.
     void init(UErrorCode &status);

     void firstStringsInScript(UVector *dest, Collator *coll, UErrorCode &status);

     UChar32 OVERFLOW_MARKER;
     UChar32 INFLOW_MARKER;
     UChar32 CGJ;
     UnicodeSet *ALPHABETIC;
     UnicodeSet *HANGUL;
     UnicodeSet *ETHIOPIC;
     UnicodeSet *CORE_LATIN;
     UnicodeSet *IGNORE_SCRIPTS;
     UnicodeSet *TO_TRY;
     UVector    *FIRST_CHARS_IN_SCRIPTS;

     // LinkedHashMap<String, Set<String>> alreadyIn = new LinkedHashMap<String, Set<String>>();
     UVector *indexCharacters_;   // Contents are (UnicodeString *)
     UVector *noDistinctSorting_;
     UVector *notAlphabetic_;
     UVector *firstScriptCharacters_;

     Locale    locale_;
     Collator  *comparator_;

     UnicodeString  inflowLabel_;
     UnicodeString  overflowLabel_;
     UnicodeString  underflowLabel_;
     UnicodeString  overflowComparisonString_;


};

U_NAMESPACE_END
#endif

