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
class U_I18N_API TimeZoneFormat : public UMemory {
public:
    /**
     * @draft ICU 49
     */
    virtual ~TimeZoneFormat();

    /**
     * @draft ICU 49
     */
    static TimeZoneFormat* U_EXPORT2 createInstance(const Locale& locale, UErrorCode& status);

    /**
     * @draft ICU 49
     */
    virtual const TimeZoneNames* getTimeZoneNames() const = 0;

    /**
     * @draft ICU 49
     */
    virtual UnicodeString& format(UTimeZoneFormatStyle style, const TimeZone& tz, UDate date,
        UnicodeString& name, UTimeZoneFormatTimeType* timeType = NULL) const = 0;

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
};

U_NAMESPACE_END

#endif  /* U_HIDE_DRAFT_API */
#endif
#endif

