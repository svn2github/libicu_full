/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* affixpatternparser.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __AFFIX_PATTERN_PARSER_H__
#define __AFFIX_PATTERN_PARSER_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "pluralaffix.h"

U_NAMESPACE_BEGIN

class PluralRules;
class FixedPrecision;

class U_I18N_API CurrencyAffixInfo : public UMemory {
public:
    UnicodeString fSymbol;
    UnicodeString fISO;
    PluralAffix fLong;
    void set(
            const char *locale, const PluralRules *rules,
            const UChar *currency, UErrorCode &status);
    static void adjustPrecision(
            const UChar *currency, const UCurrencyUsage usage,
            FixedPrecision &precision, UErrorCode &status);
};


class U_I18N_API AffixPatternParser : public UMemory {
public:
CurrencyAffixInfo fCurrencyAffixInfo;
int32_t parse(
        const UnicodeString &affixStr,
        PluralAffix &affix,
        UErrorCode &status) const;
static UBool usesCurrencies(
        const UnicodeString &affixStr);
};


U_NAMESPACE_END

#endif  // __AFFIX_PATTERN_PARSER_H__
