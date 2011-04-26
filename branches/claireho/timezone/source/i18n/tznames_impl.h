/*
 *******************************************************************************
 * Copyright (C) 2011, International Business Machines Corporation and         *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 */

#ifndef __TZNAMES_IMPL_H__
#define __TZNAMES_IMPL_H__


/**
 * \file
 * \brief C++ API: TimeZoneNames object
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "tznames.h"
#include "uhash.h"
#include "zstrfmt.h"
#include "unicode/uobject.h"
#include "unicode/ures.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

typedef struct UMzMapEntry {
    UnicodeString mzID;
    int32_t from;
    int32_t to;
} UMzMapEntry;


/**
 * This class stores name data for a meta zone.
 */
class ZNames : public UMemory {
public:
    virtual ~ZNames();

    static ZNames* createInstance(UResourceBundle* rb, const char* key);
    const UChar* getName(UTimeZoneNameType type);

protected:
    ZNames(const UChar** names, UBool shortCommonlyUsed);
    static const UChar** loadData(UResourceBundle* rb, const char* key, UBool& shortCommonlyUsed);

private:
    const UChar** fNames;
    UBool fShortCommonlyUsed;
};

/**
 * This class stores name data for single timezone.
 */
class TZNames : public ZNames {
public:
    virtual ~TZNames();

    static TZNames* createInstance(UResourceBundle* rb, const char* key);
    const UChar* getLocationName(void);

private:
    TZNames(const UChar** names, UBool shortCommonlyUsed, const UChar* locationName);
    const UChar* fLocationName;
};

/**
 * The default implementation of <code>TimeZoneNames</code> used by {@link TimeZoneNames#getInstance(ULocale)} when
 * the ICU4J tznamedata component is not available.
 */
class TimeZoneNamesImpl : public TimeZoneNames {
public:
    TimeZoneNamesImpl(const Locale& locale, UErrorCode& status);

    virtual ~TimeZoneNamesImpl();

    StringEnumeration* getAvailableMetaZoneIDs(UErrorCode& status) const;
    StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID, UErrorCode& status) const;

    UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const;
    UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const;

    UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const;
    UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const;

    UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

private:
    void initialize(const Locale& locale, UErrorCode& status);
    void cleanup();

    ZNames* loadMetaZoneNames(const UnicodeString& mzId) const;
    TZNames* loadTimeZoneNames(const UnicodeString& mzId) const;

    Locale fLocale;
    UResourceBundle* fZoneStrings;

    UHashtable* fMZNamesMap;
    UHashtable* fTZNamesMap;
};

class MetaZoneIDsEnumeration : public StringEnumeration {
public:
    MetaZoneIDsEnumeration();
    MetaZoneIDsEnumeration(const UVector& mzIDs);
    MetaZoneIDsEnumeration(UVector* mzIDs);
    virtual ~MetaZoneIDsEnumeration();
    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
    virtual const UnicodeString* snext(UErrorCode& status);
    virtual void reset(UErrorCode& status);
    virtual int32_t count(UErrorCode& status) const;
private:
    int32_t fLen;
    int32_t fPos;
    const UVector* fMetaZoneIDs;
    UVector *fLocalVector;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // __TZNAMES_IMPL_H__
//eof
//
