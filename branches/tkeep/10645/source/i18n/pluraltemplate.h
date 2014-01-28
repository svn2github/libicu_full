/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* pluraltemplate.h
*/

#ifndef __PLURAL_TEMPLATE_H__
#define __PLURAL_TEMPLATE_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class Template;
class UnicodeString;
class PluralRules;
class NumberFormat;
class Formattable;

/**
 * A plural aware template that is good for expressing a single quantity and
 * a unit.
 * <p>
 * First use the add() methods to add a pattern for each plural variant.
 * There must be a pattern for the "other" variant.
 * Then use the evaluate() method to evaluate the template.
 * <p>
 * Concurrent calls only to const methods on a PluralTemplate object are safe,
 * but concurrent const and non-const method calls on a PluralTemplate object
 * are not safe and require synchronization.
 * 
 */
class U_COMMON_API PluralTemplate : public UMemory {
// TODO(Travis Keep): Add test for copy constructor, assignment, and reset.
public:
    /**
     * Default constructor.
     */
    PluralTemplate();

    /**
     * Copy constructor.
     */
    PluralTemplate(const PluralTemplate& other);

    /**
     * Assignment operator
     */
    PluralTemplate &operator=(const PluralTemplate& other);

    /**
     * Destructor.
     */
    ~PluralTemplate();

    /**
     * Removes all variants from this object including the "other" variant.
     */
    void reset();

    /**
      * Adds a plural variant.
      *
      * @param pluralForm "zero", "one", "two", "few", "many", "other"
      * @param rawPattern the pattern for the variant e.g "{0} meters"
      * @param status any error returned here.
      * @return TRUE on success; FALSE otherwise.
      */
    UBool add(
            const char *pluralForm,
            const UnicodeString &rawPattern,
            UErrorCode &status);

    /**
     * Evaluates this object appending the result to appendTo.
     * At least the "other" variant must be added to this object for this
     * method to work.
     * 
     * @param quantity the single quantity.
     * @param rules computes the plural variant to use.
     * @param fmt formats the quantity
     * @param appendTo result appended here.
     * @param status any error returned here.
     * @return appendTo
     */
    UnicodeString &evaluate(
            const Formattable &quantity,
            const PluralRules &rules,
            const NumberFormat &fmt,
            UnicodeString &appendTo,
            UErrorCode &status) const;

private:
    Template *templates[6];
};

U_NAMESPACE_END

#endif
