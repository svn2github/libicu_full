/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#ifndef SCIFORMATTER_H
#define SCIFORMATTER_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#ifndef U_HIDE_DRAFT_API

#include "unicode/unistr.h"

/**
 * \file 
 * \brief C++ API: Formats in scientific notation.
 */

U_NAMESPACE_BEGIN

class FieldPositionIterator;
class DecimalFormatStaticSets;
class DecimalFormatSymbols;
class DecimalFormat;
class Formattable;

/**
 * A formatter that formats in user-friendly scientific notation.
 *
 * Sample code:
 * <pre>
 * UErrorCode status = U_ZERO_ERROR;
 * DecimalFormat *decfmt = (DecimalFormat *)
 *     NumberFormat::createScientificInstance("en", status);
 * ScientificFormatter fmt(decfmt, new MarkupStyle("<sup>", "</sup>"));
 * UnicodeString appendTo;
 *
 * // appendTo = "1.23456x10<sup>-78</sup>"
 * fmt.format(1.23456e-78, appendTo, status);
 * </pre>
 *
 * @draft ICU 55
 */
class U_I18N_API ScientificFormatter : public UObject {
public:

    /**
     * Base class for ScientificFormatter styles.
     *
     * @draft ICU 55
     */
    class U_I18N_API Style : public UObject {
    public:

        /**
         * Returns a clone of this object.
         *
         * @draft ICU 55
         */
        virtual Style *clone() const = 0;
    protected:
        virtual UnicodeString &format(
                const UnicodeString &original,
                FieldPositionIterator &fpi,
                const UnicodeString &preExponent,
                const DecimalFormatStaticSets &decimalFormatSets,
                UnicodeString &appendTo,
                UErrorCode &status) const = 0;
    private:
        friend class ScientificFormatter;
    };

    /**
     * A ScientificFormatter style that uses unicode superscript
     * codepoints for exponent digits.
     *
     * @draft ICU 55
     */
    class U_I18N_API SuperscriptStyle : public Style {
    public:

        /**
         * Returns a clone of this object.
         *
         * @draft ICU 55
         */
        virtual Style *clone() const;
    protected:
        virtual UnicodeString &format(
                const UnicodeString &original,
                FieldPositionIterator &fpi,
                const UnicodeString &preExponent,
                const DecimalFormatStaticSets &decimalFormatSets,
                UnicodeString &appendTo,
                UErrorCode &status) const;
    private:
        friend class ScientificFormatHelper;
    };

    /**
     * A ScientificFormatter style that uses html markup for exponent
     * digits.
     *
     * @draft ICU 55
     */
    class U_I18N_API MarkupStyle : public Style {
    public:
        /**
         * Constructor.
         * @param beginMarkup the html tag to start superscript e.g "<sup">
         * @param endMarkup the html tag to end superscript e.g "</sup>"
         *
         * @draft ICU 55
         */
        MarkupStyle(
                const UnicodeString &beginMarkup,
                const UnicodeString &endMarkup)
                : Style(),
                  fBeginMarkup(beginMarkup),
                  fEndMarkup(endMarkup) { }
        /**
         * Returns a clone of this object.
         *
         * @draft ICU 55
         */
        virtual Style *clone() const;
    protected:
        virtual UnicodeString &format(
                const UnicodeString &original,
                FieldPositionIterator &fpi,
                const UnicodeString &preExponent,
                const DecimalFormatStaticSets &decimalFormatSets,
                UnicodeString &appendTo,
                UErrorCode &status) const;
    private:
        UnicodeString fBeginMarkup;
        UnicodeString fEndMarkup;
        friend class ScientificFormatHelper;
    };

    /**
     * Constructor.
     *
     * @param fmtToAdopt DecimalFormat instance to adopt.
     * @param styleToAdopt the style to adopt.
     *
     * @draft ICU 55
     */
    ScientificFormatter(
            DecimalFormat *fmtToAdopt,
            Style *styleToAdopt,
            UErrorCode &status);

    /**
     * Copy constructor.
     * @draft ICU 55
     */
    ScientificFormatter(const ScientificFormatter &other);

    /**
     * Assignment operator.
     * @draft ICU 55
     */
    ScientificFormatter &operator=(const ScientificFormatter &other);

    /**
     * Destructor.
     * @draft ICU 55
     */
    virtual ~ScientificFormatter();

    /**
     * Formats a number into user friendly scientific notation.
     *
     * @param number the number to format.
     * @param appendTo formatted string appended here.
     * @param status any error returned here.
     * @return appendTo
     *
     * @draft ICU 55
     */
    UnicodeString &format(
            const Formattable &number,
            UnicodeString &appendTo,
            UErrorCode &status) const;
 private:
    UnicodeString fPreExponent;
    DecimalFormat *fDecimalFormat;
    Style *fStyle;
    const DecimalFormatStaticSets *fStaticSets;
    static void getPreExponent(
            const DecimalFormatSymbols &dfs, UnicodeString &preExponent);
    friend class ScientificFormatHelper;
};

U_NAMESPACE_END

#endif /* U_HIDE_DRAFT_API */

#endif /* !UCONFIG_NO_FORMATTING */
#endif 
