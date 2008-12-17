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
 *
 *   There are three relatively independent sets of Spoof data,
 *      Confusables,
 *      Whole Script Confusables
 *      ID character extensions.
 *
 *    The data tables for each are built separately, each from its own definitions
 */

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/unorm.h"
#include "unicode/uregex.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "uspoof_impl.h"
#include "uhash.h"
#include "uvector.h"
#include "uassert.h"
#include "uarrsort.h"



#include <stdio.h>   // DEBUG

U_NAMESPACE_USE

// Forward Declarations

static  void buildConfusableData(SpoofImpl *This, const char *confusables,
    int32_t confusablesLen, int32_t *errorType, UParseError *pe, UErrorCode *status);


static void buildConfusableWSData(SpoofImpl *This, const char *confusablesWholeScript, 
    int32_t confusablesWholeScriptLen, UErrorCode *status);


static void  buildXidData(SpoofImpl *This, const char *xidModifications,
    int32_t confusablesWholeScriptLen, UErrorCode *status);

// The main data building functin
U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSource(const char *confusables,  int32_t confusablesLen,
                      const char *confusablesWholeScript, int32_t confusablesWholeScriptLen,
                      const char *xidModifications, int32_t xidModificationsLen,
                      int32_t *errorType, UParseError *pe, UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (errorType!=NULL) {
        *errorType = 0;
    }
    if (pe != NULL) {
        pe->line = 0;
        pe->offset = 0;
        pe->preContext[0] = 0;
        pe->postContext[0] = 0;
    }
    
    SpoofImpl *This = new SpoofImpl(NULL, *status);

    buildConfusableData(This, confusables, confusablesLen, errorType, pe, status);
    buildConfusableWSData(This, confusablesWholeScript, confusablesWholeScriptLen, status);
    buildXidData(This, xidModifications, xidModificationsLen, status);

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
    SPUString(UnicodeString *s) {fStr = s; fStrTableIndex = 0;};
    ~SPUString() {delete fStr;};
};

class SPUStringPool {
  public:
    SPUStringPool(UErrorCode *status);
    ~SPUStringPool();
    
    // Add a string. Return the string from the table.
    // If the input parameter string is already in the table, delete the
    //  input parameter and return the existing string.
    SPUString *addString(UnicodeString *src, UErrorCode *status);


    // Get the n-th string in the collection.
    SPUString *getByIndex(int32_t i);

    // Sort the contents; affects the ordering of getByIndex().
    void sort(UErrorCode *status);

    int32_t size();

  private:
    UVector     *fVec;    // Elements are SPUString *
    UHashtable  *fHash;   // Key: UnicodeString  Value: SPUString
};

SPUStringPool::SPUStringPool(UErrorCode *status) : fVec(NULL), fHash(NULL) {
    fVec = new UVector(*status);
    fHash = uhash_open(uhash_hashUnicodeString,           // key hash function
                       uhash_compareUnicodeString,        // Key Comparator
                       NULL,                              // Value Comparator
                       status);
}


SPUStringPool::~SPUStringPool() {
    int i;
    for (i=fVec->size()-1; i>=0; i--) {
        SPUString *s = static_cast<SPUString *>(fVec->elementAt(i));
        delete s;
    }
    delete fVec;
    uhash_close(fHash);
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
// Conforms to the type signature for a USortComparator in uvector.h

static int8_t U_CALLCONV SPUStringCompare(UHashTok left, UHashTok right) {
    const UnicodeString *sL = static_cast<const UnicodeString *>(left.pointer);
    const UnicodeString *sR = static_cast<const UnicodeString *>(right.pointer);
    int32_t lenL = sL->length();
    int32_t lenR = sR->length();
    if (lenL < lenR) {
        return -1;
    } else if (lenL > lenR) {
        return 1;
    } else {
        return sL->compare(*sR);
    }
}

void SPUStringPool::sort(UErrorCode *status) {
    fVec->sort(SPUStringCompare, *status);
}


SPUString *SPUStringPool::addString(UnicodeString *src, UErrorCode *status) {
    SPUString *hashedString = static_cast<SPUString *>(uhash_get(fHash, src));
    if (hashedString != NULL) {
        delete src;
    } else {
        hashedString = new SPUString(src);
        uhash_put(fHash, src, hashedString, status);
        fVec->addElement(hashedString, *status);
    }
    return hashedString;
}

    
// Convert a text format hex number.  Input: UChar *string text.  Output: a UChar32
// Input has been pre-checked.  It shouldn't have any non-hex chars.
// The number must fall in the code point range of 0..0x10ffff
static UChar32 ScanHex(const UChar *s, int32_t start, int32_t limit, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    U_ASSERT(limit-start > 0);
    uint32_t val = 0;
    int i;
    for (i=start; i<limit; i++) {
        int digitVal = s[i] - 0x30;
        if (digitVal>9) {
            digitVal = 0xa + (s[i] - 0x41);  // Upper Case 'A'
        }
        if (digitVal>15) {
            digitVal = 0xa + (s[i] - 0x61);  // Lower Case 'a'
        }
        U_ASSERT(digitVal <= 0xf);
        val <<= 4;
        val += digitVal;
    }
    if (val > 0x10ffff) {
        *status = U_PARSE_ERROR;
        val = 0;
    }
    return (UChar32)val;
}



void buildConfusableData(SpoofImpl * This, const char * confusables,
    int32_t confusablesLen, int32_t *errorType, UParseError *pe, UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return;
    }
    // Declarations for allocated items that need to be closed or deleted
    //   at the end.  Declatations are here at the top to allow common
    //   cleanup code in the event of errors.
    UChar *input = NULL;
    UHashtable *SLTable = NULL;
    UHashtable *SATable = NULL; 
    UHashtable *MLTable = NULL; 
    UHashtable *MATable = NULL;
    UHashtable *keySet  = NULL;       // A set of all keys (UChar32s) that go into the four mapping tables.
    UVector    *keyVec  = NULL;
    SPUStringPool *stringPool = NULL;
    URegularExpression *parseLine = NULL;
    URegularExpression *parseHexNum  = NULL;
    int32_t lineNum = 0;

    // Convert the user input data from UTF-8 to UChar (UTF-16)
    
    int32_t inputLen = 0;
    u_strFromUTF8(NULL, 0, &inputLen, confusables, confusablesLen, status);
    if (*status != U_BUFFER_OVERFLOW_ERROR) {
        goto cleanup;
    }
    *status = U_ZERO_ERROR;
    input = static_cast<UChar *>(uprv_malloc((inputLen+1) * sizeof(UChar)));
    if (input == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    }
    u_strFromUTF8(input, inputLen+1, NULL, confusables, confusablesLen, status);

    // Set up intermediate data structures to collect the confusable data as it is being
    // parsed.
    
    SLTable = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    SATable = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    MLTable = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    MATable = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    keySet  = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    keyVec  = new UVector(*status);
    stringPool = new SPUStringPool(status);
    if (U_FAILURE(*status)) {
        goto cleanup;
    }

    // Regular Expression to parse a line from Confusables.txt.  The expression will match
    // any line.  What was matched is determined by examining which capture groups have a match.
    //   Capture Group 1:  the source char
    //   Capture Group 2:  the replacement chars
    //   Capture Group 3-6  the table type, SL, SA, ML, or MA
    //   Capture Group 7:  A blank or comment only line.
    //   Capture Group 8:  A syntactically invalid line.  Anything that didn't match before.
    // Example Line from the confusables.txt source file:
    //   "1D702 ;	006E 0329 ;	SL	# MATHEMATICAL ITALIC SMALL ETA ... "
    parseLine = uregex_openC(
        "(?m)^[ \\t]*([0-9A-Fa-f]+)[ \\t]+;"      // Match the source char
        "[ \\t]*([0-9A-Fa-f]+"                    // Match the replacement char(s)
           "(?:[ \\t]+[0-9A-Fa-f]+)*)[ \\t]*;"    //     (continued)
        "\\s*(?:(SL)|(SA)|(ML)|(MA))"             // Match the table type
        "[ \\t]*(?:#.*?)?$"                       // Match any trailing #comment
        "|^([ \\t]*(?:#.*?)?)$"       // OR match empty lines or lines with only a #comment
        "|^(.*?)$",                   // OR match any line, which catches illegal lines.
        0, NULL, status);
        
    // Regular expression for parsing a hex number out of a space-separated list of them.
    //   Capture group 1 gets the number, with spaces removed.
    parseHexNum = uregex_openC("\\s*([0-9A-F]+)", 0, NULL, status);

    // Zap any Byte Order Mark at the start of input.  Changing it to a space is benign
    //   given the syntax of the input.
    if (*input == 0xfeff) {
        *input = 0x20;
    }

    // Parse the input, one line per iteration of this loop.
    uregex_setText(parseLine, input, inputLen, status);
    while (uregex_findNext(parseLine, status)) {
        lineNum++;
        if (uregex_start(parseLine, 7, status) >= 0) {
            // this was a blank or comment line.
            continue;
        }
        if (uregex_start(parseLine, 8, status) >= 0) {
            // input file syntax error.
            *status = U_PARSE_ERROR;
            goto cleanup;
        }

        // We have a good input line.  Extract the key character and mapping string, and
        //    put them into the appropriate mapping table.
        UChar32 keyChar = ScanHex(input, uregex_start(parseLine, 1, status), 
                          uregex_end(parseLine, 1, status), status);
                          
        int32_t mapStringStart = uregex_start(parseLine, 2, status);
        int32_t mapStringLength = uregex_end(parseLine, 2, status) - mapStringStart;
        uregex_setText(parseHexNum, &input[mapStringStart], mapStringLength, status);
        
        UnicodeString  *mapString = new UnicodeString();
        if (mapString == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            goto cleanup;
        }
        while (uregex_findNext(parseHexNum, status)) {
            UChar32 c = ScanHex(&input[mapStringStart], uregex_start(parseHexNum, 1, status),
                                 uregex_end(parseHexNum, 1, status), status);
            mapString->append(c);
        }
        U_ASSERT(mapString->length() >= 1);
        
        // Put the map (value) string into the string pool
        // This a little like a Java intern() - any duplicates will be eliminated.
        SPUString *smapString = stringPool->addString(mapString, status);
        
        // Add the UChar -> string mapping to the appropriate table.
        UHashtable *table = uregex_start(parseLine, 3, status) >= 0 ? SLTable :
                            uregex_start(parseLine, 4, status) >= 0 ? SATable :
                            uregex_start(parseLine, 5, status) >= 0 ? MLTable :
                            uregex_start(parseLine, 6, status) >= 0 ? MATable :
                            NULL;
        U_ASSERT(table != NULL);
        uhash_iput(table, keyChar, smapString, status);
        uhash_iput(table, keyChar, NULL, status);
        if (U_FAILURE(*status)) {
            goto cleanup;
        }
    }

    // Build the Keys table.  To do that, we first must get a list of all the
    //  key characters, and sort it into code point order.
    {
    int32_t  iterationPosition = -1;
    const UHashElement *keyElement = NULL;
    while ((keyElement = uhash_nextElement(keySet, &iterationPosition)) != NULL) {
        UChar32 keyChar = keyElement->key.integer;
        keyVec->addElement(keyChar, *status);
    }
    }
    keyVec->sorti(*status);

    stringPool->sort(status);
    

  cleanup:
    if (U_FAILURE(*status)) {
        *errorType = USPOOF_SINGLE_SCRIPT_CONFUSABLE;
        pe->line = lineNum;
    }
        
    uprv_free(input);
    uregex_close(parseLine);
    uregex_close(parseHexNum);
    uhash_close(SLTable);
    uhash_close(SATable);
    uhash_close(MLTable);
    uhash_close(MATable);
    uhash_close(keySet);
    delete keyVec;
    delete stringPool;
    return;
}

//---------------------------------------------------------------------
//
//
//

static void buildConfusableWSData(SpoofImpl* /* This*/ , const char* /* confusablesWholeScript */,
    int32_t /* confusablesWholeScriptLen */, UErrorCode * /* status */) {
}


static void  buildXidData(SpoofImpl * /* This */, const char * /* xidModifications */,
    int32_t /* confusablesWholeScriptLen */, UErrorCode * /* status */) {
}


