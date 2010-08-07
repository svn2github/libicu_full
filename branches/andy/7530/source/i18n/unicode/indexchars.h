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
     IndexCharacters(Locale locale, UErrorCode &status);


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
     IndexCharacters(Locale locale, const UnicodeSet &additions, UErrorCode &status);

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
     * Get the locale
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

    class Bucket: public UObject {
      public:
        Bucket(UnicodeString label);
        virtual ~Bucket();

        UnicodeString  getLabel();
        virtual void add(UnicodeString value, UErrorCode &status);
        StringEnumeration *getValues();
    }

    virtual 

        

  private:
     /**
      *   No assignment.  IndexCharacters objects are const after creation/
      */
     IndexCharacters &operator =(const IndexCharacters & /*other*/) { return *this;};
};

U_NAMESPACE_END
#endif

