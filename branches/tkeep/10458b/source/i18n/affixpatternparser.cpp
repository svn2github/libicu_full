/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: affixpatternparser.cpp
 */

#include "affixpatternparser.h"

#include "charstr.h"
#include "unicode/ucurr.h"
#include "unicode/plurrule.h"
#include "precision.h"
#include "unicode/dcfmtsym.h"

U_NAMESPACE_BEGIN

static int32_t
nextToken(const UChar *buffer, int32_t idx, int32_t len, UChar *token) {
    if (buffer[idx] != 0x27 || idx + 1 == len) {
        *token = buffer[idx];
        return 1;
    }
    *token = buffer[idx + 1];
    if (buffer[idx + 1] == 0xA4) {
        int32_t i = 2;
        for (; idx + i < len && i < 4 && buffer[idx + i] == buffer[idx + 1]; ++i);
        return i;
    }
    return 2;
}

CurrencyAffixInfo::CurrencyAffixInfo() {
    UErrorCode status = U_ZERO_ERROR;
    set(NULL, NULL, NULL, status);
}

void
CurrencyAffixInfo::set(
        const char *locale,
        const PluralRules *rules,
        const UChar *currency,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    UChar theCurrency[4];
    if (currency) {
        u_strncpy(theCurrency, currency, 3);
        theCurrency[3] = 0;
    } else {
        theCurrency[0] = 0;
    }
    if (theCurrency[0] == 0) {
        static UChar defaultSymbols[] = {0xa4, 0xa4, 0xa4};
        fSymbol.setTo(defaultSymbols, 1);
        fISO.setTo(defaultSymbols, 2);
        fLong.remove();
        fLong.append(UnicodeString(defaultSymbols, 3));
        return;
    }
    int32_t len;
    UBool unusedIsChoice;
    const UChar *symbol = ucurr_getName(
            theCurrency, locale, UCURR_SYMBOL_NAME, &unusedIsChoice,
            &len, &status);
    if (U_FAILURE(status)) {
        return;
    }
    fSymbol.setTo(symbol, len);
    fISO.setTo(theCurrency, u_strlen(theCurrency));
    fLong.remove();
    StringEnumeration* keywords = rules->getKeywords(status);
    if (U_FAILURE(status)) {
        return;
    }
    const UnicodeString* pluralCount;
    while ((pluralCount = keywords->snext(status)) != NULL) {
        CharString pCount;
        pCount.appendInvariantChars(*pluralCount, status);
        const UChar *pluralName = ucurr_getPluralName(
            theCurrency, locale, &unusedIsChoice, pCount.data(),
            &len, &status);
        fLong.setVariant(pCount.data(), UnicodeString(pluralName, len), status);
    }
    delete keywords;
}

void
CurrencyAffixInfo::adjustPrecision(
        const UChar *currency, const UCurrencyUsage usage,
        FixedPrecision &precision, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t digitCount = ucurr_getDefaultFractionDigitsForUsage(
            currency, usage, &status);
    precision.fMin.setFracDigitCount(digitCount);
    precision.fMax.setFracDigitCount(digitCount);
    double increment = ucurr_getRoundingIncrementForUsage(
            currency, usage, &status);
    if (increment == 0.0) {
        precision.fRoundingIncrement.clear();
    } else {
        precision.fRoundingIncrement.set(increment);
        // guard against round-off error
        precision.fRoundingIncrement.round(6);
    }
}

AffixPatternParser::AffixPatternParser(
        const DecimalFormatSymbols &symbols) {
    setDecimalFormatSymbols(symbols);
}

void
AffixPatternParser::setDecimalFormatSymbols(
        const DecimalFormatSymbols &symbols) {
    fPercent = symbols.getConstSymbol(DecimalFormatSymbols::kPercentSymbol);
    fPermill = symbols.getConstSymbol(DecimalFormatSymbols::kPerMillSymbol);
    fNegative = symbols.getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol);
}

UBool
AffixPatternParser::usesCurrencies(const UnicodeString &affixStr) {
    const UChar *buffer = affixStr.getBuffer();
    int32_t len = affixStr.length();
    for (int32_t i = 0; i < len; ) {
        UChar token;
        int32_t tokenSize = nextToken(buffer, i, len, &token);
        i += tokenSize;
        if (tokenSize == 1) {
            continue;
        }
        if (token == 0xa4) {
            return TRUE;
        }
    }
    return FALSE;
}

int32_t
AffixPatternParser::parse(
        const UnicodeString &affixStr,
        PluralAffix &affix, 
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return 0;
    }
    int32_t result = 0;
    int32_t len = affixStr.length();
    const UChar *buffer = affixStr.getBuffer();
    for (int32_t i = 0; i < len; ) {
        UChar token;
        int32_t tokenSize = nextToken(buffer, i, len, &token);
        i += tokenSize;
        if (tokenSize == 1) {
            affix.appendUChar(token);
            continue;
        }
        switch (token) {
        case 0x25:
            affix.append(fPercent, UNUM_PERCENT_FIELD);
            result = 2;
            break;
        case 0x2030:
            affix.append(fPermill, UNUM_PERMILL_FIELD);
            result = 3;
            break;
        case 0x2D:
            affix.append(fNegative, UNUM_SIGN_FIELD);
            break;
        case 0xA4:
        {
            switch (tokenSize - 1) { // subtract 1 for leading single quote
                case 1:
                    affix.append(
                            fCurrencyAffixInfo.fSymbol, UNUM_CURRENCY_FIELD);
                    break;
                case 2:
                    affix.append(
                            fCurrencyAffixInfo.fISO, UNUM_CURRENCY_FIELD);
                    break;
                case 3:
                    affix.append(
                            fCurrencyAffixInfo.fLong, UNUM_CURRENCY_FIELD, status);
                    break;
                default:
                    status = U_PARSE_ERROR;
                    return 0;
            }
            break;
        }
        default:
            affix.appendUChar(token);
            break;
        }
    }
    return result;
}


U_NAMESPACE_END

