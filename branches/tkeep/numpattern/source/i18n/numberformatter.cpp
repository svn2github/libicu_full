/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numberformatter.h"
#include "intformatter.h"
#include "unicode/decimfmt.h"
#include "unicode/plurrule.h"
#include "plurrule_impl.h"
#include "unicode/decimfmt.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

NumberFormatter::NumberFormatter(const NumberFormat &nf) {
    useNumberFormat(nf);
}

NumberFormatter::NumberFormatter(const IntFormatter &nf) {
    useIntFormatter(nf);
}

NumberFormatter::NumberFormatter(
        const NumberFormat &nf, const IntFormatter &intFormatter) {
    useNumberFormatIntFormatter(nf, intFormatter);
}

void NumberFormatter::useNumberFormat(const NumberFormat &n) {
    nf = &n;
    intFormatter = NULL;
}

void NumberFormatter::useIntFormatter(const IntFormatter &n) {
    nf = NULL;
    intFormatter = &n;
}

void NumberFormatter::useNumberFormatIntFormatter(
        const NumberFormat &n, const IntFormatter &i) {
    nf = &n;
    intFormatter = &i;
}

UnicodeString &NumberFormatter::select(
        const Formattable &quantity, 
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }
    const DecimalFormat *decFmt = NULL;
    if (shouldUseNumberFormat(quantity)) {
        decFmt = dynamic_cast<const DecimalFormat *>(nf);
    }
    if (decFmt != NULL) {
        FixedDecimal fd = decFmt->getFixedDecimal(quantity, status);
        if (U_FAILURE(status)) {
            return result;
        }
        result = rules.select(fd);
    } else if (quantity.getType() == Formattable::kDouble) {
        result = rules.select(quantity.getDouble());
    } else if (quantity.getType() == Formattable::kLong) {
        result = rules.select(quantity.getLong());
    } else if (quantity.getType() == Formattable::kInt64) {
        result = rules.select((double) quantity.getInt64());
    } else {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return result;
}

UnicodeString &NumberFormatter::format(
        const Formattable &quantity,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    if (shouldUseNumberFormat(quantity)) {
        nf->format(quantity, appendTo, pos, status);
        return appendTo;
    }
    intFormatter->format(quantity, appendTo, pos, status);
    return appendTo;
}

UBool NumberFormatter::shouldUseNumberFormat(
        const Formattable &quantity) const {
    if (nf == NULL) {
        return FALSE;
    }
    if (intFormatter == NULL) {
        return TRUE;
    }
    if (quantity.getType() == Formattable::kLong) {
        return FALSE;
    }
    return TRUE;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
