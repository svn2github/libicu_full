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
#include "numberintformatter.h"
#include "pluralutils.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

NumberFormatter::NumberFormatter() : fDelegateType(kUndefined), fDelegateOwned(FALSE) { };

NumberFormatter::NumberFormatter(const NumberFormat &nf)
        : fDelegateOwned(FALSE) {
    borrowNumberFormat(nf);
}

NumberFormatter::NumberFormatter(const IntFormatter &nf)
        : fDelegateOwned(FALSE) {
    borrowIntFormatter(nf);
}

NumberFormatter::NumberFormatter(const NumberIntFormatter &nf)
        : fDelegateOwned(FALSE) {
    borrowNumberIntFormatter(nf);
}

NumberFormatter::~NumberFormatter() {
    clear();
}

NumberFormatter &NumberFormatter::borrowNumberFormat(const NumberFormat &nf) {
    clear();
    fDelegate.numberFormat = &nf;
    fDelegateType = kNumberFormat;
    fDelegateOwned = FALSE;
    return *this;
}

NumberFormatter &NumberFormatter::borrowIntFormatter(const IntFormatter &nf) {
    clear();
    fDelegate.intFormatter = &nf;
    fDelegateType = kIntFormatter;
    fDelegateOwned = FALSE;
    return *this;
}

NumberFormatter &NumberFormatter::borrowNumberIntFormatter(
        const NumberIntFormatter &nf) {
    clear();
    fDelegate.numberIntFormatter = &nf;
    fDelegateType = kNumberIntFormatter;
    fDelegateOwned = FALSE;
    return *this;
}

NumberFormatter &NumberFormatter::adoptNumberFormat(NumberFormat *nf) {
    clear();
    fDelegate.numberFormat = nf;
    fDelegateType = kNumberFormat;
    fDelegateOwned = TRUE;
    return *this;
}

NumberFormatter &NumberFormatter::adoptIntFormatter(IntFormatter *nf) {
    clear();
    fDelegate.intFormatter = nf;
    fDelegateType = kIntFormatter;
    fDelegateOwned = TRUE;
    return *this;
}

NumberFormatter &NumberFormatter::adoptNumberIntFormatter(
        NumberIntFormatter *nf) {
    clear();
    fDelegate.numberIntFormatter = nf;
    fDelegateType = kNumberIntFormatter;
    fDelegateOwned = TRUE;
    return *this;
}

UnicodeString &NumberFormatter::select(
        const Formattable &quantity, 
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }
    if (fDelegateType == kNumberIntFormatter) {
            return fDelegate.numberIntFormatter->select(
                    quantity, rules, result, status);
    }
    const DecimalFormat *decFmt = NULL;
    if (fDelegateType == kNumberFormat) {
        decFmt = dynamic_cast<const DecimalFormat *>(fDelegate.numberFormat);
    }
    if (decFmt != NULL) {
        return pluralutils_fd_select(quantity, *decFmt, rules, result, status);
    }
    return pluralutils_select(quantity, rules, result, status);
}

UnicodeString &NumberFormatter::format(
        const Formattable &quantity,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    switch (fDelegateType) {
        case kNumberFormat:
            return fDelegate.numberFormat->format(
                    quantity, appendTo, pos, status);
        case kIntFormatter:
            return fDelegate.intFormatter->format(
                    quantity, appendTo, pos, status);
        case kNumberIntFormatter:
            return fDelegate.numberIntFormatter->format(
                    quantity, appendTo, pos, status);
        default:
            status = U_INTERNAL_PROGRAM_ERROR;
            break;
    }
    return appendTo;
}

void NumberFormatter::clear() {
    if (!fDelegateOwned) {
        return;
    }
    switch (fDelegateType) {
        case kNumberFormat:
            delete fDelegate.numberFormat;
            break;
        case kIntFormatter:
            delete fDelegate.intFormatter;
            break;
        case kNumberIntFormatter:
            delete fDelegate.numberIntFormatter;
            break;
        default:
            break;
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

