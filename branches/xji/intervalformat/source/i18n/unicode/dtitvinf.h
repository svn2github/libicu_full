/*
*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTITVINF.H 
*
*******************************************************************************
*/

#ifndef __DTITVINF_H__
#define __DTITVINF_H__

/**
 * \file
 * \brief C++ API: Date/Time interval patterns for formatting date/time interval
 */

#if !UCONFIG_NO_FORMATTING

#include "gregoimp.h"
#include "uresimp.h"
#include "unicode/utypes.h"
#include "unicode/udat.h"
#include "unicode/locid.h"
#include "unicode/ucal.h"
#include "unicode/dtptngen.h"
#include "dtitv_impl.h"



U_NAMESPACE_BEGIN


/**
 * DateIntervalInfo is a public class for encapsulating localizable
 * date time interval patterns. It is used by DateIntervalFormat.
 *
 * <P>
 * Logically, the interval patterns are mappings 
 * from (skeleton, the_largest_different_calendar_field)  
 * to (date_interval_pattern).
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
 * There are 2 dates in interval pattern. For most locales, the first date
 * in an interval pattern is the earlier date. There might be a locale in which
 * the first date in an interval pattern is the later date.
 * We use fallback format for the default order for the locale.
 * For example, if the fallback format is "{0} - {1}", it means
 * the first date in the interval pattern for this locale is earlier date.
 * If the fallback format is "{1} - {0}", it means the first date is the 
 * later date.
 * For a paticular interval pattern, the default order can be overriden
 * by prefixing "latestFirst:" or "earliestFirst:" to the interval pattern.
 * For example, if the fallback format is "{0}-{1}",
 * but for skeleton "yMMMd", the interval pattern when day is different is 
 * "latestFirst:d-d MMM yy", it means by default, the first date in interval
 * pattern is the earlier date. But for skeleton "yMMMd", when day is different,
 * the first date in "d-d MMM yy" is the later date.
 * 
 * <P>
 * The recommended way to create a DateIntervalFormat object is to pass in 
 * the locale plus the format style or skeleton itself.
 * By using a Locale parameter, the DateIntervalFormat object is 
 * initialized with the pre-defined interval patterns for a given or 
 * default locale.
 * <P>
 * Users can also create DateIntervalFormat object 
 * by supplying their own interval patterns.
 * It provides flexibility for powerful usage.
 *
 * <P>
 * After a DateIntervalInfo object is created, clients may modify
 * the interval patterns using setIntervalPatterns function as so desired.
 * Currently, users can only set interval patterns when the following 
 * calendar fields are different: ERA, YEAR, MONTH, DATE,  DAY_OF_MONTH, 
 * DAY_OF_WEEK, AM_PM,  HOUR, HOUR_OF_DAY, and MINUTE.
 * Interval patterns when other calendar fields are different is not supported.
 * <P>
 * DateIntervalInfo objects are clonable. 
 * When clients obtain a DateIntervalInfo object, 
 * they can feel free to modify it as necessary.
 * <P>
 * DateIntervalInfo are not expected to be subclassed. 
 * Data for a calendar is loaded out of resource bundles. 
 * To ICU 4.0, date interval patterns are only supported in Gregorian calendar. 
 * @draft ICU 4.0
**/

class U_I18N_API DateIntervalInfo : public UObject {
public:

    /**
     * Default constructor.
     * It does not initialize any interval patterns.
     * It should be followed by setIntervalPattern(), 
     * and is recommended to be used only for powerful users who
     * wants to create their own interval patterns and use them to create
     * date interval formatter.
     */
    DateIntervalInfo();

    /** 
     * Construct DateIntervalInfo for the default local,
     * Gregorian calendar, and date/time skeleton "yMdhm".
     * @param status  output param set to success/failure code on exit, which
     * @draft ICU 4.0
     */
    DateIntervalInfo(UErrorCode& status);

    /** 
     * Construct DateIntervalInfo for the given local,
     * Gregorian calendar, and date/time skeleton "yMdhm".
     * @param locale  the interval patterns are loaded from the Gregorian 
     *                calendar data in this locale.
     * @param status  output param set to success/failure code on exit, which
     * @draft ICU 4.0
     */
    DateIntervalInfo(const Locale& locale, UErrorCode& status);


    /** 
     * Construct DateIntervalInfo for the given local, 
     * Gregorian calendar and skeleton.
     * @param locale    the interval patterns are loaded from the 
     *                  Gregorian calendar data in this locale.
     * @param skeleton  the interval patterns are loaded from this skeleton
     *                  in the calendar data.
     * @param status    output param set to success/failure code on exit, which
     * @draft ICU 4.0
     */
    DateIntervalInfo(const Locale& locale, 
                     const UnicodeString& skeleton, 
                     UErrorCode& status);


    /**
     * Copy constructor.
     * @draft ICU 4.0
     */
    DateIntervalInfo(const DateIntervalInfo&);

    /**
     * Assignment operator
     * @draft ICU 4.0
     */
    DateIntervalInfo& operator=(const DateIntervalInfo&);

    /**
     * Clone this object polymorphically.
     * The caller owns the result and should delete it when done.
     * @return   a copy of the object
     * @draft    ICU4.0
     */
    DateIntervalInfo* clone(void) const;

    /**
     * Destructor.
     * It is virtual to be safe, but it is not designed to be subclassed.
     * @draft ICU 4.0
     */
    virtual ~DateIntervalInfo();


    /**
     * Return true if another object is semantically equal to this one.
     *
     * @param other    the DateIntervalInfo object to be compared with.
     * @return         true if other is semantically equal to this.
     * @stable ICU 4.0
     */
    UBool operator==(const DateIntervalInfo& other) const;

    /**
     * Return true if another object is semantically unequal to this one.
     *
     * @param other    the DateIntervalInfo object to be compared with.
     * @return         true if other is semantically unequal to this.
     * @stable ICU 4.0
     */
    UBool operator!=(const DateIntervalInfo& other) const;


    /** 
     * Provides a way for client to build interval patterns.
     * User could construct DateIntervalInfo by providing 
     * a list of patterns.
     * <P>
     * For example:
     * <pre>
     * DateIntervalInfo* dIntervalInfo = new DateIntervalInfo();
     * dIntervalInfo->setIntervalPattern(UCAL_YEAR, "'from' YYYY-M-d 'to' YYYY-M-d"); 
     * dIntervalInfo->setIntervalPattern(UCAL_MONTH, "'from' YYYY MMM d 'to' MMM d");
     * dIntervalInfo->setIntervalPattern(UCAL_DAY, "YYYY MMM d-d");
     * dIntervalInfo->setFallbackIntervalPattern("yyyy mm dd - yyyy mm dd");
     * </pre>
     *
     * Restriction: 
     * Currently, users can only set interval patterns when the following 
     * calendar fields are different: ERA, YEAR, MONTH, DATE,  DAY_OF_MONTH, 
     * DAY_OF_WEEK, AM_PM,  HOUR, HOUR_OF_DAY, and MINUTE.
     * Interval patterns when other calendar fields are different are 
     * not supported.
     *
     * @param lrgDiffCalUnit   the largest different calendar unit.
     * @param intervalPattern  the interval pattern on the largest different
     *                         calendar unit.
     *                         For example, if lrgDiffCalUnit is 
     *                         "year", the interval pattern for en_US when year
     *                         is different could be "'from' yyyy 'to' yyyy".
     * @param status           output param set to success/failure code on exit,
     * @draft ICU 4.0
     */
    void setIntervalPattern(UCalendarDateFields lrgDiffCalUnit,
                            const UnicodeString& intervalPattern,
                            UErrorCode& status);

    /**
     * Get the interval pattern given the largest different calendar field.
     * @param field      the largest different calendar field
     * @param status     output param set to success/failure code on exit,
     * @return interval pattern
     * @draft ICU 4.0 
     */
    const UnicodeString* getIntervalPattern(UCalendarDateFields field,
                                            UErrorCode& status) const;

    /**
     * Get the fallback interval pattern.
     * @return fallback interval pattern
     * @draft ICU 4.0 
     */
    const UnicodeString* getFallbackIntervalPattern() const;

    /**
     * Set the fallback interval pattern.
     * Fall-back interval pattern is get from locale resource.
     * If a user want to set their own fall-back interval pattern,
     * they can do so by calling the following method.
     * For users who construct DateIntervalInfo() by default constructor,
     * all interval patterns ( including fall-back ) are not set,
     * those users need to call setIntervalPattern() to set their own
     * interval patterns, and call setFallbackIntervalPattern() to set
     * their own fall-back interval patterns. If a certain interval pattern
     * ( for example, the interval pattern when 'year' is different ) is not
     * found, fall-back pattern will be used. 
     * For those users who set all their patterns ( instead of calling 
     * non-default constructor to let constructor get those patterns from 
     * locale ), if they do not set the fall-back interval pattern, 
     * it will be fall-back to '{date0} - {date1}'.
     *
     * @param fallbackPattern     fall-back interval pattern.
     * @draft ICU 4.0 
     */
    void setFallbackIntervalPattern(const UnicodeString& fallbackPattern); 


    /**
     * check whether the first date in interval pattern is the later date.
     * An example of interval pattern is "d MMM, yyyy - d MMM, yyyy",
     * in which, the first part of the pattern is the earlier date.
     * If the interval pattern is prefixed with "later_first:", for example:
     * "later_first:d MMM, yyyy - d MMM, yyyy", it means the first part
     * of the patter is the later date.
     * @return  true if the first date in interval pattern is the later date.
     * @draft ICU 4.0
     */
    UBool firstDateInPtnIsLaterDate() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @stable ICU 4.0
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @stable ICU 4.0
     */
    static UClassID U_EXPORT2 getStaticClassID();

private:

    
    /** 
     * Initialize the DateIntervalInfo from locale and skeleton.
     * @param locale   the given locale.
     * @param skeleton the given skeleton on which the interval patterns based.
     * @param status   Output param filled with success/failure status.
     * @draft ICU 4.0 
     */
    void init(const Locale& locale, const UnicodeString& skeleton, 
              UErrorCode& status);

    /** 
     * get separated date and time skeleton from a combined skeleton.
     *
     * The difference between date skeleton and normalizedDateSkeleton are:
     * 1. both 'y' and 'd' are appeared only once in normalizeDateSkeleton
     * 2. 'E' and 'EE' are normalized into 'EEE'
     * 3. 'MM' is normalized into 'M'
     *
     ** the difference between time skeleton and normalizedTimeSkeleton are:
     * 1. both 'H' and 'h' are normalized as 'h' in normalized time skeleton,
     * 2. 'a' is omitted in normalized time skeleton.
     * 3. there is only one appearance for 'h', 'm','v', 'z' in normalized time
     *    skeleton
     *
     *
     *  @param skeleton               given combined skeleton.
     *  @param dateSkeleton           Output parameter for date only skeleton.
     *  @param normalizedDateSkeleton Output parameter for normalized date only
     *
     *  @param timeSkeleton           Output parameter for time only skeleton.
     *  @param normalizedTimeSkeleton Output parameter for normalized time only
     *                                skeleton.
     *
     * @draft ICU 4.0 
     */
    static void getDateTimeSkeleton(const UnicodeString& skeleton,
                                    UnicodeString& dateSkeleton,
                                    UnicodeString& normalizedDateSkeleton,
                                    UnicodeString& timeSkeleton,
                                    UnicodeString& normalizedTimeSkeleton);

    /**
     * generate the following fallback patterns:
     * fFallbackIntervalPattern;
     * fDateFallbackIntervalPattern;
     * fTimeFallbackIntervalPattern;
     * fDateTimeFallbackIntervalPattern;
     *
     * @param dtpng    the date time pattern generator,
     *                 used to generate pattern from skeleton.
     *                 NOTE: dtpng should be a constant, 
     *                 but DateTimePatternGenerator::getBestPattern() 
     *                 is not a const function, 
     *                 so, can not declare dtpng as constant.
     * @param itvDtPtnResource       interval date time patterns resource
     * @param skeleton               on which the fFallbackIntervalPattern based
     * @param shtDateFmt             short date format
     * @param shtDateFmtLength       short date format length
     * @param shtTimeFmt             short time format
     * @param shtTimeFmtLength       short time format length
     * @param dtFmt                  date and time format
     * @param dtFmtLength            date and time format length
     * @param status            Output param filled with success/failure status.
     * @draft ICU 4.0 
     */
    void genFallbackPattern(DateTimePatternGenerator* dtpng,
                            const UResourceBundle* itvDtPtnResource,
                            const UnicodeString& skeleton,
                            const UChar* shtDateFmt,
                            int32_t shtDateFmtLength,
                            const UChar* shtTimeFmt,
                            int32_t shtTimeFmtLength,
                            const UChar* dtFmt,
                            int32_t dtFmtLength,
                            UErrorCode& status);


    /**
     * generate fallback {pattern} - {pattern} from pattern.
     * @param intervalPattern   Outupt parameter, the fallback interval pattern
     * @param pattern           the given pattern, based on which interval 
     *                          pattern is generated.
     * @param itvDtPtnResource  interval date time pattern resource
     * @param status            Output param filled with success/failure status.
     * @draft ICU 4.0 
     */
    void genFallbackFromPattern(const UnicodeString& pattern,
                                const UResourceBundle* itvDtPtnResource,
                                UnicodeString& intervalPattern,
                                UErrorCode& status);

    /**
     * generate fallback {string} - {string}
     * @param intervalPattern   Outupt parameter, the fallback interval pattern
     * @param pattern           the given pattern string, based on which 
     *                          interval pattern is generated.
     * @param strLength         pattern string length
     * @param itvDtPtnResource  interval date time pattern resource
     * @param status            Output param filled with success/failure status.
     * @draft ICU 4.0 
     */
    void genFallbackFromPattern(const UChar* string,
                                int32_t strLength,
                                const UResourceBundle* itvDtPtnResource,
                                UnicodeString& intervalPattern,
                                UErrorCode& status);

    /**
     * generate fallback {date0} - {date1}
     * NOTE: both date0 and date1 can not be declared as const,
     * since in Formattable::adoptString(Unicode* str), str is not constant.
     * @param intervalPattern   Outupt parameter, the fallback interval pattern
     * @param date0             the from pattern string
     * @param date1             the to pattern string
     * @param itvDtPtnResource  interval date time pattern resource
     * @param status            Output param filled with success/failure status.
     *                          caller needs to make sure it is SUCCESS at
     *                          the entrance.
     * @draft ICU 4.0 
     */
    void genFallbackFromPattern(UnicodeString* date0,
                                UnicodeString* date1,
                                const UResourceBundle* itvDtPtnResource,
                                UnicodeString& intervalPattern,
                                UErrorCode& status);

    
    /**
     * check whether a calendar field present in a skeleton.
     * @param field      calendar field need to check
     * @param skeleton   given skeleton on which to check the calendar field
     * @return           true if field present in a skeleton.
     * @draft ICU 4.0 
     */
    static UBool fieldExistsInSkeleton(UCalendarDateFields field, 
                                       const UnicodeString& skeleton);


    /**
     * initialize interval pattern from resource file
     *
     * fill interval pattern for year/month/day differs for date only skeleton.
     * fill interval pattern for ampm/hour/minute differ for time only and 
     * date/time skeleton.  
     * @param intervalDateTimePtn    interval date time pattern resource
     * @param dateSkeleton           date skeleton
     * @param timeSkeleton           time skeleton
     * @param status           Output param filled with success/failure status.
     * @draft ICU 4.0 
     */
    void initIntvPtnFromRes(const UResourceBundle* intervalDateTimePtn,
                            const UnicodeString& dateSkeleton,
                            const UnicodeString& timeSkeleton,
                            UErrorCode& status);


    /**
     * Concat a single date pattern with a time interval pattern,
     * set it into the fIntervalPattern[field], while field is time field.
     * This is used to handle time interval patterns on skeleton with
     * both time and date. Present the date followed by 
     * the range expression for the time.
     * @param dtfmt                  date and time format
     * @param dtfmtLength            date and time format length
     * @param datePattern            date pattern
     * @param field                  time calendar field: AM_PM, HOUR, MINUTE
     * @param status           Output param filled with success/failure status.
     * @draft ICU 4.0 
     */
    void concatSingleDate2TimeInterval(
                                 const UChar* dtfmt,
                                 int32_t dtfmtLength,
                                 const UnicodeString& datePattern,
                                 UCalendarDateFields field,
                                 UErrorCode& status);


    /**
     * concatenate date pattern and time pattern.
     *
     * @param dtfmt                  date and time format
     * @param dtfmtLength            date and time format length
     * @param timeStr                time pattern
     * @param dateStr                date pattern
     * @param combinedPattern        Output parameter. The combined date and
     *                               time pattern
     * @param status           Output param filled with success/failure status.
     *                         caller needs to make sure it is SUCCESS at
     *                         the entrance.
     * @draft ICU 4.0 
     */
    static void concatDateTimePattern(const UChar* dtfmt,
                                      int32_t dtfmtLength,
                                      UnicodeString* timeStr,
                                      UnicodeString* dateStr,
                                      UnicodeString& combinedPattern,
                                      UErrorCode& status);



    static const UChar fgCalendarFieldToPatternLetter[];
    /**
     * the skeleton on which the interval patterns based.
     */
    UnicodeString fSkeleton;

    // the mininum different calendar field is UCAL_MINUTE
    UnicodeString        fIntervalPatterns[MINIMUM_SUPPORTED_CALENDAR_FIELD+1];

    // default interval pattern on the skeleton, {0} - {1}
    UnicodeString fFallbackIntervalPattern;

    // fall-back interval pattern, {0} - {1}, which is not skeleton dependent.
    // {0} - {1}, where {0} and {1} are SHORT date format
    UnicodeString fDateFallbackIntervalPattern;
    // {0} - {1}, where {0} and {1} are SHORT time format
    UnicodeString fTimeFallbackIntervalPattern;
    // {0} - {1}, where {0} and {1} are SHORT date/time format
    UnicodeString fDateTimeFallbackIntervalPattern;
    
    // single date pattern
    //UnicodeString fSingleDatePattern;   
    
    /**
     * Whether the first date in interval pattern is later date or not.
     * Fallback format set the default ordering.
     * And for a particular interval pattern, the order can be 
     * overriden by prefixing the interval pattern with "latestFirst:" or 
     * "earliestFirst:"
     * For example, given 2 date, Jan 10, 2007 to Feb 10, 2007.
     * if the fallback format is "{0} - {1}", 
     * and the pattern is "d MMM - d MMM yyyy", the interval format is
     * "10 Jan - 10 Feb, 2007".
     * If the pattern is "latestFirst:d MMM - d MMM yyyy", the interval format
     * is "10 Feb - 10 Jan, 2007"
     */
    UBool  fFirstDateInPtnIsLaterDate;

};// end class DateIntervalInfo


inline UBool 
DateIntervalInfo::fieldExistsInSkeleton(const UCalendarDateFields field,
                                        const UnicodeString& skeleton) {
    const UChar fieldChar = fgCalendarFieldToPatternLetter[field];
    return ( (skeleton.indexOf(fieldChar) == -1)?FALSE:TRUE ) ;
}

inline UBool 
DateIntervalInfo::operator!=(const DateIntervalInfo& other) const { 
    return !operator==(other); 
}


inline UBool
DateIntervalInfo::firstDateInPtnIsLaterDate() const {
    return fFirstDateInPtnIsLaterDate;
}


inline const UnicodeString*
DateIntervalInfo::getFallbackIntervalPattern() const {
    return (&fFallbackIntervalPattern);
}


U_NAMESPACE_END

#endif

#endif
