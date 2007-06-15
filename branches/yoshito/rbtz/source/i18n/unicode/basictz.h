/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef BASICTZ_H
#define BASICTZ_H

/**
 * \file 
 * \brief C++ API: ICU TimeZone base class
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/timezone.h"

U_NAMESPACE_BEGIN

// forward declarations
class TimeZoneRule;
class InitialTimeZoneRule;
class AnnualTimeZoneRule;
class TimeZoneTransition;

/**
 * <code>BasicTimeZone</code> is an abstract class extending <code>TimeZone</code>.
 * This class provides some additional methods to access time zone transitions and rules.
 * All ICU <code>TimeZone</code> concrete subclasses extend this class.
 * @draft ICU 3.8
 */
class U_I18N_API BasicTimeZone: public TimeZone {
public:
    /**
     * Destructor.
     * @draft ICU 3.8
     */
    virtual ~BasicTimeZone();

    /**
     * Gets the first time zone transition after the base time.
     * @param base      The base time.
     * @param inclusive Whether the base time is inclusive or not.
     * @param result    Receives the first transition after the base time.
     * @return  TRUE if the transition is found.
     * @draft ICU 3.8
     */
    virtual UBool getNextTransition(UDate base, UBool inclusive, TimeZoneTransition& result) /*const*/ = 0;

    /**
     * Gets the most recent time zone transition before the base time.
     * @param base      The base time.
     * @param inclusive Whether the base time is inclusive or not.
     * @param result    Receives the most recent transition before the base time.
     * @return  TRUE if the transition is found.
     * @draft ICU 3.8
     */
    virtual UBool getPreviousTransition(UDate base, UBool inclusive, TimeZoneTransition& result) /*const*/ = 0;

    /**
     * Checks if the time zone has equivalent transitions in the time range.
     * This method returns true when all of transition times, from/to standard
     * offsets and DST savings used by this time zone match the other in the
     * time range.
     * @param tz    The <code>BasicTimeZone</code> object to be compared with.
     * @param start The start time of the evaluated time range (inclusive)
     * @param end   The end time of the evaluated time range (inclusive)
     * @param ignoreDstAmount
     *              When true, any transitions with only daylight saving amount
     *              changes will be ignored, except either of them is zero.
     *              For example, a transition from rawoffset 3:00/dstsavings 1:00
     *              to rawoffset 2:00/dstsavings 2:00 is excluded from the comparison,
     *              but a transtion from rawoffset 2:00/dstsavings 1:00 to
     *              rawoffset 3:00/dstsavings 0:00 is included.
     * @param ec    Output param to filled in with a success or an error.
     * @return      true if the other time zone has the equivalent transitions in the
     *              time range.
     * @draft ICU 3.8
     */
    virtual UBool hasEquivalentTransitions(/*const*/ BasicTimeZone& tz, UDate start, UDate end,
        UBool ignoreDstAmount, UErrorCode& ec) /*const*/;

    /**
     * Gets the <code>InitialTimeZoneRule</code> which represents the initial time name
     * and offsets information of the time zone.  This method always returns non-NULL
     * value.  The <code>InitialTimeZoneRule</code> is owned by the caller and must be
     * deleted.
     * @param status    Receives error status code.
     * @retrun The <code>InitialTimeZoneRule</code> for the time zone.
     * @draft ICU 3.8
     */
    virtual InitialTimeZoneRule* getInitialRule(UErrorCode& status) /*const*/ = 0;

    /**
     * Returns the number of <code>TimeZoneRule</code>s which represents time transitions,
     * for this time zone, that is, all <code>TimeZoneRule</code>s for this time zone except
     * <code>InitialTimeZoneRule</code>.  The return value range is 0 or any positive value.
     * @param status    Receives error status code.
     * @return The number of <code>TimeZoneRule</code>s representing time transitions.
     * @draft ICU 3.8
     */
    virtual int16_t countTransitionRules(UErrorCode& status) /*const*/ = 0;

    /**
     * Returns the <code>TimeZoneRule</code> which represents time transitions for this time zone
     * at the index.  Valid index range is 0 to <code>countTransitionRules</code> - 1.  The caller
     * owns <code>TimeZoneRule</code> object returned by this method and must be deleted.
     * @param index The index.
     * @param status    Receives error status code.
     * @return The <code>TimeZoneRule</code> at the index.
     * @draft ICU 3.8
     */
    virtual TimeZoneRule* getTransitionRule(int16_t index, UErrorCode& status) /*const*/ = 0;

    /**
     * Gets the set of time zone rules valid at the specified time.  Some known external time zone
     * implementations are not capable to handle historic time zone rule changes.  Also some
     * implementations can only handle certain type of rule definitions.
     * If this time zone does not use any daylight saving time within about 1 year from the specified
     * time, only the <code>InitialTimeZone</code> is returned.  Otherwise, the rule for standard
     * time and daylight saving time transitions are returned in addition to the
     * <code>InitialTimeZoneRule</code>.  The standard and daylight saving time transition rules are
     * represented by <code>AnnualTimeZoneRule</code> with <code>DateTimeRule::DOW</code> for its date
     * rule and <code>DateTimeRule::WALL_TIME</code> for its time rule.  Because daylight saving time
     * rule is changing time to time in many time zones and also mapping a transition time rule to
     * different type is lossy transformation, the set of rules returned by this method may be valid
     * for short period of time.
     * The time zone rule objects returned by this method is owned by the caller, so the caller is
     * responsible for deleting them after use.
     * @param date      The date used for extracting time zone rules.
     * @param initial   Receives the <code>InitialTimeZone</code>, always not NULL.
     * @param std       Receives the <code>AnnualTimeZoneRule</code> for standard time transitions.
     *                  When this time time zone does not observe daylight saving times around the
     *                  specified date, NULL is set.
     * @param dst       Receives the <code>AnnualTimeZoneRule</code> for daylight saving time
     *                  transitions.  When this time zone does not observer daylight saving times
     *                  around the specified date, NULL is set.
     * @param status    Receives error status code.
     * @draft ICU 3.8
     */
    virtual void getSimpleRules(UDate date, InitialTimeZoneRule*& initial,
        AnnualTimeZoneRule*& std, AnnualTimeZoneRule*& dst, UErrorCode& status) /*const*/;

protected:
    /**
     * Default constructor.
     * @draft ICU 3.8
     */
    BasicTimeZone();

    /*
     * Construct a timezone with a given ID.
     * @param id a system time zone ID
     * @draft ICU 3.8
     */
    BasicTimeZone(const UnicodeString &id);

    /**
     * Copy constructor.
     * @param source the object to be copied.
     * @draft ICU 3.8
     */
    BasicTimeZone(const BasicTimeZone& source);

public:
    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @draft ICU 3.8
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
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID(void) const;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // BASICTZ_H

//eof
