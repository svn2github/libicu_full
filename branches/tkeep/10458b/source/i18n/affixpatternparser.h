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
class DecimalFormatSymbols;

/**
 * A representation of the various forms of a particular currency according
 * to some locale and usage context.
 * 
 * Includes the symbol, ISO code form, and long form(s) of the currency name
 * for each plural variation.
 */
class U_I18N_API CurrencyAffixInfo : public UMemory {
public:
    /**
     * Symbol is \u00a4; ISO form is \u00a4\u00a4;
     *  long form is \u00a4\u00a4\u00a4.
     */
    CurrencyAffixInfo();

    /**
     * The symbol form of the currency.
     */
    UnicodeString fSymbol;

    /**
     * The ISO form of the currency, usually three letter abbreviation.
     */
    UnicodeString fISO;

    /**
     * The long forms of the currency keyed by plural variation.
     */
    PluralAffix fLong;

    /**
     * Intializes this instance.
     *
     * @param locale the locale for the currency forms.
     * @param rules The plural rules for the locale. 
     * @param currency the null terminated, 3 character ISO code of the
     * currency. If NULL, resets this instance as if it were just created.
     * In this case, the first 2 parameters may be NULL as well.
     * @param status any error returned here.
     */
    void set(
            const char *locale, const PluralRules *rules,
            const UChar *currency, UErrorCode &status);

    /**
     * Adjusts the precision used for a particular currency.
     * @param currency the null terminated, 3 character ISO code of the
     * currency.
     * @param usage the usage of the currency
     * @param precision min/max fraction digits and rounding increment
     *  adjusted.
     * @params status any error reported here.
     */
    static void adjustPrecision(
            const UChar *currency, const UCurrencyUsage usage,
            FixedPrecision &precision, UErrorCode &status);
};


/**
 * A parser of a single affix pattern. Parses affix patterns produced from
 * using DecimalFormatPatternParser to parse a format pattern. Affix patterns
 * include the positive prefix and suffix and the negative prefix and suffix.
 * This class expects affix patterns to be in the same format that
 * DecimalFormatPatternParser produces. Namely special characters in the
 * affix that correspond to a field type must be prefixed with an
 * apostrophe ('). These special character sequences inluce minus (-),
 * percent (%), permile (U+2030), short currency (U+00a4),
 * medium currency (u+00a4 * 2), long currency (u+a4 * 3), and apostrophe (')
 * (apostrophe does not correspond to a field type but has to be escaped
 * because it itself is the escape character).
 * When parsing an affix pattern, this class translates these apostrophe
 * prefixed special character sequences to their equivalent in the given
 * locale using the DecimalFormatSymbols provided to the constructor.
 * If these special characters are not prefixed with an apostrophe in
 * the affix pattern, then they are treated verbatim just as any other
 * character. If an apostrophe prefixes a non special character in the
 * affix pattern, the apostrophe is simply ignored.
 */
class U_I18N_API AffixPatternParser : public UMemory {
public:
AffixPatternParser(const DecimalFormatSymbols &symbols);
void setDecimalFormatSymbols(const DecimalFormatSymbols &symbols);

/**
 * Contains the currency forms. Only needs to be initialized if the affixes
 * being parsed contain the currency symbol (U+00a4).
 */
CurrencyAffixInfo fCurrencyAffixInfo;

/**
 * Parses affixStr.
 * @param affixStr the input.
 * @param affix The result of parsing affixStr is appended here.
 * @param status any error returned here.
 */
int32_t parse(
        const UnicodeString &affixStr,
        PluralAffix &affix,
        UErrorCode &status) const;

/**
 * Determines if affixStr needs currencies. That is it determines if affixStr
 * has the currency symbol(s) (U+00a4) prefixed with apostrophe. Useful to
 * see if fCurrencyAffixInfo needs initializing.
 */
static UBool usesCurrencies(
        const UnicodeString &affixStr);
/**
 * Appends affixStr to appendTo while removing any escaping apostrophes and
 * returns appendTo. Used in figuring out the intended format width for
 * padding.
 */
static UnicodeString &unescape(
        const UnicodeString &affixStr, UnicodeString &appendTo);
private:
UnicodeString fPercent;
UnicodeString fPermill;
UnicodeString fNegative;
};


U_NAMESPACE_END

#endif  // __AFFIX_PATTERN_PARSER_H__
