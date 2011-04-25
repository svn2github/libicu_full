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

static const char* KEYS[] = {"lg", "ls", "ld", "sg", "ss", "sd"};
// static const int32_t KEYS_SIZE = 6;    // # of elements in KEYS array.
static const int32_t KEYS_SIZE = (sizeof KEYS / sizeof KEYS[0]);

typedef struct UMzMapEntry {
    UnicodeString mzID;
    int32_t from;
    int32_t to;
}UMzMapEntry;

/**
 * The default implementation of <code>TimeZoneNames</code> used by {@link TimeZoneNames#getInstance(ULocale)} when
 * the ICU4J tznamedata component is not available.
 */
class TimeZoneNamesImpl : public TimeZoneNames {
public:
    TimeZoneNamesImpl(const Locale& locale, UErrorCode& status);
    // TimeZoneNames*  createInstance(const Locale& locale, UErrorCode& status);
    virtual ~TimeZoneNamesImpl();

    StringEnumeration* getAvailableMetaZoneIDs() const;
    StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID) const;
    UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const;
    UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const;

    UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const;
    UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const;

    UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

    UEnumeration* find(const UnicodeString& text, int32_t start, int32_t types) const;

private:


    void initialize(const Locale& locale, UErrorCode& status);

    /*
      private void writeObject(ObjectOutputStream out);
      private void readObject(ObjectInputStream in);
      private synchronized ZNames loadMetaZoneNames(String mzID);
      private synchronized TZNames loadTimeZoneNames(String tzID)
      private static class NameInfo ... could be a structure
      private static class NameSearchHandle ...
      private static class ZNames ...
      private static class TZNames extends ZNames ...
      private static class MZMapEntry ...
      private static class TZ2MZsCache extends SoftCache<String, List<MZMapEntry>, String> ....
      private static class MZ2TZsCache extends SoftCache<String, Map<String, String>, String> ...

     */

    Locale fLocale;
    UResourceBundle* fZoneStrings;
    UHashtable* fTzNamesMap;
    StringEnumeration* fMetaZoneIds;
    TextTrieMap fNamesTrie;
    UBool fNamesTrieFullyLoaded;
};

/**
 * This class stores name data for a meta zone.
 */
class ZNames : public UMemory {
public:
    // TODO(claireho) Should be static?
    ZNames* getInstance(UResourceBundle* rb, const char* key);
    UnicodeString* getName(UTimeZoneNameType type);

protected:
    // TODO(claireho) Use StringEnumeration or UnicodeString[] ?
    ZNames(UnicodeString* names, UBool shortCommonlyUsed);
    ~ZNames();
    void loadData(UResourceBundle* rb, const char* key, UBool* shortCommonlyUsed,
                  UErrorCode& status);

    // TODO(claireho) should we make fName to private?
    UnicodeString fNames[KEYS_SIZE];

private:
    void cleanDataBuffer(void);
    UBool  fShortCommonlyUsed;
};

/**
 * This class stores name data for single timezone.
 */
class TZNames : ZNames {
public:
    TZNames* getInstance(UResourceBundle* rb, const char* key, UErrorCode& status);
    UnicodeString& getLocationName(void);

private:
    TZNames(UnicodeString names[], UBool shortCommonlyUsed, const UnicodeString& locationName);
    ~TZNames() { };
    UnicodeString fLocationName;

};


class MetaZoneIdsEnumeration : public StringEnumeration {
public:
    MetaZoneIdsEnumeration(UErrorCode& status);
    virtual ~MetaZoneIdsEnumeration();
    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
    virtual const UnicodeString* snext(UErrorCode& status);
    virtual void reset(UErrorCode& status);
    virtual int32_t count(UErrorCode& status) const;
private:
    int32_t pos;
    UVector fMetaZoneIds;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // __TZNAMES_IMPL_H__
//eof
//
