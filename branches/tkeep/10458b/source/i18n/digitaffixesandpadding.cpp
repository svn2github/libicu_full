/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitformatter.cpp
 */

#include "digitaffixesandpadding.h"

#include "digitlst.h"
#include "digitaffix.h"
#include "valueformatter.h"
#include "uassert.h"
#include "charstr.h"

U_NAMESPACE_BEGIN

UBool
DigitAffixesAndPadding::needsPluralRules() const {
    return (
            fPositivePrefix.hasMultipleVariants() ||
            fPositiveSuffix.hasMultipleVariants() ||
            fNegativePrefix.hasMultipleVariants() ||
            fNegativeSuffix.hasMultipleVariants());
}

UnicodeString &
DigitAffixesAndPadding::format(
        DigitList &value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo) const {
    formatter.round(value);
    UBool bPositive = value.isPositive();
    const PluralAffix *pluralPrefix = bPositive ? &fPositivePrefix : &fNegativePrefix;
    const PluralAffix *pluralSuffix = bPositive ? &fPositiveSuffix : &fNegativeSuffix;
    
    const DigitAffix *prefix;
    const DigitAffix *suffix;
    if (optPluralRules == NULL) {
        prefix = &pluralPrefix->getOtherVariant();
        suffix = &pluralSuffix->getOtherVariant();
    } else {
        UnicodeString count;
        count = formatter.select(*optPluralRules, value);
        CharString buffer;
        UErrorCode status = U_ZERO_ERROR;
        buffer.appendInvariantChars(count, status);
        prefix = &pluralPrefix->getByVariant(buffer.data());
        suffix = &pluralSuffix->getByVariant(buffer.data());
    }
    value.setPositive(TRUE);
    int32_t codePointCount = prefix->countChar32() + formatter.countChar32(value) + suffix->countChar32();
    int32_t paddingCount = fWidth - codePointCount;
    switch (fPadPosition) {
    case kPadBeforePrefix:
        appendPadding(paddingCount, appendTo);
        prefix->format(handler, appendTo);
        formatter.format(value, handler, appendTo);
        return suffix->format(handler, appendTo);
    case kPadAfterPrefix:
        prefix->format(handler, appendTo);
        appendPadding(paddingCount, appendTo);
        formatter.format(value, handler, appendTo);
        return suffix->format(handler, appendTo);
    case kPadBeforeSuffix:
        prefix->format(handler, appendTo);
        formatter.format(value, handler, appendTo);
        appendPadding(paddingCount, appendTo);
        return suffix->format(handler, appendTo);
    case kPadAfterSuffix:
        prefix->format(handler, appendTo);
        formatter.format(value, handler, appendTo);
        suffix->format(handler, appendTo);
        return appendPadding(paddingCount, appendTo);
    default:
        U_ASSERT(FALSE);
        return appendTo;
    }
}

UnicodeString &
DigitAffixesAndPadding::appendPadding(int32_t paddingCount, UnicodeString &appendTo) const {
    for (int32_t i = 0; i < paddingCount; ++i) {
        appendTo.append(fPadChar);
    }
    return appendTo;
}


U_NAMESPACE_END

