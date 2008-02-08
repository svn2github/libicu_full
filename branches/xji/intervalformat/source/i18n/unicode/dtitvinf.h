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
 * <code>DateIntervalInfo</code> is a public class for encapsulating localizable
 * date time interval patterns. It is used by <code>DateIntervalFormat</code>.
 *
 * <P>
 * Logically, the interval patterns are mappings 
 * from (skeleton, the_largest_different_calendar_field)  
 * to (date_interval_pattern).
 * <P>
 * A skeleton just includes the pattern letter and lengths,
 * without the punctuations and string literals in a pattern.
 * For example, the skeleton for pattern "d 'on' MMM" is
 * "dMMM".
 *
 * FIXME: more documents on skeleton
 *
 * <P>
 * For example, for a skeleton "dMMMy" in  en_US, if the largest different 
 * calendar field between date1 and date2 is "year", the date interval pattern 
 * is "MMM d, yyyy - MMM d, yyyy", such as "Jan 10, 2007 - Jan 10, 2008".
 * <P>
 * If the largest different calendar field between date1 and date2 is "month", 
 * the date interval pattern is "MMM d - MMM d, yyyy", 
 * such as "Jan 10 - Feb 10, 2007".
 * <P>
 * If the largest different calendar field between date1 and date2 is "day", 
 * the date interval pattern is ""MMM d-d, yyyy", such as "Jan 10-20, 2007".
 * <P>
 * If all the available fields have the exact same value, it generates a single 
 * date string. For example, if the interval skeleton is "dMMMMy" ( with only
 * day, month, and year), the interval pattern from "Jan 10, 2007" to 
 * "Jan 10, 2007" is "Jan 10, 2007".
 * <P>
 * For a skeleton "MMMy", if the largest different calendar field between date1
 * and date2 is "month". the interval pattern is "MMM-MMM, yyyy", 
 * such as "Jan-Feb, 2007".
 *
 * <P>
 * The recommendated way to create a <code>DateIntervalFormat</code> object is 
 * <pre>
 * DateIntervalFormat::getInstance(DateFormat*, Locale&).
 * </pre>
 * By using a Locale parameter, the <code>DateIntervalFormat</code> object is 
 * initialized with the default interval patterns for a given or default locale.
 * <P>
 * If clients decided to create <code>DateIntervalFormat</code> object 
 * by supplying their own interval patterns, they can do so with 
 * <pre>
 * DateIntervalFormat::getInstance(DateFormat*, DateIntervalInfo*).
 * </pre>
 * Here, <code>DateIntervalFormat</code> object is initialized with the interval * patterns client supplied. It provides flexibility for powerful usage.
 *
 * <P>
 * After a <code>DateIntervalInfo</code> object is created, clients may modify
 * the interval patterns using setIntervalPatterns function as so desired.
 * Currently, the mininum supported calendar field on which the client can
 * set interval pattern is UCAL_MINUTE.
 * <P>
 * <code>DateIntervalInfo</code> objects are clonable. 
 * When clients obtain a <code>DateIntervalInfo</code> object, 
 * they can feel free to modify it as necessary.
 * <P>
 * <code>DateIntervalInfo</code> are not expected to be subclassed. 
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
     * and is recommended to be used only for powerful users.
     * 
     */
    DateIntervalInfo();

    /** 
     * Construct <code>DateIntervalInfo</code> for the default local,
     * Gregorian calendar, and date/time skeleton "dMyhm".
     * @param status  output param set to success/failure code on exit, which
     * @draft ICU 4.0
     */
    DateIntervalInfo(UErrorCode& status);

    /** 
     * Construct <code>DateIntervalInfo</code> for the given local,
     * Gregorian calendar, and date/time skeleton "dMyhm".
     * @param locale  the interval patterns are loaded from the Gregorian 
     *                calendar data in this locale.
     * @param status  output param set to success/failure code on exit, which
     * @draft ICU 4.0
     */
    DateIntervalInfo(const Locale& locale, UErrorCode& status);


    /** 
     * Construct <code>DateIntervalInfo</code> for the given local, 
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
     * User could construct <code>DateIntervalInfo</code> by providing 
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
     * Restriction: the minimum calendar field to set interval pattern is MINUTE
     *
     * @param lrgDiffCalUnit   the largest different calendar unit.
     * @param intervalPattern  the interval pattern on the largest different
     *                         calendar unit.
     *                         For example, if <code>lrgDiffCalUnit</code> is 
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
     * non-default constructor to lset constructor get those patterns from 
     * locale ), if they do not set the fall-back interval pattern, 
     * it will be fall-back to '{date0} - {date1}'.
     *
     * @param fallbackPattern     fall-back interval pattern.
     * @draft ICU 4.0 
     */
    void setFallbackIntervalPattern(const UnicodeString& fallbackPattern); 

    /**
     * Get the smallest calendar field the interval patterns based on.
     * For example, for skeleton dMMMy, there are interval patterns when
     * year, month, and date are different. The smallest calendar field
     * is "date". So, if the hour is different between 2 dates,
     * the interval pattern will be the same as a single date format.
     * @return the smallest calendar field the interval patterns based on.
     * @draft ICU 4.0 
     */
    UCalendarDateFields getSmallestCalField() const;

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


    UBool firstDateInPtnIsLaterDate() const;
    
private:

    
    /** 
     * init the DateIntervalInfo from locale and skeleton.
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

    /* save the smallest calendar field among which a set of interval patterns
     * are defined.
     * This is used to return a single date format when the 
     * largest different fields between 2 date is smaller than the
     * smallest calendar field that a set of interval patterns are defined.
     *
     * For example, for skeleton, dMMMy, the interval patterns defined in 
     * resource bundle are those when year, month, day is different.
     * So, "day" is the smallest calendar field.
      const*
     * If the largest different calendar fields 
     * between 2 dates is hour, then, a single date format is returned.
     */
    UCalendarDateFields  fSmallestCalField;


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
     * By default, the first part of the pattern in the interval pattern is 
     * earlier date, for example, "d MMM" in "d MMM - d MMM yyy".
     * If an interval pattern is prefixed with "later_first:", for example
     * "later_first:d MMM - d MMM yyyy", it means the first part of the pattern
     * is later date. 
     * For example, given 2 date, Jan 10, 2007 to Feb 10, 2007.
     * If the pattern is "d MMM - d MMM yyyy", the interval format is
     * "10 Jan - 10 Feb, 2007".
     * If the pattern is "later_first:d MMM - d MMM yyyy", the interval format
     * is "10 Feb - 10 Jan, 2007"
     */
    UBool  fFirstDateInPtnIsLaterDate;

};// end class DateIntervalInfo


inline UCalendarDateFields
DateIntervalInfo::getSmallestCalField() const {
    return fSmallestCalField;
}



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
