/*
*******************************************************************************
* Copyright (C) 1996-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* rulebasedcollator.h
*
* created on: 2012feb14 with new and old collation code
* created by: Markus W. Scherer
*/

#ifndef __RULEBASEDCOLLATOR_H__
#define __RULEBASEDCOLLATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/ucol.h"

U_NAMESPACE_BEGIN

class CollationData;
class CollationElementIterator;

class U_I18N_API RuleBasedCollator2 : public Collator {
public:
    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator2(const UnicodeString& rules, UErrorCode& status);

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param collationStrength default strength for comparison
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator2(const UnicodeString& rules,
                       ECollationStrength collationStrength,
                       UErrorCode& status);

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param decompositionMode the normalisation mode
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator2(const UnicodeString& rules,
                    UColAttributeValue decompositionMode,
                    UErrorCode& status);

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param collationStrength default strength for comparison
     * @param decompositionMode the normalisation mode
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator2(const UnicodeString& rules,
                    ECollationStrength collationStrength,
                    UColAttributeValue decompositionMode,
                    UErrorCode& status);

    /**
     * Copy constructor.
     * @param other the RuleBasedCollator object to be copied
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator2(const RuleBasedCollator2& other);


    /** Opens a collator from a collator binary image created using
    *  cloneBinary. Binary image used in instantiation of the 
    *  collator remains owned by the user and should stay around for 
    *  the lifetime of the collator. The API also takes a base collator
    *  which usualy should be UCA.
    *  @param bin binary image owned by the user and required through the
    *             lifetime of the collator
    *  @param length size of the image. If negative, the API will try to
    *                figure out the length of the image
    *  @param base fallback collator, usually UCA. Base is required to be
    *              present through the lifetime of the collator. Currently 
    *              it cannot be NULL.
    *  @param status for catching errors
    *  @return newly created collator
    *  @see cloneBinary
    *  @stable ICU 3.4
    */
    RuleBasedCollator2(const uint8_t *bin, int32_t length, 
                    const RuleBasedCollator2 *base, 
                    UErrorCode &status);

    /**
     * Destructor.
     * @stable ICU 2.0
     */
    virtual ~RuleBasedCollator2();

    /**
     * Assignment operator.
     * @param other other RuleBasedCollator object to compare with.
     * @stable ICU 2.0
     */
    RuleBasedCollator2& operator=(const RuleBasedCollator2& other);

    /**
     * Returns true if argument is the same as this object.
     * @param other Collator object to be compared.
     * @return true if arguments is the same as this object.
     * @stable ICU 2.0
     */
    virtual UBool operator==(const Collator& other) const;

    /**
     * Returns true if argument is not the same as this object.
     * @param other Collator object to be compared
     * @return returns true if argument is not the same as this object.
     * @stable ICU 2.0
     */
    virtual UBool operator!=(const Collator& other) const;

    /**
     * Makes a deep copy of the object.
     * The caller owns the returned object.
     * @return the cloned object.
     * @stable ICU 2.0
     */
    virtual Collator* clone(void) const;

    /**
     * Creates a collation element iterator for the source string. The caller of
     * this method is responsible for the memory management of the return
     * pointer.
     * @param source the string over which the CollationElementIterator will
     *        iterate.
     * @return the collation element iterator of the source string using this as
     *         the based Collator.
     * @stable ICU 2.2
     */
    virtual CollationElementIterator* createCollationElementIterator(
                                           const UnicodeString& source) const;

    /**
     * Creates a collation element iterator for the source. The caller of this
     * method is responsible for the memory management of the returned pointer.
     * @param source the CharacterIterator which produces the characters over
     *        which the CollationElementItgerator will iterate.
     * @return the collation element iterator of the source using this as the
     *         based Collator.
     * @stable ICU 2.2
     */
    virtual CollationElementIterator* createCollationElementIterator(
                                         const CharacterIterator& source) const;

    /**
    * The comparison function compares the character data stored in two
    * different strings. Returns information about whether a string is less 
    * than, greater than or equal to another string.
    * @param source the source string to be compared with.
    * @param target the string that is to be compared with the source string.
    * @param status possible error code
    * @return Returns an enum value. UCOL_GREATER if source is greater
    * than target; UCOL_EQUAL if source is equal to target; UCOL_LESS if source is less
    * than target
    * @stable ICU 2.6
    **/
    virtual UCollationResult compare(const UnicodeString& source,
                                     const UnicodeString& target,
                                     UErrorCode &status) const;

    /**
    * Does the same thing as compare but limits the comparison to a specified 
    * length
    * @param source the source string to be compared with.
    * @param target the string that is to be compared with the source string.
    * @param length the length the comparison is limited to
    * @param status possible error code
    * @return Returns an enum value. UCOL_GREATER if source (up to the specified 
    *         length) is greater than target; UCOL_EQUAL if source (up to specified 
    *         length) is equal to target; UCOL_LESS if source (up to the specified 
    *         length) is less  than target.
    * @stable ICU 2.6
    */
    virtual UCollationResult compare(const UnicodeString& source,
                                     const UnicodeString& target,
                                     int32_t length,
                                     UErrorCode &status) const;

    /**
    * The comparison function compares the character data stored in two
    * different string arrays. Returns information about whether a string array 
    * is less than, greater than or equal to another string array.
    * @param source the source string array to be compared with.
    * @param sourceLength the length of the source string array.  If this value
    *        is equal to -1, the string array is null-terminated.
    * @param target the string that is to be compared with the source string.
    * @param targetLength the length of the target string array.  If this value
    *        is equal to -1, the string array is null-terminated.
    * @param status possible error code
    * @return Returns an enum value. UCOL_GREATER if source is greater
    * than target; UCOL_EQUAL if source is equal to target; UCOL_LESS if source is less
    * than target
    * @stable ICU 2.6
    */
    virtual UCollationResult compare(const UChar *left, int32_t leftLength,
                                     const UChar *right, int32_t rightLength,
                                     UErrorCode &errorCode) const;

    /**
    * Transforms a specified region of the string into a series of characters
    * that can be compared with CollationKey.compare. Use a CollationKey when
    * you need to do repeated comparisions on the same string. For a single
    * comparison the compare method will be faster.
    * @param source the source string.
    * @param key the transformed key of the source string.
    * @param status the error code status.
    * @return the transformed key.
    * @see CollationKey
    * @deprecated ICU 2.8 Use getSortKey(...) instead
    */
    virtual CollationKey& getCollationKey(const UnicodeString& source,
                                          CollationKey& key,
                                          UErrorCode& status) const;

    /**
    * Transforms a specified region of the string into a series of characters
    * that can be compared with CollationKey.compare. Use a CollationKey when
    * you need to do repeated comparisions on the same string. For a single
    * comparison the compare method will be faster.
    * @param source the source string.
    * @param sourceLength the length of the source string.
    * @param key the transformed key of the source string.
    * @param status the error code status.
    * @return the transformed key.
    * @see CollationKey
    * @deprecated ICU 2.8 Use getSortKey(...) instead
    */
    virtual CollationKey& getCollationKey(const UChar *source,
                                          int32_t sourceLength,
                                          CollationKey& key,
                                          UErrorCode& status) const;

    /**
     * Generates the hash code for the rule-based collation object.
     * @return the hash code.
     * @stable ICU 2.0
     */
    virtual int32_t hashCode() const;

    /**
    * Gets the locale of the Collator
    * @param type can be either requested, valid or actual locale. For more
    *             information see the definition of ULocDataLocaleType in
    *             uloc.h
    * @param status the error code status.
    * @return locale where the collation data lives. If the collator
    *         was instantiated from rules, locale is empty.
    * @deprecated ICU 2.8 likely to change in ICU 3.0, based on feedback
    */
    virtual const Locale getLocale(ULocDataLocaleType type, UErrorCode& status) const;

    /**
     * Gets the table-based rules for the collation object.
     * @return returns the collation rules that the table collation object was
     *         created from.
     * @stable ICU 2.0
     */
    // TODO: const UnicodeString& getRules(void) const;

    /**
     * Gets the version information for a Collator.
     * @param info the version # information, the result will be filled in
     * @stable ICU 2.0
     */
    virtual void getVersion(UVersionInfo info) const;

    /**
     * Return the maximum length of any expansion sequences that end with the
     * specified comparison order.
     * @param order a collation order returned by previous or next.
     * @return maximum size of the expansion sequences ending with the collation
     *         element or 1 if collation element does not occur at the end of
     *         any expansion sequence
     * @see CollationElementIterator#getMaxExpansion
     * @stable ICU 2.0
     */
    // TODO: int32_t getMaxExpansion(int32_t order) const;

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     * @return The class ID for this object. All objects of a given class have
     *         the same class ID. Objects of other classes have different class
     *         IDs.
     * @stable ICU 2.0
     */
    virtual UClassID getDynamicClassID(void) const;

    /**
     * Returns the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * Base* polymorphic_pointer = createPolymorphicObject();
     * if (polymorphic_pointer->getDynamicClassID() ==
     *                                          Derived::getStaticClassID()) ...
     * </pre>
     * @return The class ID for all objects of this class.
     * @stable ICU 2.0
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns the binary format of the class's rules. The format is that of
     * .col files.
     * @param length Returns the length of the data, in bytes
     * @param status the error code status.
     * @return memory, owned by the caller, of size 'length' bytes.
     * @stable ICU 2.2
     */
    // TODO: uint8_t *cloneRuleData(int32_t &length, UErrorCode &status);


    /** Creates a binary image of a collator. This binary image can be stored and 
    *  later used to instantiate a collator using ucol_openBinary.
    *  This API supports preflighting.
    *  @param buffer a fill-in buffer to receive the binary image
    *  @param capacity capacity of the destination buffer
    *  @param status for catching errors
    *  @return size of the image
    *  @see ucol_openBinary
    *  @stable ICU 3.4
    */
    // TODO: int32_t cloneBinary(uint8_t *buffer, int32_t capacity, UErrorCode &status);

    /**
     * Returns current rules. Delta defines whether full rules are returned or
     * just the tailoring.
     * @param delta one of UCOL_TAILORING_ONLY, UCOL_FULL_RULES.
     * @param buffer UnicodeString to store the result rules
     * @stable ICU 2.2
     */
    // TODO: void getRules(UColRuleOption delta, UnicodeString &buffer);

    /**
     * Universal attribute setter
     * @param attr attribute type
     * @param value attribute value
     * @param status to indicate whether the operation went on smoothly or there were errors
     * @stable ICU 2.2
     */
    virtual void setAttribute(UColAttribute attr, UColAttributeValue value,
                              UErrorCode &status);

    /**
     * Universal attribute getter.
     * @param attr attribute type
     * @param status to indicate whether the operation went on smoothly or there were errors
     * @return attribute value
     * @stable ICU 2.2
     */
    virtual UColAttributeValue getAttribute(UColAttribute attr,
                                            UErrorCode &status);

    /**
     * Sets the variable top to a collation element value of a string supplied.
     * @param varTop one or more (if contraction) UChars to which the variable top should be set
     * @param len length of variable top string. If -1 it is considered to be zero terminated.
     * @param status error code. If error code is set, the return value is undefined. Errors set by this function are: <br>
     *    U_CE_NOT_FOUND_ERROR if more than one character was passed and there is no such a contraction<br>
     *    U_PRIMARY_TOO_LONG_ERROR if the primary for the variable top has more than two bytes
     * @return a 32 bit value containing the value of the variable top in upper 16 bits. Lower 16 bits are undefined
     * @stable ICU 2.0
     */
    virtual uint32_t setVariableTop(const UChar *varTop, int32_t len, UErrorCode &status);

    /**
     * Sets the variable top to a collation element value of a string supplied.
     * @param varTop an UnicodeString size 1 or more (if contraction) of UChars to which the variable top should be set
     * @param status error code. If error code is set, the return value is undefined. Errors set by this function are: <br>
     *    U_CE_NOT_FOUND_ERROR if more than one character was passed and there is no such a contraction<br>
     *    U_PRIMARY_TOO_LONG_ERROR if the primary for the variable top has more than two bytes
     * @return a 32 bit value containing the value of the variable top in upper 16 bits. Lower 16 bits are undefined
     * @stable ICU 2.0
     */
    virtual uint32_t setVariableTop(const UnicodeString varTop, UErrorCode &status);

    /**
     * Sets the variable top to a collation element value supplied. Variable top is set to the upper 16 bits.
     * Lower 16 bits are ignored.
     * @param varTop CE value, as returned by setVariableTop or ucol)getVariableTop
     * @param status error code (not changed by function)
     * @stable ICU 2.0
     */
    virtual void setVariableTop(const uint32_t varTop, UErrorCode &status);

    /**
     * Gets the variable top value of a Collator.
     * Lower 16 bits are undefined and should be ignored.
     * @param status error code (not changed by function). If error code is set, the return value is undefined.
     * @stable ICU 2.0
     */
    virtual uint32_t getVariableTop(UErrorCode &status) const;

    /**
     * Get an UnicodeSet that contains all the characters and sequences tailored in 
     * this collator.
     * @param status      error code of the operation
     * @return a pointer to a UnicodeSet object containing all the 
     *         code points and sequences that may sort differently than
     *         in the UCA. The object must be disposed of by using delete
     * @stable ICU 2.4
     */
    virtual UnicodeSet *getTailoredSet(UErrorCode &status) const;

    /**
     * Thread safe cloning operation.
     * @return pointer to the new clone, user should remove it.
     * @stable ICU 2.2
     */
    virtual Collator* safeClone();

    /**
     * Get the sort key as an array of bytes from an UnicodeString.
     * @param source string to be processed.
     * @param result buffer to store result in. If NULL, number of bytes needed
     *        will be returned.
     * @param resultLength length of the result buffer. If if not enough the
     *        buffer will be filled to capacity.
     * @return Number of bytes needed for storing the sort key
     * @stable ICU 2.0
     */
    virtual int32_t getSortKey(const UnicodeString& source, uint8_t *result,
                               int32_t resultLength) const;

    /**
     * Get the sort key as an array of bytes from an UChar buffer.
     * @param source string to be processed.
     * @param sourceLength length of string to be processed. If -1, the string
     *        is 0 terminated and length will be decided by the function.
     * @param result buffer to store result in. If NULL, number of bytes needed
     *        will be returned.
     * @param resultLength length of the result buffer. If if not enough the
     *        buffer will be filled to capacity.
     * @return Number of bytes needed for storing the sort key
     * @stable ICU 2.2
     */
    virtual int32_t getSortKey(const UChar *source, int32_t sourceLength,
                               uint8_t *result, int32_t resultLength) const;

    /** TODO: Move to base class.
    * Determines the minimum strength that will be use in comparison or
    * transformation.
    * <p>E.g. with strength == SECONDARY, the tertiary difference is ignored
    * <p>E.g. with strength == PRIMARY, the secondary and tertiary difference
    * are ignored.
    * @return the current comparison level.
    * @see RuleBasedCollator#setStrength
    * @deprecated ICU 2.6 Use getAttribute(UCOL_STRENGTH...) instead
    */
    virtual ECollationStrength getStrength(void) const;

    /** TODO: Move to base class.
    * Sets the minimum strength to be used in comparison or transformation.
    * @see RuleBasedCollator#getStrength
    * @param newStrength the new comparison level.
    * @deprecated ICU 2.6 Use setAttribute(UCOL_STRENGTH...) instead
    */
    virtual void setStrength(ECollationStrength newStrength);

    /**
     * Retrieves the reordering codes for this collator.
     * @param dest The array to fill with the script ordering.
     * @param destCapacity The length of dest. If it is 0, then dest may be NULL and the function
     *  will only return the length of the result without writing any of the result string (pre-flighting).
     * @param status A reference to an error code value, which must not indicate
     * a failure before the function call.
     * @return The length of the script ordering array.
     * @see ucol_setReorderCodes
     * @see Collator#getEquivalentReorderCodes
     * @see Collator#setReorderCodes
     * @draft ICU 4.8 
     */
     virtual int32_t getReorderCodes(int32_t *dest,
                                    int32_t destCapacity,
                                    UErrorCode& status) const;

    /**
     * Sets the ordering of scripts for this collator.
     * @param reorderCodes An array of script codes in the new order. This can be NULL if the 
     * length is also set to 0. An empty array will clear any reordering codes on the collator.
     * @param reorderCodesLength The length of reorderCodes.
     * @param status error code
     * @see Collator#getReorderCodes
     * @see Collator#getEquivalentReorderCodes
     * @draft ICU 4.8 
     */
     virtual void setReorderCodes(const int32_t* reorderCodes,
                                int32_t reorderCodesLength,
                                UErrorCode& status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Retrieves the reorder codes that are grouped with the given reorder code. Some reorder
     * codes will be grouped and must reorder together.
     * @param reorderCode The reorder code to determine equivalence for. 
     * @param dest The array to fill with the script equivalene reordering codes.
     * @param destCapacity The length of dest. If it is 0, then dest may be NULL and the 
     * function will only return the length of the result without writing any of the result 
     * string (pre-flighting).
     * @param status A reference to an error code value, which must not indicate 
     * a failure before the function call.
     * @return The length of the of the reordering code equivalence array.
     * @see ucol_setReorderCodes
     * @see Collator#getReorderCodes
     * @see Collator#setReorderCodes
     * @draft ICU 4.8 
     */
    static int32_t getEquivalentReorderCodes(int32_t reorderCode,
                                int32_t* dest,
                                int32_t destCapacity,
                                UErrorCode& status);
#endif  /* U_HIDE_DRAFT_API */

public:  // TODO: Public only for testing.
    RuleBasedCollator2(const CollationData *d)
            : data(d), defaultData(d), ownedData(NULL) {}
    RuleBasedCollator2(const CollationData *d, CollationData *od)
            : data(d), defaultData(d), ownedData(od) {}

private:
    UCollationResult compare(const UChar *left, int32_t leftLength,
                             const UChar *right, int32_t rightLength,
                             UBool maybeNUL,
                             UErrorCode &errorCode) const;

    const CollationData *data;  // == defaultData or ownedData
    const CollationData *defaultData;
    CollationData *ownedData;  // NULL until cloned from defaultData & modified
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __RULEBASEDCOLLATOR_H__
