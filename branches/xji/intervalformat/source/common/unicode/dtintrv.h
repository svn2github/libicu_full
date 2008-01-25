/*
*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTINTRV.H 
*
*******************************************************************************
*/

#ifndef __DTINTRV_H__
#define __DTINTRV_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN


/**
 * This class represents date interval.
 * It is a pair of UDate representing from UDate 1 to UDate 2.
 * @draft ICU 4.0
**/
class U_COMMON_API DateInterval : public UObject {
public:

    /** 
     * Default constructor.
     * @draft ICU 4.0
     */
    DateInterval();

    /** 
     * Constructor given from date and to date.
     * @param fromDate  The from date in date interval.
     * @param toDate    The to date in date interval.
     * @draft ICU 4.0
     */
    DateInterval(UDate fromDate, UDate toDate);

    /**
     * destructor
     * @draft ICU 4.0
     */
    ~DateInterval();
 
    /* 
     * Set the from date.
     * @param date  The from date to be set in date interval.
     * @draft ICU 4.0
     */
    void setFromDate(UDate date);

    /* 
     * Set the to date.
     * @param date  The to date to be set in date interval.
     * @draft ICU 4.0
     */
    void setToDate(UDate date);

    /* 
     * Get the from date.
     * @return  the from date in dateInterval.
     * @draft ICU 4.0
     */
    UDate getFromDate();

    /* 
     * Get the to date.
     * @return  the to date in dateInterval.
     * @draft ICU 4.0
     */
    UDate getToDate();

private:
    UDate fromDate;
    UDate toDate;

} ;// end class DateInterval

U_NAMESPACE_END

#endif
