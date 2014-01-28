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
 */
class U_COMMON_API PluralTemplate : public UMemory {
public:
    /**
     * Default constructor
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
     */
    UBool add(
            const char *pluralForm,
            const UnicodeString &rawPattern,
            UErrorCode &status);
    /**
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
