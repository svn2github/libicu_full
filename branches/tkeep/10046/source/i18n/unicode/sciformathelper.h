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
 * A helper class for formatting in pretty scientific notation.
 *
 * Sample code:
 * <pre>
 * UErrorCode status = U_ZERO_ERROR;
 * DecimalFormat *decfmt = (DecimalFormat *)
 *     NumberFormat::createScientificInstance("en", status);
 * UnicodeString appendTo;
 * FieldPositionIterator fpositer;
 * decfmt->format(1.23456e-78, appendTo, &fpositer, status);
 * SciFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
 * UnicodeString result;
 *
 * // result = "1.23456×10<sup>-78</sup>"
 * helper.insetMarkup(appendTo, fpositer, "<sup>", "</sup>", result, status));
 * </pre>
 *
 * @see NumberFormat
 * @draft ICU 54
 */
class U_I18N_API SciFormatHelper : public UMemory {
 public:
    /**
     * Constructor.
     * @param symbols comes from DecimalFormat instance used for default
     *  scientific notation.
     * @param status any error reported here.
     * @draft ICU 54
     */
    SciFormatHelper(const DecimalFormatSymbols &symbols, UErrorCode& status);

    /**
     * Copy constructor.
     * @draft ICU 54
     */
    SciFormatHelper(const SciFormatHelper &other);

    /**
     * Assignment operator.
     * @draft ICU 54
     */
    SciFormatHelper &operator=(const SciFormatHelper &other);

    /**
     * Destructor.
     * @draft ICU 54
     */
    ~SciFormatHelper();

    /**
     * Makes scientific notation pretty by surrounding exponent with
     * html to make it superscript.
     * @param s the original formatted scientific notation e.g "6.02e23"
     * @param fpi the FieldPositionIterator from the format call.
     * @param beginMarkup the start html for the exponent e.g "<sup>"
     * @param endMarkup the end html for the exponent e.g "</sup>"
     * @param result pretty scientific notation stored here.
     * @param status any error returned here. When status is set to a non-zero
     * error, the value of result is unspecified, and client should fallback
     * to using s for scientific notation.
     * @return the value stored in result.
     * @draft ICU 54
     */
    UnicodeString &insetMarkup(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        const UnicodeString &beginMarkup,
        const UnicodeString &endMarkup,
        UnicodeString &result,
        UErrorCode &status) const;

    /**
     * Makes scientific notation pretty by using specific code points
     * for superscript 0..9 and - in the exponent rather than by using
     * html. It is the caller's responsibility to ensure that original
     * formatted scientific notation has 0..9 or - in the exponent.
     * Any other characters will result in U_INVALID_CHAR_FOUND error
     * because most characters do not have a superscript equivalent
     * in unicode.
     * @param s the original formatted scientific notation e.g "6.02e23"
     * @param fpi the corresponding FieldPositionIterator from the format call.
     * @param result pretty scientific notation stored here.
     * @param status any error returned here. When status is set to a non-zero
     * error, the value of result is unspecified, and client should fallback
     * to using s for scientific notation.
     * @return the value stored in result.
     * @draft ICU 54
     */
    UnicodeString &toSuperscriptExponentDigits(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        UnicodeString &result,
        UErrorCode &status) const;
 private:
    UnicodeString fPreExponent;
};

U_NAMESPACE_END

#endif // #ifndef SCIFORMATHELPER_H
