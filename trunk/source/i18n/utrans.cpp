/*
*******************************************************************************
*   Copyright (C) 1997-2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   Date        Name        Description
*   06/21/00    aliu        Creation.
*******************************************************************************
*/

#include "unicode/utrans.h"
#include "unicode/putil.h"
#include "unicode/rbt.h"
#include "unicode/rep.h"
#include "unicode/translit.h"
#include "unicode/unifilt.h"
#include "unicode/uniset.h"
#include "unicode/ustring.h"
#include "cpputils.h"

// Following macro is to be followed by <return value>';' or just ';'
#define utrans_ENTRY(s) if ((s)==NULL || U_FAILURE(*(s))) return

/********************************************************************
 * Replaceable-UReplaceableCallbacks glue
 ********************************************************************/

/**
 * Make a UReplaceable + UReplaceableCallbacks into a Replaceable object.
 */
class ReplaceableGlue : public Replaceable {

    UChar *buf;
    int32_t bufLen;
    UReplaceable *rep;
    UReplaceableCallbacks *func;

    enum { BUF_PAD = 8 };

public:

    ReplaceableGlue(UReplaceable *replaceable,
                    UReplaceableCallbacks *funcCallback);

    virtual ~ReplaceableGlue();

    virtual void handleReplaceBetween(UTextOffset start,
                                      UTextOffset limit,
                                      const UnicodeString& text);

    virtual void copy(int32_t start, int32_t limit, int32_t dest);

protected:

    virtual int32_t getLength() const;

    virtual UChar getCharAt(UTextOffset offset) const;

    virtual UChar32 getChar32At(UTextOffset offset) const;
};


ReplaceableGlue::ReplaceableGlue(UReplaceable *replaceable,
                                 UReplaceableCallbacks *funcCallback)
  : Replaceable()
{
    this->rep = replaceable;
    this->func = funcCallback;
    buf = 0;
    bufLen = 0;
}

ReplaceableGlue::~ReplaceableGlue() {
    delete[] buf;
}

int32_t ReplaceableGlue::getLength() const {
    return (*func->length)(rep);
}

UChar ReplaceableGlue::getCharAt(UTextOffset offset) const {
    return (*func->charAt)(rep, offset);
}

UChar32 ReplaceableGlue::getChar32At(UTextOffset offset) const {
    return (*func->char32At)(rep, offset);
}

void ReplaceableGlue::handleReplaceBetween(UTextOffset start,
                          UTextOffset limit,
                          const UnicodeString& text) {
    int32_t len = text.length();
    if (buf == 0 || bufLen < len) {
        delete[] buf;
        bufLen = len + BUF_PAD;
        buf = new UChar[bufLen];
    }
    text.extract(0, len, buf);
    (*func->replace)(rep, start, limit, buf, len);
}

void ReplaceableGlue::copy(int32_t start, int32_t limit, int32_t dest) {
    (*func->copy)(rep, start, limit, dest);
}

/********************************************************************
 * General API
 ********************************************************************/
#if 0

U_CAPI UTransliterator*
utrans_open(const char* id,
            UTransDirection dir,
            UParseError* parseError,
            UErrorCode* status) {

    utrans_ENTRY(status) NULL;

    if (id == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    UnicodeString ID(id, ""); // use invariant converter
    Transliterator *trans = NULL;

    trans = Transliterator::createInstance(ID, dir, *parseError, *status);

    if (trans == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return (UTransliterator*) trans;
}

U_CAPI UTransliterator*
utrans_openRules(const char* id,
                 const UChar* rules,
                 int32_t rulesLength, /* -1 if null-terminated */
                 UTransDirection dir,
                 UParseError* parseErr, /* may be NULL */
                 UErrorCode* status) {

    utrans_ENTRY(status) NULL;

    if (id == NULL || rules == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    UnicodeString ID(id, ""); // use invariant converter
    UnicodeString ruleStr(rulesLength < 0,
                          rules,
                          rulesLength); // r-o alias

    RuleBasedTransliterator *trans = NULL;

    // Use if() to avoid construction of ParseError object on stack
    // unless it is called for by user.
    if (parseErr != NULL) {
        trans = new RuleBasedTransliterator(ID, ruleStr, dir,
                                            NULL, *parseErr, *status);
    } else {
        trans = new RuleBasedTransliterator(ID, ruleStr, dir,
                                            NULL, *status);
    }

    if (trans == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    } else if (U_FAILURE(*status)) {
        delete trans;
        trans = NULL;
    }
    return (UTransliterator*) trans;
}
#endif

U_CAPI UTransliterator*
utrans_open(const char* id,
            UTransDirection dir,
            const UChar* rules,         /* may be Null */
            int32_t rulesLength,        /* -1 if null-terminated */ 
            UParseError* parseError,    /* may be Null */
            UErrorCode* status) {

    utrans_ENTRY(status) NULL;

    if (id == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    UParseError temp;
    
    if(parseError == NULL){
        parseError = &temp;
    }
    
    UnicodeString ID(id, ""); // use invariant converter

    if(rules==NULL){

        Transliterator *trans = NULL;

        trans = Transliterator::createInstance(ID, dir, *parseError, *status);
        
        if(U_FAILURE(*status)){
            return NULL;
        }
        return (UTransliterator*) trans;
    }else{
        UnicodeString ruleStr(rulesLength < 0,
                              rules,
                              rulesLength); // r-o alias

        RuleBasedTransliterator *trans = NULL;
        trans = new RuleBasedTransliterator(ID, ruleStr, dir,
                                            NULL, *parseError, *status);
        if (trans == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
        } else if (U_FAILURE(*status)) {
            delete trans;
            trans = NULL;
        }
        return (UTransliterator*) trans;
    }
}

U_CAPI UTransliterator*
utrans_openInverse(const UTransliterator* trans,
                   UErrorCode* status) {

    utrans_ENTRY(status) NULL;

    UTransliterator* result =
        (UTransliterator*) ((Transliterator*) trans)->createInverse(*status);

    return result;
}

U_CAPI UTransliterator*
utrans_clone(const UTransliterator* trans,
             UErrorCode* status) {

    utrans_ENTRY(status) NULL;

    if (trans == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    Transliterator *t = ((Transliterator*) trans)->clone();
    if (t == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    }
    return (UTransliterator*) t;
}

U_CAPI void
utrans_close(UTransliterator* trans) {
    delete (Transliterator*) trans;
}

U_CAPI int32_t
utrans_getID(const UTransliterator* trans,
             char* buf,
             int32_t bufCapacity) {
    return ((Transliterator*) trans)->getID().extract(0, 0x7fffffff, buf, bufCapacity, "");
}

U_CAPI void
utrans_register(UTransliterator* adoptedTrans,
                UErrorCode* status) {
    utrans_ENTRY(status);
    // status currently ignored; may remove later
    Transliterator::registerInstance((Transliterator*) adoptedTrans);
}

U_CAPI void
utrans_unregister(const char* id) {
    UnicodeString ID(id, ""); // use invariant converter
    Transliterator::unregister(ID);
}

U_CAPI void
utrans_setFilter(UTransliterator* trans,
                 const UChar* filterPattern,
                 int32_t filterPatternLen,
                 UErrorCode* status) {

    utrans_ENTRY(status);
    UnicodeFilter* filter = NULL;
    if (filterPattern != NULL && *filterPattern != 0) {
        // Create read only alias of filterPattern:
        UnicodeString pat(filterPatternLen < 0, filterPattern, filterPatternLen);
        filter = new UnicodeSet(pat, *status);
        if (U_FAILURE(*status)) {
            delete filter;
            filter = NULL;
        }
    }
    ((Transliterator*) trans)->adoptFilter(filter);
}

U_CAPI int32_t
utrans_countAvailableIDs(void) {
    return Transliterator::countAvailableIDs();
}

U_CAPI int32_t
utrans_getAvailableID(int32_t index,
                      char* buf, // may be NULL
                      int32_t bufCapacity) {
    return Transliterator::getAvailableID(index).extract(0, 0x7fffffff, buf, bufCapacity, "");
}

/********************************************************************
 * Transliteration API
 ********************************************************************/

U_CAPI void
utrans_trans(const UTransliterator* trans,
             UReplaceable* rep,
             UReplaceableCallbacks* repFunc,
             int32_t start,
             int32_t* limit,
             UErrorCode* status) {

    utrans_ENTRY(status);

    if (trans == 0 || rep == 0 || repFunc == 0 || limit == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    ReplaceableGlue r(rep, repFunc);

    *limit = ((Transliterator*) trans)->transliterate(r, start, *limit);
}

U_CAPI void
utrans_transIncremental(const UTransliterator* trans,
                        UReplaceable* rep,
                        UReplaceableCallbacks* repFunc,
                        UTransPosition* pos,
                        UErrorCode* status) {

    utrans_ENTRY(status);

    if (trans == 0 || rep == 0 || repFunc == 0 || pos == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    ReplaceableGlue r(rep, repFunc);

    ((Transliterator*) trans)->transliterate(r, *pos, *status);
}

U_CAPI void
utrans_transUChars(const UTransliterator* trans,
                   UChar* text,
                   int32_t* textLength,
                   int32_t textCapacity,
                   int32_t start,
                   int32_t* limit,
                   UErrorCode* status) {

    utrans_ENTRY(status);

    if (trans == 0 || text == 0 || limit == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
 
    int32_t textLen = (textLength == NULL || *textLength < 0)
        ? u_strlen(text) : *textLength;
    // writeable alias: for this ct, len CANNOT be -1 (why?)
    UnicodeString str(text, textLen, textCapacity);

    *limit = ((Transliterator*) trans)->transliterate(str, start, *limit);

    // Copy the string buffer back to text (only if necessary)
    // and fill in *neededCapacity (if neededCapacity != NULL).
    textLen = str.extract(text, textCapacity, *status);
    if(textLength != NULL) {
        *textLength = textLen;
    }
}

U_CAPI void
utrans_transIncrementalUChars(const UTransliterator* trans,
                              UChar* text,
                              int32_t* textLength,
                              int32_t textCapacity,
                              UTransPosition* pos,
                              UErrorCode* status) {

    utrans_ENTRY(status);

    if (trans == 0 || text == 0 || pos == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t textLen = (textLength == NULL || *textLength < 0)
        ? u_strlen(text) : *textLength;
    // writeable alias: for this ct, len CANNOT be -1 (why?)
    UnicodeString str(text, textLen, textCapacity);

    ((Transliterator*) trans)->transliterate(str, *pos, *status);

    // Copy the string buffer back to text (only if necessary)
    // and fill in *neededCapacity (if neededCapacity != NULL).
    textLen = str.extract(text, textCapacity, *status);
    if(textLength != NULL) {
        *textLength = textLen;
    }
}
