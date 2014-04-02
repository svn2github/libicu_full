/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#ifndef SCIFORMATHELPER_H
#define SCIFORMATHELPER_H

#include "unicode/utypes.h"
#include "unicode/unistr.h"

// TODO: Add U_DRAFT_API directives.
// TODO: Add U_FORMATTING directives

/**
 * \file 
 * \brief C++ API: Formatter for measure objects.
 */

U_NAMESPACE_BEGIN

class DecimalFormatSymbols;
class FieldPositionIterator;

/**
 * A helper class for formatting scientific notation.
 *
 * @see NumberFormat
 */
class U_I18N_API SciFormatHelper : public UMemory {
 public:
    SciFormatHelper(const DecimalFormatSymbols &symbols, UErrorCode& status);
    UnicodeString &insetMarkup(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        const UnicodeString &beginMarkup,
        const UnicodeString &endMarkup,
        UnicodeString &result,
        UErrorCode &status) const;
    UnicodeString &toSuperscriptExponentDigits(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        UnicodeString &result,
        UErrorCode &status) const;
 private:
    UnicodeString fPreExponent;
};

U_NAMESPACE_END

#endif // #ifndef MEASUREFORMAT_H
