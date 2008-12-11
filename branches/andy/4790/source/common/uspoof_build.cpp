/*
 ***************************************************************************
 * Copyright (C) 2008, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***************************************************************************
 *   file name:  uspoof_build.cpp
 *   encoding:   US-ASCII
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2008 Dec 8
 *   created by: Andy Heninger
 *
 *   Unicode Spoof Detection Data Builder
 *   These functions are kept in a separate file so that applications not needing
 *   the builder can more easily exclude them, typically by means of static linking.
 */

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/unorm.h"
#include "cmemory.h"
#include "uspoof_impl.h"

U_NAMESPACE_USE

// Forward Declarations

static  void buildConfusableData(SpoofImpl *This, const char *confusables,
    int32_t confusablesLen, UErrorCode *status);


U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSource(const char *confusables,  int32_t confusablesLen,
                      const char *confusablesWholeScript, int32_t confusablesWholeScriptLen,
                      const char *xidModifications, int32_t xidModificationsLen,
                      int32_t *errType, UParseError *pe, UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return NULL;
    }
    SpoofImpl *This = new SpoofImpl(NULL, *status);

    buildConfusableData(This, confusables, confusablesLen, status);
    buildConfusableWSData(This, confusablesWholeScript, confusablesWholeScriptLen, status);
    buildXidData(xidModifications, confusablesWholeScriptLen, status);

    if (U_FAILURE(*status)) {
        delete This;
        This = NULL;
    }
    return (USpoofChecker *)This;
}

//---------------------------------------------------------------------
//
//  buildConfusableData   Compile the source confusable data, as defined by
//                        the Unicode data file confusables.txt, into the binary
//                        structures used by the confusable detector.
//
//                        The binary structures are described in uspoof_impl.h
//
//     1.  parse the data, building 4 hash tables, one each for the SL, SA, ML and MA
//         tables.  Each maps from a UChar32 to a String.
//
//     2.  Sort all of the strings encountered by length, since they will need to
//         be stored in that order in the final string table.
//
//     3.  Build a list of keys (UChar32s) from the four mapping tables.  Sort the
//         list because that will be the ordering of our runtime table.
//
//     4.  Generate the run time string table.  This is generated before the key & value
//         tables because we need the string indexes when building those tables.
//
//     5.  Build the run-time key and value tables.  These are parallel tables, and are built
//         at the same time
//


//  String Pool   A utility class for holding the strings that are the result of
//                the spoof mappings.  These strings will utimately end up in the
//                run-time String Table.
//                This is sort of like a sorted set of strings, except that ICU's anemic
//                built-in collections don't support those, so it is implemented with a
//                combination of a uhash and a UVector.

struct SPUString {
    UnicodeString  *fStr;
    int32_t         fStrTableIndex;
    SPUString(UnicodeString *s) {fStr = s; fStrTableIndex = NULL;};
    }

class SPUStringPool {
  public:
    SPUStringPool(UErrorCode *status);
    ~SPUStringPool();
    
    // Add a string. Return the string from the table.
    // If the input parameter string is already in the table, delete the
    //  input parameter and return the existing string.
    UnicodeString *addString(UnicodeString *src);


    // Get the n-th string in the collection.
    SPUString *getByIndex(int32_t i);

    int32_t size();

  private:
    UVector     *fVec;    // Elements are SPUString *
    UHashtable  *fHash;   // Key: UnicodeString  Value: SPUString
}

SPUStringPool::SPUStringPool(UErrorCode *status) : fVec(NULL), fHash(NULL) {
    fVec = new UVector(*status);
    fHash = uhash_open(uhash_uhashUnicodeString,          // key hash function
                       uhash_uhashCompareUnicodeString,   // Key Comparator
                       NULL,                              // Value Comparator
                       status);
}


SPUStringPool::~SPUStringPool() {
    int i;
    for (i=fVec->size()-1; i>=0; i--) {
        SPUString *s = static_cast<SPUString *>(fVec->elementAt(i);
        delete s;
    }
    delete fVec;
    uhasn_close(fHash);
}


int32_t SPUStringPool::size() {
    return fVec->size();
}

SPUString *SPUStringPool::getByIndex(int32_t index) {
    SPUString *retString = (SPUString *)fVec->elementAt(index);
    return retString;
}

// Comparison function for ordering strings in the string pool.
// Compare by length first, then, within a group of the same length,
// by code point order.
static int8_t U_CALLCONV SPUStringCompare(UHashTok tok1, UHashTok tok2) {
    UnicodeString *s1 = static_cast<UnicodeString *>(tok1.pointer);
    UnicodeString *s2 = static_cast<UnicodeString *>(tok2.pointer);
    int32_t len1 = s1->length();
    int32_t len2 = s2->length();
    if (len1 < len2) {
        return -1;
    } else if (len1 > len2) {
        return 1;
    } else {
        return s1->compareCodePointOrder(*s2);
    }
}
        

SPUString *SPUStringPool::addString(UnicodeString *src, UErrorCode *status) {
    SPUString *hashedString = uhash_get(fHash, src);
    if (hashedString != NULL) {
        delete src;
    } else {
        hashedString = new SPUString(src);
        uhash_put(fhash, src, hashedString, status);
        fVec->sortedInsert(hashedString, SPUStringCompare, *status);
    }
    return hashedString;
}

    
    
    
//
// Regular Expression to parse a line from Confusables.txt
//   Capture Group 1:  the source char
//   Capture Group 2:  the replacement chars
//   Capture Group 3:  the table type, SL, SA, ML, or MA
static const char * confusableLine = "([0-9A-F]+)\\s+;([^;]+);\\s+(..)";

// Regexp for parsing a hex number out of a space-separated list of them.
//   Capture group 1 gets the number, with spaces removed.
static const char * parseHexNumber = "\\s*([0-9A-F]+)";


void buildConfusableData(SpoofImpl *This, const char *confusables,
    int32_t confusablesLen, UErrorCode *status) {
    
    }
