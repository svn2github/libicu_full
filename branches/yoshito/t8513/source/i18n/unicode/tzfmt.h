/*
*******************************************************************************
* Copyright (C) 2011-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef __TZFMT_H
#define __TZFMT_H

/**
 * \file 
 * \brief C++ API: TimeZoneFormat
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef U_HIDE_DRAFT_API

#include "unicode/uobject.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/format.h"
#include "unicode/timezone.h"
#include "unicode/tznames.h"

U_CDECL_BEGIN
/**
 * Constants for time zone display format style used by format/parse APIs
 * in TimeZoneFormat.
 * @draft ICU 49
 */
typedef enum UTimeZoneFormatStyle {
    /**
     * Generic location format, such as "United States Time (New York)", "Italy Time"
     * @draft ICU 49
     */
    UTZFMT_STYLE_LOCATION,
    /**
     * Generic long non-location format, such as "Eastern Time".
     * @draft ICU 49
     */
    UTZFMT_STYLE_GENERIC_LONG,
    /**
     * Generic short non-location format, such as "ET".
     * @draft ICU 49
     */
    UTZFMT_STYLE_GENERIC_SHORT,
    /**
     * Specific long format, such as "Eastern Standard Time".
     * @draft ICU 49
     */
    UTZFMT_STYLE_SPECIFIC_LONG,
    /**
     * Specific short format, such as "EST", "PDT".
     * @draft ICU 49
     */
    UTZFMT_STYLE_SPECIFIC_SHORT,
    /**
     * RFC822 format, such as "-0500"
     * @draft ICU 49
     */
    UTZFMT_STYLE_RFC822,
    /**
     * Localized GMT offset format, such as "GMT-05:00", "UTC+0100"
     * @draft ICU 49
     */
    UTZFMT_STYLE_LOCALIZED_GMT,
    /**
     * ISO8601 format, such as "-08:00", "Z"
     * @draft ICU 49
     */
    UTZFMT_STYLE_ISO8601
} UTimeZoneFormatStyle;

/**
 * Constants for GMT offset pattern types.
 * @draft ICU 49
 */
typedef enum UTimeZoneFormatGMTOffsetPatternType {
    /**
     * Positive offset with hour and minute fields
     * @draft ICU 49
     */
    UTZFMT_PAT_POSITIVE_HM,
    /**
     * Positive offset with hour, minute and second fields
     * @draft ICU 49
     */
    UTZFMT_PAT_POSITIVE_HMS,
    /**
     * Negative offset with hour and minute fields
     * @draft ICU 49
     */
    UTZFMT_PAT_NEGATIVE_HM,
    /**
     * Negative offset with hour, minute and second fields
     * @draft ICU 49
     */
    UTZFMT_PAT_NEGATIVE_HMS
} UTimeZoneFormatGMTOffsetPatternType;

/**
 * Constants for time types used by TimeZoneFormat APIs for
 * receiving time type (standard time, daylight time or unknown).
 * @draft ICU 49
 */
typedef enum UTimeZoneFormatTimeType {
    /**
     * Unknown
     * @draft ICU 49
     */
    UTZFMT_TIME_TYPE_UNKNOWN,
    /**
     * Standard time
     * @draft ICU 49
     */
    UTZFMT_TIME_TYPE_STANDARD,
    /**
     * Daylight saving time
     * @draft ICU 49
     */
    UTZFMT_TIME_TYPE_DAYLIGHT
} UTimeZoneFormatTimeType;

U_CDECL_END

U_NAMESPACE_BEGIN

class TimeZoneNames;
class TimeZoneGenericNames;

/**
 * <code>TimeZoneFormat</code> supports time zone display name formatting and parsing.
 * An instance of TimeZoneFormat works as a subformatter of {@link SimpleDateFormat},
 * but you can also directly get a new instance of <code>TimeZoneFormat</code> and
 * formatting/parsing time zone display names.
 * <p>
 * ICU implements the time zone display names defined by <a href="http://www.unicode.org/reports/tr35/">UTS#35
 * Unicode Locale Data Markup Language (LDML)</a>. {@link TimeZoneNames} represents the
 * time zone display name data model and this class implements the algorithm for actual
 * formatting and parsing.
 * 
 * @see SimpleDateFormat
 * @see TimeZoneNames
 * @draft ICU 49
 */
class U_I18N_API TimeZoneFormat : public Format {
public:
    /**
     * @draft ICU 49
     */
    virtual ~TimeZoneFormat();

    /**
     * Creates an instance of <code>TimeZoneFormat</code> for the given locale.
     * @param locale The locale.
     * @param status Recevies the status.
     * @return An instance of <code>TimeZoneFormat</code> for the given locale,
     *          owned by the caller.
     * @draft ICU 49
     */
    static TimeZoneFormat* U_EXPORT2 createInstance(const Locale& locale, UErrorCode& status);

    /**
     * Returns the time zone display name data used by this instance.
     * @return The time zone display name data.
     * @draft ICU 49
     */
    const TimeZoneNames* getTimeZoneNames() const;

    /**
     * Sets the time zone display name data to this format instnace.
     * The caller should not delete the TimeZoenNames object after it is adopted
     * by this call.
     * @param tznames TimeZoneNames object to be adopted.
     * @draft ICU 49
     */
    void adoptTimeZoneNames(TimeZoneNames *tznames);

    /**
     * Sets the time zone display name data to this format instnace.
     * @param tznames TimeZoneNames object to be set.
     * @draft ICU 49
     */
    void setTimeZoneNames(const TimeZoneNames &tznames);

    /**
     * Returns the localized GMT format pattern.
     * @param pattern Receives the localized GMT format pattern.
     * @return A reference to the result pattern.
     * @see #setGMTPattern
     * @draft ICU 49
     */
    UnicodeString& getGMTPattern(UnicodeString& pattern) const;

    /**
     * Sets the localized GMT format pattern. The pattern must contain
     * a single argument {0}, for example "GMT {0}".
     * @param pattern The localized GMT format pattern to be used by this object.
     * @see #getGMTPattern
     * @draft ICU 49
     */
    void setGMTPattern(const UnicodeString& pattern);

    /**
     * Returns the offset pattern used for localized GMT format.
     * @param type The offset pattern type enum.
     * @param pattern Receives the offset pattern.
     * @return A reference to the result pattern.
     * @see #setGMTOffsetPattern
     * @draft ICU 49
     */
    UnicodeString& getGMTOffsetPattern(UTimeZoneFormatGMTOffsetPatternType type, UnicodeString& pattern) const;

    /**
     * Sets the offset pattern for the given offset type.
     * @param type The offset pattern type enum.
     * @param pattern The offset pattern used for localized GMT format for the type.
     * @see #getGMTOffsetPattern
     * @draft ICU 49
     */
    void setGMTOffsetPattern(UTimeZoneFormatGMTOffsetPatternType type, const UnicodeString& pattern);

    /**
     * Returns the decimal digit characters used for localized GMT format in a single string
     * containing from 0 to 9 in the ascending order.
     * @param digits Receives the decimal digits used for localized GMT format.
     * @see #setGMTOffsetDigits
     */
    UnicodeString& getGMTOffsetDigits(UnicodeString& digits) const;

    /**
     * Sets the decimal digit characters used for localized GMT format.
     * @param digits The decimal digits used for localized GMT format.
     * @see #getGMTOffsetDigits
     */
    void setGMTOffsetDigits(const UnicodeString& digits);

    /**
     * Returns the localized GMT format string for GMT(UTC) itself (GMT offset is 0).
     * @param gmtZeroFormat Receives the localized GMT string string for GMT(UTC) itself.
     * @return A reference to the result GMT string.
     * @see #setGMTZeroFormat
     */
    UnicodeString& getGMTZeroFormat(UnicodeString& gmtZeroFormat) const;

    /**
     * Sets the localized GMT format string for GMT(UTC) itself (GMT offset is 0).
     * @param gmtZeroFormat The localized GMT format string for GMT(UTC).
     * @see #getGMTZeroFormat
     */
    void setGMTZeroFormat(const UnicodeString& gmtZeroFormat);

    /**
     * Returns <code>TRUE</code> when this <code>TimeZoneFormat</code> is configured for parsing
     * display names including names that are only used by other styles by {@link #parse}.
     * <p><b>Note</b>: An instance created by {@link #createInstance} is configured NOT
     * parsing all styles (<code>FALSE</code>) by defaut.
     * 
     * @return <code>TRUE</code> when this instance is configure for parsing all available names.
     * @see #setParseAllStyles
     * @draft ICU 49
     */
    UBool isParseAllStyles() const;

    /**
     * Sets if {@link #parse} to parse display names including names that are only used by other styles.
     * @param parseAllStyles <code>true</code> to parse all available names.
     * @see #isParseAllStyles
     * @draft ICU 49
     */
    void setParseAllStyles(UBool parseAllStyles);

    /**
     * Returns the RFC822 style time zone string for the given offset.
     * For example, "-0800".
     * @param offset The offset from GMT(UTC) in milliseconds.
     * @param result Recevies the RFC822 style GMT(UTC) offset format.
     * @return A reference to the result.
     * @see #parseOffsetRFC822
     * @draft ICU 49
     */
    UnicodeString& formatOffsetRFC822(int32_t offset, UnicodeString& result) const;

    /**
     * Returns the ISO 8601 style time zone string for the given offset.
     * For example, "-08:00" and "Z".
     * @param offset The offset from GMT(UTC) in milliseconds.
     * @param result Recevies the ISO 8601 style GMT(UTC) offset format.
     * @return A reference to the result.
     * @see #parseOffsetISO8601
     * @draft ICU 49
     */
    UnicodeString& formatOffsetISO8601(int32_t offset, UnicodeString& result) const;

    /**
     * Returns the localized GMT(UTC) offset format for the given offset.
     * The localized GMT offset is defined by;
     * <ul>
     * <li>GMT format pattern (e.g. "GMT {0}" - see {@link #getGMTPattern})
     * <li>Offset time pattern (e.g. "+HH:mm" - see {@link #getGMTOffsetPattern})
     * <li>Offset digits (e.g. "0123456789" - see {@link #getGMTOffsetDigits})
     * <li>GMT zero format (e.g. "GMT" - see {@link #getGMTZeroFormat})
     * </ul>
     * @param offset the offset from GMT(UTC) in milliseconds.
     * @param result Receives the localized GMT format string.
     * @return A reference to the result.
     * @see #parseOffsetLocalizedGMT
     * @draft ICU 49
     */
    UnicodeString& formatOffsetLocalizedGMT(int32_t offset, UnicodeString& result) const;

    /**
     * @draft ICU 49
     */
    virtual UnicodeString& format(UTimeZoneFormatStyle style, const TimeZone& tz, UDate date,
        UnicodeString& name, UTimeZoneFormatTimeType* timeType = NULL) const = 0;

    /**
     * Returns offset from GMT(UTC) in milliseconds for the given RFC822
     * style time zone string. When the given string is not an RFC822 time zone
     * string, this method sets the current position as the error index
     * to <code>ParsePosition pos</code> and returns 0.
     * @param text The text contains RFC822 style time zone string (e.g. "-0800")
     *              at the position.
     * @param pos The ParsePosition object.
     * @return The offset from GMT(UTC) in milliseconds for the given RFC822 style
     *              time zone string.
     * @see #formatOffsetRFC822
     * @draft ICU 49
     */
    int32_t parseOffsetRFC822(const UnicodeString& text, ParsePosition& pos) const;

    /**
     * Returns offset from GMT(UTC) in milliseconds for the given ISO 8601
     * style time zone string. When the given string is not an ISO 8601 time zone
     * string, this method sets the current position as the error index
     * to <code>ParsePosition pos</code> and returns 0.
     * @param text The text contains RFC822 style time zone string (e.g. "-08:00", "Z")
     *              at the position.
     * @param pos The ParsePosition object.
     * @return The offset from GMT(UTC) in milliseconds for the given ISO 8601 style
     *              time zone string.
     * @see #formatOffsetISO8601
     * @draft ICU 49
     */
    int32_t parseOffsetISO8601(const UnicodeString& text, ParsePosition& pos) const;

    /**
     * Returns offset from GMT(UTC) in milliseconds for the given localized GMT
     * offset format string. When the given string cannot be parsed, this method
     * sets the current position as the error index to <code>ParsePosition pos</code>
     * and returns 0.
     * @param text The text contains a localized GMT offset string at the position.
     * @param pos The ParsePosition object.
     * @return The offset from GMT(UTC) in milliseconds for the given localized GMT
     *          offset format string.
     * @see #formatOffsetLocalizedGMT
     * @draft ICU 49
     */
    int32_t parseOffsetLocalizedGMT(const UnicodeString& text, ParsePosition& pos) const;

    /**
     * @draft ICU 49
     */
    virtual UnicodeString& parse(UTimeZoneFormatStyle style, const UnicodeString& text, ParsePosition& pos,
        UnicodeString& tzID, UTimeZoneFormatTimeType* timeType = NULL) const = 0;

    /**
     * @draft ICU 49
     */
    TimeZone* parse(UTimeZoneFormatStyle style, const UnicodeString& text, ParsePosition& pos,
        UTimeZoneFormatTimeType* timeType = NULL) const;



    /* ----------------------------------------------
     * Format APIs
     * ---------------------------------------------- */

     /**
     * @draft ICU 49
     */
    virtual UnicodeString& format(const Formattable& obj, UnicodeString& appendTo,
        FieldPosition& pos, UErrorCode& status) const;

    /**
     * @draft ICU 49
     */
    virtual void parseObject(const UnicodeString& source, Formattable& result, ParsePosition& parse_pos) const;

    /**
     * @draft ICU 49
     */
    virtual UBool operator==(const Format& other) const;

    /**
     * @draft ICU 49
     */
    virtual Format* clone() const;

private:
    TimeZoneNames*      ftznames;
    UnicodeString       fgmtPattern;
    UnicodeString       fgmtOffsetPatterns[4];
    UnicodeString       fgmtOffsetDigits[10];
    UnicodeString       fgmtZeroFormat;
    UBool               fparseAllStlyes;
};

U_NAMESPACE_END

#endif  /* U_HIDE_DRAFT_API */
#endif
#endif

