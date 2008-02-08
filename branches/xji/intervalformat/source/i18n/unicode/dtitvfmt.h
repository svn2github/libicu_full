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
 * <code>DateIntervalFormat</code> is a class for formatting and parsing date 
 * intervals in a language-independent manner. 
 * <P>
 * Date interval means from one date to another date,
 * for example, from "Jan 11, 2008" to "Jan 18, 2008".
 * We introduced class <code>DateInterval</code> to represent it.
 * <code>DateInterval</code> is a pair of <code>UDate</code>, which is 
 * the standard milliseconds since 24:00 GMT, Jan 1, 1970.
 * <P>
 * <code>DateIntervalFormat</code> formats a <code>DateInterval</code> into
 * text. And it parses text into <code>DateInterval</code>, 
 * although initially, parsing is not supported. 
 * <P>
 * Formatting a <code>DateInterval</code> is pattern-driven. It is very
 * similar to formatting in <code>DateIntervalFormat</code>.
 * We introduce class <code>DateIntervalInfo</code> to save date interval 
 * patterns, similar to date time pattern in <code>DateIntervalFormat</code>.
 * <P>
 * <code>DateIntervalFormat</code> needs the following information for correct 
 * formatting: time zone, calendar type, pattern, date format symbols, 
 * and DateIntervalInfo. 
 * <P>
 * Clients are encouraged to create a date-time interval formatter using 
 * locale and pre-defined skeleton macros
 * <pre>
 * DateIntervalFormat::createInstance(UnicodeString& skeleton, 
 *                       UBool adjustWidth, const Locale&, UErrorCode& status).
 * </pre>
 * <P>
 * Following are the predefined skeleton macros ( defined in udat.h).
 * <P>
 * If clients decided to create <code>DateIntervalFormat</code> object 
 * by supplying their own interval patterns, they can do so with 
 * <pre>
 * DateIntervalFormat::createInstance(UnicodeString& skeleton,
 *  *        UBool adjustWidth, ,const DateIntervalInfo*, UErrorCode& status).
 * </pre>
 * Here, <code>DateIntervalFormat</code> object is initialized with the interval * patterns client supplied. It provides flexibility for powerful usage.
 *
 * <P>
 * <code>DateIntervalFormat</code> uses the same syntax as that of
 * Date/Time format.
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
 *                           UnicodeString(DAY_MONTH_YEAR_FULL_FORMAT), 
 *                           FALSE, Locale("en", "GB", ""), status);
 *   UnicodeString dateIntervalString;
 *   // formatting
 *   dtIntervalFmt->format(dtInterval, dateIntervalString, status);
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
     * It uses the interval patterns of skeleton ("dMMMyhm") 
     * from the given locale.
     * @param locale    the given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 createInstance(const Locale& locale, 
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
     * Construct a DateIntervalFormat from skeleton and a given locale.
     *
     * Following are the skeletons (defined in udate.h) having predefined 
     * interval pattern in resource files.
     * Users are encouraged to use those macros.
     * For example: 
     * DateIntervalFormat::createInstance(UnicodeString(DAY_MONTH_FULL_FORMAT),
     *                                    FALSE, loc, status) 
     * 
     * #define DAY_MONTH_YEAR_DOW_LONG_FORMAT   "EEEEdMMMMy"
     * #define DAY_MONTH_YEAR_LONG_FORMAT       "dMMMMy"
     * #define DAY_MONTH_LONG_FORMAT            "dMMMM"
     * #define MONTH_YEAR_LONG_FORMAT           "MMMMy"
     * #define DAY_MONTH_DOW_LONG_FORMAT        "EEEEdMMMM"
     * #define DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT "EEEdMMMy"
     * #define DAY_MONTH_YEAR_MEDIUM_FORMAT     "dMMMy"
     * #define DAY_MONTH_MEDIUM_FORMAT          "dMMM"
     * #define MONTH_YEAR_MEDIUM_FORMAT         "MMMy"
     * #define DAY_MONTH_DOW_MEDIUM_FORMAT      "EEEdMMM"
     * #define DAY_MONTH_YEAR_DOW_SHORT_FORMAT  "EEEdMy"
     * #define DAY_MONTH_YEAR_SHORT_FORMAT      "dMy"
     * #define DAY_MONTH_SHORT_FORMAT           "dM"
     * #define MONTH_YEAR_SHORT_FORMAT          "My"
     * #define DAY_MONTH_DOW_SHORT_FORMAT       "EEEdM"
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
     * The given <code>Locale</code> provides the interval patterns.
     * For example, for en_GB, if skeleton is DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT,
     * which is "EEEdMMMyyy",
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
     * and a <code>DateIntervalInfo</code>.
     *
     * Following are the skeletons having predefined interval pattern
     * in resource files. ( they are defined in udat.h )
     * Users are encouraged to use those macros.
     * 
     * #define DAY_MONTH_YEAR_DOW_LONG_FORMAT   "EEEEdMMMMy"
     * #define DAY_MONTH_YEAR_LONG_FORMAT       "dMMMMy"
     * #define DAY_MONTH_LONG_FORMAT            "dMMMM"
     * #define MONTH_YEAR_LONG_FORMAT           "MMMMy"
     * #define DAY_MONTH_DOW_LONG_FORMAT        "EEEEdMMMM"
     * #define DAY_MONTH_YEAR_DOW_MEDIUM_FORMAT "EEEdMMMy"
     * #define DAY_MONTH_YEAR_MEDIUM_FORMAT     "dMMMy"
     * #define DAY_MONTH_MEDIUM_FORMAT          "dMMM"
     * #define MONTH_YEAR_MEDIUM_FORMAT         "MMMy"
     * #define DAY_MONTH_DOW_MEDIUM_FORMAT      "EEEdMMM"
     * #define DAY_MONTH_YEAR_DOW_SHORT_FORMAT  "EEEdMy"
     * #define DAY_MONTH_YEAR_SHORT_FORMAT      "dMy"
     * #define DAY_MONTH_SHORT_FORMAT           "dM"
     * #define MONTH_YEAR_SHORT_FORMAT          "My"
     * #define DAY_MONTH_DOW_SHORT_FORMAT       "EEEdM"
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
     * the <code>DateIntervalInfo</code> provides the interval patterns.
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
     * Note: the <code>DateIntervalFormat</code> takes ownership of 
     * <code>DateIntervalInfo</code> objects. 
     * Caller should not delete them.
     *
     * @param skeleton  the skeleton on which interval format based.
     * @param adjustFieldWidth  whether adjust the skeleton field width or not
     *                          It is used for DateTimePatternGenerator on 
     *                          whether to adjust field width when get 
     *                          full pattern from skeleton
     * @param locale    the given locale.
     * @param dtitvinf  the <code>DateIntervalInfo</code> object to be adopted.
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
     * objects with a <code>DateInterval</code> type. 
     * If a the Formattable object type is not a <code>DateInterval</code>,
     * then it returns a failing UErrorCode.
     *
     * @param obj               The object to format. 
     *                          Must be a <code>DateInterval</code>.
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
     * Format a <code>DateInterval</code> to produce a string. 
     *
     * @param dtInterval        <code>DateInterval</code> to be formatted.
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
     * Format a <code>DateInterval</code> to produce a string. 
     *
     * Note: should "fromCalendar" and "toCalendar"  be  const?
     * but since it calles SimpleDateFormat::format(Calendar&),
     * it can not be defined as a const.
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
     * <code>DateInterval</code> type, which is a pair of UDate.
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
     * Construct a DateIntervalFormat from <code>DateFormat</code>
     * and a <code>DateIntervalInfo</code>.
     * <code>DateFormat</code> provides the timezone, calendar,
     * full pattern, and date format symbols information.
     * It should be a <code>SimpleDateFormat</code> object which 
     * has a pattern in it.
     * the <code>DateIntervalInfo</code> provides the interval patterns.
     *
     * Note: the <code>DateIntervalFormat</code> takes ownership of both 
     * <code>DateFormat</code> and <code>DateIntervalInfo</code> objects. 
     * Caller should not delete them.
     *
     * @param dtfmt     the <code>SimpleDateFormat</code> object to be adopted.
     * @param dtitvinf  the <code>DateIntervalInfo</code> object to be adopted.
     * @param status    Output param set to success/failure code.
     * @draft ICU 4.0
     */
    DateIntervalFormat(DateFormat* dtfmt, 
                       DateIntervalInfo* dtItvInfo, 
                       UErrorCode& status);


    /**
     * Construct a DateIntervalFormat from <code>DateFormat</code>
     * and a given locale.
     * <code>DateFormat</code> provides the timezone, calendar,
     * full pattern, and date format symbols information.
     * It should be a <code>SimpleDateFormat</code> object which 
     * has a pattern in it.
     *
     * Note: the <code>DateIntervalFormat</code> takes ownership of the
     * <code>DateFormat</code> object. Caller should not delete it.
     *
     * The given <code>Locale</code> provides the interval patterns.
     * A skeleton is derived from the full pattern. 
     * And the interval patterns are those the skeleton maps to
     * in the given locale.
     * For example, for en_GB, if the pattern in the <code>SimpleDateFormat</code>
     * is "EEE, d MMM, yyyy", the skeleton is "EEEdMMMyyy".
     * And the interval patterns are:
     * "EEE, d MMM, yyyy - EEE, d MMM, yyyy" for year differs,
     * "EEE, d MMM - EEE, d MMM, yyyy" for month differs,
     * "EEE, d - EEE, d MMM, yyyy" for day differs,
     * @param dtfmt     the <code>DateFormat</code> object to be adopted.
     * @param locale    the given locale.
     * @param status    Output param set to success/failure code.
     * @return          a date time interval formatter whick the caller owns.
     * @draft ICU 4.0
     */
    static DateIntervalFormat* U_EXPORT2 create(DateFormat* dtfmt,
                                                const Locale& locale, 
                                                UErrorCode& status);

    /**
     * Construct a DateIntervalFormat from <code>DateFormat</code>
     * and a <code>DateIntervalInfo</code>.
     * <code>DateFormat</code> provides the timezone, calendar,
     * full pattern, and date format symbols information.
     * It should be a <code>SimpleDateFormat</code> object which 
     * has a pattern in it.
     * the <code>DateIntervalInfo</code> provides the interval patterns.
     *
     * Note: the <code>DateIntervalFormat</code> takes ownership of both 
     * <code>DateFormat</code> and <code>DateIntervalInfo</code> objects. 
     * Caller should not delete them.
     *
     * @param dtfmt     the <code>DateFormat</code> object to be adopted.
     * @param dtitvinf  the <code>DateIntervalInfo</code> object to be adopted.
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
