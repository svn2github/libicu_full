/*
*******************************************************************************
* Copyright (C) 2007, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef VTZONE_H
#define VTZONE_H

#include "unicode/utypes.h"

/**
 * \file 
 * \brief C++ API: RFC2445 VTIMEZONE support
 */

#if !UCONFIG_NO_FORMATTING

#include "unicode/basictz.h"

U_NAMESPACE_BEGIN

class U_I18N_API VTimeZone : public BasicTimeZone {
public:
    /**
     * Copy constructor.
     * @param source    The <code>VTimeZone</code> object to be copied.
     * @draft ICU 3.8
     */
    VTimeZone(const VTimeZone& source);

    /**
     * Destructor.
     * @draft ICU 3.8
     */
    virtual ~VTimeZone();

    /**
     * Assignment operator.
     * @param right The object to be copied.
     * @draft ICU 3.8
     */
    VTimeZone& operator=(const VTimeZone& right);

    /**
     * Return true if the given <code>VTimeZone</code> objects are
     * semantically equal. Objects of different subclasses are considered unequal.
     * @param that  The object to be compared with.
     * @return  true if the given <code>VTimeZone</code> objects are
      *semantically equal.
     * @draft ICU 3.8
     */
    virtual UBool operator==(const VTimeZone& that) const;

    /**
     * Return true if the given <code>VTimeZone</code> objects are
     * semantically unequal. Objects of different subclasses are considered unequal.
     * @param that  The object to be compared with.
     * @return  true if the given <code>VTimeZone</code> objects are
     * semantically unequal.
     * @draft ICU 3.8
     */
    virtual UBool operator!=(const VTimeZone& that) const;

    /**
     * Create a <code>VTimeZone</code> instance by the time zone ID.
     * @param ID The time zone ID, such as America/New_York
     * @return A <code>VTimeZone</code> object initialized by the time zone ID,
     * or NULL when the ID is unknown.
     * @draft ICU 3.8
     */
    static VTimeZone* createVTimeZone(const UnicodeString& ID);

    /**
     * Create a <code>VTimeZone</code> instance by RFC2445 VTIMEZONE data
     * 
     * @param vtzdata The string including VTIMEZONE data block
     * @param status Output param to filled in with a success or an error.
     * @return A <code>VTimeZone</code> initialized by the VTIMEZONE data or
     * NULL if failed to load the rule from the VTIMEZONE data.
     * @draft ICU 3.8
     */
    static VTimeZone* createVTimeZone(const UnicodeString& vtzdata, UErrorCode& status);

    /**
     * Gets the RFC2445 TZURL property value.  When a <code>VTimeZone</code> instance was
     * created from VTIMEZONE data, the initial value is set by the TZURL property value
     * in the data.  Otherwise, the initial value is not set.
     * @param url Receives the RFC2445 TZURL property value.
     * @return TRUE if TZURL attribute is available and value is set.
     * @draft ICU 3.8
     */
    UBool getTZURL(UnicodeString& url) const;

    /**
     * Sets the RFC2445 TZURL property value.
     * @param url The TZURL property value.
     * @draft ICU 3.8
     */
    void setTZURL(const UnicodeString& url);

    /**
     * Gets the RFC2445 LAST-MODIFIED property value.  When a <code>VTimeZone</code> instance
     * was created from VTIMEZONE data, the initial value is set by the LAST-MODIFIED property
     * value in the data.  Otherwise, the initial value is not set.
     * @param lastModified Receives the RFC2445 LAST-MODIFIED property value.
     * @return TRUE if lastModified attribute is available and value is set.
     * @draft ICU 3.8
     */
    UBool getLastModified(UnicodeString& lastModified) const;

    /**
     * Sets the RFC2445 LAST-MODIFIED property value.
     * @param lastModified The LAST-MODIFIED property value.
     * @draft ICU 3.8
     */
    void setLastModified(const UnicodeString& lastModified);

    /**
     * Writes RFC2445 VTIMEZONE data for this time zone
     * @param result Output param to filled in with the VTIMEZONE data.
     * @param status Output param to filled in with a success or an error.
     * @draft ICU 3.8
     */
    void write(UnicodeString& result, UErrorCode& status) const;

    /**
     * Writes RFC2445 VTIMEZONE data for this time zone applicalbe
     * for dates after the specified cutover time.
     * @param cutover The cutover date.
     * @param result Output param to filled in with the VTIMEZONE data.
     * @param status Output param to filled in with a success or an error.
     * @draft ICU 3.8
     */
    void write(UDate cutover, UnicodeString& result, UErrorCode& status) const;

    /**
     * Writes RFC2445 VTIMEZONE data applicalbe for the specified date.
     * Some common iCalendar implementations can only handle a single time
     * zone property or a pair of standard and daylight time properties using
     * BYDAY rule with day of week (such as BYDAY=1SUN).  This method produce
     * the VTIMEZONE data which can be handled these implementations.  The rules
     * produced by this method can be used only for calculating time zone offset
     * around the specified date.
     * @param time The date used for rule extraction.
     * @param result Output param to filled in with the VTIMEZONE data.
     * @param status Output param to filled in with a success or an error.
     * @draft ICU 3.8
     */
    void writeSimple(UDate time, UnicodeString& result, UErrorCode& status) const;

    /**
     * Clones TimeZone objects polymorphically. Clients are responsible for deleting
     * the TimeZone object cloned.
     * @return   A new copy of this TimeZone object.
     * @draft ICU 3.8
     */
    virtual TimeZone* clone(void) const;

    /**
     * Returns the TimeZone's adjusted GMT offset (i.e., the number of milliseconds to add
     * to GMT to get local time in this time zone, taking daylight savings time into
     * account) as of a particular reference date.  The reference date is used to determine
     * whether daylight savings time is in effect and needs to be figured into the offset
     * that is returned (in other words, what is the adjusted GMT offset in this time zone
     * at this particular date and time?).  For the time zones produced by createTimeZone(),
     * the reference data is specified according to the Gregorian calendar, and the date
     * and time fields are local standard time.
     *
     * <p>Note: Don't call this method. Instead, call the getOffset(UDate...) overload,
     * which returns both the raw and the DST offset for a given time. This method
     * is retained only for backward compatibility.
     *
     * @param era        The reference date's era
     * @param year       The reference date's year
     * @param month      The reference date's month (0-based; 0 is January)
     * @param day        The reference date's day-in-month (1-based)
     * @param dayOfWeek  The reference date's day-of-week (1-based; 1 is Sunday)
     * @param millis     The reference date's milliseconds in day, local standard time
     * @param status     Output param to filled in with a success or an error.
     * @return           The offset in milliseconds to add to GMT to get local time.
     * @draft ICU 3.8
     */
    virtual int32_t getOffset(uint8_t era, int32_t year, int32_t month, int32_t day,
                              uint8_t dayOfWeek, int32_t millis, UErrorCode& status) const;

    /**
     * Gets the time zone offset, for current date, modified in case of
     * daylight savings. This is the offset to add *to* UTC to get local time.
     *
     * <p>Note: Don't call this method. Instead, call the getOffset(UDate...) overload,
     * which returns both the raw and the DST offset for a given time. This method
     * is retained only for backward compatibility.
     *
     * @param era        The reference date's era
     * @param year       The reference date's year
     * @param month      The reference date's month (0-based; 0 is January)
     * @param day        The reference date's day-in-month (1-based)
     * @param dayOfWeek  The reference date's day-of-week (1-based; 1 is Sunday)
     * @param millis     The reference date's milliseconds in day, local standard time
     * @param monthLength The length of the given month in days.
     * @param status     Output param to filled in with a success or an error.
     * @return           The offset in milliseconds to add to GMT to get local time.
     * @draft ICU 3.8
     */
    virtual int32_t getOffset(uint8_t era, int32_t year, int32_t month, int32_t day,
                           uint8_t dayOfWeek, int32_t millis,
                           int32_t monthLength, UErrorCode& status) const;

    /**
     * Returns the time zone raw and GMT offset for the given moment
     * in time.  Upon return, local-millis = GMT-millis + rawOffset +
     * dstOffset.  All computations are performed in the proleptic
     * Gregorian calendar.  The default implementation in the TimeZone
     * class delegates to the 8-argument getOffset().
     *
     * @param date moment in time for which to return offsets, in
     * units of milliseconds from January 1, 1970 0:00 GMT, either GMT
     * time or local wall time, depending on `local'.
     * @param local if true, `date' is local wall time; otherwise it
     * is in GMT time.
     * @param rawOffset output parameter to receive the raw offset, that
     * is, the offset not including DST adjustments
     * @param dstOffset output parameter to receive the DST offset,
     * that is, the offset to be added to `rawOffset' to obtain the
     * total offset between local and GMT time. If DST is not in
     * effect, this value is zero; otherwise it is a positive value,
     * typically one hour.
     * @param ec input-output error code
     * @draft ICU 3.8
     */
    virtual void getOffset(UDate date, UBool local, int32_t& rawOffset,
                           int32_t& dstOffset, UErrorCode& ec) const;

    /**
     * Sets the TimeZone's raw GMT offset (i.e., the number of milliseconds to add
     * to GMT to get local time, before taking daylight savings time into account).
     *
     * @param offsetMillis  The new raw GMT offset for this time zone.
     * @draft ICU 3.8
     */
    virtual void setRawOffset(int32_t offsetMillis);

    /**
     * Returns the TimeZone's raw GMT offset (i.e., the number of milliseconds to add
     * to GMT to get local time, before taking daylight savings time into account).
     *
     * @return   The TimeZone's raw GMT offset.
     * @draft ICU 3.8
     */
    virtual int32_t getRawOffset(void) const;

    /**
     * Queries if this time zone uses daylight savings time.
     * @return true if this time zone uses daylight savings time,
     * false, otherwise.
     * @draft ICU 3.8
     */
    virtual UBool useDaylightTime(void) const;

    /**
     * Queries if the given date is in daylight savings time in
     * this time zone.
     * This method is wasteful since it creates a new GregorianCalendar and
     * deletes it each time it is called. This is a deprecated method
     * and provided only for Java compatibility.
     *
     * @param date the given UDate.
     * @param status Output param filled in with success/error code.
     * @return true if the given date is in daylight savings time,
     * false, otherwise.
     * @deprecated ICU 2.4. Use Calendar::inDaylightTime() instead.
     */
    virtual UBool inDaylightTime(UDate date, UErrorCode& status) const;

    /**
     * Returns true if this zone has the same rule and offset as another zone.
     * That is, if this zone differs only in ID, if at all.
     * @param other the <code>TimeZone</code> object to be compared with
     * @return true if the given zone is the same as this one,
     * with the possible exception of the ID
     * @stable ICU 3.8
     */
    virtual UBool hasSameRules(const TimeZone& other) const;

    /**
     * Gets the first time zone transition after the base time.
     * @param base      The base time.
     * @param inclusive Whether the base time is inclusive or not.
     * @param result    Receives the first transition after the base time.
     * @return  TRUE if the transition is found.
     * @draft ICU 3.8
     */
    virtual UBool getNextTransition(UDate base, UBool inclusive, TimeZoneTransition& result) /*const*/;

    /**
     * Gets the most recent time zone transition before the base time.
     * @param base      The base time.
     * @param inclusive Whether the base time is inclusive or not.
     * @param result    Receives the most recent transition before the base time.
     * @return  TRUE if the transition is found.
     * @draft ICU 3.8
     */
    virtual UBool getPreviousTransition(UDate base, UBool inclusive, TimeZoneTransition& result) /*const*/;

    /**
     * Gets the <code>InitialTimeZoneRule</code> which represents the initial time name
     * and offsets information of the time zone.  This method always returns non-NULL
     * value.  The <code>InitialTimeZoneRule</code> is owned by the caller and must be
     * deleted.
     * @param status    Receives error status code.
     * @retrun The <code>InitialTimeZoneRule</code> for the time zone.
     * @draft ICU 3.8
     */
    virtual InitialTimeZoneRule* getInitialRule(UErrorCode& status) /*const*/;

    /**
     * Returns the number of <code>TimeZoneRule</code>s which represents time transitions,
     * for this time zone, that is, all <code>TimeZoneRule</code>s for this time zone except
     * <code>InitialTimeZoneRule</code>.  The return value range is 0 or any positive value.
     * @param status    Receives error status code.
     * @return The number of <code>TimeZoneRule</code>s representing time transitions.
     * @draft ICU 3.8
     */
    virtual int16_t countTransitionRules(UErrorCode& status) /*const*/;

    /**
     * Returns the <code>TimeZoneRule</code> which represents time transitions for this time zone
     * at the index.  Valid index range is 0 to <code>countTransitionRules</code> - 1.  The caller
     * owns <code>TimeZoneRule</code> object returned by this method and must be deleted.
     * @param index The index.
     * @param status    Receives error status code.
     * @return The <code>TimeZoneRule</code> at the index.
     * @draft ICU 3.8
     */
    virtual TimeZoneRule* getTransitionRule(int16_t index, UErrorCode& status) /*const*/;

private:
    BasicTimeZone   *tz;

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

#endif // VTZONE_H
//eof
