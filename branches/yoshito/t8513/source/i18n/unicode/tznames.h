/*
*******************************************************************************
* Copyright (C) 2011-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef __TZNAMES_H
#define __TZNAMES_H

/**
 * \file 
 * \brief C++ API: TimeZoneNames
 */
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef U_HIDE_DRAFT_API

#include "unicode/uloc.h"
#include "unicode/unistr.h"

U_CDECL_BEGIN

/**
 * Constants for time zone display name types.
 * @draft ICU 49
 */
typedef enum UTimeZoneNameType {
    /**
     * Unknown display name type.
     * @internal
     */
    UTZNM_UNKNOWN           = 0x00,
    /**
     * Long display name, such as "Eastern Time".
     * @draft ICU 49
     */
    UTZNM_LONG_GENERIC      = 0x01,
    /**
     * Long display name for standard time, such as "Eastern Standard Time".
     * @draft ICU 49
     */
    UTZNM_LONG_STANDARD     = 0x02,
    /**
     * Long display name for daylight saving time, such as "Eastern Daylight Time".
     * @draft ICU 49
     */
    UTZNM_LONG_DAYLIGHT     = 0x04,
    /**
     * Short display name, such as "ET".
     * @draft ICU 49
     */
    UTZNM_SHORT_GENERIC     = 0x08,
    /**
     * Short display name for standard time, such as "EST".
     * @draft ICU 49
     */
    UTZNM_SHORT_STANDARD    = 0x10,
    /**
     * Short display name for daylight saving time, such as "EDT".
     * @draft ICU 49
     */
    UTZNM_SHORT_DAYLIGHT    = 0x20
} UTimeZoneNameType;

U_CDECL_END

U_NAMESPACE_BEGIN

/*
class U_I18N_API TimeZoneNameMatchInfo : public UMemory {
public:
    virtual ~TimeZoneNameMatchInfo();

    virtual int32_t size() const = 0;
    virtual UTimeZoneNameType getNameType(int32_t index) const = 0;
    virtual int32_t getMatchLength(int32_t index) const = 0;
    virtual UnicodeString& getTimeZoneID(int32_t index, UnicodeString& tzID) const = 0;
    virtual UnicodeString& getMetaZoneID(int32_t index, UnicodeString& mzID) const = 0;
};
*/

/**
 * <code>TimeZoneNames</code> is an abstract class representing the time zone display name data model defined
 * by <a href="http://www.unicode.org/reports/tr35/">UTS#35 Unicode Locale Data Markup Language (LDML)</a>.
 * The model defines meta zone, which is used for storing a set of display names. A meta zone can be shared
 * by multiple time zones. Also a time zone may have multiple meta zone historic mappings.
 * <p>
 * For example, people in the United States refer the zone used by the east part of North America as "Eastern Time".
 * The tz database contains multiple time zones "America/New_York", "America/Detroit", "America/Montreal" and some
 * others that belong to "Eastern Time". However, assigning different display names to these time zones does not make
 * much sense for most of people.
 * <p>
 * In <a href="http://cldr.unicode.org/">CLDR</a> (which uses LDML for representing locale data), the display name
 * "Eastern Time" is stored as long generic display name of a meta zone identified by the ID "America_Eastern".
 * Then, there is another table maintaining the historic mapping to meta zones for each time zone. The time zones in
 * the above example ("America/New_York", "America/Detroit"...) are mapped to the meta zone "America_Eastern".
 * <p>
 * Sometimes, a time zone is mapped to a different time zone in the past. For example, "America/Indiana/Knox"
 * had been moving "Eastern Time" and "Central Time" back and forth. Therefore, it is necessary that time zone
 * to meta zones mapping data are stored by date range.
 * 
 * <p><b>Note:</b>
 * The methods in this class assume that time zone IDs are already canonicalized. For example, you may not get proper
 * result returned by a method with time zone ID "America/Indiana/Indianapolis", because it's not a canonical time zone
 * ID (the canonical time zone ID for the time zone is "America/Indianapolis". See
 * {@link TimeZone#getCanonicalID(const UnicodeString& id, UnicodeString& canonicalID, UErrorCode& status)} about ICU
 * canonical time zone IDs.
 * 
 * <p>
 * In CLDR, most of time zone display names except location names are provided through meta zones. But a time zone may
 * have a specific name that is not shared with other time zones.
 *
 * For example, time zone "Europe/London" has English long name for standard time "Greenwich Mean Time", which is also
 * shared with other time zones. However, the long name for daylight saving time is "British Summer Time", which is only
 * used for "Europe/London".
 * 
 * <p>
 * {@link #getTimeZoneDisplayName} is designed for accessing a name only used by a single time zone.
 * But is not necessarily mean that a subclass implementation use the same model with CLDR. A subclass implementation
 * may provide time zone names only through {@link #getTimeZoneDisplayName}, or only through {@link #getMetaZoneDisplayName},
 * or both.
 * 
 * @draft ICU 49
 */
class U_I18N_API TimeZoneNames : public UObject {
public:
    /**
     * Destructor.
     * @draft ICU 49
     */
    virtual ~TimeZoneNames();

    /**
     * Clone this object polymorphically.  The caller is responsible
     * for deleting the result when done.
     * @return A copy of the object
     * @draft ICU 49
     */
    virtual TimeZoneNames* clone() const = 0;

    /**
     * Returns an instance of <code>TimeZoneDisplayNames</code> for the specified locale.
     * 
     * @param locale The locale.
     * @param status Recevies the status.
     * @return An instance of <code>TimeZoneDisplayNames</code>
     * @draft ICU 49
     */
    static TimeZoneNames* U_EXPORT2 createInstance(const Locale& locale, UErrorCode& status);

    /**
     * Returns an enumeration of all available meta zone IDs.
     * @param status Recevies the status.
     * @return an enumeration object, owned by the caller.
     * @draft ICU 49
     */
    virtual StringEnumeration* getAvailableMetaZoneIDs(UErrorCode& status) const = 0;

    /**
     * Returns an enumeration of all available meta zone IDs used by the given time zone.
     * @param tzID The canoical tiem zone ID.
     * @param status Recevies the status.
     * @return an enumeration object, owned by the caller.
     * @draft ICU 49
     */
    virtual StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) const = 0;

    /**
     * Returns the meta zone ID for the given canonical time zone ID at the given date.
     * @param tzID The canonical time zone ID.
     * @param date The date.
     * @param mzID Receives the meta zone ID for the given time zone ID at the given date. If the time zone does not have a
     *          corresponding meta zone at the given date or the implementation does not support meta zones, "bogus" state
     *          is set.
     * @return A reference to the result.
     * @draft ICU 49
     */
    virtual UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const = 0;

    /**
     * Returns the reference zone ID for the given meta zone ID for the region.
     * @param mzID The meta zone ID.
     * @param region The region.
     * @param tzID Receives the reference zone ID ("golden zone" in the LDML specification) for the given time zone ID for the
     *          region. If the meta zone is unknown or the implementation does not support meta zones, "bogus" state
     *          is set.
     * @return A reference to the result.
     * @draft ICU 49
     */
    virtual UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const = 0;

    /**
     * Returns the display name of the meta zone.
     * @param mzID The meta zone ID.
     * @param type The display name type. See {@link #UTimeZoneNameType}.
     * @param name Receives the display name of the meta zone. When this object does not have a localized display name for the given
     *         meta zone with the specified type or the implementation does not provide any display names associated
     *         with meta zones, "bogus" state is set.
     * @return A reference to the result.
     * @draft ICU 49
     */
    virtual UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const = 0;

    /**
     * Returns the display name of the time zone. Unlike {@link #getDisplayName},
     * this method does not get a name from a meta zone used by the time zone.
     * @param tzID The canonical time zone ID.
     * @param type The display name type. See {@link #UTimeZoneNameType}.
     * @param name Receives the display name for the time zone. When this object does not have a localized display name for the given
     *         time zone with the specified type, "bogus" state is set.
     * @return A reference to the result.
     * @draft ICU 49
     */
    virtual UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const = 0;

    /**
     * Returns the exemplar location name for the given time zone. When this object does not have a localized location
     * name, the default implementation may still returns a programmatically generated name with the logic described
     * below.
     * <ol>
     * <li>Check if the ID contains "/". If not, return null.
     * <li>Check if the ID does not start with "Etc/" or "SystemV/". If it does, return null.
     * <li>Extract a substring after the last occurrence of "/".
     * <li>Replace "_" with " ".
     * </ol>
     * For example, "New York" is returned for the time zone ID "America/New_York" when this object does not have the
     * localized location name.
     * 
     * @param tzID The canonical time zone ID
     * @param name Receives the exemplar location name for the given time zone, or "bogus" state is set when a localized
     *          location name is not available and the fallback logic described above cannot extract location from the ID.
     * @return A reference to the result.
     * @draft ICU 49
     */
    virtual UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

    /**
     * Returns the display name of the time zone at the given date.
     * <p>
     * <b>Note:</b> This method calls the subclass's {@link #getTimeZoneDisplayName} first. When the
     * result is bogus, this method calls {@link #getMetaZoneID} to get the meta zone ID mapped from the
     * time zone, then calls {@link #getMetaZoneDisplayName}.
     * 
     * @param tzID The canonical time zone ID.
     * @param type The display name type. See {@link #UTimeZoneNameType}.
     * @param date The date.
     * @param name Receives the display name for the time zone at the given date. When this object does not have a localized display
     *          name for the time zone with the specified type and date, "bogus" state is set.
     * @return A reference to the result.
     * @draft ICU 49
     */
    virtual UnicodeString& getDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UDate date, UnicodeString& name) const;

    /**
     * <code>MatchInfoCollection</code> represents a collection of time zone name matches used by
     * {@link TimeZoneNames#find}.
     * @draft ICU 49
     */
    class U_I18N_API MatchInfoCollection : public UMemory {
    public:
        /**
         * Constructor.
         * @draft ICU 49
         */
        MatchInfoCollection();
        /**
         * Destructor.
         * @draft ICU 49
         */
        virtual ~MatchInfoCollection();

        /**
         * Adds a zone match.
         * @param nameType The name type.
         * @param matchLength The match length.
         * @param tzID The time zone ID.
         * @param status Receives the status
         * @draft ICU 49
         */
        void addZone(UTimeZoneNameType nameType, int32_t matchLength,
            const UnicodeString& tzID, UErrorCode& status);

        /**
         * Adds a meata zone match.
         * @param nameType The name type.
         * @param matchLength The match length.
         * @param mzID The metazone ID.
         * @param status Receives the status
         * @draft ICU 49
         */
        void addMetaZone(UTimeZoneNameType nameType, int32_t matchLength,
            const UnicodeString& mzID, UErrorCode& status);

        /**
         * Returns the number of entries available in this object.
         * @return The number of entries.
         * @draft ICU 49
         */
        int32_t size() const;

        /**
         * Returns the time zone name type of a match at the specified index.
         * @param idx The index
         * @param status Receives the status
         * @return The time zone name type
         * @see UTimeZoneNameType
         * @draft ICU 49
         */
        UTimeZoneNameType getNameTypeAt(int32_t idx, UErrorCode& status) const;

        /**
         * Returns the match length of a match at the specified index.
         * @param idx The index
         * @param status Receives the status
         * @return The match length
         * @draft ICU 49
         */
        int32_t getMatchLengthAt(int32_t idx, UErrorCode& status) const;

        /**
         * Gets the zone ID of a match at the specified index.
         * @param idx The index
         * @param tzID Receives the zone ID.
         * @param status Receives the status.
         * @return TRUE if zone ID is available.
         * @draft ICU 49
         */
        UBool getTimeZoneIDAt(int32_t idx, UnicodeString& tzID, UErrorCode& status) const;

        /**
         * Gets the metazone ID of a match at the specified index.
         * @param idx The index
         * @param mzID Receives the metazone ID
         * @param status Receives the status.
         * @return TRUE if meta zone ID is availabe.
         * @draft ICU 49
         */
        UBool getMetaZoneID(int32_t idx, UnicodeString& mzID, UErrorCode& status) const;
    };

    /**
     * Finds time zone name prefix matches for the input text at the
     * given offset and returns a collection of the matches.
     * @param text The text.
     * @param start The starting offset within the text.
     * @param types The set of name types represented by bitwise flags of UTimeZoneNameType enums,
     *              or UTZNM_UNKNOWN for all name types.
     * @param status Receives the status.
     * @return A collection of matches, owned by the caller.
     * @see UTimeZoneNameType
     * @see MatchInfoCollection
     * @draft ICU 49
     */
    virtual MatchInfoCollection* find(const UnicodeString& text, int32_t start, uint32_t types, UErrorCode& status) const = 0;

    //virtual TimeZoneNameMatchInfo* find(const UnicodeString& text, int32_t start, uint32_t types, UErrorCode& status) const = 0;
};

U_NAMESPACE_END

#endif  /* U_HIDE_DRAFT_API */
#endif
#endif
