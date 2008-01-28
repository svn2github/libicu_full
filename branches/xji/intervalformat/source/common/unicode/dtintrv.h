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
 
    /** 
     * Set the from date.
     * @param date  The from date to be set in date interval.
     * @draft ICU 4.0
     */
    inline void setFromDate(const UDate date) { fromDate = date; }

    /**
     * Set the to date.
     * @param date  The to date to be set in date interval.
     * @draft ICU 4.0
     */
    inline void setToDate(const UDate date) { toDate = date; }

    /** 
     * Get the from date.
     * @return  the from date in dateInterval.
     * @draft ICU 4.0
     */
    inline UDate getFromDate() const { return fromDate; }

    /** 
     * Get the to date.
     * @return  the to date in dateInterval.
     * @draft ICU 4.0
     */
    inline UDate getToDate() const { return toDate; }


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

    
    /**
     * Copy constructor.
     * @draft ICU 4.0
     */
    DateInterval(const DateInterval& other);

    /**
     * Default assignment operator
     * @draft ICU 4.0
     */
    DateInterval& operator=(const DateInterval&);

    /**
     * Equality operator.
     * @return TRUE if the two DateIntervals are the same
     * @draft ICU 4.0
     */
    inline UBool operator==(const DateInterval& other) const { 
        return ( fromDate == other.fromDate && toDate == other.toDate );
    }

    /**
     * Non-equality operator
     * @return TRUE if the two DateIntervals are not the same
     * @draft ICU 4.0
     */
    inline UBool operator!=(const DateInterval& other) const { 
        return ( !operator==(other) );
    }


    /**
     * clone this object. 
     * The caller owns the result and should delete it when done.
     * @return a cloned DateInterval
     * @draft ICU 4.0
     */
    inline DateInterval* clone() const {
        return new DateInterval(*this);
    }

private:
    UDate fromDate;
    UDate toDate;

} ;// end class DateInterval

U_NAMESPACE_END

#endif
