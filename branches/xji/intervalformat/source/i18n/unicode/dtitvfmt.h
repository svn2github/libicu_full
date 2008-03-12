/********************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and others. All Rights Reserved.
*******************************************************************************
*
* File DTITVFMT.H
*
*******************************************************************************
*/

#ifndef DTITVFMT_H__
#define DTITVFMT_H__


/**
 * \file 
 * \brief C++ API: Format and parse date interval in a language-independent manner.
 */
 
#if !UCONFIG_NO_FORMATTING

#include "unicode/utypes.h"
#include "unicode/ucal.h"
#include "unicode/smpdtfmt.h"
#include "unicode/dtintrv.h"
#include "unicode/dtitvinf.h"

U_NAMESPACE_BEGIN


/**
 *
 * DateIntervalFormat is a class for formatting and parsing date 
 * intervals in a language-independent manner. 
 *
 * <P>
 * Date interval means from one date to another date,
 * for example, from "Jan 11, 2008" to "Jan 18, 2008".
 * We introduced class DateInterval to represent it.
 * DateInterval is a pair of UDate, which is 
 * the standard milliseconds since 24:00 GMT, Jan 1, 1970.
 *
 * <P>
 * DateIntervalFormat formats a DateInterval into
 * text as compactly as possible. 
 * For example, the date interval format from "Jan 11, 2008" to "Jan 18,. 2008"
 * is "Jan 11-18, 2008" for English.
 * And it parses text into DateInterval, 
 * although initially, parsing is not supported. 
 *
 * <P>
 * There is no structural information in date time patterns. 
 * For any punctuations and string literals inside a date time pattern, 
 * we do not know whether it is just a separator, or a prefix, or a suffix. 
 * Without such information, so, it is difficult to generate a sub-pattern 
 * (or super-pattern) by algorithm.
 * So, formatting a DateInterval is pattern-driven. It is very
 * similar to formatting in SimpleDateFormat.
 * We introduce class DateIntervalInfo to save date interval 
 * patterns, similar to date time pattern in SimpleDateFormat.
 *
 * <P>
 * Logically, the interval patterns are mappings
 * from (skeleton, the_largest_different_calendar_field)
 * to (date_interval_pattern).
 *
 * <P>
 * A skeleton 
 * <ul>
 * <li>
 * 1. only keeps the field pattern letter and ignores all other parts 
 *    in a pattern, such as space, punctuations, and string literals.
 * <li>
 * 2. hides the order of fields. 
 * <li>
 * 3. might hide a field's pattern letter length.
 *
 *    For those non-digit calendar fields, the pattern letter length is 
 *    important, such as MMM, MMMM, and MMMMM; EEE and EEEE, 
 *    and the field's pattern letter length is honored.
 *    
 *    For the digit calendar fields,  such as M or MM, d or dd, yy or yyyy, 
 *    the field pattern length is ignored and the best match, which is defined 
 *    in date time patterns, will be returned without honor the field pattern
 *    letter length in skeleton.
 * </ul>
 *
 * <P>
 * There is a set of pre-defined skeleton macros (defined in udat.h).
 * The skeletons defined consist of the desired calendar field set 
 * (for example,  DAY, MONTH, YEAR) and the format length (long, medium, short)
 * used in date time patterns.
 * 
 * For example, skeleton MONTH_YEAR_MEDIUM_FORMAT consists month and year,
 * and it's corresponding full pattern is medium format date pattern.
 * So, the skeleton is "yMMM", for English, the full pattern is "MMM yyyy", 
 * which is the format by removing DATE from medium date format.
 *
 * For example, skeleton DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT consists day, month,
 * year, and day-of-week, and it's corresponding full pattern is the medium
 * format date pattern. So, the skeleton is "yMMMEEEd", for English,
 * the full pattern is "EEE, MMM d, yyyy", which is the medium date format
 * plus day-of-week.
 *
 * <P>
 * The calendar fields we support for interval formatting are:
 * year, month, date, day-of-week, am-pm, hour, hour-of-day, and minute.
 * Those calendar fields can be defined in the following order:
 * year >  month > date > hour (in day) >  minute 
 *  
 * The largest different calendar fields between 2 calendars is the
 * first different calendar field in above order.
 *
 * For example: the largest different calendar fields between "Jan 10, 2007" 
 * and "Feb 20, 2008" is year.
 *   
 * <P>
 * There are pre-defined interval patterns for those pre-defined skeletons
 * in locales' resource files.
 * For example, for a skeleton DAY_MONTH_YEAR_MEDIUM_FORMAT, which is  "yMMMd",
 * in  en_US, if the largest different calendar field between date1 and date2 
 * is "year", the date interval pattern  is "MMM d, yyyy - MMM d, yyyy", 
 * such as "Jan 10, 2007 - Jan 10, 2008".
 * If the largest different calendar field between date1 and date2 is "month",
 * the date interval pattern is "MMM d - MMM d, yyyy",
 * such as "Jan 10 - Feb 10, 2007".
 * If the largest different calendar field between date1 and date2 is "day",
 * the date interval pattern is ""MMM d-d, yyyy", such as "Jan 10-20, 2007".
 *
 * For date skeleton, the interval patterns when year, or month, or date is 
 * different are defined in resource files.
 * For time skeleton, the interval patterns when am/pm, or hour, or minute is
 * different are defined in resource files.
 *
 * <P>
 * If a skeleton is not found in a locale's DateIntervalInfo, which means
 * the interval patterns for the skeleton is not defined in resource file,
 * the interval pattern will falls back to the interval "fallback" pattern 
 * defined in resource file.
 * If the interval "fallback" pattern is not defined, the default fall-back
 * is "{date0} - {data1}".
 *
 * <P>
 * For the combination of date and time, 
 * The rule to genearte interval patterns are:
 * <ul>
 * <li>
 *    1) when the year, month, or day differs, falls back to fall-back
 *    interval pattern, which mostly is the concatenate the two original 
 *    expressions with a separator between, 
 *    For example, interval pattern from "Jan 10, 2007 10:10 am" 
 *    to "Jan 11, 2007 10:10am" is 
 *    "Jan 10, 2007 10:10 am - Jan 11, 2007 10:10am" 
 * <li>
 *    2) otherwise, present the date followed by the range expression 
 *    for the time.
 *    For example, interval pattern from "Jan 10, 2007 10:10 am" 
 *    to "Jan 10, 2007 11:10am" is "Jan 10, 2007 10:10 am - 11:10am" 
 * </ul>
 *
 *
 * <P>
 * If two dates are the same, the interval pattern is the single date pattern.
 * For example, interval pattern from "Jan 10, 2007" to "Jan 10, 2007" is 
 * "Jan 10, 2007".
 *
 * Or if the presenting fields between 2 dates have the exact same values,
 * the interval pattern is the  single date pattern. 
 * For example, if user only requests year and month,
 * the interval pattern from "Jan 10, 2007" to "Jan 20, 2007" is "Jan 2007".
 *
 * <P>
 * DateIntervalFormat needs the following information for correct 
 * formatting: time zone, calendar type, pattern, date format symbols, 
 * and date interval patterns.
 * It can be instantiated in several ways:
 * <ul>
 * <li>
 * 1. create a date interval instance based on default or given locale plus 
 *    date format style FULL, or LONG, or MEDIUM, or SHORT.
 * 2. create a time interval instance based on default or given locale plus 
 *    time format style FULL, or LONG, or MEDIUM, or SHORT.
 * 3. create date and time interval instance based on default or given locale
 *    plus data and time format style
 * 4. create an instance using default or given locale plus default skeleton, 
 *    which  is "dMyhm"
 * 5. create an instance using default or given locale plus given skeleton.
 *    Users are encouraged to created date interval formatter this way and 
 *    to use the pre-defined skeleton macros, such as
 *    MONTH_YEAR_SHORT_FORMAT, which consists the calendar fields and
 *    the format style. 
 * 6. create an instance using default or given locale plus given skeleton
 *    plus a given DateIntervalInfo.
 *    This factory method is for powerful users who want to provide their own 
 *    interval patterns. 
 *    Locale provides the timezone, calendar, and format symbols information.
 *    Local plus skeleton provides full pattern information.
 *    DateIntervalInfo provides the date interval patterns.
 * <li>
 *
 * <P>
 * For the calendar field pattern letter, such as G, y, M, d, a, h, H, m, s etc.
 * DateIntervalFormat uses the same syntax as that of
 * DateTime format.
 * 
 * <P>
 * Code Sample:
 * <pre>
 * \code
 *   // the date interval object which the DateIntervalFormat formats on
 *   // and parses into
 *   DateInterval*  dtInterval = new DateInterval(1000*3600*24, 1000*3600*24*2);
 *   UErrorCode status = U_ZERO_ERROR;
 *   DateIntervalFormat* dtIntervalFmt = DateIntervalFormat::createInstance(
 *                           DAY_MONTH_YEAR_FULL_FORMAT, 
 *                           FALSE, Locale("en", "GB", ""), status);
 *   UnicodeString dateIntervalString;
 *   FieldPosition pos = 0;
 *   // formatting
 *   dtIntervalFmt->format(dtInterval, dateIntervalString, pos, status);
 *   delete dtIntervalFmt;
 * \endcode
 * </pre>
 */

class U_I18N_API DateIntervalFormat : public Format {
public:

    /**
     * Construct a DateIntervalFormat from default locale. 
     * It uses the interval patterns of skeleton ("dMyhm") 
     * from the default locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(UErrorCode& status);

    /**
     * Construct a DateIntervalFormat using given locale.
     * It uses the interval patterns of skeleton ("dMyhm") 
     * from the given locale.
     * @param locale    the given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(const Locale& locale, 
                                                        UErrorCode& status);


    /**
     * Construct a DateIntervalFormat using default locale.
     * The interval pattern is based on the date format only.
     * For full date format, interval pattern is based on skeleton "EEEEdMMMMy".
     * For long date format, interval pattern is based on skeleton "dMMMMy".
     * For medium date format, interval pattern is based on skeleton "dMMMy".
     * For short date format, interval pattern is based on skeleton "dMy".
     * @param style     The given date formatting style. For example,
     *                  SHORT for "M/d/yy" in the US locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createDateIntervalInstance( 
                                DateFormat::EStyle style,
                                UErrorCode& status);


    /**
     * Construct a DateIntervalFormat using given locale.
     * The interval pattern is based on the date format only.
     * For full date format, interval pattern is based on skeleton "EEEEdMMMMy".
     * For long date format, interval pattern is based on skeleton "dMMMMy".
     * For medium date format, interval pattern is based on skeleton "dMMMy".
     * For short date format, interval pattern is based on skeleton "dMy".
     * @param style     The given date formatting style. For example,
     *                  SHORT for "M/d/yy" in the US locale.
     * @param locale    The given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createDateIntervalInstance( 
                                DateFormat::EStyle style,
                                const Locale& locale,
                                UErrorCode& status);



    /**
     * Construct a DateIntervalFormat using default locale.
     * The interval pattern is based on the time format only.
     * @param style     The given time formatting style. For example,
     *                  SHORT for "h:mm a" in the US locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createTimeIntervalInstance( 
                                DateFormat::EStyle style,
                                UErrorCode& status);

    /**
     * Construct a DateIntervalFormat using given locale.
     * The interval pattern is based on the time format only.
     * @param style     The given time formatting style. For example,
     *                  SHORT for "h:mm a" in the US locale.
     * @param locale    The given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createTimeIntervalInstance( 
                                DateFormat::EStyle style,
                                const Locale& locale,
                                UErrorCode& status);



    /**
     * Construct a DateIntervalFormat using default locale.
     * The interval pattern is based on the date/time format.
     * @param dateStyle The given date formatting style. For example,
     *                  SHORT for "M/d/yy" in the US locale.
     * @param timeStyle The given time formatting style. For example,
     *                  SHORT for "h:mm a" in the US locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createDateTimeIntervalInstance( 
                            DateFormat::EStyle dateStyle,
                            DateFormat::EStyle timeStyle,
                            UErrorCode& status);

    /**
     * Construct a DateIntervalFormat using given locale.
     * The interval pattern is based on the date/time format.
     * @param dateStyle The given date formatting style. For example,
     *                  SHORT for "M/d/yy" in the US locale.
     * @param timeStyle The given time formatting style. For example,
     *                  SHORT for "h:mm a" in the US locale.
     * @param locale    The given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createDateTimeIntervalInstance( 
                            DateFormat::EStyle dateStyle,
                            DateFormat::EStyle timeStyle,
                            const Locale& locale,
                            UErrorCode& status);


    /**
     * Construct a DateIntervalFormat from skeleton and  default locale.
     *
     * Following are the skeletons (defined in udate.h) having predefined 
     * interval patterns in resource files.
     * Users are encouraged to use those macros.
     * For example: 
     * DateIntervalFormat::createInstance(DAY_MONTH_FULL_FORMAT, FALSE, status) 
     * 
     * #define DAY_MONTH_YEAR_DOW_LONG_FORMAT   "yMMMMEEEEd"
     * #define DAY_MONTH_YEAR_LONG_FORMAT       "yMMMMd"
     * #define DAY_MONTH_LONG_FORMAT            "MMMMd"
     * #define MONTH_YEAR_LONG_FORMAT           "yMMMM"
     * #define DAY_MONTH_DOW_LONG_FORMAT        "MMMMEEEEd"
     * #define DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT "yMMMEEEd"
     * #define DAY_MONTH_YEAR_MEDIUM_FORMAT     "yMMMd"
     * #define DAY_MONTH_MEDIUM_FORMAT          "MMMd"
     * #define MONTH_YEAR_MEDIUM_FORMAT         "yMMM"
     * #define DAY_MONTH_DOW_MEDIUM_FORMAT      "MMMEEEd"
     * #define DAY_MONTH_YEAR_DOW_SHORT_FORMAT  "yMEEEd"
     * #define DAY_MONTH_YEAR_SHORT_FORMAT      "yMd"
     * #define DAY_MONTH_SHORT_FORMAT           "Md"
     * #define MONTH_YEAR_SHORT_FORMAT          "yM"
     * #define DAY_MONTH_DOW_SHORT_FORMAT       "MEEEd"
     * #define DAY_ONLY_SHORT_FORMAT            "d"
     * #define DAY_DOW_SHORT_FORMAT             "EEEd"
     * #define YEAR_ONLY_SHORT_FORMAT           "y"
     * #define MONTH_ONLY_SHORT_FORMAT          "M"
     * #define MONTH_ONLY_MEDIUM_FORMAT         "MMM"
     * #define MONTH_ONLY_LONG_FORMAT           "MMMM"
     * #define HOUR_MINUTE_FORMAT               "hm"
     * #define HOUR_MINUTE_GENERAL_TZ_FORMAT    "hmv"
     * #define HOUR_MINUTE_DAYLIGNT_TZ_FORMAT   "hmz"
     * #define HOUR_ONLY_FORMAT                 "h"
     * #define HOUR_GENERAL_TZ_FORMAT           "hv"
     * #define HOUR_DAYLIGNT_TZ_FORMAT          "hz"
     * #define HOUR_DAYLIGNT_TZ_FORMAT          "hz"
     * 
     * The default Locale provides the interval patterns.
     * For example, if the default locale is en_GB, 
     * and if skeleton is DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT,
     * which is "yMMMEEEd",
     * the interval patterns defined in resource file for above skeleton are:
     * "EEE, d MMM, yyyy - EEE, d MMM, yyyy" for year differs,
     * "EEE, d MMM - EEE, d MMM, yyyy" for month differs,
     * "EEE, d - EEE, d MMM, yyyy" for day differs,
     *
     * @param skeleton  the skeleton on which interval format based.
     * @param adjustFieldWidth  whether adjust the skeleton field width or not
     *                          It is used for DateTimePatternGenerator on 
     *                          whether to adjust field width when get 
     *                          full pattern from skeleton
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(
                                               const UnicodeString& skeleton,
                                               UBool adjustFieldWidth,
                                               UErrorCode& status);

    /**
     * Construct a DateIntervalFormat from skeleton and a given locale.
     *
     * Following are the skeletons (defined in udate.h) having predefined 
     * interval pattern in resource files.
     * Users are encouraged to use those macros.
     * For example: 
     * DateIntervalFormat::createInstance(DAY_MONTH_FULL_FORMAT,
     *                                    FALSE, loc, status) 
     * 
     * #define DAY_MONTH_YEAR_DOW_LONG_FORMAT   "yMMMMEEEEd"
     * #define DAY_MONTH_YEAR_LONG_FORMAT       "yMMMMd"
     * #define DAY_MONTH_LONG_FORMAT            "MMMMd"
     * #define MONTH_YEAR_LONG_FORMAT           "yMMMM"
     * #define DAY_MONTH_DOW_LONG_FORMAT        "MMMMEEEEd"
     * #define DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT "yMMMEEEd"
     * #define DAY_MONTH_YEAR_MEDIUM_FORMAT     "yMMMd"
     * #define DAY_MONTH_MEDIUM_FORMAT          "MMMd"
     * #define MONTH_YEAR_MEDIUM_FORMAT         "yMMM"
     * #define DAY_MONTH_DOW_MEDIUM_FORMAT      "MMMEEEd"
     * #define DAY_MONTH_YEAR_DOW_SHORT_FORMAT  "yMEEEd"
     * #define DAY_MONTH_YEAR_SHORT_FORMAT      "yMd"
     * #define DAY_MONTH_SHORT_FORMAT           "Md"
     * #define MONTH_YEAR_SHORT_FORMAT          "yM"
     * #define DAY_MONTH_DOW_SHORT_FORMAT       "MEEEd"
     * #define DAY_ONLY_SHORT_FORMAT            "d"
     * #define DAY_DOW_SHORT_FORMAT             "EEEd"
     * #define YEAR_ONLY_SHORT_FORMAT           "y"
     * #define MONTH_ONLY_SHORT_FORMAT          "M"
     * #define MONTH_ONLY_MEDIUM_FORMAT         "MMM"
     * #define MONTH_ONLY_LONG_FORMAT           "MMMM"
     * #define HOUR_MINUTE_FORMAT               "hm"
     * #define HOUR_MINUTE_GENERAL_TZ_FORMAT    "hmv"
     * #define HOUR_MINUTE_DAYLIGNT_TZ_FORMAT   "hmz"
     * #define HOUR_ONLY_FORMAT                 "h"
     * #define HOUR_GENERAL_TZ_FORMAT           "hv"
     * #define HOUR_DAYLIGNT_TZ_FORMAT          "hz"
     *
     * The given Locale provides the interval patterns.
     * For example, for en_GB, if skeleton is DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT,
     * which is "yMMMEEEdMMM",
     * the interval patterns defined in resource file to above skeleton are:
     * "EEE, d MMM, yyyy - EEE, d MMM, yyyy" for year differs,
     * "EEE, d MMM - EEE, d MMM, yyyy" for month differs,
     * "EEE, d - EEE, d MMM, yyyy" for day differs,
     * @param skeleton  the skeleton on which interval format based.
     * @param adjustFieldWidth  whether adjust the skeleton field width or not
     *                          It is used for DateTimePatternGenerator on 
     *                          whether to adjust field width when get 
     *                          full pattern from skeleton
     * @param locale    the given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(
                                               const UnicodeString& skeleton,
                                               UBool adjustFieldWidth,
                                               const Locale& locale, 
                                               UErrorCode& status);

    /**
     * Construct a DateIntervalFormat from skeleton
     * and a DateIntervalInfo.
     *
     * Following are the skeletons having predefined interval pattern
     * in resource files. ( they are defined in udat.h )
     * Users are encouraged to use those macros.
     * For example: 
     * DateIntervalFormat::createInstance(DAY_MONTH_FULL_FORMAT,
     *                                    FALSE, itvinf, status) 
     * 
     * #define DAY_MONTH_YEAR_DOW_LONG_FORMAT   "yMMMMEEEEd"
     * #define DAY_MONTH_YEAR_LONG_FORMAT       "yMMMMd"
     * #define DAY_MONTH_LONG_FORMAT            "MMMMd"
     * #define MONTH_YEAR_LONG_FORMAT           "yMMMM"
     * #define DAY_MONTH_DOW_LONG_FORMAT        "MMMMEEEEd"
     * #define DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT "yMMMEEEd"
     * #define DAY_MONTH_YEAR_MEDIUM_FORMAT     "yMMMd"
     * #define DAY_MONTH_MEDIUM_FORMAT          "MMMd"
     * #define MONTH_YEAR_MEDIUM_FORMAT         "yMMM"
     * #define DAY_MONTH_DOW_MEDIUM_FORMAT      "MMMEEEd"
     * #define DAY_MONTH_YEAR_DOW_SHORT_FORMAT  "yMEEEd"
     * #define DAY_MONTH_YEAR_SHORT_FORMAT      "yMd"
     * #define DAY_MONTH_SHORT_FORMAT           "Md"
     * #define MONTH_YEAR_SHORT_FORMAT          "yM"
     * #define DAY_MONTH_DOW_SHORT_FORMAT       "MEEEd"
     * #define DAY_ONLY_SHORT_FORMAT            "d"
     * #define DAY_DOW_SHORT_FORMAT             "EEEd"
     * #define YEAR_ONLY_SHORT_FORMAT           "y"
     * #define MONTH_ONLY_SHORT_FORMAT          "M"
     * #define MONTH_ONLY_MEDIUM_FORMAT         "MMM"
     * #define MONTH_ONLY_LONG_FORMAT           "MMMM"
     * #define HOUR_MINUTE_FORMAT               "hm"
     * #define HOUR_MINUTE_GENERAL_TZ_FORMAT    "hmv"
     * #define HOUR_MINUTE_DAYLIGNT_TZ_FORMAT   "hmz"
     * #define HOUR_ONLY_FORMAT                 "h"
     * #define HOUR_GENERAL_TZ_FORMAT           "hv"
     * #define HOUR_DAYLIGNT_TZ_FORMAT          "hz"
     * the DateIntervalInfo provides the interval patterns.
     *
     * User are encouraged to set default interval pattern in DateIntervalInfo
     * as well, if they want to set other interval patterns ( instead of
     * reading the interval patterns from resource files).
     * When the corresponding interval pattern for a largest calendar different
     * field is not found ( if user not set it ), interval format fallback to
     * the default interval pattern.
     * If user does not provide default interval pattern, it fallback to
     * "{date0} - {date1}" 
     *
     * Note: the DateIntervalFormat takes ownership of 
     * DateIntervalInfo objects. 
     * Caller should not delete them.
     *
     * @param skeleton  the skeleton on which interval format based.
     * @param adjustFieldWidth  whether adjust the skeleton field width or not
     *                          It is used for DateTimePatternGenerator on 
     *                          whether to adjust field width when get 
     *                          full pattern from skeleton
     * @param dtitvinf  the DateIntervalInfo object to be adopted.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(
                                              const UnicodeString& skeleton,
                                              UBool adjustFieldWidth,
                                              DateIntervalInfo* dtitvinf,
                                              UErrorCode& status);

    /**
     * Construct a DateIntervalFormat from skeleton
     * and a DateIntervalInfo.
     *
     * Following are the skeletons having predefined interval pattern
     * in resource files. ( they are defined in udat.h )
     * Users are encouraged to use those macros.
     * For example: 
     * DateIntervalFormat::createInstance(DAY_MONTH_FULL_FORMAT,
     *                                    FALSE, locale, itvinf, status) 
     * 
     * #define DAY_MONTH_YEAR_DOW_LONG_FORMAT   "yMMMMEEEEd"
     * #define DAY_MONTH_YEAR_LONG_FORMAT       "yMMMMd"
     * #define DAY_MONTH_LONG_FORMAT            "MMMMd"
     * #define MONTH_YEAR_LONG_FORMAT           "yMMMM"
     * #define DAY_MONTH_DOW_LONG_FORMAT        "MMMMEEEEd"
     * #define DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT "yMMMEEEd"
     * #define DAY_MONTH_YEAR_MEDIUM_FORMAT     "yMMMd"
     * #define DAY_MONTH_MEDIUM_FORMAT          "MMMd"
     * #define MONTH_YEAR_MEDIUM_FORMAT         "yMMM"
     * #define DAY_MONTH_DOW_MEDIUM_FORMAT      "MMMEEEd"
     * #define DAY_MONTH_YEAR_DOW_SHORT_FORMAT  "yMEEEd"
     * #define DAY_MONTH_YEAR_SHORT_FORMAT      "yMd"
     * #define DAY_MONTH_SHORT_FORMAT           "Md"
     * #define MONTH_YEAR_SHORT_FORMAT          "yM"
     * #define DAY_MONTH_DOW_SHORT_FORMAT       "MEEEd"
     * #define DAY_ONLY_SHORT_FORMAT            "d"
     * #define DAY_DOW_SHORT_FORMAT             "EEEd"
     * #define YEAR_ONLY_SHORT_FORMAT           "y"
     * #define MONTH_ONLY_SHORT_FORMAT          "M"
     * #define MONTH_ONLY_MEDIUM_FORMAT         "MMM"
     * #define MONTH_ONLY_LONG_FORMAT           "MMMM"
     * #define HOUR_MINUTE_FORMAT               "hm"
     * #define HOUR_MINUTE_GENERAL_TZ_FORMAT    "hmv"
     * #define HOUR_MINUTE_DAYLIGNT_TZ_FORMAT   "hmz"
     * #define HOUR_ONLY_FORMAT                 "h"
     * #define HOUR_GENERAL_TZ_FORMAT           "hv"
     * #define HOUR_DAYLIGNT_TZ_FORMAT          "hz"
     *
     * the DateIntervalInfo provides the interval patterns.
     *
     * User are encouraged to set default interval pattern in DateIntervalInfo
     * as well, if they want to set other interval patterns ( instead of
     * reading the interval patterns from resource files).
     * When the corresponding interval pattern for a largest calendar different
     * field is not found ( if user not set it ), interval format fallback to
     * the default interval pattern.
     * If user does not provide default interval pattern, it fallback to
     * "{date0} - {date1}" 
     *
     * Note: the DateIntervalFormat takes ownership of 
     * DateIntervalInfo objects. 
     * Caller should not delete them.
     *
     * @param skeleton  the skeleton on which interval format based.
     * @param adjustFieldWidth  whether adjust the skeleton field width or not
     *                          It is used for DateTimePatternGenerator on 
     *                          whether to adjust field width when get 
     *                          full pattern from skeleton
     * @param locale    the given locale.
     * @param dtitvinf  the DateIntervalInfo object to be adopted.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(
                                              const UnicodeString& skeleton,
                                              UBool adjustFieldWidth,
                                              const Locale& locale, 
                                              DateIntervalInfo* dtitvinf,
                                              UErrorCode& status);

    /**
     * Destructor.
     * @draft ICU 4.0
     */
    virtual ~DateIntervalFormat();

    /**
     * Clone this Format object polymorphically. The caller owns the result and
     * should delete it when done.
     * @return    A copy of the object.
     * @draft ICU 4.0
     */
    virtual Format* clone(void) const;

    /**
     * Return true if the given Format objects are semantically equal. Objects
     * of different subclasses are considered unequal.
     * @param other    the object to be compared with.
     * @return         true if the given Format objects are semantically equal.
     * @draft ICU 4.0
     */
    virtual UBool operator==(const Format& other) const;

    /**
     * Return true if the given Format objects are not semantically equal. 
     * Objects of different subclasses are considered unequal.
     * @param other the object to be compared with.
     * @return      true if the given Format objects are not semantically equal.
     * @draft ICU 4.0
     */
    UBool operator!=(const Format& other) const;

    /**
     * Format an object to produce a string. This method handles Formattable
     * objects with a DateInterval type. 
     * If a the Formattable object type is not a DateInterval,
     * then it returns a failing UErrorCode.
     *
     * @param obj               The object to format. 
     *                          Must be a DateInterval.
     * @param appendTo          Output parameter to receive result.
     *                          Result is appended to existing contents.
     * @param fieldPosition     On input: an alignment field, if desired.
     *                          On output: the offsets of the alignment field.
     * @param status            Output param filled with success/failure status.
     * @return                  Reference to 'appendTo' parameter.
     * @draft ICU 4.0
     */
    virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& appendTo,
                                  FieldPosition& fieldPosition,
                                  UErrorCode& status) const ;
                                    
                                    

    /**
     * Format a DateInterval to produce a string. 
     *
     * @param dtInterval        DateInterval to be formatted.
     * @param appendTo          Output parameter to receive result.
     *                          Result is appended to existing contents.
     * @param fieldPosition     On input: an alignment field, if desired.
     *                          On output: the offsets of the alignment field.
     * @param status            Output param filled with success/failure status.
     * @return                  Reference to 'appendTo' parameter.
     * @draft ICU 4.0
     */
    UnicodeString& format(const DateInterval* dtInterval,
                          UnicodeString& appendTo,
                          FieldPosition& fieldPosition,
                          UErrorCode& status) const ;
                                    
                                    
    /**
     * Format 2 Calendars to produce a string. 
     *
     * Note: "fromCalendar" and "toCalendar" are not const,
     * since calendar is not const in  SimpleDateFormat::format(Calendar&),
     *
     * @param fromCalendar      calendar set to the from date in date interval
     *                          to be formatted into date interval stirng
     * @param toCalendar        calendar set to the to date in date interval
     *                          to be formatted into date interval stirng
     * @param appendTo          Output parameter to receive result.
     *                          Result is appended to existing contents.
     * @param fieldPosition     On input: an alignment field, if desired.
     *                          On output: the offsets of the alignment field.
     * @param status            Output param filled with success/failure status.
     *                          Caller needs to make sure it is SUCCESS
     *                          at the function entrance
     * @return                  Reference to 'appendTo' parameter.
     * @draft ICU 4.0
     */
    UnicodeString& format(Calendar& fromCalendar,
                          Calendar& toCalendar,
                          UnicodeString& appendTo,
                          FieldPosition& fieldPosition,
                          UErrorCode& status) const ;

    /**
     * Parse a string to produce an object. This methods handles parsing of
     * date time interval strings into Formattable objects with 
     * DateInterval type, which is a pair of UDate.
     * <P>
     * In ICU 4.0, date interval format is not supported.
     * <P>
     * Before calling, set parse_pos.index to the offset you want to start
     * parsing at in the source. After calling, parse_pos.index is the end of
     * the text you parsed. If error occurs, index is unchanged.
     * <P>
     * When parsing, leading whitespace is discarded (with a successful parse),
     * while trailing whitespace is left as is.
     * <P>
     * See Format::parseObject() for more.
     *
     * @param source    The string to be parsed into an object.
     * @param result    Formattable to be set to the parse result.
     *                  If parse fails, return contents are undefined.
     * @param parse_pos The position to start parsing at. Upon return
     *                  this param is set to the position after the
     *                  last character successfully parsed. If the
     *                  source is not parsed successfully, this param
     *                  will remain unchanged.
     * @return          A newly created Formattable* object, or NULL
     *                  on failure.  The caller owns this and should
     *                  delete it when done.
     * @draft ICU 4.0
     */
    virtual void parseObject(const UnicodeString& source,
                             Formattable& result,
                             ParsePosition& parse_pos) const;


    /**
     * Gets the date time interval patterns.
     * @return a copy of the date time interval patterns associated with
     * this date interval formatter.
     * @draft ICU 4.0
     */
    const DateIntervalInfo* getDateIntervalInfo(void) const;


    /**
     * Set the date time interval patterns. 
     * @param newIntervalPatterns   the given interval patterns to copy.
     * @draft ICU 4.0
     */
    void setDateIntervalInfo(const DateIntervalInfo& newIntervalPatterns);

    /**
     * Set the date time interval patterns. 
     * The caller no longer owns the DateIntervalInfo object and
     * should not delete it after making this call.
     * @param newIntervalPatterns   the given interval patterns to copy.
     * @draft ICU 4.0
     */
    void adoptDateIntervalInfo(DateIntervalInfo* newIntervalPatterns);


    /**
     * Gets the date formatter
     * @return a copy of the date formatter associated with
     * this date interval formatter.
     * @draft ICU 4.0
     */
    const DateFormat* getDateFormat(void) const;


    /**
     * Set the date formatter.
     * @param newDateFormat   the given date formatter to copy.
     *                        caller needs to make sure that
     *                        it is a SimpleDateFormatter.
     * @param status          Output param set to success/failure code.
     *                        caller needs to make sure it is SUCCESS
     *                        at the function entrance.
     * @draft ICU 4.0
     */
    void setDateFormat(const DateFormat& newDateFormat, UErrorCode& status);

    /**
     * Set the date formatter.
     * The caller no longer owns the DateFormat object and
     * should not delete it after making this call.
     * @param newDateFormat   the given date formatter to copy.
     *                        caller needs to make sure that
     *                        it is a SimpleDateFormatter.
     * @param status          Output param set to success/failure code.
     *                        caller needs to make sure it is SUCCESS
     *                        at the function entrance.
     * @draft ICU 4.0
     */
    void adoptDateFormat(DateFormat* newDateFormat, UErrorCode& status);


    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @draft ICU 4.0
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @draft ICU 4.0
     */
    virtual UClassID getDynamicClassID(void) const;

protected:

    /**
     * Copy constructor.
     * @draft ICU 4.0
     */
    DateIntervalFormat(const DateIntervalFormat&);

    /**
     * Assignment operator.
     * @draft ICU 4.0
     */
    DateIntervalFormat& operator=(const DateIntervalFormat&);

private:

    /* 
     * Default constructor, not implemented
     */
    DateIntervalFormat(); 

    /**
     * Construct a DateIntervalFormat from DateFormat
     * and a DateIntervalInfo.
     * DateFormat provides the timezone, calendar,
     * full pattern, and date format symbols information.
     * It should be a SimpleDateFormat object which 
     * has a pattern in it.
     * the DateIntervalInfo provides the interval patterns.
     *
     * Note: the DateIntervalFormat takes ownership of both 
     * DateFormat and DateIntervalInfo objects. 
     * Caller should not delete them.
     *
     * @param dtfmt     the SimpleDateFormat object to be adopted.
     * @param dtitvinf  the DateIntervalInfo object to be adopted.
     * @param status    Output param set to success/failure code.
     * @draft ICU 4.0
     */
    DateIntervalFormat(DateFormat* dtfmt, 
                       DateIntervalInfo* dtItvInfo, 
                       UErrorCode& status);


    /**
     * Construct a DateIntervalFormat from DateFormat
     * and a given locale.
     * DateFormat provides the timezone, calendar,
     * full pattern, and date format symbols information.
     * It should be a SimpleDateFormat object which 
     * has a pattern in it.
     *
     * Note: the DateIntervalFormat takes ownership of the
     * DateFormat object. Caller should not delete it.
     *
     * The given Locale provides the interval patterns.
     * A skeleton is derived from the full pattern. 
     * And the interval patterns are those the skeleton maps to
     * in the given locale.
     * For example, for en_GB, if the pattern in the SimpleDateFormat
     * is "EEE, d MMM, yyyy", the skeleton is "EEEdMMMyyyy".
     * And the interval patterns are:
     * "EEE, d MMM, yyyy - EEE, d MMM, yyyy" for year differs,
     * "EEE, d MMM - EEE, d MMM, yyyy" for month differs,
     * "EEE, d - EEE, d MMM, yyyy" for day differs,
     * @param dtfmt     the DateFormat object to be adopted.
     * @param locale    the given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 create(DateFormat* dtfmt,
                                                const Locale& locale, 
                                                UErrorCode& status);

    /**
     * Construct a DateIntervalFormat from DateFormat
     * and a DateIntervalInfo.
     * DateFormat provides the timezone, calendar,
     * full pattern, and date format symbols information.
     * It should be a SimpleDateFormat object which 
     * has a pattern in it.
     * the DateIntervalInfo provides the interval patterns.
     *
     * Note: the DateIntervalFormat takes ownership of both 
     * DateFormat and DateIntervalInfo objects. 
     * Caller should not delete them.
     *
     * @param dtfmt     the DateFormat object to be adopted.
     * @param dtitvinf  the DateIntervalInfo object to be adopted.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 create(DateFormat* dtfmt,
                                              DateIntervalInfo* dtitvinf,
                                              UErrorCode& status);

    /**
     * The interval patterns for this formatter.
     */
    DateIntervalInfo*     fIntervalPattern;

    /**
     * The DateFormat object used to format single pattern
     */
    SimpleDateFormat*     fDateFormat;

    /**
     * The 2 calendars with the from and to date.
     * could re-use the calendar in fDateFormat,
     * but keeping 2 calendars make it clear and clean.
     */
    Calendar* fFromCalendar;
    Calendar* fToCalendar;
};
 


inline UBool 
DateIntervalFormat::operator!=(const Format& other) const  {
    return !operator==(other); 
}
 
inline const DateIntervalInfo*
DateIntervalFormat::getDateIntervalInfo() const {
    return fIntervalPattern;
}


inline void
DateIntervalFormat::setDateIntervalInfo(const DateIntervalInfo& newItvPattern) {
    delete fIntervalPattern;
    fIntervalPattern = new DateIntervalInfo(newItvPattern);
}


inline void 
DateIntervalFormat::adoptDateIntervalInfo(DateIntervalInfo* newItvPattern) {
    delete fIntervalPattern;
    fIntervalPattern = newItvPattern;
}

 
inline const DateFormat*
DateIntervalFormat::getDateFormat() const {
    return fDateFormat;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // _DTITVFMT_H__
//eof
