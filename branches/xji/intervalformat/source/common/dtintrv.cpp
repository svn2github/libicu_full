/*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTINTRV.CPP 
*
*******************************************************************************
*/



#include "unicode/dtintrv.h"


U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateInterval)

//DateInterval::DateInterval(){}


DateInterval::DateInterval(const UDate from, const UDate to)
:   fromDate(from),
    toDate(to)
{}


DateInterval::~DateInterval(){};


DateInterval::DateInterval(const DateInterval& other) {
    *this = other;
}   


DateInterval&
DateInterval::operator=(const DateInterval& other) {
    if ( this == &other ) {
        return *this;
    }

    fromDate = other.fromDate;
    toDate = other.toDate;
}



U_NAMESPACE_END

