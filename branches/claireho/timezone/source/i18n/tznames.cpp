/*
*******************************************************************************
* Copyright (C) 2011, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/locid.h"
#include "unicode/uenum.h"
#include "tznames.h"

#if !UCONFIG_NO_FORMATTING
U_NAMESPACE_BEGIN

TimeZoneNames::TimeZoneNames() {
}

TimeZoneNames::~TimeZoneNames() {
}

TimeZoneNames*
TimeZoneNames::createInstance(const Locale& locale, UErrorCode& status) {
    //TODO
    return NULL;
}

UnicodeString&
TimeZoneNames::getZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UDate date, UnicodeString& name) const {
    //TODO
    return name;
}

class TimeZoneNamesImpl : public TimeZoneNames {
public:
    TimeZoneNamesImpl(const Locale& locale);
    virtual ~TimeZoneNamesImpl();

    virtual StringEnumeration* getAvailableMetaZoneIDs() const;
    virtual StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID) const;
    virtual UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const;
    virtual UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const;

    virtual UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const;
    virtual UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const;
    virtual UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

    virtual UEnumeration* find(const UnicodeString& text, int32_t start, int32_t types) const;
private:
    Locale fLocale;
};

TimeZoneNamesImpl::TimeZoneNamesImpl(const Locale& locale)
:   fLocale(locale) {
    //TODO
}

TimeZoneNamesImpl::~TimeZoneNamesImpl() {
    //TODO
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs() const {
    //TODO
    return NULL;
}

StringEnumeration*
TimeZoneNamesImpl::getAvailableMetaZoneIDs(const UnicodeString& tzID) const {
    //TODO
    return NULL;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    //TODO
    return mzID;
}

UnicodeString&
TimeZoneNamesImpl::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    //TODO
    return tzID;
}

UnicodeString&
TimeZoneNamesImpl::getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const {
    //TODO
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    //TODO
    return name;
}

UnicodeString&
TimeZoneNamesImpl::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    //TODO
    return name;
}

UEnumeration*
TimeZoneNamesImpl::find(const UnicodeString& text, int32_t start, int32_t types) const {
    return NULL;
}

class TimeZoneNamesDelegate : public TimeZoneNames {
public:
    TimeZoneNamesDelegate(const Locale& locale);
    virtual ~TimeZoneNamesDelegate();

    virtual StringEnumeration* getAvailableMetaZoneIDs() const;
    virtual StringEnumeration* getAvailableMetaZoneIDs(const UnicodeString& tzID) const;
    virtual UnicodeString& getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const;
    virtual UnicodeString& getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const;

    virtual UnicodeString& getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const;
    virtual UnicodeString& getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const;
    virtual UnicodeString& getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const;

    virtual UEnumeration* find(const UnicodeString& text, int32_t start, int32_t types) const;
private:
    TimeZoneNamesImpl* fTZNSImpl;
};

TimeZoneNamesDelegate::TimeZoneNamesDelegate(const Locale& locale) {
    //TODO
}

TimeZoneNamesDelegate::~TimeZoneNamesDelegate() {
    //TODO
}

StringEnumeration*
TimeZoneNamesDelegate::getAvailableMetaZoneIDs() const {
    return fTZNSImpl->getAvailableMetaZoneIDs();
}

StringEnumeration*
TimeZoneNamesDelegate::getAvailableMetaZoneIDs(const UnicodeString& tzID) const {
    return fTZNSImpl->getAvailableMetaZoneIDs(tzID);
}

UnicodeString&
TimeZoneNamesDelegate::getMetaZoneID(const UnicodeString& tzID, UDate date, UnicodeString& mzID) const {
    return fTZNSImpl->getMetaZoneID(tzID, date, mzID);
}

UnicodeString&
TimeZoneNamesDelegate::getReferenceZoneID(const UnicodeString& mzID, const char* region, UnicodeString& tzID) const {
    return fTZNSImpl->getReferenceZoneID(mzID, region, tzID);
}

UnicodeString&
TimeZoneNamesDelegate::getMetaZoneDisplayName(const UnicodeString& mzID, UTimeZoneNameType type, UnicodeString& name) const {
    return fTZNSImpl->getMetaZoneDisplayName(mzID, type, name);
}

UnicodeString&
TimeZoneNamesDelegate::getTimeZoneDisplayName(const UnicodeString& tzID, UTimeZoneNameType type, UnicodeString& name) const {
    return fTZNSImpl->getTimeZoneDisplayName(tzID, type, name);
}

UnicodeString&
TimeZoneNamesDelegate::getExemplarLocationName(const UnicodeString& tzID, UnicodeString& name) const {
    return fTZNSImpl->getExemplarLocationName(tzID, name);
}

UEnumeration*
TimeZoneNamesDelegate::find(const UnicodeString& text, int32_t start, int32_t types) const {
    return fTZNSImpl->find(text, start, types);
}

U_NAMESPACE_END
#endif
