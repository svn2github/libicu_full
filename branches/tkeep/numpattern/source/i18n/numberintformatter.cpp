/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numberintformatter.h"
#include "intformatter.h"
#include "unicode/decimfmt.h"
#include "pluralutils.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

UnicodeString &NumberIntFormatter::select(
        const Formattable &quantity, 
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }

    // If we don't prefer integers and we aren't handed an integer,
    // find plural form using regular number format
    if (!preferInt && quantity.getType() != Formattable::kLong) {
        return pluralutils_nf_select(
                quantity, numberFormat, rules, result, status);
    }

    // If this call fails, fall back to number format.
    intFormatter.select(quantity, rules, result, status);
    if (U_SUCCESS(status)) {
        return result;
    }
    status = U_ZERO_ERROR;
    return pluralutils_nf_select(
            quantity, numberFormat, rules, result, status);
}

UnicodeString &NumberIntFormatter::format(
        const Formattable &quantity,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }

    // If we don't prefer integers and we aren't formatting one,
    // format using regular number format.
    if (!preferInt && quantity.getType() != Formattable::kLong) {
        return numberFormat.format(quantity, appendTo, pos, status);
    }

    // If this call fails, fall back to number format.
    intFormatter.format(quantity, appendTo, pos, status);
    if (U_SUCCESS(status)) {
        return appendTo;
    }
    status = U_ZERO_ERROR;
    return numberFormat.format(quantity, appendTo, pos, status);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
