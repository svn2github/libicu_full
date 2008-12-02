/*
 ******************************************************************************
 *   Copyright (C) 1996-2008, International Business Machines                 *
 *   Corporation and others.  All Rights Reserved.                            *
 ******************************************************************************
 */

/**
 * \file 
 * \brief C++ API: Collation data used to compute minLengthInChars.
 */
 
#ifndef COLL_DATA_H
#define COLL_DATA_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uobject.h"
#include "unicode/ucol.h"

U_NAMESPACE_BEGIN

class U_I18N_API CEList : public UObject
{
public:
    CEList(UCollator *coll, const UnicodeString &string);
    ~CEList();

    int32_t size() const;
    int32_t get(int32_t index) const;
    UBool matchesAt(int32_t offset, const CEList *other) const; 

    int32_t &operator[](int32_t index) const;

    virtual UClassID getDynamicClassID() const;
    static UClassID getStaticClassID();

private:
    void add(int32_t ce);

    int32_t *ces;
    int32_t listMax;
    int32_t listSize;
};

class U_I18N_API StringList : public UObject
{
public:
    StringList();
    ~StringList();

    void add(const UnicodeString *string);
    void add(const UChar *chars, int32_t count);
    const UnicodeString *get(int32_t index) const;
    int32_t size() const;

    virtual UClassID getDynamicClassID() const;
    static UClassID getStaticClassID();

private:
    UnicodeString *strings;
    int32_t listMax;
    int32_t listSize;
};

class StringToCEsMap;
class CEToStringsMap;

class U_I18N_API CollData : public UObject
{
public:
    CollData(UCollator *collator);
    ~CollData();

    UCollator *getCollator() const;

    const StringList *getStringList(int32_t ce) const;

    const CEList *getCEList(const UnicodeString *string) const;

    int32_t minLengthInChars(const CEList *ces, int32_t offset) const;

    int32_t minLengthInChars(const CEList *ces, int32_t offset, int32_t *history) const;

    static CollData *open(UCollator *collator);
    static void close(CollData *collData);

    virtual UClassID getDynamicClassID() const;
    static UClassID getStaticClassID();

private:
    UCollator      *coll;
    StringToCEsMap *charsToCEList;
    CEToStringsMap *ceToCharsStartingWith;
};

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_COLLATION
#endif // #ifndef COLL_DATA_H
