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
    DateInterval(const UDate fromDate, const UDate toDate);

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
    void setFromDate(const UDate date);

    /* 
     * Set the to date.
     * @param date  The to date to be set in date interval.
     * @draft ICU 4.0
     */
    void setToDate(const UDate date);

    /* 
     * Get the from date.
     * @return  the from date in dateInterval.
     * @draft ICU 4.0
     */
    UDate getFromDate() const;

    /* 
     * Get the to date.
     * @return  the to date in dateInterval.
     * @draft ICU 4.0
     */
    UDate getToDate() const;


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


private:
    UDate fromDate;
    UDate toDate;

} ;// end class DateInterval

U_NAMESPACE_END

#endif
