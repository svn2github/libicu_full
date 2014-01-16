/*
**********************************************************************
* Copyright (c) 2004-2011, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 20, 2004
* Since: ICU 3.0
**********************************************************************
*/
#ifndef MEASUREFORMAT_H
#define MEASUREFORMAT_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/format.h"

/**
 * \file 
 * \brief C++ API: Formatter for measure objects.
 */

/**
 * Constants for various widths.
 * There are 3 widths: Wide, Short, Narrow.
 * For example, for English, when formatting "3 hours"
 * Wide is "3 hours"; short is "3 hrs"; narrow is "3h"
 * @draft ICU 53
 */
enum UMeasureFormatWidth {
    /** @draft ICU 53 */
    UMEASFMT_WIDTH_WIDE,
    /** @draft ICU 53 */
    UMEASFMT_WIDTH_SHORT,
    /** @draft ICU 53 */
    UMEASFMT_WIDTH_NARROW,
    /** @draft ICU 53 */
    UMEASFMT_WIDTH_NUMERIC,
    /** @draft ICU 53 */
    UMEASFMT_WIDTH_COUNT
};
/** @draft ICU 53 */
typedef enum UMeasureFormatWidth UMeasureFormatWidth; 

U_NAMESPACE_BEGIN

class NumberFormat;

/**
 * 
 * A formatter for measure objects.
 *
 * @see Format
 * @author Alan Liu
 * @stable ICU 3.0
 */
class U_I18N_API MeasureFormat : public Format {
 public:
    using Format::parseObject;
    using Format::format;

    /**
     * Constructor.
     * @draft ICU 53.
     */
    MeasureFormat(
            const Locale &locale, UMeasureFormatWidth width, UErrorCode &status);

    /**
     * Constructor.
     * @draft ICU 53.
     */
    MeasureFormat(
            const Locale &locale,
            UMeasureFormatWidth width,
            NumberFormat *nfToAdopt,
            UErrorCode &status);

    /**
     * Copy constructor.
     * @draft ICU 53.
     */
    MeasureFormat(const MeasureFormat &other);

    /**
     * Assignment operator.
     * @draft ICU 53.
     */
    MeasureFormat &operator=(const MeasureFormat &rhs);
    
    /**
     * Destructor.
     * @stable ICU 3.0
     */
    virtual ~MeasureFormat();

    /**
     * Return true if given Format objects are semantically equal.
     * @draft ICU 53
     */
    virtual UBool operator==(const Format &other) const;

    /**
     * Clones this object polymorphically.
     * @draft ICU 53
     */
    virtual Format *clone() const;

    /**
     * Formats object to produce a string.
     * @draft ICU 53
     */
    virtual UnicodeString &format(
            const Formattable &obj,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;

    /**
     * Parse a string to produce an object. This implementation sets
     * status to U_UNSUPPORTED_ERROR.
     *
     * @draft ICU 53
     */
    virtual void parseObject(
            const UnicodeString &source,
            Formattable &reslt,
            ParsePosition &pos) const;

    /**
     * Formats measure objects to produce a string.
     * @param measures wrapped measure objects.
     * @param measureCount the number of measure objects.
     * @param appendTo formatted string appended here.
     * @param pos the field position.
     * @param status the error.
     * @return appendTo reference
     *
     * @draft ICU 53
     */
    UnicodeString &formatMeasures(
            const Formattable *measures,
            int32_t measureCount,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;


    /**
     * Return a formatter for CurrencyAmount objects in the given
     * locale.
     * @param locale desired locale
     * @param ec input-output error code
     * @return a formatter object, or NULL upon error
     * @stable ICU 3.0
     */
    static MeasureFormat* U_EXPORT2 createCurrencyFormat(const Locale& locale,
                                               UErrorCode& ec);

    /**
     * Return a formatter for CurrencyAmount objects in the default
     * locale.
     * @param ec input-output error code
     * @return a formatter object, or NULL upon error
     * @stable ICU 3.0
     */
    static MeasureFormat* U_EXPORT2 createCurrencyFormat(UErrorCode& ec);

 protected:
    /**
     * Default constructor.
     * @stable ICU 3.0
     */
    MeasureFormat();

};

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_FORMATTING
#endif // #ifndef MEASUREFORMAT_H
