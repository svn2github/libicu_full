/*
********************************************************************************
*   Copyright (C) 2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*/
#ifndef NUMBERINTFORMATTER_H
#define NUMBERINTFORMATTER_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class NumberFormat;
class IntFormatter;
class Formattable;
class PluralRules;
class FieldPosition;
class UnicodeString;

/**
 * NumberIntFormatter delegates to an IntFormatter to format int32_t and
 * delegates to a NumberFormat to format everything else. NumberIntFormatter
 * instances borrow its delegates instead of copying them, so the delegates
 * must outlive their enclosing instance.
 */
class U_I18N_API NumberIntFormatter : public UMemory {
public:
    NumberIntFormatter(
            const NumberFormat &nfToBorrow, const IntFormatter &intFToBorrow) :
            numberFormat(nfToBorrow), intFormatter(intFToBorrow) { }

    /**
     * Selects the plural rule. 
     */
    UnicodeString &select(
            const Formattable &quantity,
            const PluralRules &rules,
            UnicodeString &result,
            UErrorCode &status) const;

    /**
     * Does the formatting.
     */
    UnicodeString &format(
            const Formattable &quantity,
            UnicodeString &appendTo, 
            FieldPosition &pos,
            UErrorCode &status) const;
private:
    const NumberFormat &numberFormat;
    const IntFormatter &intFormatter;
    NumberIntFormatter(const NumberIntFormatter &other);
    NumberIntFormatter &operator=(const NumberIntFormatter &other);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // NUMBERINTFORMATTER_H
//eof
