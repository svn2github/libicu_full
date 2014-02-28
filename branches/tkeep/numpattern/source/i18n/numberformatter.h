/*
********************************************************************************
*   Copyright (C) 2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*/
#ifndef NUMBERFORMATTER_H
#define NUMBERFORMATTER_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class NumberFormat;
class IntFormatter;
class Formattable;
class PluralRules;
class FieldPosition;

class U_I18N_API NumberFormatter : public UMemory {
public:
    NumberFormatter(const NumberFormat &nf);
    NumberFormatter(const IntFormatter &intFormatter);
    NumberFormatter(const NumberFormat &nf, const IntFormatter &intFormatter);
    void useNumberFormat(const NumberFormat &nf);
    void useIntFormatter(const IntFormatter &other);
    void useNumberFormatIntFormatter(const NumberFormat &nf, const IntFormatter &intFormatter);
    UnicodeString &select(
            const Formattable &quantity,
            const PluralRules &rules,
            UnicodeString &result,
            UErrorCode &status) const;
    UnicodeString &format(
            const Formattable &quantity,
            UnicodeString &appendTo, 
            FieldPosition &pos,
            UErrorCode &status) const;
private:
    const NumberFormat *nf;
    const IntFormatter *intFormatter;
    NumberFormatter(const NumberFormatter &other);
    NumberFormatter &operator=(const NumberFormatter &other);
    UBool shouldUseNumberFormat(
            const Formattable &quantity) const;
};


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // NUMBERFORMATTER_H
//eof
