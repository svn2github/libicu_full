/*
**********************************************************************
* Copyright (c) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include <string.h>
#include "unicode/uperf.h"
#include "unicode/ucol.h"
#include "unicode/coll.h"
#include "unicode/uiter.h"
#include "unicode/sortkey.h"
#include "uoptions.h"

#define COMPATCT_ARRAY(CompactArrays, UNIT) \
struct CompactArrays{\
    CompactArrays(const CompactArrays & );\
    CompactArrays & operator=(const CompactArrays & );\
    int32_t   count;/*total number of the strings*/ \
    int32_t * index;/*relative offset in data*/ \
    UNIT    * data; /*the real space to hold strings*/ \
    \
    ~CompactArrays(){free(index);free(data);} \
    CompactArrays():data(NULL), index(NULL), count(0){ \
    index = (int32_t *) realloc(index, sizeof(int32_t)); \
    index[0] = 0; \
    } \
    void append_one(int32_t theLen){ /*include terminal NULL*/ \
    count++; \
    index = (int32_t *) realloc(index, sizeof(int32_t) * (count + 1)); \
    index[count] = index[count - 1] + theLen; \
    data = (UNIT *) realloc(data, sizeof(UNIT) * index[count]); \
    } \
    UNIT * last(){return data + index[count - 1];} \
    UNIT * dataOf(int32_t i){return data + index[i];} \
    int32_t lengthOf(int i){return index[i+1] - index[i] - 1; } /*exclude terminating NULL*/  \
};

COMPATCT_ARRAY(CA_uchar, UChar)
COMPATCT_ARRAY(CA_char, char)

// C API test cases
class Strcoll : public UPerfFunction
{
public:
    Strcoll(const UCollator* coll, CA_uchar* source, CA_uchar* target, UBool useLen, UBool roundRobin);
    ~Strcoll();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    CA_uchar *source;
    CA_uchar *target;
    UBool useLen;
    UBool roundRobin;
};

Strcoll::Strcoll(const UCollator* coll, CA_uchar* source, CA_uchar* target, UBool useLen, UBool roundRobin)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen),
        roundRobin(roundRobin)
{
}

Strcoll::~Strcoll()
{
}

void Strcoll::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    int32_t srcLen, tgtLen;
    if (roundRobin) {
        // call strcoll for all permutations
        int32_t cmp = 0;
        for (int32_t i = 0; i < source->count; i++) {
            srcLen = useLen ? source->lengthOf(i) : -1;
            for (int32_t j = 0; j < target->count; j++) {
                tgtLen = useLen ? target->lengthOf(j) : -1;
                cmp += ucol_strcoll(coll, source->dataOf(i), srcLen, target->dataOf(j), tgtLen);
            }
        }
        // At the end, cmp must be 0
        if (cmp != 0) {
            *status = U_INTERNAL_PROGRAM_ERROR;
        }
    } else {
        // call strcoll for two strings at the same index
        if (source->count < target->count) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        } else {
            for (int32_t i = 0; i < source->count; i++) {
                srcLen = useLen ? source->lengthOf(i) : -1;
                tgtLen = useLen ? target->lengthOf(i) : -1;
                ucol_strcoll(coll, source->dataOf(i), srcLen, target->dataOf(i), tgtLen);
            }
        }
    }
}

long Strcoll::getOperationsPerIteration()
{
    long numOps = roundRobin? (source->count) * (target->count) : source->count;
    return numOps > 0 ? numOps : 1;
}

class StrcollUTF8 : public UPerfFunction
{
public:
    StrcollUTF8(const UCollator* coll, CA_char* source, CA_char* target, UBool useLen, UBool roundRobin);
    ~StrcollUTF8();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    CA_char *source;
    CA_char *target;
    UBool useLen;
    UBool roundRobin;
};

StrcollUTF8::StrcollUTF8(const UCollator* coll, CA_char* source, CA_char* target, UBool useLen, UBool roundRobin)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen),
        roundRobin(roundRobin)
{
}

StrcollUTF8::~StrcollUTF8()
{
}

void StrcollUTF8::call(UErrorCode* status)
{
    int32_t srcLen, tgtLen;
    if (roundRobin) {
        // call strcollUTF8 for all permutations
        int32_t cmp = 0;
        for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
            srcLen = useLen ? source->lengthOf(i) : -1;
            for (int32_t j = 0; j < target->count; j++) {
                tgtLen = useLen ? target->lengthOf(j) : -1;
                cmp += ucol_strcollUTF8(coll, source->dataOf(i), srcLen, target->dataOf(j), tgtLen, status);
            }
        }
        // At the end, cmp must be 0
        if (cmp != 0) {
            *status = U_INTERNAL_PROGRAM_ERROR;
        }
    } else {
        // call strcoll for two strings at the same index
        if (source->count < target->count) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        } else {
            for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
                srcLen = useLen ? source->lengthOf(i) : -1;
                tgtLen = useLen ? target->lengthOf(i) : -1;
                ucol_strcollUTF8(coll, source->dataOf(i), srcLen, target->dataOf(i), tgtLen, status);
            }
        }
    }
}

long StrcollUTF8::getOperationsPerIteration()
{
    long numOps = (source->count) * (target->count);
    return numOps > 0 ? numOps : 1;
}


class GetSortKey : public UPerfFunction
{
public:
    GetSortKey(const UCollator* coll, CA_uchar* source, UBool useLen);
    ~GetSortKey();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    CA_uchar *source;
    UBool useLen;
};

GetSortKey::GetSortKey(const UCollator* coll, CA_uchar* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
}

GetSortKey::~GetSortKey()
{
}

#define KEY_BUF_SIZE 512

void GetSortKey::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    uint8_t key[KEY_BUF_SIZE];
    int32_t len;

    if (useLen) {
        for (int32_t i = 0; i < source->count; i++) {
            len = ucol_getSortKey(coll, source->dataOf(i), source->lengthOf(i), key, KEY_BUF_SIZE);
        }
    } else {
        for (int32_t i = 0; i < source->count; i++) {
            len = ucol_getSortKey(coll, source->dataOf(i), -1, key, KEY_BUF_SIZE);
        }
    }
}

long GetSortKey::getOperationsPerIteration()
{
    return source->count > 0 ? source->count : 1;
}


class NextSortKeyPart : public UPerfFunction
{
public:
    NextSortKeyPart(const UCollator* coll, CA_uchar* source, int32_t bufSize, int32_t maxIteration = -1);
    ~NextSortKeyPart();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const UCollator *coll;
    CA_uchar *source;
    int32_t bufSize;
    int32_t maxIteration;
    long ops;
};

NextSortKeyPart::NextSortKeyPart(const UCollator* coll, CA_uchar* source, int32_t bufSize, int32_t maxIteration /* = -1 */)
    :   coll(coll),
        source(source),
        bufSize(bufSize),
        maxIteration(maxIteration),
        ops(0)
{
}

NextSortKeyPart::~NextSortKeyPart()
{
}

void NextSortKeyPart::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    uint8_t *part = (uint8_t *)malloc(bufSize);
    uint32_t state[2];
    UCharIterator iter;

    ops = 0;
    for (int i = 0; i < source->count && U_SUCCESS(*status); i++) {
        uiter_setString(&iter, source->dataOf(i), source->lengthOf(i));
        state[0] = 0;
        state[1] = 0;
        int32_t partLen = bufSize;
        for (int32_t n = 0; partLen == bufSize && (maxIteration < 0 || n < maxIteration); n++) {
            partLen = ucol_nextSortKeyPart(coll, &iter, state, part, bufSize, status);
            ops++;
        }
    }
    free(part);
}

long NextSortKeyPart::getOperationsPerIteration()
{
    return ops > 0 ? ops : 1;
}


// CPP API test cases
class CppCompare : public UPerfFunction
{
public:
    CppCompare(const Collator* coll, CA_uchar* source, CA_uchar* target, UBool useLen, UBool roundRobin);
    ~CppCompare();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    CA_uchar *source;
    CA_uchar *target;
    UBool useLen;
    UBool roundRobin;
};

CppCompare::CppCompare(const Collator* coll, CA_uchar* source, CA_uchar* target, UBool useLen, UBool roundRobin)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen),
        roundRobin(roundRobin)
{
}

CppCompare::~CppCompare()
{
}

void CppCompare::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    int32_t srcLen, tgtLen;
    if (roundRobin) {
        // call strcoll for all permutations
        int32_t cmp = 0;
        for (int32_t i = 0; i < source->count; i++) {
            srcLen = useLen ? source->lengthOf(i) : -1;
            for (int32_t j = 0; j < target->count; j++) {
                tgtLen = useLen ? target->lengthOf(j) : -1;
                cmp += coll->compare(source->dataOf(i), srcLen, target->dataOf(j), tgtLen);
            }
        }
        // At the end, cmp must be 0
        if (cmp != 0) {
            *status = U_INTERNAL_PROGRAM_ERROR;
        }
    } else {
        // call strcoll for two strings at the same index
        if (source->count < target->count) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        } else {
            for (int32_t i = 0; i < source->count; i++) {
                srcLen = useLen ? source->lengthOf(i) : -1;
                tgtLen = useLen ? target->lengthOf(i) : -1;
                coll->compare(source->dataOf(i), srcLen, target->dataOf(i), tgtLen);
            }
        }
    }
}

long CppCompare::getOperationsPerIteration()
{
    long numOps = (source->count) * (target->count);
    return numOps > 0 ? numOps : 1;
}


class CppCompareUTF8 : public UPerfFunction
{
public:
    CppCompareUTF8(const Collator* coll, CA_char* source, CA_char* target, UBool useLen, UBool roundRobin);
    ~CppCompareUTF8();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    CA_char *source;
    CA_char *target;
    UBool useLen;
    UBool roundRobin;
};

CppCompareUTF8::CppCompareUTF8(const Collator* coll, CA_char* source, CA_char* target, UBool useLen, UBool roundRobin)
    :   coll(coll),
        source(source),
        target(target),
        useLen(useLen),
        roundRobin(roundRobin)
{
}

CppCompareUTF8::~CppCompareUTF8()
{
}

void CppCompareUTF8::call(UErrorCode* status) {
    if (U_FAILURE(*status)) return;

    StringPiece src, tgt;
    if (roundRobin) {
        // call compareUTF8 for all permutations
        int32_t cmp = 0;
        for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
            if (useLen) {
                src.set(source->dataOf(i), source->lengthOf(i));
            } else {
                src.set(source->dataOf(i));
            }
            for (int32_t j = 0; j < target->count; j++) {
                if (useLen) {
                    tgt.set(target->dataOf(i), target->lengthOf(i));
                } else {
                    tgt.set(target->dataOf(i));
                }
                cmp += coll->compareUTF8(src, tgt, *status);
            }
        }
        // At the end, cmp must be 0
        if (cmp != 0) {
            *status = U_INTERNAL_PROGRAM_ERROR;
        }
    } else {
        // call strcoll for two strings at the same index
        if (source->count < target->count) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        } else {
            for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
                if (useLen) {
                    src.set(source->dataOf(i), source->lengthOf(i));
                    tgt.set(target->dataOf(i), target->lengthOf(i));
                } else {
                    src.set(source->dataOf(i));
                    tgt.set(target->dataOf(i));
                }
                coll->compareUTF8(src, tgt, *status);
            }
        }
    }
}

long CppCompareUTF8::getOperationsPerIteration()
{
    long numOps = (source->count) * (target->count);
    return numOps > 0 ? numOps : 1;
}


class CppGetCollationKey : public UPerfFunction
{
public:
    CppGetCollationKey(const Collator* coll, CA_uchar* source, UBool useLen);
    ~CppGetCollationKey();
    virtual void call(UErrorCode* status);
    virtual long getOperationsPerIteration();

private:
    const Collator *coll;
    CA_uchar *source;
    UBool useLen;
};

CppGetCollationKey::CppGetCollationKey(const Collator* coll, CA_uchar* source, UBool useLen)
    :   coll(coll),
        source(source),
        useLen(useLen)
{
}

CppGetCollationKey::~CppGetCollationKey()
{
}

void CppGetCollationKey::call(UErrorCode* status)
{
    if (U_FAILURE(*status)) return;

    CollationKey key;
    for (int32_t i = 0; U_SUCCESS(*status) && i < source->count; i++) {
        coll->getCollationKey(source->dataOf(i), source->lengthOf(i), key, *status);
    }
}

long CppGetCollationKey::getOperationsPerIteration() {
    return source->count > 0 ? source->count : 1;
}


class CollPerf2Test : public UPerfTest
{
public:
    CollPerf2Test(int32_t argc, const char *argv[], UErrorCode &status);
    ~CollPerf2Test();
    virtual UPerfFunction* runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *par = NULL);

private:
    UCollator* coll;
    Collator* collObj;

    int32_t count;
    CA_uchar* data16;
    CA_char* data8;

    CA_uchar* modData16;
    CA_char* modData8;

    CA_uchar* getData16(UErrorCode &status);
    CA_char* getData8(UErrorCode &status);

    CA_uchar* getModData16(UErrorCode &status);
    CA_char* getModData8(UErrorCode &status);

    UPerfFunction* TestStrcoll();
    UPerfFunction* TestStrcollNull();
    UPerfFunction* TestStrcollSimilar();

    UPerfFunction* TestStrcollUTF8();
    UPerfFunction* TestStrcollUTF8Null();
    UPerfFunction* TestStrcollUTF8Similar();

    UPerfFunction* TestGetSortKey();
    UPerfFunction* TestGetSortKeyNull();

    UPerfFunction* TestNextSortKeyPart16();
    UPerfFunction* TestNextSortKeyPart32();
    UPerfFunction* TestNextSortKeyPart32_2();

    UPerfFunction* TestCppCompare();
    UPerfFunction* TestCppCompareNull();
    UPerfFunction* TestCppCompareSimilar();

    UPerfFunction* TestCppCompareUTF8();
    UPerfFunction* TestCppCompareUTF8Null();
    UPerfFunction* TestCppCompareUTF8Similar();

    UPerfFunction* TestCppGetCollationKey();
    UPerfFunction* TestCppGetCollationKeyNull();

};

CollPerf2Test::CollPerf2Test(int32_t argc, const char *argv[], UErrorCode &status) :
    UPerfTest(argc, argv, status),
    coll(NULL),
    collObj(NULL),
    count(0),
    data16(NULL),
    data8(NULL),
    modData16(NULL),
    modData8(NULL)
{
    if (U_FAILURE(status)) {
        return;
    }

    UOption options[] = {
        UOPTION_DEF("c_french", 'f', UOPT_REQUIRES_ARG),    // --french <on | OFF>
        UOPTION_DEF("c_alternate", 'a', UOPT_REQUIRES_ARG), // --alternate <NON_IGNORE | shifted>
        UOPTION_DEF("c_casefirst", 'c', UOPT_REQUIRES_ARG), // --casefirst <lower | upper | OFF>
        UOPTION_DEF("c_caselevel", 'l', UOPT_REQUIRES_ARG), // --caselevel <on | OFF>
        UOPTION_DEF("c_normal", 'n', UOPT_REQUIRES_ARG),    // --normal <on | OFF> 
        UOPTION_DEF("c_strength", 's', UOPT_REQUIRES_ARG),  // --strength <1-5>
    };
    int32_t opt_len = (sizeof(options)/sizeof(options[0]));
    enum {f,a,c,l,n,s};   // The buffer between the option items' order and their references

    _remainingArgc = u_parseArgs(_remainingArgc, (char**)argv, opt_len, options);
    if (_remainingArgc < 0) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (locale == NULL){
        locale = "en_US";   // set default locale
    }

    //  Set up an ICU collator
    coll = ucol_open(locale, &status);
    collObj = Collator::createInstance(locale, status);
    if (U_FAILURE(status)) {
        return;
    }

    if (options[f].doesOccur) {
        if (strcmp("on", options[f].value) == 0) {
            ucol_setAttribute(coll, UCOL_FRENCH_COLLATION, UCOL_ON, &status);
            collObj->setAttribute(UCOL_FRENCH_COLLATION, UCOL_ON, status);
        } else if (strcmp("off", options[f].value) == 0) {
            ucol_setAttribute(coll, UCOL_FRENCH_COLLATION, UCOL_OFF, &status);
            collObj->setAttribute(UCOL_FRENCH_COLLATION, UCOL_OFF, status);
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }

    if (options[a].doesOccur) {
        if (strcmp("shifted", options[a].value) == 0) {
            ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
            collObj->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, status);
        } else if (strcmp("non-ignorable", options[a].value) == 0) {
            ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);
            collObj->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, status);
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }

    if (options[c].doesOccur) {
        if (strcmp("lower", options[c].value) == 0) {
            ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_LOWER_FIRST, &status);
            collObj->setAttribute(UCOL_CASE_FIRST, UCOL_LOWER_FIRST, status);
        } else if (strcmp("upper", options[c].value) == 0) {
            ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &status);
            collObj->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST, status);
        } else if (strcmp("off", options[c].value) == 0) {
            ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_OFF, &status);
            collObj->setAttribute(UCOL_CASE_FIRST, UCOL_OFF, status);
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }

    if (options[l].doesOccur){
        if (strcmp("on", options[l].value) == 0) {
            ucol_setAttribute(coll, UCOL_CASE_LEVEL, UCOL_ON, &status);
            collObj->setAttribute(UCOL_CASE_LEVEL, UCOL_ON, status);
        } else if (strcmp("off", options[l].value) == 0) {
            ucol_setAttribute(coll, UCOL_CASE_LEVEL, UCOL_OFF, &status);
            collObj->setAttribute(UCOL_CASE_LEVEL, UCOL_OFF, status);
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }

    if (options[n].doesOccur){
        if (strcmp("on", options[n].value) == 0) {
            ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
            collObj->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);
        } else if (strcmp("off", options[n].value) == 0) {
            ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_OFF, &status);
            collObj->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF, status);
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }

    if (options[s].doesOccur) {
        char *endp;
        int tmp = strtol(options[s].value, &endp, 0);
        if (endp == options[s].value) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        } else {
            UColAttributeValue strength = UCOL_DEFAULT;
            switch (tmp) {
                case 1: strength = UCOL_PRIMARY; break;
                case 2: strength = UCOL_SECONDARY; break;
                case 3: strength = UCOL_TERTIARY; break;
                case 4: strength = UCOL_QUATERNARY; break;
                case 5: strength = UCOL_IDENTICAL; break;
                default: status = U_ILLEGAL_ARGUMENT_ERROR; return;
            }
            if (U_SUCCESS(status)) {
                ucol_setAttribute(coll, UCOL_STRENGTH, strength, &status);
                collObj->setAttribute(UCOL_STRENGTH, strength, status);
            }
        }
    }
}

CollPerf2Test::~CollPerf2Test()
{
    ucol_close(coll);
    delete collObj;

    delete data16;
    delete data8;
    delete modData16;
    delete modData8;
}

#define MAX_NUM_DATA 10000

CA_uchar* CollPerf2Test::getData16(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (data16) return data16;

    CA_uchar* d16 = new CA_uchar();
    const UChar *line = NULL;
    int32_t len = 0;
    int32_t numData = 0;

    for (;;) {
        line = ucbuf_readline(ucharBuf, &len, &status);
        if (line == NULL || U_FAILURE(status)) break;

        // Refer to the source code of ucbuf_readline()
        // 1. 'len' includes the line terminal symbols
        // 2. The length of the line terminal symbols is only one character
        // 3. The Windows CR LF line terminal symbols will be converted to CR

        if (len == 1 || line[0] == 0x23 /* '#' */) {
            continue; // skip empty/comment line
        } else {
            d16->append_one(len);
            memcpy(d16->last(), line, len * sizeof(UChar));
            d16->last()[len - 1] = NULL;

            numData++;
            if (numData >= MAX_NUM_DATA) break;
        }
    }

    if (U_SUCCESS(status)) {
        data16 = d16;
    } else {
        delete d16;
    }

    return data16;
}

CA_char* CollPerf2Test::getData8(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (data8) return data8;

    // UTF-16 -> UTF-8 conversion
    CA_uchar* d16 = getData16(status);
    UConverter *conv = ucnv_open("utf-8", &status);
    if (U_FAILURE(status)) return NULL;

    CA_char* d8 = new CA_char();
    for (int32_t i = 0; i < d16->count; i++) {
        int32_t s, t;

        // get length in UTF-8
        s = ucnv_fromUChars(conv, NULL, 0, d16->dataOf(i), d16->lengthOf(i), &status);
        if (status == U_BUFFER_OVERFLOW_ERROR || status == U_ZERO_ERROR){
            status = U_ZERO_ERROR;
        } else {
            break;
        }
        d8->append_one(s + 1); // plus terminal NULL

        // convert to UTF-8
        t = ucnv_fromUChars(conv, d8->last(), s, d16->dataOf(i), d16->lengthOf(i), &status);
        if (U_FAILURE(status)) break;
        if (t != s) {
            status = U_INVALID_FORMAT_ERROR;
            break;
        }
        d8->last()[s] = 0;
    }
    ucnv_close(conv);

    if (U_SUCCESS(status)) {
        data8 = d8;
    } else {
        delete d8;
    }

    return data8;
}

CA_uchar* CollPerf2Test::getModData16(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (modData16) return modData16;

    CA_uchar* d16 = getData16(status);
    if (U_FAILURE(status)) return NULL;

    CA_uchar* modData16 = new CA_uchar();

    for (int32_t i = 0; i < d16->count; i++) {
        UChar *s = d16->dataOf(i);
        int32_t len = d16->lengthOf(i) + 1; // including NULL terminator

        modData16->append_one(len);
        memcpy(modData16->last(), s, len * sizeof(UChar));
        modData16->last()[len - 1] = NULL;

        // replacing the last character with a different character
        UChar *lastChar = &modData16->last()[len -2];
        for (int32_t j = i + 1; j != i; j++) {
            if (j >= d16->count) {
                j = 0;
            }
            UChar *s1 = d16->dataOf(j);
            UChar lastChar1 = s1[d16->lengthOf(j) - 1];
            if (*lastChar != lastChar1) {
                *lastChar = lastChar1;
                break;
            }
        }
    }

    return modData16;
}

CA_char* CollPerf2Test::getModData8(UErrorCode &status)
{
    if (U_FAILURE(status)) return NULL;
    if (modData8) return modData8;

    // UTF-16 -> UTF-8 conversion
    CA_uchar* md16 = getModData16(status);
    UConverter *conv = ucnv_open("utf-8", &status);
    if (U_FAILURE(status)) return NULL;

    CA_char* md8 = new CA_char();
    for (int32_t i = 0; i < md16->count; i++) {
        int32_t s, t;

        // get length in UTF-8
        s = ucnv_fromUChars(conv, NULL, 0, md16->dataOf(i), md16->lengthOf(i), &status);
        if (status == U_BUFFER_OVERFLOW_ERROR || status == U_ZERO_ERROR){
            status = U_ZERO_ERROR;
        } else {
            break;
        }
        md8->append_one(s + 1); // plus terminal NULL

        // convert to UTF-8
        t = ucnv_fromUChars(conv, md8->last(), s, md16->dataOf(i), md16->lengthOf(i), &status);
        if (U_FAILURE(status)) break;
        if (t != s) {
            status = U_INVALID_FORMAT_ERROR;
            break;
        }
        md8->last()[s] = 0;
    }
    ucnv_close(conv);

    if (U_SUCCESS(status)) {
        modData8 = md8;
    } else {
        delete md8;
    }

    return modData8;
}

UPerfFunction*
CollPerf2Test::runIndexedTest(int32_t index, UBool exec, const char *&name, char *par /*= NULL*/)
{
    switch (index) {
        TESTCASE(0, TestStrcoll);
        TESTCASE(1, TestStrcollNull);
        TESTCASE(2, TestStrcollSimilar);

        TESTCASE(3, TestStrcollUTF8);
        TESTCASE(4, TestStrcollUTF8Null);
        TESTCASE(5, TestStrcollUTF8Similar);

        TESTCASE(6, TestGetSortKey);
        TESTCASE(7, TestGetSortKeyNull);

        TESTCASE(8, TestNextSortKeyPart16);
        TESTCASE(9, TestNextSortKeyPart32);
        TESTCASE(10, TestNextSortKeyPart32_2);

        TESTCASE(11, TestCppCompare);
        TESTCASE(12, TestCppCompareNull);
        TESTCASE(13, TestCppCompareSimilar);

        TESTCASE(14, TestCppCompareUTF8);
        TESTCASE(15, TestCppCompareUTF8Null);
        TESTCASE(16, TestCppCompareUTF8Similar);

        TESTCASE(17, TestCppGetCollationKey);
        TESTCASE(18, TestCppGetCollationKeyNull);

    default:
            name = ""; 
            return NULL;
    }
    return NULL;
}



UPerfFunction* CollPerf2Test::TestStrcoll()
{
    UErrorCode status = U_ZERO_ERROR;
    Strcoll *testCase = new Strcoll(coll, getData16(status), getData16(status), TRUE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollNull()
{
    UErrorCode status = U_ZERO_ERROR;
    Strcoll *testCase = new Strcoll(coll, getData16(status), getData16(status), FALSE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollSimilar()
{
    UErrorCode status = U_ZERO_ERROR;
    Strcoll *testCase = new Strcoll(coll, getData16(status), getModData16(status), TRUE /* useLen */, FALSE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollUTF8()
{
    UErrorCode status = U_ZERO_ERROR;
    StrcollUTF8 *testCase = new StrcollUTF8(coll, getData8(status), getData8(status), TRUE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollUTF8Null()
{
    UErrorCode status = U_ZERO_ERROR;
    StrcollUTF8 *testCase = new StrcollUTF8(coll, getData8(status), getData8(status), FALSE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestStrcollUTF8Similar()
{
    UErrorCode status = U_ZERO_ERROR;
    StrcollUTF8 *testCase = new StrcollUTF8(coll, getData8(status), getModData8(status), TRUE /* useLen */, FALSE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestGetSortKey()
{
    UErrorCode status = U_ZERO_ERROR;
    GetSortKey *testCase = new GetSortKey(coll, getData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestGetSortKeyNull()
{
    UErrorCode status = U_ZERO_ERROR;
    GetSortKey *testCase = new GetSortKey(coll, getData16(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart16()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 16 /* bufSize */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart32()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 32 /* bufSize */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestNextSortKeyPart32_2()
{
    UErrorCode status = U_ZERO_ERROR;
    NextSortKeyPart *testCase = new NextSortKeyPart(coll, getData16(status), 32 /* bufSize */, 2 /* maxIteration */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompare()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompare *testCase = new CppCompare(collObj, getData16(status), getData16(status), TRUE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareNull()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompare *testCase = new CppCompare(collObj, getData16(status), getData16(status), FALSE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareSimilar()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompare *testCase = new CppCompare(collObj, getData16(status), getModData16(status), TRUE /* useLen */, FALSE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareUTF8()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompareUTF8 *testCase = new CppCompareUTF8(collObj, getData8(status), getData8(status), TRUE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareUTF8Null()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompareUTF8 *testCase = new CppCompareUTF8(collObj, getData8(status), getData8(status), FALSE /* useLen */, TRUE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppCompareUTF8Similar()
{
    UErrorCode status = U_ZERO_ERROR;
    CppCompareUTF8 *testCase = new CppCompareUTF8(collObj, getData8(status), getModData8(status), TRUE /* useLen */, FALSE /* roundRobin */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppGetCollationKey()
{
    UErrorCode status = U_ZERO_ERROR;
    CppGetCollationKey *testCase = new CppGetCollationKey(collObj, getData16(status), TRUE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}

UPerfFunction* CollPerf2Test::TestCppGetCollationKeyNull()
{
    UErrorCode status = U_ZERO_ERROR;
    CppGetCollationKey *testCase = new CppGetCollationKey(collObj, getData16(status), FALSE /* useLen */);
    if (U_FAILURE(status)) {
        delete testCase;
        return NULL;
    }
    return testCase;
}


int main(int argc, const char *argv[])
{
    UErrorCode status = U_ZERO_ERROR;
    CollPerf2Test test(argc, argv, status);

    if (U_FAILURE(status)){
        printf("The error is %s\n", u_errorName(status));
        //TODO: print usage here
        return status;
    }

    if (test.run() == FALSE){
        fprintf(stderr, "FAILED: Tests could not be run please check the arguments.\n");
        return -1;
    }
    return 0;
}

