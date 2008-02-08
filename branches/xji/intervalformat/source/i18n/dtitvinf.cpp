/*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTITVINF.CPP 
*
*******************************************************************************
*/


//FIXME: how to define it in compiler time
//#define DTITVINF_DEBUG 1


#ifdef DTITVINF_DEBUG 
#include <iostream>
#endif

#include "cstring.h"
#include "unicode/msgfmt.h"
#include "unicode/dtitvinf.h"
#include "dtitv_impl.h"

#if !UCONFIG_NO_FORMATTING


U_NAMESPACE_BEGIN


#ifdef DTITVINF_DEBUG 
#define PRINTMESG(msg) { std::cout << "(" << __FILE__ << ":" << __LINE__ << ") " << msg << "\n"; }
#endif

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateIntervalInfo)

// dMyhm
static const UChar gShortFormSkeleton[]={LOW_D, CAP_M, LOW_Y, LOW_H, LOW_M, 0};
static const char gIntervalDateTimePatternTag[]="IntervalDateTimePatterns";
static const char gDateTimePatternsTag[]="DateTimePatterns";
static const char gFallbackPatternTag[]="Fallback";
// later_first:
static const UChar gLaterFirstPrefix[] = {LOW_L, LOW_A, LOW_T, LOW_E, LOW_R, LOW_LINE, LOW_F, LOW_I, LOW_R, LOW_S, LOW_T, COLON, 0};
// {0}
static const UChar gFirstPattern[] = {LEFT_CURLY_BRACKET, DIGIT_ZERO, RIGHT_CURLY_BRACKET, 0};
// {1}
static const UChar gSecondPattern[] = {LEFT_CURLY_BRACKET, DIGIT_ONE, RIGHT_CURLY_BRACKET, 0};


DateIntervalInfo::DateIntervalInfo() {
    fSmallestCalField = UCAL_ERA;
    fFirstDateInPtnIsLaterDate = FALSE;
}


DateIntervalInfo::DateIntervalInfo(UErrorCode& status) {
    init(Locale::getDefault(), UnicodeString(gShortFormSkeleton), status);
}

DateIntervalInfo::DateIntervalInfo(const Locale& locale, UErrorCode& status) {
    init(locale, UnicodeString(gShortFormSkeleton), status);
}


DateIntervalInfo::DateIntervalInfo(const Locale& locale, const UnicodeString& skeleton, UErrorCode& status) {
    init(locale, skeleton, status);
}


void
DateIntervalInfo::setIntervalPattern(UCalendarDateFields lrgDiffCalUnit, const UnicodeString& intervalPattern, UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    if ( lrgDiffCalUnit > MINIMUM_SUPPORTED_CALENDAR_FIELD ) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    // check for "later_first:" prefix
    uint32_t prefixLength = sizeof(gLaterFirstPrefix)/sizeof(gLaterFirstPrefix[0]);

    const UnicodeString* finalPattern;
    if ( intervalPattern.compare(gLaterFirstPrefix, prefixLength) == 0 ) {
        fFirstDateInPtnIsLaterDate = TRUE;
        UnicodeString realPattern;
        intervalPattern.extract(prefixLength, 
                                intervalPattern.length() - prefixLength, 
                                realPattern);
        finalPattern = &realPattern;
    } else {
        finalPattern = &intervalPattern;
    }
    fIntervalPatterns[lrgDiffCalUnit] = *finalPattern;
    if ( lrgDiffCalUnit == UCAL_HOUR_OF_DAY ) {
        fIntervalPatterns[UCAL_AM_PM] = *finalPattern;
        fIntervalPatterns[UCAL_HOUR] = *finalPattern;
    }
}


void
DateIntervalInfo::setFallbackIntervalPattern(const UnicodeString& fallbackPattern) {
    // check for "later_first:" prefix
    uint32_t prefixLength = sizeof(gLaterFirstPrefix)/sizeof(gLaterFirstPrefix[0]);

    if ( fallbackPattern.compare(gLaterFirstPrefix, prefixLength) == 0 ) {
        fFirstDateInPtnIsLaterDate = TRUE;
        UnicodeString realPattern;
        fallbackPattern.extract(prefixLength, 
                                fallbackPattern.length() - prefixLength, 
                                realPattern);
        fFallbackIntervalPattern = realPattern;
    } else {
        fFallbackIntervalPattern = fallbackPattern;
    }
}



/**
 * init the DateIntervalInfo from locale and skeleton.
 * @param locale   the given locale.
 * @param skeleton the given skeleton on which the interval patterns based.
 * @param status   Output param filled with success/failure status.
 *
 * It mainly init the following fields: fIntervalPattern[];
 * 
 * This code is a bit complicated since 
 * 1. the interval patterns saved in resource bundle files are interval
 *    patterns based on date or time only.
 *    It does not have interval patterns based on both date and time.
 *    Interval patterns on both date and time are algorithm generated.
 *
 *    For example, it has interval patterns on skeleton "dMy" and "hm",
 *    but it does not have interval patterns on skeleton "dMyhm".
 *    
 *    The rule to genearte interval patterns for both date and time skeleton are
 *    1) when the year, month, or day differs, concatenate the two original 
 *    expressions with a separator between, 
 *    For example, interval pattern from "Jan 10, 2007 10:10 am" 
 *    to "Jan 11, 2007 10:10am" is 
 *    "Jan 10, 2007 10:10 am - Jan 11, 2007 10:10am" 
 *
 *    2) otherwise, present the date followed by the range expression 
 *    for the time.
 *    For example, interval pattern from "Jan 10, 2007 10:10 am" 
 *    to "Jan 10, 2007 11:10am" is 
 *    "Jan 10, 2007 10:10 am - 11:10am" 
 *
 * 2. even a pattern does not request a certion calendar field,
 *    the interval pattern needs to include such field if such fields are
 *    different between 2 dates.
 *    For example, a pattern/skeleton is "hm", but the interval pattern 
 *    includes year, month, and date when year, month, and date differs.
 *
 * 3. the minimum supported calendar field in interval pattern is MINUTE.
 *    So, if the largest different calendar field is less than MINUTE,
 *    the interval pattern is just a single date pattern.
 *    fSmallestCalField is used for this purpose.
 *
 *
 * The process is:
 * 1. calculate fFallbackIntervalPattern, which is defined in resource file and
 *    mostly {0} - {1}. The single date/time pattern is the pattern based on 
 *    skeleton. Or could use the  fPattern in SimpleDateFormat.
 *    For example, for en_US, skeleton "dMMM", single date pattern 
 *    is "MMM d", fall back interval pattern will be "MMM d - MMM d".
 *
 * 2. calculate other 3 fallback patterns. 
 *    fDateFallbackIntervalPattern - use SHORT date format as single date format
 *    fTimeFallbackIntervalPattern - use SHORT time format as single time format
 *    fDateTimeFallbackIntervalPattern - use SHORT date and time format as
 *                                       single date format
 *
 * 3. set fSmallestCalField = MINUTE and
 *    fill Interval Patterns for year/month/day/am_pm/hour/minute differs.
 *
 *    check whether skeleton found
 * 3.1 No
 *     if the largest different calendar field on which the interval pattern
 *     based on exists in the skeleton, fallback to fFallbackIntervalPattern;
 *     elsefallback to 
 *         fDateFallbackIntervalPattern or
 *         fTimeFallbackIntervalPattern or
 *         fallback to fDateTimeFallbackIntervalPattern;
 *
 * 3.2 Yes
 *     if skeleton only contains date pattern,
 *     fill interval patterns for year/month/day differs from resource file,
 *     set fSmallestCalField as DATE
 *
 *     else if skeleton only contains time pattern,
 *     fill interval patterns for am_pm/hour/minute differs from resource file,
 *     fill interval patterns for year/month/day differs as 
 *     fDateTimeFallbackIntervalPattern
 *
 *     else ( skeleton contains both date and time )
 *     fill interval patterns for year/month/day differs as
 *         fFallbackIntervalPattern if year/month/day exists in skeleton, 
 *         fDateTimeFallbackIntervalPattern if not exists
 *     fill interval patterns for am_pm/hour/minute differs as
 *         concatnation of (single date pattern) and (time interval pattern)
 */
void
DateIntervalInfo::init(const Locale& locale, const UnicodeString& skeleton, UErrorCode& status) {

    if ( U_FAILURE(status) ) {
        return;
    }


    fSkeleton = skeleton;
    fSmallestCalField = UCAL_MINUTE;
    fFirstDateInPtnIsLaterDate = FALSE;

    DateTimePatternGenerator* dtpng = DateTimePatternGenerator::createInstance(locale, status);
    if (U_FAILURE(status)) {
        return;
    }

    /* Check whether the skeleton is a combination of date and time.
     * For the complication reason 1 explained above.
     */

    UnicodeString dateSkeleton;
    UnicodeString timeSkeleton;
    UnicodeString normalizedTimeSkeleton;
    UnicodeString normalizedDateSkeleton;
    /* the difference between time skeleton and normalizedTimeSkeleton are:
     * 1. both 'H' and 'h' are normalized as 'h' in normalized time skeleton,
     * 2. 'a' is omitted in normalized time skeleton.
     * 3. there is only one appearance for 'h', 'm','v', 'z' in normalized time
     *    skeleton
     *
     * The difference between date skeleton and normalizedDateSkeleton are:
     * 1. both 'y' and 'd' are appeared only once in normalizeDateSkeleton
     * 2. 'E' and 'EE' are normalized into 'EEE'
     * 3. 'MM' is normalized into 'M'
     */
    getDateTimeSkeleton(skeleton, dateSkeleton, normalizedDateSkeleton,
                        timeSkeleton, normalizedTimeSkeleton);


#ifdef DTITVINF_DEBUG 
    char skeletonInChar[1000];
    char mesg[2000];
    skeleton.extract(0, skeleton.length(), skeletonInChar);
    sprintf(mesg, "skeleton is: %s", skeletonInChar);
    PRINTMESG(mesg);
    dateSkeleton.extract(0, dateSkeleton.length(), skeletonInChar);
    sprintf(mesg, "date skeleton is: %s", skeletonInChar);
    PRINTMESG(mesg);
    timeSkeleton.extract(0, timeSkeleton.length(), skeletonInChar);
    sprintf(mesg, "time skeleton is: %s", skeletonInChar);
    PRINTMESG(mesg);
    normalizedTimeSkeleton.extract(0, normalizedTimeSkeleton.length(), skeletonInChar);
    sprintf(mesg, "normalizedTime skeleton is: %s", skeletonInChar);
    PRINTMESG(mesg);
#endif


    // key-value pair: largest_different_calendar_unit, interval_pattern
    
    CalendarData* calData = new CalendarData(locale, NULL, status);

    if ( calData == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    const UResourceBundle* dateTimePatternsRes = calData->getByKey(
                                               gDateTimePatternsTag, status);
    int32_t dateTimeFormatLength;
    const UChar* dateTimeFormat = ures_getStringByIndex(dateTimePatternsRes,
                                            (int32_t)DateFormat::kDateTime,
                                            &dateTimeFormatLength, &status);

    int32_t shortDateFormatLength;
    const UChar* shortDateFormat =ures_getStringByIndex(dateTimePatternsRes,
                              DateFormat::kShort + DateFormat::kDateOffset,
                              &shortDateFormatLength, &status);

    int32_t shortTimeFormatLength;
    const UChar* shortTimeFormat =ures_getStringByIndex(dateTimePatternsRes,
                                          DateFormat::kShort,
                                          &shortTimeFormatLength, &status);

    /* getByKey return result owned by calData.
     * It override the dateTimePatternsRes get earlier.
     * That is why we need to save above data for further process.
     */
    const UResourceBundle* itvDtPtnResource = calData->getByKey( 
                                       gIntervalDateTimePatternTag, status);

    genFallbackPattern(dtpng, itvDtPtnResource, skeleton, 
                  shortDateFormat, shortDateFormatLength, 
                  shortTimeFormat, shortTimeFormatLength,
                  dateTimeFormat, dateTimeFormatLength, status);

    if ( timeSkeleton.isEmpty() ) {
        fSmallestCalField = UCAL_DATE;
    }

    /*
     * fill fIntervalPattern from resource file.
     * fill interval pattern for year/month/day differs for date only skeleton.
     * fill interval pattern for ampm/hour/minute differ for time only and 
     * date/time skeleton.  
     */
    initIntvPtnFromRes(itvDtPtnResource, normalizedDateSkeleton, normalizedTimeSkeleton, status);

    if ( U_FAILURE(status) ) {
        // skeleton not found
        const UnicodeString* shortFormatFallback;
        // set interval patterns for year, month, date.
        if ( timeSkeleton.isEmpty() ) {
            shortFormatFallback = &fDateFallbackIntervalPattern;
        } else {
            shortFormatFallback = &fDateTimeFallbackIntervalPattern;
        }

        if ( fieldExistsInSkeleton(UCAL_YEAR, skeleton) ) {
            fIntervalPatterns[UCAL_YEAR] = fFallbackIntervalPattern;
        } else {
            fIntervalPatterns[UCAL_YEAR] = (*shortFormatFallback);
        }
        if ( fieldExistsInSkeleton(UCAL_MONTH, skeleton) ) {
            fIntervalPatterns[UCAL_MONTH] = fFallbackIntervalPattern;
        } else {
            fIntervalPatterns[UCAL_MONTH] = (*shortFormatFallback);
        }
        if ( fieldExistsInSkeleton(UCAL_DATE, skeleton) ) {
            fIntervalPatterns[UCAL_DATE] = fFallbackIntervalPattern;
        } else {
            fIntervalPatterns[UCAL_DATE] = (*shortFormatFallback);
        }

        if ( !timeSkeleton.isEmpty() ) {
            // set interval pattern for am_pm/hour/minute.
            if ( dateSkeleton.isEmpty() ) {
                shortFormatFallback = &fTimeFallbackIntervalPattern;
            } else {
                shortFormatFallback = &fDateTimeFallbackIntervalPattern;
            }

            if ( fieldExistsInSkeleton(UCAL_AM_PM, timeSkeleton) ||
                 fieldExistsInSkeleton(UCAL_HOUR_OF_DAY, timeSkeleton) ) {
                fIntervalPatterns[UCAL_AM_PM] = fFallbackIntervalPattern;
            } else {
                fIntervalPatterns[UCAL_AM_PM] = *shortFormatFallback;
            }
            if ( fieldExistsInSkeleton(UCAL_HOUR, timeSkeleton) ||
                 fieldExistsInSkeleton(UCAL_HOUR_OF_DAY, timeSkeleton) ) {
                fIntervalPatterns[UCAL_HOUR] = fFallbackIntervalPattern;
            } else {
                fIntervalPatterns[UCAL_HOUR] = *shortFormatFallback;
            }
            if ( fieldExistsInSkeleton(UCAL_MINUTE, timeSkeleton) ) {
                fIntervalPatterns[UCAL_MINUTE] = fFallbackIntervalPattern;
            } else {
                fIntervalPatterns[UCAL_MINUTE] = *shortFormatFallback;
            }
        }
        status = U_ZERO_ERROR;
        return; // end of skeleton not found
    } 

    // skeleton found
    if ( timeSkeleton.isEmpty() ) {
    } else if ( dateSkeleton.isEmpty() ) {
        // fill interval patterns for year/month/day differ 
        fIntervalPatterns[UCAL_DATE] = fDateTimeFallbackIntervalPattern;
        fIntervalPatterns[UCAL_MONTH] = fDateTimeFallbackIntervalPattern;
        fIntervalPatterns[UCAL_YEAR] = fDateTimeFallbackIntervalPattern;
    } else {
        /* if both present,
         * 1) when the year, month, or day differs, 
         * concatenate the two original expressions with a separator between, 
         * 2) otherwise, present the date followed by the 
         * range expression for the time. 
         */
        /*
         * 1) when the year, month, or day differs, 
         * concatenate the two original expressions with a separator between, 
         */
        if ( fieldExistsInSkeleton(UCAL_YEAR, dateSkeleton) ) {
            fIntervalPatterns[UCAL_YEAR] = fFallbackIntervalPattern;
        } else {
            fIntervalPatterns[UCAL_YEAR] = fDateTimeFallbackIntervalPattern;
        }
        if ( fieldExistsInSkeleton(UCAL_MONTH, dateSkeleton) ) {
            fIntervalPatterns[UCAL_MONTH] = fFallbackIntervalPattern;
        } else {
            fIntervalPatterns[UCAL_MONTH] = fDateTimeFallbackIntervalPattern;
        }
        if ( fieldExistsInSkeleton(UCAL_DATE, dateSkeleton) ) {
            fIntervalPatterns[UCAL_DATE] = fFallbackIntervalPattern;
        } else {
            fIntervalPatterns[UCAL_DATE] = fDateTimeFallbackIntervalPattern;
        }
        
        /*
         * 2) otherwise, present the date followed by the 
         * range expression for the time. 
         */
        UnicodeString datePattern =dtpng->getBestPattern(dateSkeleton,
                                                         status);
        concatSingleDate2TimeInterval(dateTimeFormat, dateTimeFormatLength,
                                      datePattern, UCAL_AM_PM, status);
        concatSingleDate2TimeInterval(dateTimeFormat, dateTimeFormatLength,
                                      datePattern, UCAL_HOUR, status);
        concatSingleDate2TimeInterval(dateTimeFormat, dateTimeFormatLength,
                                      datePattern, UCAL_MINUTE, status);
    }
    delete dtpng;
    delete calData;
}


void
DateIntervalInfo::initIntvPtnFromRes(const UResourceBundle* intervalDateTimePtn,
                                     const UnicodeString& dateSkeleton,
                                     const UnicodeString& timeSkeleton,
                                     UErrorCode& status) {

    if ( U_FAILURE(status) ) {
        return;
    }
    // has to use UnicodeString instead of char*, even it is slow
    const UnicodeString* skeleton;
    if ( !timeSkeleton.isEmpty() ) {
        skeleton = &timeSkeleton;
    } else {
        skeleton = &dateSkeleton;
    }

    char* skeletonString = new char(skeleton->length()+1);
    if ( skeletonString == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    skeleton->extract(0, skeleton->length(), skeletonString);
    UResourceBundle* intervalPatterns = ures_getByKey(
                                                  intervalDateTimePtn,
                                                  (const char*)skeletonString,
                                                  NULL,
                                                  &status);
    delete skeletonString;

    if ( U_FAILURE(status) ) {
        return;
    }

    // return if interval patterns for skeleton not found
    if ( intervalPatterns == NULL ) {
        status = U_MISSING_RESOURCE_ERROR;
        return;
    }

    //UResourceBundle keyValuePair;
    int32_t size = ures_getSize(intervalPatterns);
    int32_t index;
    const UChar* pattern;
    const char* key;
    int32_t ptLength;
    UBool laterFirst = FALSE; // assume it applies to all patterns
    for ( index = 0; index < size; ++index ) {
        pattern = ures_getNextString(intervalPatterns, &ptLength, &key, &status);
        // FIXME: use pre-defined string literal
        UCalendarDateFields calendarField = UCAL_FIELD_COUNT;
        if ( !uprv_strcmp(key, "year") ) {
            calendarField = UCAL_YEAR;    
        } else if ( !uprv_strcmp(key, "month") ) {
            calendarField = UCAL_MONTH;
        } else if ( !uprv_strcmp(key, "day") ) {
            calendarField = UCAL_DATE;
        } else if ( !uprv_strcmp(key, "am-pm") ) {
            calendarField = UCAL_AM_PM;    
        } else if ( !uprv_strcmp(key, "hour") ) {
            calendarField = UCAL_HOUR;    
        } else if ( !uprv_strcmp(key, "minute") ) {
            calendarField = UCAL_MINUTE;    
        }
     
        /* check prefix "later_first:" in pattern.
         * assume it applies to all patterns, so only check it once.
         */
        if ( index == 0 ) {
            //checkPatternPrefix(pattern, ptLength);
            int32_t prefixIndex = 0;
            int32_t prefixIndexLength = sizeof(gLaterFirstPrefix)/sizeof(gLaterFirstPrefix[0]);
            if ( prefixIndexLength < ptLength ) {
                for (; prefixIndex < prefixIndexLength; ++prefixIndex ) {
                    if (pattern[prefixIndex] != gLaterFirstPrefix[prefixIndex]){
                        break;
                    }    
                }
                if ( prefixIndex >= prefixIndexLength ) {
                    fFirstDateInPtnIsLaterDate = TRUE;
                    pattern += prefixIndexLength;
                }
            }
        }
      
        if ( calendarField != UCAL_FIELD_COUNT ) {
            fIntervalPatterns[calendarField] = pattern;
        }
    }
}



void
DateIntervalInfo::concatSingleDate2TimeInterval(
                                 const UChar* dateTimeFormat,
                                 int32_t dateTimeFormatLength,
                                 const UnicodeString& datePattern, 
                                 UCalendarDateFields field,
                                 UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    UnicodeString combinedPattern;

    UnicodeString  timeIntervalPattern = fIntervalPatterns[field];
    // timeStr and dateStr not owned by this function, do not delete 
    UnicodeString* timeStr = new UnicodeString(timeIntervalPattern);
    UnicodeString* dateStr = new UnicodeString(datePattern);

    concatDateTimePattern(dateTimeFormat, dateTimeFormatLength, 
                          timeStr, dateStr, combinedPattern, status);

    if ( U_FAILURE(status) ) {
        return;
    }

    fIntervalPatterns[field] = combinedPattern;

}


void
DateIntervalInfo::concatDateTimePattern(
                                 const UChar* dateTimeFormat,
                                 int32_t dateTimeFormatLength,
                                 UnicodeString* timeStr,
                                 UnicodeString* dateStr,
                                 UnicodeString& combinedPattern,
                                 UErrorCode& status) {
    
    if ( timeStr == NULL || dateStr == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        delete timeStr;
        delete dateStr;
        return;
    }

    Formattable timeDateArray[2];
    timeDateArray[0].adoptString(timeStr);
    timeDateArray[1].adoptString(dateStr);
    
    MessageFormat::format(
                UnicodeString(TRUE, dateTimeFormat, dateTimeFormatLength), 
                timeDateArray, 2, combinedPattern, status);

}



/* dtpng should be a constant, but DateTimePatternGenerator::getBestPattern()
 * is not a const function, so, can not declare dtpng as constant 
 */
void
DateIntervalInfo::genFallbackPattern(DateTimePatternGenerator* dtpng,
                                     const UResourceBundle* itvDtPtnResource,
                                     const UnicodeString& skeleton,
                                     const UChar* date,
                                     int32_t dateResStrLen,
                                     const UChar* time,
                                     int32_t timeResStrLen,
                                     const UChar* dateTimeFormat,
                                     int32_t dateTimeFormatLength,
                                     UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    // generate fFallbackIntervalPattern based on skeleton
    UnicodeString pattern = dtpng->getBestPattern(skeleton, status);
    genFallbackFromPattern(pattern, itvDtPtnResource,fFallbackIntervalPattern, status);

    /* generate the other 3 fallback patterns use SHORT format
     *    fDateFallbackIntervalPattern - 
     *                use SHORT date format as single date format
     *    fTimeFallbackIntervalPattern - 
     *                use SHORT time format as single time format
     *    fDateTimeFallbackIntervalPattern - 
     *                use SHORT date and time format as
     */

    genFallbackFromPattern(date, dateResStrLen, itvDtPtnResource, fDateFallbackIntervalPattern, status);
    genFallbackFromPattern(time, timeResStrLen, itvDtPtnResource, fTimeFallbackIntervalPattern, status);
                           
    UnicodeString combinedPattern;
    // timeStr and dateStr not owned by this function, do not delete 
    UnicodeString* timeStr = new UnicodeString(TRUE, time, timeResStrLen);
    UnicodeString* dateStr = new UnicodeString(TRUE, date, dateResStrLen);
    
    if ( U_FAILURE(status) ) {
        return;
    }
    concatDateTimePattern(dateTimeFormat, dateTimeFormatLength, 
                          timeStr, dateStr, combinedPattern, status);
    genFallbackFromPattern(combinedPattern, itvDtPtnResource, fDateTimeFallbackIntervalPattern, status);
}

   
void
DateIntervalInfo::genFallbackFromPattern(const UnicodeString& pattern,
                            const UResourceBundle* itvDtPtnResource,
                            UnicodeString& intervalPattern,
                            UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    // date0 and date1 not owned by this function, do not delete 
    UnicodeString* date0 = new UnicodeString(pattern);
    UnicodeString* date1 = new UnicodeString(pattern);
    genFallbackFromPattern(date0, date1, itvDtPtnResource, intervalPattern, status);
}



void
DateIntervalInfo::genFallbackFromPattern(const UChar* string,
                                        int32_t strLength,
                                        const UResourceBundle* itvDtPtnResource,
                                        UnicodeString& intervalPattern,
                                        UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    // date0 and date1 not owned by this function, do not delete 
    UnicodeString* date0 = new UnicodeString(TRUE, string, strLength);
    UnicodeString* date1 = new UnicodeString(TRUE, string, strLength);
    genFallbackFromPattern(date0, date1, itvDtPtnResource, intervalPattern, status);
}


void
DateIntervalInfo::genFallbackFromPattern(UnicodeString* date0,
                                        UnicodeString* date1,
                                        const UResourceBundle* itvDtPtnResource,
                                        UnicodeString& intervalPattern,
                                        UErrorCode& status) {
    if ( date0 == NULL || date1 == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        delete date0;
        delete date1;
        return;
    }

    Formattable timeDateArray[2];
    timeDateArray[0].adoptString(date0);
    timeDateArray[1].adoptString(date1);
    const UChar* resStr;
    int32_t resStrLen = 0;
    UResourceBundle* fbStr = ures_getByKey(itvDtPtnResource, gFallbackPatternTag, NULL,&status);

    if ( U_FAILURE(status) ) {
        return;
    }
    resStr = ures_getStringByIndex(fbStr, 0, &resStrLen, &status);
    if ( U_FAILURE(status) ) {
        return;
    }

    // check whether it is later_first
    UnicodeString  pattern = UnicodeString(TRUE, resStr, resStrLen);
    int32_t firstPatternIndex = pattern.indexOf(gFirstPattern, 
                        sizeof(gFirstPattern)/sizeof(gFirstPattern[0]), 0);
    int32_t secondPatternIndex = pattern.indexOf(gSecondPattern, 
                        sizeof(gSecondPattern)/sizeof(gSecondPattern[0]), 0);
    if ( firstPatternIndex < secondPatternIndex ) { 
        fFirstDateInPtnIsLaterDate = TRUE;
    }
    MessageFormat::format(pattern, timeDateArray, 2, intervalPattern, status);
}


void
DateIntervalInfo::getDateTimeSkeleton(const UnicodeString& skeleton, 
                                      UnicodeString& dateSkeleton, 
                                      UnicodeString& normalizedDateSkeleton, 
                                      UnicodeString& timeSkeleton,
                                      UnicodeString& normalizedTimeSkeleton) {
    // dateSkeleton follows the sequence of E*d*M*y*
    // timeSkeleton follows the sequence of hm*[v|z]?
    int32_t lastIndex;
    int32_t i;
    int32_t ECount = 0;
    int32_t dCount = 0;
    int32_t MCount = 0;
    int32_t yCount = 0;
    int32_t hCount = 0;
    int32_t mCount = 0;
    int32_t vCount = 0;
    int32_t zCount = 0;

    for (i = 0; i < skeleton.length(); ++i) {
        UChar ch = skeleton[i];
        switch ( ch ) {
          case CAP_E:
            dateSkeleton.append(ch);
            ++ECount;
            break;
          case LOW_D:
            dateSkeleton.append(ch);
            ++dCount;
            break;
          case CAP_M:
            dateSkeleton.append(ch);
            ++MCount;
            break;
          case LOW_Y:
            dateSkeleton.append(ch);
            ++yCount;
            break;
          case CAP_G:
          case CAP_Y:
          case LOW_U:
          case CAP_Q:
          case LOW_Q:
          case CAP_L:
          case LOW_L:
          case CAP_W:
          case LOW_W:
          case CAP_D:
          case CAP_F:
          case LOW_G:
          case LOW_E:
          case LOW_C:
            normalizedDateSkeleton.append(ch);
            dateSkeleton.append(ch);
            break;
          case LOW_A:
            // 'a' is implicitly handled 
            timeSkeleton.append(ch);
            break;
          case LOW_H:
          case CAP_H:
            timeSkeleton.append(ch);
            ++hCount;
            break;
          case LOW_M:
            timeSkeleton.append(ch);
            ++mCount;
            break;
          case LOW_Z:
            ++zCount;
            timeSkeleton.append(ch);
            break;
          case LOW_V:
            ++vCount;
            timeSkeleton.append(ch);
            break;
          // FIXME: what is the difference between CAP_V/Z and LOW_V/Z
          case CAP_V:
          case CAP_Z:
          case LOW_K:
          case CAP_K:
          case LOW_J:
          case LOW_S:
          case CAP_S:
          case CAP_A:
            timeSkeleton.append(ch);
            normalizedTimeSkeleton.append(ch);
            break;     
        }
    }

    /* generate normalized form for date*/
    if ( ECount != 0 ) {
        if ( ECount <= 3 ) {
            normalizedDateSkeleton.append(CAP_E);
            normalizedDateSkeleton.append(CAP_E);
            normalizedDateSkeleton.append(CAP_E);
        } else {
            for ( int32_t i = 0; i < ECount && i < MAX_E_COUNT; ++i ) {
                 normalizedDateSkeleton.append(CAP_E);
            }
        }
    }
    if ( dCount != 0 ) {
        normalizedDateSkeleton.append(LOW_D);
    }
    if ( MCount != 0 ) {
        if ( MCount < 3 ) {
            normalizedDateSkeleton.append(CAP_M);
        } else {
            for ( int32_t i = 0; i < MCount && i < MAX_M_COUNT; ++i ) {
                 normalizedDateSkeleton.append(CAP_M);
            }
        }
    }
    if ( yCount != 0 ) {
        normalizedDateSkeleton.append(LOW_Y);
    }

    /* generate normalized form for time */
    if ( hCount != 0 ) {
        normalizedTimeSkeleton.append(LOW_H);
    }
    if ( mCount != 0 ) {
        normalizedTimeSkeleton.append(LOW_M);
    }
    if ( zCount != 0 ) {
        normalizedTimeSkeleton.append(LOW_Z);
    }
    if ( vCount != 0 ) {
        normalizedTimeSkeleton.append(LOW_V);
    }
}



DateIntervalInfo::DateIntervalInfo(const DateIntervalInfo& dtitvinf) {
    *this = dtitvinf;
}
    


DateIntervalInfo&
DateIntervalInfo::operator=(const DateIntervalInfo& dtitvinf) {
    if ( this == &dtitvinf ) {
        return *this;
    }
    
    UCalendarDateFields i;
    for ( i = UCAL_ERA; i <= MINIMUM_SUPPORTED_CALENDAR_FIELD; ) {
        fIntervalPatterns[i] = dtitvinf.fIntervalPatterns[i];
        i = (UCalendarDateFields)((uint32_t)i + 1);
    }
    fSkeleton = dtitvinf.fSkeleton;
    fSmallestCalField = dtitvinf.fSmallestCalField;
    fFallbackIntervalPattern = dtitvinf.fFallbackIntervalPattern;
    fDateFallbackIntervalPattern = dtitvinf.fDateFallbackIntervalPattern;
    fTimeFallbackIntervalPattern = dtitvinf.fTimeFallbackIntervalPattern;
    fDateTimeFallbackIntervalPattern = dtitvinf.fDateTimeFallbackIntervalPattern;
    fFirstDateInPtnIsLaterDate = dtitvinf.fFirstDateInPtnIsLaterDate;
}


DateIntervalInfo*
DateIntervalInfo::clone() const {
    return new DateIntervalInfo(*this);
}


DateIntervalInfo::~DateIntervalInfo() {
}


UBool
DateIntervalInfo::operator==(const DateIntervalInfo& other) const {
    UBool equal = ( fSkeleton == other.fSkeleton &&
      fSmallestCalField == other.fSmallestCalField &&
      fFallbackIntervalPattern == other.fFallbackIntervalPattern &&
      fDateFallbackIntervalPattern==other.fDateFallbackIntervalPattern &&
      fTimeFallbackIntervalPattern==other.fTimeFallbackIntervalPattern &&
      fFirstDateInPtnIsLaterDate == other.fFirstDateInPtnIsLaterDate &&
      fDateTimeFallbackIntervalPattern==other.fDateTimeFallbackIntervalPattern);

   if ( equal == TRUE ) {
        UCalendarDateFields i;
        for ( i = UCAL_ERA; i <= MINIMUM_SUPPORTED_CALENDAR_FIELD && equal; ) {
            equal = equal && 
                    ( fIntervalPatterns[i] == other.fIntervalPatterns[i]);
            i = (UCalendarDateFields)((uint32_t)i + 1);
        }
    }

    return equal;
}


const UnicodeString*
DateIntervalInfo::getIntervalPattern(UCalendarDateFields field,
                                     UErrorCode& status) const {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    if ( field > MINIMUM_SUPPORTED_CALENDAR_FIELD ) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    return (&(fIntervalPatterns[field]));
}


const UChar 
DateIntervalInfo::fgCalendarFieldToPatternLetter[] = 
{
    /*GyM*/ CAP_G, LOW_Y, CAP_M,
    /*wWd*/ LOW_W, CAP_W, LOW_D,
    /*DEF*/ CAP_D, CAP_E, CAP_F,
    /*ahH*/ LOW_A, LOW_H, CAP_H,
    /*m..*/ LOW_M,
};


U_NAMESPACE_END

#endif
