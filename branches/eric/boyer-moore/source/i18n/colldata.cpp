/*
 ******************************************************************************
 *   Copyright (C) 1996-2008, International Business Machines                 *
 *   Corporation and others.  All Rights Reserved.                            *
 ******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/unistr.h"
#include "unicode/putil.h"
#include "unicode/usearch.h"

#include "cmemory.h"
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/coleitr.h"
#include "unicode/ucoleitr.h"

#include "unicode/regex.h"        // TODO: make conditional on regexp being built.

#include "unicode/uniset.h"
#include "unicode/uset.h"
#include "unicode/ustring.h"
#include "hash.h"
#include "uhash.h"
#include "ucol_imp.h"

#include "unicode/colldata.h"

U_NAMESPACE_BEGIN

#define NEW_ARRAY(type, count) (type *) uprv_malloc((count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *) (array))

static inline USet *uset_openEmpty()
{
    return uset_open(1, 0);
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CEList)

CEList::CEList(UCollator *coll, const UnicodeString &string)
    : ces(NULL), listMax(8), listSize(0)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollationElements *elems = ucol_openElements(coll, string.getBuffer(), string.length(), &status);
    uint32_t strengthMask = 0;
    int32_t order;

    switch (ucol_getStrength(coll)) 
    {
    default:
        strengthMask |= UCOL_TERTIARYORDERMASK;
        /* fall through */

    case UCOL_SECONDARY:
        strengthMask |= UCOL_SECONDARYORDERMASK;
        /* fall through */

    case UCOL_PRIMARY:
        strengthMask |= UCOL_PRIMARYORDERMASK;
    }

    ces = NEW_ARRAY(int32_t, listMax);

    while ((order = ucol_next(elems, &status)) != UCOL_NULLORDER) {
        order &= strengthMask;

        if (order == UCOL_IGNORABLE) {
            continue;
        }

        add(order);
    }

    ucol_closeElements(elems);
}

CEList::~CEList()
{
    DELETE_ARRAY(ces);
}

void CEList::add(int32_t ce)
{
    if (listSize >= listMax) {
        listMax *= 2;

        int32_t *newCEs = NEW_ARRAY(int32_t, listMax);

        uprv_memcpy(newCEs, ces, listSize * sizeof(int32_t));
        DELETE_ARRAY(ces);
        ces = newCEs;
    }

    ces[listSize++] = ce;
}

int32_t CEList::get(int32_t index) const
{
    if (index >= 0 && index < listSize) {
        return ces[index];
    }

    return -1;
}

int32_t &CEList::operator[](int32_t index) const
{
    return ces[index];
}

UBool CEList::matchesAt(int32_t offset, const CEList *other) const
{
    if (listSize - offset < other->size()) {
        return FALSE;
    }

    for (int32_t i = offset, j = 0; j < other->size(); i += 1, j += 1) {
        if (ces[i] != other->get(j)) {
            return FALSE;
        }
    }

    return TRUE;
}

int32_t CEList::size() const
{
    return listSize;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(StringList)

StringList::StringList()
    : strings(NULL), listMax(16), listSize(0)
{
    strings = new UnicodeString [listMax];
}

StringList::~StringList()
{
    delete[] strings;
}

void StringList::add(const UnicodeString *string)
{
    if (listSize >= listMax) {
        listMax *= 2;

        UnicodeString *newStrings = new UnicodeString[listMax];

        uprv_memcpy(newStrings, strings, listSize * sizeof(UnicodeString));

        delete[] strings;
        strings = newStrings;
    }

    // The ctor initialized all the strings in
    // the array to empty strings, so this
    // is the same as copying the source string.
    strings[listSize++].append(*string);
}

void StringList::add(const UChar *chars, int32_t count)
{
    const UnicodeString string(chars, count);

    add(&string);
}

const UnicodeString *StringList::get(int32_t index) const
{
    if (index >= 0 && index < listSize) {
        return &strings[index];
    }

    return NULL;
}

int32_t StringList::size() const
{
    return listSize;
}


U_CFUNC void deleteStringList(void *obj);

class CEToStringsMap : public UMemory
{
public:

    CEToStringsMap();
    ~CEToStringsMap();

    void put(int32_t ce, UnicodeString *string);
    StringList *getStringList(int32_t ce) const;

private:
 
    void putStringList(int32_t ce, StringList *stringList);
    UHashtable *map;
};

CEToStringsMap::CEToStringsMap()
{
    UErrorCode status = U_ZERO_ERROR;

    map = uhash_open(uhash_hashLong, uhash_compareLong,
                     uhash_compareCaselessUnicodeString,
                     &status);

    uhash_setValueDeleter(map, deleteStringList);
}

CEToStringsMap::~CEToStringsMap()
{
    uhash_close(map);
}

void CEToStringsMap::put(int32_t ce, UnicodeString *string)
{
    StringList *strings = getStringList(ce);

    if (strings == NULL) {
        strings = new StringList();
        putStringList(ce, strings);
    }

    strings->add(string);
}

StringList *CEToStringsMap::getStringList(int32_t ce) const
{
    return (StringList *) uhash_iget(map, ce);
}

void CEToStringsMap::putStringList(int32_t ce, StringList *stringList)
{
    UErrorCode status = U_ZERO_ERROR;

    uhash_iput(map, ce, (void *) stringList, &status);
}

U_CFUNC void deleteStringList(void *obj)
{
    StringList *strings = (StringList *) obj;

    delete strings;
}

U_CFUNC void deleteCEList(void *obj);
U_CFUNC void deleteUnicodeStringKey(void *obj);

class StringToCEsMap : public UMemory
{
public:
    StringToCEsMap();
    ~StringToCEsMap();

    void put(const UnicodeString *string, const CEList *ces);
    const CEList *get(const UnicodeString *string);

private:


    UHashtable *map;
};

StringToCEsMap::StringToCEsMap()
{
    UErrorCode status = U_ZERO_ERROR;

    map = uhash_open(uhash_hashUnicodeString,
                     uhash_compareUnicodeString,
                     uhash_compareLong,
                     &status);

    uhash_setValueDeleter(map, deleteCEList);
    uhash_setKeyDeleter(map, deleteUnicodeStringKey);
}

StringToCEsMap::~StringToCEsMap()
{
    uhash_close(map);
}

void StringToCEsMap::put(const UnicodeString *string, const CEList *ces)
{
    UErrorCode status = U_ZERO_ERROR;

    uhash_put(map, (void *) string, (void *) ces, &status);
}

const CEList *StringToCEsMap::get(const UnicodeString *string)
{
    return (const CEList *) uhash_get(map, string);
}

U_CFUNC void deleteCEList(void *obj)
{
    CEList *list = (CEList *) obj;

    delete list;
}

U_CFUNC void deleteUnicodeStringKey(void *obj)
{
    UnicodeString *key = (UnicodeString *) obj;

    delete key;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CollData)

CollData::CollData(UCollator *collator)
  : coll(collator)
{
    UErrorCode status = U_ZERO_ERROR;
    U_STRING_DECL(test_pattern, "[[:assigned:]-[:ideographic:]-[:hangul:]-[:c:]]", 47);
    U_STRING_INIT(test_pattern, "[[:assigned:]-[:ideographic:]-[:hangul:]-[:c:]]", 47);
    USet *charsToTest  = uset_openPattern(test_pattern, 47, &status);
    USet *expansions   = uset_openEmpty();
    USet *contractions = uset_openEmpty();
    int32_t itemCount;

    charsToCEList = new StringToCEsMap();
    ceToCharsStartingWith = new CEToStringsMap();

    ucol_getContractionsAndExpansions(coll, contractions, expansions, FALSE, &status);

    uset_addAll(charsToTest, contractions);
    uset_addAll(charsToTest, expansions);

    itemCount = uset_getItemCount(charsToTest);
    for(int32_t item = 0; item < itemCount; item += 1) {
        UChar32 start = 0, end = 0;
        UChar buffer[16];
        int32_t len = uset_getItem(charsToTest, item, &start, &end,
                                   buffer, 16, &status);

        if (len == 0) {
            for (UChar32 ch = start; ch <= end; ch += 1) {
                UnicodeString *st = new UnicodeString(ch);
                CEList *ceList = new CEList(coll, *st);

                charsToCEList->put(st, ceList);
                ceToCharsStartingWith->put(ceList->get(0), st);
            }
        } else if (len > 0) {
            UnicodeString *st = new UnicodeString(buffer, len);
            CEList *ceList = new CEList(coll, *st);

            charsToCEList->put(st, ceList);
            ceToCharsStartingWith->put(ceList->get(0), st);
        } else {
            // shouldn't happen...
        }
    }

    uset_close(contractions);
    uset_close(expansions);
    uset_close(charsToTest);
}

CollData::~CollData()
{
   delete ceToCharsStartingWith;
   delete charsToCEList;
}

UCollator *CollData::getCollator() const
{
    return coll;
}

const StringList *CollData::getStringList(int32_t ce) const
{
    return ceToCharsStartingWith->getStringList(ce);
}

const CEList *CollData::getCEList(const UnicodeString *string) const
{
    return charsToCEList->get(string);
}

int32_t CollData::minLengthInChars(const CEList *ceList, int32_t offset, int32_t *history) const
{
    // find out shortest string for the longest sequence of ces.
    // this can probably be folded with the minLengthCache...

    if (history[offset] >= 0) {
        return history[offset];
    }

    int32_t ce = ceList->get(offset);
    int32_t maxOffset = ceList->size();
    int32_t shortestLength = INT32_MAX;
    const StringList *strings = ceToCharsStartingWith->getStringList(ce);

    if (strings != NULL) {
        int32_t stringCount = strings->size();
      
        for (int32_t s = 0; s < stringCount; s += 1) {
            const UnicodeString *string = strings->get(s);
            const CEList *ceList2 = charsToCEList->get(string);

            if (ceList->matchesAt(offset, ceList2)) {
                int32_t clength = ceList2->size();
                int32_t slength = string->length();
                int32_t roffset = offset + clength;
                int32_t rlength = 0;
                
                if (roffset < maxOffset) {
                    rlength = minLengthInChars(ceList, roffset, history);

                    if (rlength <= 0) {
                        // ignore any dead ends
                        continue;
                    }
                }

                if (shortestLength > slength + rlength) {
                    shortestLength = slength + rlength;
                }
            }
        }
    }

    if (shortestLength == INT32_MAX) {
        // no matching strings at this offset - just move on.
      //shortestLength = offset < (maxOffset - 1)? minLengthInChars(ceList, offset + 1, history) : 0;
        return -1;
    }

    history[offset] = shortestLength;

    return shortestLength;
}

int32_t CollData::minLengthInChars(const CEList *ceList, int32_t offset) const
{
    int32_t clength = ceList->size();
    int32_t *history = NEW_ARRAY(int32_t, clength);

    for (int32_t i = 0; i < clength; i += 1) {
        history[i] = -1;
    }

    int32_t minLength = minLengthInChars(ceList, offset, history);

    DELETE_ARRAY(history);

    return minLength;
}

CollData *CollData::open(UCollator *collator)
{
    return new CollData(collator);
}

void CollData::close(CollData *collData)
{
    delete collData;
}

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_COLLATION
