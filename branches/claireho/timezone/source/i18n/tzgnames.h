/*
*******************************************************************************
* Copyright (C) 2011, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef __TZGNAMES_H
#define __TZGNAMES_H

/**
 * \file 
 * \brief C API: Time zone generic names classe
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/locid.h"
#include "unicode/timezone.h"
#include "unicode/unistr.h"
#include "tznames.h"
#include "tznames_impl.h"
#include "uhash.h"
#include "umutex.h"

U_CDECL_BEGIN

typedef enum UTimeZoneGenericNameType {
    UTTGNM_UNKNOWN      = 0x00,
    UTZGNM_LOCATION     = 0x01,
    UTZGNM_LONG         = 0x02,
    UTZGNM_SHORT        = 0x04,
} UTimeZoneGenericNameType;

U_CDECL_END

U_NAMESPACE_BEGIN

class LocaleDisplayNames;
class MessageFormat;
class TimeZone;

class U_I18N_API TimeZoneGenericNames : public UMemory {
public:
    TimeZoneGenericNames(const Locale& locale, UErrorCode& status);
    virtual ~TimeZoneGenericNames();

    UnicodeString& getDisplayName(const TimeZone& tz, UTimeZoneGenericNameType type,
                        UDate date, UnicodeString& name) const;

    UnicodeString& getGenericLocationName(const UnicodeString& tzID, UnicodeString& name) const;

private:
    Locale fLocale;
    char fTargetRegion[ULOC_COUNTRY_CAPACITY];
    const TimeZoneNames* fTimeZoneNames;
    LocaleDisplayNames* fLocaleDisplayNames;

    UMTX fLock;

    MessageFormat* fRegionFormat;
    MessageFormat* fFallbackRegionFormat;
    MessageFormat* fFallbackFormat;

    UHashtable* fLocationNamesMap;
    UHashtable* fPartialLocationNamesMap;

    ZSFStringPool fStringPool;

    TextTrieMap fGNamesTrie;
    UBool fGNamesTrieFullyLoaded;

    void initialize(const Locale& locale, UErrorCode& status);
    void cleanup();

    UnicodeString& formatGenericNonLocationName(const TimeZone& tz, UTimeZoneGenericNameType type,
                        UDate date, UnicodeString& name) const;

    UnicodeString& getPartialLocationName(const UnicodeString& tzCanonicalID,
                        const UnicodeString& mzID, UBool isLong, const UnicodeString& mzDisplayName,
                        UnicodeString& name) const;
};

U_NAMESPACE_END
#endif
#endif
