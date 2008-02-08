/*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTITVFMT.CPP 
*
*******************************************************************************
*/


//FIXME: put in compilation
#define DTITVFMT_DEBUG 1

#include "unicode/dtptngen.h"
#include "unicode/dtitvfmt.h"
#include "unicode/calendar.h"
#include "dtitv_impl.h"

#ifdef DTITVFMT_DEBUG 
#include <iostream>
#include "cstring.h"
#endif


#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN



#ifdef DTITVFMT_DEBUG 
#define PRINTMESG(msg) { std::cout << "(" << __FILE__ << ":" << __LINE__ << ") " << msg << "\n"; }
#endif


//FIXME: a better way to initialize
static const UChar gDateFormatSkeleton[][11] = {
//EEEEdMMMMy
{CAP_E, CAP_E, CAP_E, CAP_E, LOW_D, CAP_M, CAP_M, CAP_M, CAP_M, LOW_Y, 0},
//dMMMMy
{LOW_D, CAP_M, CAP_M, CAP_M, CAP_M, LOW_Y, 0},
//dMMMy
{LOW_D, CAP_M, CAP_M, CAP_M, LOW_Y, 0},
//dMy
{LOW_D, CAP_M, LOW_Y, 0} };


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateIntervalFormat)


DateIntervalFormat* U_EXPORT2
DateIntervalFormat::createInstance(UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createInstance();
    DateIntervalInfo* dtitvinf = new DateIntervalInfo(status);
    return create(dtfmt, dtitvinf, status);
}


DateIntervalFormat* U_EXPORT2
DateIntervalFormat::createInstance(const Locale& locale, UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createDateTimeInstance(DateFormat::kShort, 
                                                   DateFormat::kShort, locale);
    DateIntervalInfo* dtitvinf = new DateIntervalInfo(locale, status);
    return create(dtfmt, dtitvinf, status);
}


DateIntervalFormat*  U_EXPORT2
DateIntervalFormat::createDateIntervalInstance(DateFormat::EStyle style,
                                               const Locale& locale,
                                               UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createDateInstance(style, locale);
    DateIntervalInfo* dtitvinf = new DateIntervalInfo(locale, 
                            UnicodeString(gDateFormatSkeleton[(int32_t)style]),
                            status);
    return create(dtfmt, dtitvinf, status);
}


DateIntervalFormat*  U_EXPORT2
DateIntervalFormat::createTimeIntervalInstance(DateFormat::EStyle style,
                                               const Locale& locale,
                                               UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createTimeInstance(style, locale);
    return create(dtfmt, locale, status);
}


DateIntervalFormat*  U_EXPORT2
DateIntervalFormat::createDateTimeIntervalInstance(DateFormat::EStyle dateStyle,
                                                   DateFormat::EStyle timeStyle,
                                                   const Locale& locale,
                                                   UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createDateTimeInstance(dateStyle, timeStyle, locale);
    return create(dtfmt, locale, status);
}


DateIntervalFormat* U_EXPORT2
DateIntervalFormat::createInstance(const UnicodeString& skeleton, 
                                   UBool adjustFieldWidth,
                                   const Locale& locale, UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createInstance(skeleton, adjustFieldWidth, locale);

#ifdef DTITVFMT_DEBUG
    char result[1000];
    char result_1[1000];
    char mesg[2000];
    skeleton.extract(0,  skeleton.length(), result, "UTF-8");
    const UnicodeString& pat = ((SimpleDateFormat*)dtfmt)->fPattern;
    pat.extract(0,  pat.length(), result_1, "UTF-8");
    sprintf(mesg, "skeleton: %s; pattern: %s\n", result, result_1);
    PRINTMESG(mesg)
#endif

    DateIntervalInfo* dtitvinf = new DateIntervalInfo(locale, skeleton,status);
    return create(dtfmt, dtitvinf, status);
}


DateIntervalFormat* U_EXPORT2
DateIntervalFormat::createInstance(const UnicodeString& skeleton,
                                   UBool adjustFieldWidth,
                                   const Locale& locale,
                                   DateIntervalInfo* dtitvinf,
                                   UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        delete dtitvinf;
        return NULL;
    }
    DateFormat* dtfmt = DateFormat::createInstance(skeleton, adjustFieldWidth, locale);
    return create(dtfmt, dtitvinf, status);
}


DateIntervalFormat::DateIntervalFormat()
:   fDateFormat(0),
    fIntervalPattern(0),
    fFromCalendar(0),
    fToCalendar(0)
{}


DateIntervalFormat::DateIntervalFormat(const DateIntervalFormat& itvfmt)
:   Format(itvfmt),
    fDateFormat(0),
    fIntervalPattern(0),
    fFromCalendar(0),
    fToCalendar(0) {
    *this = itvfmt;
}


DateIntervalFormat&
DateIntervalFormat::operator=(const DateIntervalFormat& itvfmt) {
    if ( this != &itvfmt ) {
        delete fDateFormat;
        delete fIntervalPattern;
        delete fFromCalendar;
        delete fToCalendar;
        if ( itvfmt.fDateFormat ) {
            fDateFormat = (SimpleDateFormat*)itvfmt.fDateFormat->clone();
        } else {
            fDateFormat = NULL;
        }
        if ( itvfmt.fIntervalPattern ) {
            fIntervalPattern = itvfmt.fIntervalPattern->clone();
        } else {
            fIntervalPattern = NULL;
        }
        if ( itvfmt.fFromCalendar ) {
            fFromCalendar = itvfmt.fFromCalendar->clone();
        } else {
            fFromCalendar = NULL;
        }
        if ( itvfmt.fToCalendar ) {
            fToCalendar = itvfmt.fToCalendar->clone();
        } else {
            fToCalendar = NULL;
        }
    }
    return *this;
}


DateIntervalFormat::~DateIntervalFormat() {
    delete fIntervalPattern;
    delete fDateFormat;
    delete fFromCalendar;
    delete fToCalendar;
}


Format*
DateIntervalFormat::clone(void) const {
    return new DateIntervalFormat(*this);
}


UBool
DateIntervalFormat::operator==(const Format& other) const {
    if ( other.getDynamicClassID() == DateIntervalFormat::getStaticClassID() ) {
        DateIntervalFormat* fmt = (DateIntervalFormat*)&other;
#ifdef DTITVFMT_DEBUG
    UBool equal;
    equal = (this == fmt);
    equal = (*fIntervalPattern == *fmt->fIntervalPattern);
    equal = (*fDateFormat == *fmt->fDateFormat);
    equal = fFromCalendar->isEquivalentTo(*fmt->fFromCalendar) ;
    equal = fToCalendar->isEquivalentTo(*fmt->fToCalendar) ;
#endif
        return ( this == fmt ) ||
               ( Format::operator==(other) && 
                 fIntervalPattern && 
                 ( *fIntervalPattern == *fmt->fIntervalPattern ) &&
                 fDateFormat &&
                 ( *fDateFormat == *fmt->fDateFormat ) &&
                 fFromCalendar &&
                 fFromCalendar->isEquivalentTo(*fmt->fFromCalendar) &&
                 fToCalendar &&
                 fToCalendar->isEquivalentTo(*fmt->fToCalendar) );
    } 
    return FALSE;
}




UnicodeString&
DateIntervalFormat::format(const Formattable& obj,
                           UnicodeString& appendTo,
                           FieldPosition& fieldPosition,
                           UErrorCode& status) const {
    if ( U_FAILURE(status) ) {
        return appendTo;
    }

    if ( obj.getType() == Formattable::kObject ) {
        const UObject* formatObj = obj.getObject();
        if (formatObj->getDynamicClassID() == DateInterval::getStaticClassID()){
            return format((DateInterval*)formatObj, appendTo, fieldPosition, status);
        }
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return appendTo;
}


UnicodeString&
DateIntervalFormat::format(const DateInterval* dtInterval,
                           UnicodeString& appendTo,
                           FieldPosition& fieldPosition,
                           UErrorCode& status) const {
    if ( U_FAILURE(status) ) {
        return appendTo;
    }

    if ( fFromCalendar != NULL && fToCalendar != NULL && 
         fDateFormat != NULL && fIntervalPattern != NULL ) {
        fFromCalendar->setTime(dtInterval->getFromDate(), status);
        fToCalendar->setTime(dtInterval->getToDate(), status);
        if ( U_SUCCESS(status) ) {
            return format(*fFromCalendar, *fToCalendar, appendTo,fieldPosition, status);
        }
    }
    return appendTo;
}


UnicodeString&
DateIntervalFormat::format(Calendar& fromCalendar,
                           Calendar& toCalendar,
                           UnicodeString& appendTo,
                           FieldPosition& pos,
                           UErrorCode& status) const {
    if ( U_FAILURE(status) ) {
        return appendTo;
    }

    // not support different calendar types and time zones
    //if ( fromCalendar.getType() != toCalendar.getType() ) {
    if ( !fromCalendar.isEquivalentTo(toCalendar) ) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }

    // First, find the largest different calendar field.
    UCalendarDateFields field = UCAL_FIELD_COUNT;
    const UnicodeString* intervalPattern = NULL;

    // FIXME: CAN NOT iterate through UCalendarDateFields, any better way?
    // FIXME: use for loop? better readability, worse performance
    if ( fromCalendar.get(UCAL_ERA,status) != toCalendar.get(UCAL_ERA,status)) {
        intervalPattern = fIntervalPattern->getFallbackIntervalPattern();
        field = UCAL_ERA;
    } else if ( fromCalendar.get(UCAL_YEAR, status) != 
                toCalendar.get(UCAL_YEAR, status) ) {
        field = UCAL_YEAR;
    } else if ( fromCalendar.get(UCAL_MONTH, status) !=
                toCalendar.get(UCAL_MONTH, status) ) {
        field = UCAL_MONTH;
    } else if ( fromCalendar.get(UCAL_DATE, status) !=
                toCalendar.get(UCAL_DATE, status) ) {
        field = UCAL_DATE;
    } else if ( fromCalendar.get(UCAL_AM_PM, status) !=
                toCalendar.get(UCAL_AM_PM, status) ) {
        field = UCAL_AM_PM;
    } else if ( fromCalendar.get(UCAL_HOUR, status) !=
                toCalendar.get(UCAL_HOUR, status) ) {
        field = UCAL_HOUR;
    } else if ( fromCalendar.get(UCAL_MINUTE, status) !=
                toCalendar.get(UCAL_MINUTE, status) ) {
        field = UCAL_MINUTE;
    }

    if ( U_FAILURE(status) ) {
        return appendTo;
    }
    if ( field == UCAL_FIELD_COUNT ) {
        /* ignore the second/millisecond etc. small fields' difference.
         * use single date when all the above are the same.
         * FIXME: should we? or fall back?
         */
        return fDateFormat->format(fromCalendar, appendTo, pos);
    }
    
    // get interval pattern
    if ( field != UCAL_ERA ) {
        intervalPattern = fIntervalPattern->getIntervalPattern(field, status);
    }

    if ( U_FAILURE(status) ) {
        return appendTo;
    }

    if ( intervalPattern->isEmpty() ) {
        //if ( field > fIntervalPattern->getSmallestCalField() ) {
        if ( fDateFormat->smallerFieldUnit(field) ) {
            /* the largest different calendar field is small than
             * the smallest calendar field in pattern,
             * return single date format.
             */
            return fDateFormat->format(fromCalendar, appendTo, pos);
        }

        /* following only happen if user create a default DateIntervalInfo and 
         * add interval pattern, then pass this DateIntervalInfo to create
         * a DateIntervalFormat.
         * otherwise, the DateIntervalInfo in DateIntervalFormat is created
         * from locale, and the interval pattern should not be empty
         */
        intervalPattern = fIntervalPattern->getFallbackIntervalPattern();
        if ( intervalPattern->isEmpty() ) {
            /* user did not setFallbackIntervalPattern,
             * the final fall back is:
             * {date0} - {date1}
             */
            fDateFormat->format(fromCalendar, appendTo, pos);
            appendTo += SPACE;
            appendTo += EN_DASH;
            appendTo += SPACE;
            fDateFormat->format(toCalendar, appendTo, pos);
            return appendTo;
        }
    }

    // begin formatting
    pos.setBeginIndex(0);
    pos.setEndIndex(0);

    UBool inQuote = FALSE;
    UChar prevCh = 0;
    int32_t count = 0;

    /* repeatedPattern used to record whether a pattern has already seen.
       It is a pattern applies to first calendar if it is first time seen,
       otherwise, it is a pattern applies to the second calendar
     */
    UBool patternRepeated[] = 
    {
    //       A   B   C   D   E   F   G   H   I   J   K   L   M   N   O
        0, 0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 0,  0, 0, 0,
    //   P   Q   R   S   T   U   V   W   X   Y   Z
        0, 0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 0,  0, 0, 0,
    //       a   b   c   d   e   f   g   h   i   j   k   l   m   n   o
        0, 0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 0,  0, 0, 0,
    //   p   q   r   s   t   u   v   w   x   y   z
        0, 0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 0,  0, 0, 0,
    };

    uint32_t patternCharBase = 0x40;
    
    /* loop through the pattern string character by character looking for
     * the first repeated pattern letter, which breaks the interval pattern
     * into 2 parts. 
     */
    int32_t i;
    for (i = 0; i < intervalPattern->length(); ++i){
        UChar ch = (*intervalPattern)[i];
        
        if (ch != prevCh && count > 0) {
            // check the repeativeness of pattern letter
            UBool repeated = patternRepeated[(int)(prevCh - patternCharBase)];
            if ( repeated == FALSE ) {
                patternRepeated[prevCh - patternCharBase] = TRUE;
            } else {
                break;
            }
            count = 0;
        }
        if (ch == QUOTE) {
            // Consecutive single quotes are a single quote literal,
            // either outside of quotes or between quotes
            if ((i+1) < intervalPattern->length() && intervalPattern[i+1] == QUOTE) {
                ++i;
            } else {
                inQuote = ! inQuote;
            }
        } 
        else if ( ! inQuote && ((ch >= 0x0061 /*'a'*/ && ch <= 0x007A /*'z'*/) 
                    || (ch >= 0x0041 /*'A'*/ && ch <= 0x005A /*'Z'*/))) {
            // ch is a date-time pattern character 
            prevCh = ch;
            ++count;
        }
    }

    int32_t splitPoint = i - count;
    if ( splitPoint < intervalPattern->length() ) {
        Calendar* firstCal;
        Calendar* secondCal;
        if ( fIntervalPattern->firstDateInPtnIsLaterDate() ) {
            firstCal = &toCalendar;
            secondCal = &fromCalendar;
        } else {
            firstCal = &fromCalendar;
            secondCal = &toCalendar;
        }
        // break the interval pattern into 2 parts
        UnicodeString firstPart;
        UnicodeString secondPart;
        intervalPattern->extract(0, splitPoint, firstPart);
        intervalPattern->extract(splitPoint, intervalPattern->length() - splitPoint, secondPart);

        fDateFormat->applyPattern(firstPart);
        fDateFormat->format(*firstCal, appendTo, pos);
        fDateFormat->applyPattern(secondPart);
        fDateFormat->format(*secondCal, appendTo, pos);
    } else {
        // single date pattern
        fDateFormat->format(fromCalendar, appendTo, pos);
    }
    return appendTo;
}



void
DateIntervalFormat::parseObject(const UnicodeString& source, 
                                Formattable& result,
                                ParsePosition& parse_pos) const {
    // FIXME: THERE is no error code,
    // then, where to set the not-supported error
}




void
DateIntervalFormat::setDateFormat(const DateFormat& newDateFormat,
                                  UErrorCode& status) {
    if ( newDateFormat.getDynamicClassID() == SimpleDateFormat::getStaticClassID() ) {
        delete fDateFormat;
        fDateFormat = new SimpleDateFormat((SimpleDateFormat&)newDateFormat);
        if ( fDateFormat && fDateFormat->getCalendar() ) {
            fFromCalendar = fDateFormat->getCalendar()->clone();
            fToCalendar = fDateFormat->getCalendar()->clone();
        } else {
            fFromCalendar = NULL;
            fToCalendar = NULL;
        }
    } else {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
}


void 
DateIntervalFormat::adoptDateFormat(DateFormat* newDateFormat,
                                    UErrorCode& status) {
    if ( newDateFormat->getDynamicClassID() == SimpleDateFormat::getStaticClassID() ) {
        delete fDateFormat;
        fDateFormat = (SimpleDateFormat*)newDateFormat;
        if ( fDateFormat && fDateFormat->getCalendar() ) {
            fFromCalendar = fDateFormat->getCalendar()->clone();
            fToCalendar = fDateFormat->getCalendar()->clone();
        } else {
            fFromCalendar = NULL;
            fToCalendar = NULL;
        }
    } else {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
}


DateIntervalFormat::DateIntervalFormat(DateFormat* dtfmt,
                                       DateIntervalInfo* dtItvInfo,
                                       UErrorCode& status) 
:   fDateFormat(0),
    fIntervalPattern(0),
    fFromCalendar(0),
    fToCalendar(0)
{
    if ( U_FAILURE(status) ) {
        delete dtfmt;
        delete dtItvInfo;
        return;
    }
    if ( dtfmt == NULL || dtItvInfo == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        // safe to delete NULL
        delete dtfmt;
        delete dtItvInfo;
        return;
    }
    fIntervalPattern = dtItvInfo;
    fDateFormat = (SimpleDateFormat*)dtfmt;
    if ( dtfmt->getCalendar() ) {
        fFromCalendar = dtfmt->getCalendar()->clone();
        fToCalendar = dtfmt->getCalendar()->clone();
    } else {
        fFromCalendar = NULL;
        fToCalendar = NULL;
    }
}


DateIntervalFormat* U_EXPORT2
DateIntervalFormat::create(DateFormat* dtfmt,
                           DateIntervalInfo* dtitvinf,
                           UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        delete dtfmt;
        delete dtitvinf;
        return NULL;
    }

    DateIntervalFormat* f = new DateIntervalFormat(dtfmt, dtitvinf, status);
    if ( f == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        delete dtfmt;
        delete dtitvinf;
    } else if ( U_FAILURE(status) ) {
        // safe to delete f, although nothing acutally is saved
        delete f;
        f = 0;
    }
    return f;
}



DateIntervalFormat* U_EXPORT2
DateIntervalFormat::create(DateFormat* dtfmt,
                           const Locale& locale, UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        delete dtfmt;
        return NULL;
    }
    if ( dtfmt 
         && dtfmt->getDynamicClassID() == SimpleDateFormat::getStaticClassID()){
        // gen skeleton from pattern
        DateTimePatternGenerator* dtptg = 
                   DateTimePatternGenerator::createInstance(locale, status);
        if ( dtptg == NULL ) {
            status = U_MEMORY_ALLOCATION_ERROR;
            delete dtfmt;
            return NULL;
        }
        UnicodeString skeleton = dtptg->getSkeleton(
                                 ((SimpleDateFormat*)dtfmt)->fPattern, status);
        DateIntervalInfo* dtitvinf = new DateIntervalInfo(locale, skeleton,status);
        delete dtptg;
        return create(dtfmt, dtitvinf, status);
    } else {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        delete dtfmt;
        return NULL;
    }
}


U_NAMESPACE_END

#endif
