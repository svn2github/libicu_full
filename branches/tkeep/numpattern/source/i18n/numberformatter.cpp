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
#include "sharednumberformat.h"

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

NumberFormatter::NumberFormatter(const SharedNumberFormat *borrowed)
        : fDelegateOwned(FALSE) {
    borrowSharedNumberFormat(borrowed);
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

NumberFormatter &NumberFormatter::borrowSharedNumberFormat(
        const SharedNumberFormat *borrowed) {
    if (borrowed->getIntFormatter() == NULL) {
        return borrowNumberFormat(**borrowed);
    }
    NumberIntFormatter *nif = new NumberIntFormatter(
            **borrowed, *borrowed->getIntFormatter());
    if (nif == NULL) {
        return borrowNumberFormat(**borrowed);
    }
    return adoptNumberIntFormatter(nif);
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

const IntFormatter *NumberFormatter::getIntFormatter() const {
    return fDelegateType == kIntFormatter ? fDelegate.intFormatter : NULL;
}

const NumberFormat *NumberFormatter::getNumberFormat() const {
    return fDelegateType == kNumberFormat ? fDelegate.numberFormat : NULL;
}

const NumberIntFormatter *NumberFormatter::getNumberIntFormatter() const {
    return fDelegateType == kNumberIntFormatter ? fDelegate.numberIntFormatter : NULL;
}

UnicodeString &NumberFormatter::select(
        const Formattable &quantity, 
        const PluralRules &rules,
        UnicodeString &result,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }
    switch (fDelegateType) {
        case kNumberFormat:
            return pluralutils_nf_select(
                    quantity, *fDelegate.numberFormat, rules, result, status);
        case kIntFormatter:
            return fDelegate.intFormatter->select(
                    quantity, rules, result, status);
        case kNumberIntFormatter:
            return fDelegate.numberIntFormatter->select(
                    quantity, rules, result, status);
        default:
            status = U_INTERNAL_PROGRAM_ERROR;
            break;
    }
    return result;
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

