/*
 *******************************************************************************
 * Copyright (C) 2013-2013, Google, International Business Machines Corporation
 * and others. All Rights Reserved.
 *******************************************************************************
 */

#ifndef __TIMEPERIOD_H__
#define __TIMEPERIOD_H__

#include "unicode/utypes.h"

/**
 * \file
 * \brief C++ API: Format and parse duration in single time unit
 */


#if !UCONFIG_NO_FORMATTING

#include "unicode/tmunit.h"
#include "unicode/tmutamt.h"

U_NAMESPACE_BEGIN

class U_I18N_API TimePeriod: public UObject {
public:
    // copy constructor.
    TimePeriod(const TimePeriod& other);

    /** 
     * Factory method.
     * @param timeUnitAmounts an array of TimeUnitAmounts pointers. TimePeriod copies the
     * data in this array. The caller is responsible for freeing the TimeUnitAmount objects
     * and the array.
     * @param length the number of timeUnitAmount pointers in timeUnitAmounts array.
     * @param status error returned here if timeUnitAmounts is empty;
     *    timeUnitAmounts has duplicate time units; or any timeUnitAmount except the 
     *    smallest has a non-integer value.
     * @return pointer to newly created TimePeriod object or NULL on error.
     * @draft ICU 52
     */  
    static TimePeriod* forAmounts(
        TimeUnitAmount **timeUnitAmounts, int32_t length, UErrorCode& status);

    /** 
     * Destructor
     * @draft ICU 52
     */  
    virtual ~TimePeriod();

    /** 
     * Equality operator.
     * @param other the object to compare to. 
     * @return true if this object the same TimeUnitAmount objects as the given
     *   object.
     * @draft ICU 52
     */  
    virtual UBool operator==(const UObject& other) const;

   /** 
     * Not-equality operator.
     * @param other the object to compare to. 
     * @return true if this object has different TimeUnitAmount objects as the given
     *   object.
     * @draft ICU 52
     */  
    virtual UBool operator!=(const UObject& other) const;

    /** 
     * Gets a specific field out of a time period.
     * @param field is the field to fetch
     * @param result fetched field is stored here, if it exists.
     * @status any error encountered is stored here. 
     * @return true if a field in this object does exist for field or false
         otherwise. Note that when this method returns false, no error is set 
         in status as a missing field is part of the normal course of operation.
     */  
    UBool getAmount(TimeUnit::UTimeUnitFields field, TimeUnitAmount& result, UErrorCode& status) const;


private:
    // Clients use forAmount and never use ctor directly.
    TimePeriod() {
    }   

    /** 
     * No assignment operator needed because class is immutable.
     */  
    TimePeriod& operator=(const TimePeriod& other);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // __TMUTFMT_H__
//eof
