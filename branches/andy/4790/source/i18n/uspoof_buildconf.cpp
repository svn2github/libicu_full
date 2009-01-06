/*
******************************************************************************
*
*   Copyright (C) 2008-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_buildconf.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan05  (refactoring earlier files)
*   created by: Andy Heninger
*
*   Internal classes for compililing confusable data into its binary (runtime) form.
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
#include "uspoof_buildconf.h"

U_NAMESPACE_USE


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

SPUString::SPUString(UnicodeString *s) {
    fStr = s;
    fStrTableIndex = 0;
}


SPUString::~SPUString() {
    delete fStr;
}


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
// Input has been pre-checked, and will have no non-hex chars.
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



ConfusabledataBuilder::ConfusabledataBuilder(SpoofImpl *spImpl, UErrorCode *status) :
    fSpoofImpl(spImpl),
    fInput(NULL),
    fSLTable(NULL),
    fSATable(NULL), 
    fMLTable(NULL),
    fMATable(NULL),
    keySet(NULL), 
    keySetVec(NULL),
    keyVec(NULL),
    valueVec(NULL),
    stringTable(NULL),
    stringLengthsTable(NULL),
    stringPool(NULL),
    fParseLine(NULL),
    fParseHexNum(NULL),
    fLineNum(0)
{
    if (U_FAILURE(*status)) {
        return;
    }
    fSLTable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    fSATable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    fMLTable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    fMATable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    keySet     = uhash_open(uhash_hashLong, uhash_compareLong, NULL, status);
    keySetVec  = new UVector(*status);
    keyVec     = new UVector(*status);
    stringPool = new SPUStringPool(status);
}


ConfusabledataBuilder::~ConfusabledataBuilder() {
    uprv_free(fInput);
    uregex_close(fParseLine);
    uregex_close(fParseHexNum);
    uhash_close(fSLTable);
    uhash_close(fSATable);
    uhash_close(fMLTable);
    uhash_close(fMATable);
    uhash_close(keySet);
    delete keyVec;
    delete stringPool;
}


void ConfusabledataBuilder::buildConfusableData(SpoofImpl * spImpl, const char * confusables,
    int32_t confusablesLen, int32_t *errorType, UParseError *pe, UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return;
    }
    ConfusabledataBuilder builder(spImpl, status);
    builder.build(confusables, confusablesLen, status);
    if (U_FAILURE(*status) && errorType != NULL) {
        *errorType = USPOOF_SINGLE_SCRIPT_CONFUSABLE;
        pe->line = builder.fLineNum;
    }
}


void ConfusabledataBuilder::build(const char * confusables, int32_t confusablesLen,
               UErrorCode *status) {
               
    // Convert the user input data from UTF-8 to UChar (UTF-16)
    int32_t inputLen = 0;
    if (U_FAILURE(*status)) {
        return;
    }
    u_strFromUTF8(NULL, 0, &inputLen, confusables, confusablesLen, status);
    if (*status != U_BUFFER_OVERFLOW_ERROR) {
        return;
    }
    *status = U_ZERO_ERROR;
    fInput = static_cast<UChar *>(uprv_malloc((inputLen+1) * sizeof(UChar)));
    if (fInput == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    }
    u_strFromUTF8(fInput, inputLen+1, NULL, confusables, confusablesLen, status);


    // Regular Expression to parse a line from Confusables.txt.  The expression will match
    // any line.  What was matched is determined by examining which capture groups have a match.
    //   Capture Group 1:  the source char
    //   Capture Group 2:  the replacement chars
    //   Capture Group 3-6  the table type, SL, SA, ML, or MA
    //   Capture Group 7:  A blank or comment only line.
    //   Capture Group 8:  A syntactically invalid line.  Anything that didn't match before.
    // Example Line from the confusables.txt source file:
    //   "1D702 ;	006E 0329 ;	SL	# MATHEMATICAL ITALIC SMALL ETA ... "
    fParseLine = uregex_openC(
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
    fParseHexNum = uregex_openC("\\s*([0-9A-F]+)", 0, NULL, status);

    // Zap any Byte Order Mark at the start of input.  Changing it to a space is benign
    //   given the syntax of the input.
    if (*fInput == 0xfeff) {
        *fInput = 0x20;
    }

    // Parse the input, one line per iteration of this loop.
    uregex_setText(fParseLine, fInput, inputLen, status);
    while (uregex_findNext(fParseLine, status)) {
        fLineNum++;
        if (uregex_start(fParseLine, 7, status) >= 0) {
            // this was a blank or comment line.
            continue;
        }
        if (uregex_start(fParseLine, 8, status) >= 0) {
            // input file syntax error.
            *status = U_PARSE_ERROR;
            return;
        }

        // We have a good input line.  Extract the key character and mapping string, and
        //    put them into the appropriate mapping table.
        UChar32 keyChar = ScanHex(fInput, uregex_start(fParseLine, 1, status),
                          uregex_end(fParseLine, 1, status), status);
                          
        int32_t mapStringStart = uregex_start(fParseLine, 2, status);
        int32_t mapStringLength = uregex_end(fParseLine, 2, status) - mapStringStart;
        uregex_setText(fParseHexNum, &fInput[mapStringStart], mapStringLength, status);
        
        UnicodeString  *mapString = new UnicodeString();
        if (mapString == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        while (uregex_findNext(fParseHexNum, status)) {
            UChar32 c = ScanHex(&fInput[mapStringStart], uregex_start(fParseHexNum, 1, status),
                                 uregex_end(fParseHexNum, 1, status), status);
            mapString->append(c);
        }
        U_ASSERT(mapString->length() >= 1);
        
        // Put the map (value) string into the string pool
        // This a little like a Java intern() - any duplicates will be eliminated.
        SPUString *smapString = stringPool->addString(mapString, status);
        
        // Add the UChar -> string mapping to the appropriate table.
        UHashtable *table = uregex_start(fParseLine, 3, status) >= 0 ? fSLTable :
                            uregex_start(fParseLine, 4, status) >= 0 ? fSATable :
                            uregex_start(fParseLine, 5, status) >= 0 ? fMLTable :
                            uregex_start(fParseLine, 6, status) >= 0 ? fMATable :
                            NULL;
        U_ASSERT(table != NULL);
        uhash_iput(table, keyChar, smapString, status);
        uhash_iput(table, keyChar, NULL, status);
        if (U_FAILURE(*status)) {
            return;
        }
    }

    // Input data is now all parsed and collected.
    // Now create the run-time binary form of the data.
    //
    // This is done in two steps.  First the data is assembled into vectors and strings,
    //   for ease of construction, then the contents of these collections are dumped
    //   into the actual raw-bytes data storage.
    {

        // Build up the string array, and record the index of each string therein
        //  in the (build time only) string pool.
        // Strings of length one are not entered into the strings array.
        // At the same time, build up the string lengths table, which records the
        // position in the string table of the first string of each length >= 4.
        // (Strings in the table are sorted by length)
        stringPool->sort(status);     
        stringTable = new UnicodeString();
        stringLengthsTable = new UVector(*status);
        int32_t previousStringLength = 0;     
        int32_t previousStringIndex  = 0;
        int32_t poolSize = stringPool->size();
        int32_t i;
        for (i=0; i<poolSize; i++) {
            SPUString *s = stringPool->getByIndex(i);
            int32_t strLen = s->fStr->length();
            int32_t strIndex = stringTable->length();
            U_ASSERT(strLen >= previousStringLength);
            if (strLen == 1) {
                // strings of length one do not get an entry in the string table.
                s->fStrTableIndex = -1;
            } else {
                if ((strLen > previousStringLength) && (previousStringLength >= 4)) {
                    stringLengthsTable->addElement(previousStringLength, *status);
                    stringLengthsTable->addElement(previousStringIndex, *status);
                }
                s->fStrTableIndex = strIndex;
                stringTable->append(*(s->fStr));
            }
            previousStringLength = strLen;
            previousStringIndex  = strIndex;
        }
        // Make the final entry to the string lengths table.
        //   (it holds an entry for the _last_ string of each length, so adding the
        //    final one doesn't happen in the main loop because no longer string was encountered.)
        if (previousStringLength >= 4) {
            stringLengthsTable->addElement(previousStringLength, *status);
            stringLengthsTable->addElement(previousStringIndex, *status);
        }

        // Build up the Key and Value tables

        // Create a sorted list of all key code points.
        //   (This is an intermediate thing, not part of the final data)
        int32_t  iterationPosition = -1;
        const UHashElement *keyElement = NULL;
        while ((keyElement = uhash_nextElement(keySet, &iterationPosition)) != NULL) {
            UChar32 keyChar = keyElement->key.integer;
            keySetVec->addElement(keyChar, *status);
        }
        keySetVec->sorti(*status);

        // For each key code point, check which mapping tables it applies to,
        //   and create the final data for the key & value structures.
        //
        //   The four logical mapping tables are conflated into one combined table.
        //   If multiple logical tables have the same mapping for some key, they
        //     share a single entry in the combined table.
        //   If more than one mapping exists for the same key code point, multiple
        //     entries will be created in the table.
        int32_t keyCharIndex;
        for (keyCharIndex=0; keyCharIndex<keyVec->size(); keyCharIndex++) {
            UChar32 keyChar = keySetVec->elementAti(keyCharIndex);
            addKeyEntry(keyChar, fSLTable, USPOOF_SL_TABLE_FLAG, status);
            addKeyEntry(keyChar, fSATable, USPOOF_SA_TABLE_FLAG, status);
            addKeyEntry(keyChar, fMLTable, USPOOF_ML_TABLE_FLAG, status);
            addKeyEntry(keyChar, fMATable, USPOOF_MA_TABLE_FLAG, status);
        }
    }
    return;
}


void ConfusabledataBuilder::addKeyEntry(
    UChar32     keyChar,     // The key character
    UHashtable *table,       // The table, one of SATable, MATable, etc.
    int32_t     tableFlag,   // One of USPOOF_SA_TABLE_FLAG, etc.
    UErrorCode *status) {

}


