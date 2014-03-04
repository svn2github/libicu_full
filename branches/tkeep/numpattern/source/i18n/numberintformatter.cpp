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
    const DecimalFormat *decFmt = NULL;
    if (quantity.getType() != Formattable::kLong) {
        decFmt = dynamic_cast<const DecimalFormat *>(&numberFormat);
    }
    if (decFmt != NULL) {
        return pluralutils_fd_select(quantity, *decFmt, rules, result, status);
    }
    return pluralutils_select(quantity, rules, result, status);
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
