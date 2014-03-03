/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/
#ifndef __PLURALUTILS_H
#define __PLURALUTILS_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

/**
 * Determines the plural form e.g "one", "two", "few",...
 *
 * @param quantity the quantity
 * @param rules computes the form
 * @param result the computed form stored here
 * @return result
 */
UnicodeString &pluralutils_select(
    const Formattable &quantity,
    const PluralRules &rules,
    UnicodeString &result,
    UErrorCode &status);

/**
 * Determines plural form using fractional plurals.
 *
 * @param quantity the quantity
 * @param format what will format quantity.
 * @param rules computes the form
 * @param result the computed form stored here
 * @return result
 */
UnicodeString &pluralutils_fd_select(
    const Formattable &quantity,
    const DecimalFormat &format,
    const PluralRules &rules,
    UnicodeString &result,
    UErrorCode &status);


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif /* __PLURALUTILS_H */
