/*
*******************************************************************************
*
*   Copyright (C) 2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformatter.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#include "unicode/listformatter.h"
#include "mutex.h"
#include "hash.h"
#include "cstring.h"
#include "ulocimp.h"
#include "charstr.h"
#include "ucln_cmn.h"
#include "uresimp.h"

U_NAMESPACE_BEGIN

static Hashtable* listPatternHash = NULL;
static UMutex listFormatterMutex = U_MUTEX_INITIALIZER;
static UChar FIRST_PARAMETER[] = { 0x7b, 0x30, 0x7d };  // "{0}"
static UChar SECOND_PARAMETER[] = { 0x7b, 0x31, 0x7d };  // "{0}"

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_listformatter_cleanup() {
    delete listPatternHash;
    listPatternHash = NULL;
    return TRUE;
}

static void U_CALLCONV
uprv_deleteListFormatData(void *obj) {
    delete static_cast<ListFormatData *>(obj);
}

U_CDECL_END

static ListFormatData* loadListFormatData(const Locale& locale, UErrorCode& errorCode);
static void getStringByKey(const UResourceBundle* rb, const char* key, UnicodeString* result, UErrorCode& errorCode);

void ListFormatter::initializeHash(UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }

    listPatternHash = new Hashtable();
    if (listPatternHash == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    listPatternHash->setValueDeleter(uprv_deleteListFormatData);
    ucln_common_registerCleanup(UCLN_COMMON_LIST_FORMATTER, uprv_listformatter_cleanup);

}

void ListFormatter::addDataToHash(
    const char* locale,
    const char* two,
    const char* start,
    const char* middle,
    const char* end,
    UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    UnicodeString key(locale, -1, US_INV);
    ListFormatData* value = new ListFormatData(
        UnicodeString(two, -1, US_INV).unescape(),
        UnicodeString(start, -1, US_INV).unescape(),
        UnicodeString(middle, -1, US_INV).unescape(),
        UnicodeString(end, -1, US_INV).unescape());

    if (value == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    listPatternHash->put(key, value, errorCode);
}

const ListFormatData* ListFormatter::getListFormatData(
        const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    UnicodeString key(locale.getName(), -1, US_INV);
    ListFormatData* result = NULL;
    {
        Mutex m(&listFormatterMutex);
        if (listPatternHash == NULL) {
            initializeHash(errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
        result = static_cast<ListFormatData*>(listPatternHash->get(key));
    }
    if (result) {
        return result;
    }
    result = loadListFormatData(locale, errorCode);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }

    {
        Mutex m(&listFormatterMutex);
        ListFormatData* temp = static_cast<ListFormatData*>(listPatternHash->get(key));
        if (temp) {
            delete result;
            result = temp;
        } else {
            listPatternHash->put(key, result, errorCode);
            if (U_FAILURE(errorCode)) {
                return NULL;
            }
        }
    }
    return result;
}

#define ADD(locale, two, start, mid, end, err) if (uprv_strcmp(locale, name) == 0) return new ListFormatData(UnicodeString(two, -1, US_INV).unescape(), UnicodeString(start, -1, US_INV).unescape(), UnicodeString(mid, -1, US_INV).unescape(), UnicodeString(end, -1, US_INV).unescape())

static ListFormatData* loadListFormatData(const Locale& locale, UErrorCode& errorCode) {
    UResourceBundle* rb = ures_open(NULL, locale.getName(), &errorCode);
    if (U_FAILURE(errorCode)) {
        ures_close(rb);
        return NULL;
    }
    rb = ures_getByKeyWithFallback(rb, "listPattern", rb, &errorCode);
    rb = ures_getByKeyWithFallback(rb, "standard", rb, &errorCode);
    if (U_FAILURE(errorCode)) {
        ures_close(rb);
        return NULL;
    }
    UnicodeString two, start, middle, end;
    getStringByKey(rb, "2", &two, errorCode);
    getStringByKey(rb, "start", &start, errorCode);
    getStringByKey(rb, "middle", &middle, errorCode);
    getStringByKey(rb, "end", &end, errorCode);
    ures_close(rb);
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    ListFormatData* result = new ListFormatData(two, start, middle, end);
    if (result == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return result;
}

static void getStringByKey(const UResourceBundle* rb, const char* key, UnicodeString* result, UErrorCode& errorCode) {
    int32_t len;
    const UChar* ustr = ures_getStringByKeyWithFallback(rb, key, &len, &errorCode);
    if (U_FAILURE(errorCode)) {
      return;
    }
    result->setTo(ustr, len);
}

ListFormatter* ListFormatter::createInstance(UErrorCode& errorCode) {
    Locale locale;  // The default locale.
    return createInstance(locale, errorCode);
}

ListFormatter* ListFormatter::createInstance(const Locale& locale, UErrorCode& errorCode) {
    Locale tempLocale = locale;
    for (;;) {
        const ListFormatData* listFormatData = getListFormatData(tempLocale, errorCode);
        if (U_FAILURE(errorCode)) {
            return NULL;
        }
        if (listFormatData != NULL) {
            ListFormatter* p = new ListFormatter(*listFormatData);
            if (p == NULL) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }
            return p;
        }
        errorCode = U_ZERO_ERROR;
        Locale correctLocale;
        getFallbackLocale(tempLocale, correctLocale, errorCode);
        if (U_FAILURE(errorCode)) {
            return NULL;
        }
        if (correctLocale.isBogus()) {
            return createInstance(Locale::getRoot(), errorCode);
        }
        tempLocale = correctLocale;
    }
}

ListFormatter::ListFormatter(const ListFormatData& listFormatterData) : data(listFormatterData) {
}

ListFormatter::~ListFormatter() {}

void ListFormatter::getFallbackLocale(const Locale& in, Locale& out, UErrorCode& errorCode) {
    if (uprv_strcmp(in.getName(), "zh_TW") == 0) {
        out = Locale::getTraditionalChinese();
    } else {
        const char* localeString = in.getName();
        const char* extStart = locale_getKeywordsStart(localeString);
        if (extStart == NULL) {
            extStart = uprv_strchr(localeString, 0);
        }
        const char* last = extStart;

        // TODO: Check whether uloc_getParent() will work here.
        while (last > localeString && *(last - 1) != '_') {
            --last;
        }

        // Truncate empty segment.
        while (last > localeString) {
            if (*(last-1) != '_') {
                break;
            }
            --last;
        }

        size_t localePortionLen = last - localeString;
        CharString fullLocale;
        fullLocale.append(localeString, localePortionLen, errorCode).append(extStart, errorCode);

        if (U_FAILURE(errorCode)) {
            return;
        }
        out = Locale(fullLocale.data());
    }
}

UnicodeString& ListFormatter::format(const UnicodeString items[], int32_t nItems,
                      UnicodeString& appendTo, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return appendTo;
    }

    if (nItems > 0) {
        UnicodeString newString = items[0];
        if (nItems == 2) {
            addNewString(data.twoPattern, newString, items[1], errorCode);
        } else if (nItems > 2) {
            addNewString(data.startPattern, newString, items[1], errorCode);
            int i;
            for (i = 2; i < nItems - 1; ++i) {
                addNewString(data.middlePattern, newString, items[i], errorCode);
            }
            addNewString(data.endPattern, newString, items[nItems - 1], errorCode);
        }
        if (U_SUCCESS(errorCode)) {
            appendTo += newString;
        }
    }
    return appendTo;
}

/**
 * Joins originalString and nextString using the pattern pat and puts the result in
 * originalString.
 */
void ListFormatter::addNewString(const UnicodeString& pat, UnicodeString& originalString,
                                 const UnicodeString& nextString, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return;
    }

    int32_t p0Offset = pat.indexOf(FIRST_PARAMETER, 3, 0);
    if (p0Offset < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int32_t p1Offset = pat.indexOf(SECOND_PARAMETER, 3, 0);
    if (p1Offset < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t i, j;

    const UnicodeString* firstString;
    const UnicodeString* secondString;
    if (p0Offset < p1Offset) {
        i = p0Offset;
        j = p1Offset;
        firstString = &originalString;
        secondString = &nextString;
    } else {
        i = p1Offset;
        j = p0Offset;
        firstString = &nextString;
        secondString = &originalString;
    }

    UnicodeString result = UnicodeString(pat, 0, i) + *firstString;
    result += UnicodeString(pat, i+3, j-i-3);
    result += *secondString;
    result += UnicodeString(pat, j+3);
    originalString = result;
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(ListFormatter)

U_NAMESPACE_END
