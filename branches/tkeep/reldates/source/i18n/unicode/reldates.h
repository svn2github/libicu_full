/*
********************************************************************************
* Copyright (C) 1997-2013, International Business Machines Corporation and others.
* All Rights Reserved.
********************************************************************************
*
* File RELDATES.H
********************************************************************************
*/

#ifndef __RELDATES_H
#define __RELDATES_H

#include "unicode/utypes.h"
#include "unicode/locid.h"

U_NAMESPACE_BEGIN 

class NumberFormat;

/**
 * TODO (tkeep): Copy class level comments from JAVA once they
 * are approved.
*/
class U_I18N_API RelativeDateTimeFormatter : public UObject {
public:

    /**
     * TODO (tkeep): Copy comments from JAVA once approved.
     */
    enum RelativeUnit {

        /**
         * @draft ICU 53
         */
        kSeconds,

        /**
         * @draft ICU 53
         */
        kMinutes,

        /**
         * @draft ICU 53
         */
        kHours,

        /**
         * @draft ICU 53
         */
        kDays,

        /**
         * @draft ICU 53
         */
        kWeeks,

        /**
         * @draft ICU 53
         */
        kMonths,

        /**
         * @draft ICU 53
         */
        kYears
    };

    /**
     * TODO (tkeep): Copy comments from JAVA once approved.
     */
    enum AbsoluteUnit {

        /**
         * @draft ICU 53
         */
        kSunday,

        /**
         * @draft ICU 53
         */
        kMonday,

        /**
         * @draft ICU 53
         */
        kTuesday,

        /**
         * @draft ICU 53
         */
        kWednesday,

        /**
         * @draft ICU 53
         */
        kThursday,

        /**
         * @draft ICU 53
         */
        kFriday,

        /**
         * @draft ICU 53
         */
        kSaturday,

        /**
         * @draft ICU 53
         */
        kDay,

        /**
         * @draft ICU 53
         */
        kWeek,

        /**
         * @draft ICU 53
         */
        kMonth,

        /**
         * @draft ICU 53
         */
        kYear,

        /**
         * @draft ICU 53
         */
        kNow
    };

    /**
     * TODO (tkeep): Copy comments from JAVA once approved.
     */
    enum Direction {

        /**
         * @draft ICU 53
         */
        kLast,

        /**
         * @draft ICU 53
         */
        kThis,

        /**
         * @draft ICU 53
         */
        kNext,

        /**
         * @draft ICU 53
         */
        kPlain
    };

    /**
     * Create RelativeDateTimeFormatter with default locale.
     * @draft ICU 53
     */
    RelativeDateTimeFormatter(UErrorCode& status);


    /**
     * Create RelativeDateTimeFormatter with given locale.
     * @draft ICU 53
     */
    RelativeDateTimeFormatter(const Locale& locale, UErrorCode& status);

    /**
     * Copy constructor.
     * @draft ICU 53
     */
    RelativeDateTimeFormatter(const RelativeDateTimeFormatter& other);

    /**
     * Assignment operator.
     * @draft ICU 53
     */
    RelativeDateTimeFormatter& operator=(const RelativeDateTimeFormatter& other);

    /**
     * Destructor.
     * @draft ICU 53
     */
    virtual ~RelativeDateTimeFormatter();


    /**
     * TODO(tkeep): Copy comment from JAVA once approved.
     *
     * @param appendTo The string to which the formatted result will be
     *  appended
     * @param status ICU error code returned here.
     * @return appendTo
     * @draft ICU 53
     */
    UnicodeString& format(double quantity, Direction direction, RelativeUnit unit, UnicodeString& appendTo, UErrorCode& status) const;

    /**
     * TODO(tkeep): Copy comment from JAVA once approved.
     *
     * @param appendTo The string to which the formatted result will be
     *  appended
     * @param status ICU error code returned here.
     * @return appendTo
     * @draft ICU 53
     */
    UnicodeString& format(Direction direction, AbsoluteUnit unit, UnicodeString& appendTo, UErrorCode& status) const;

    /**
     * TODO(tkeep): Copy comment from JAVA once approved.
     *
     * @param relativeDateString the relative date, e.g 'yesterday'
     * @param timeString the time e.g '3:45'
     * @param appendTo concatenated date and time appended here
     * @param status ICU error code returned here.
     * @return appendTo
     * @draft ICU 53
     */
    UnicodeString& combineDateAndTime(
        const UnicodeString& relativeDateString, const UnicodeString& timeString,
        UnicodeString& appendTo, UErrorCode& status) const;

    /**
     * TODO(tkeep): Copy comment from JAVA once approved.
     *
     * @param nf the NumberFormat object to use.
     * @draft ICU 53
     */
    void setNumberFormat(const NumberFormat& nf);
private:
    RelativeDateTimeFormatter();
};

U_NAMESPACE_END

#endif
