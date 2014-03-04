/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/decimfmt.h"
#include "unicode/plurrule.h"
#include "plurrule_impl.h"

U_NAMESPACE_BEGIN

UnicodeString &pluralutils_select(
        const Formattable &quantity,
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return result;
    }
    if (quantity.getType() == Formattable::kDouble) {
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

UnicodeString &pluralutils_fd_select(
        const Formattable &quantity,
        const DecimalFormat &format,
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) {
    FixedDecimal fd = format.getFixedDecimal(quantity, status);
    if (U_FAILURE(status)) {
        return result;
    }
    result = rules.select(fd);
    return result;
}

UnicodeString &pluralutils_nf_select(
        const Formattable &quantity, 
        const NumberFormat &nf,
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return result;
    }
    const DecimalFormat *decFmt =
            dynamic_cast<const DecimalFormat *>(&nf);
    if (decFmt != NULL) {
        return pluralutils_fd_select(quantity, *decFmt, rules, result, status);
    }
    return pluralutils_select(quantity, rules, result, status);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
