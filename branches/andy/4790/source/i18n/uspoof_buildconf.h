/*
******************************************************************************
*
*   Copyright (C) 2008-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_buildconf.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan05
*   created by: Andy Heninger
*
*   Internal classes for compiling confusable data into its binary (runtime) form.
*/

#ifndef __USPOOF_BUILDCONF_H__
#define __USPOOF_BUILDCONF_H__

#include "uspoof_impl.h"

// SPUString
//              Holds a string that is the result of one of the mappings defined
//              by the confusable mapping data (confusables.txt from Unicode.org)
//              Instances of SPUString exist during the compilation process only.

struct SPUString {
    UnicodeString  *fStr;             // The actual string.
    int32_t         fStrTableIndex;   // Index into the final runtime data for this string
    SPUString(UnicodeString *s);
    ~SPUString();
};


//  String Pool   A utility class for holding the strings that are the result of
//                the spoof mappings.  These strings will utimately end up in the
//                run-time String Table.
//                This is sort of like a sorted set of strings, except that ICU's anemic
//                built-in collections don't support those, so it is implemented with a
//                combination of a uhash and a UVector.


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


// class ConfusabledataBuilder
//     An instance of this class exists while the confusable data is being built from source.
//     It encapsulates the intermediate data structures that are used for building.
//     It exports one static function, to do a confusable data build.

class ConfusabledataBuilder {
  private:
    SpoofImpl  *fSpoofImpl;
    UChar      *fInput;
    UHashtable *fSLTable;
    UHashtable *fSATable; 
    UHashtable *fMLTable; 
    UHashtable *fMATable;
    UHashtable *keySet;     // A set of all keys (UChar32s) that go into the four mapping tables.
    UVector    *keySetVec;     // A sorted vector of the above set of keys.

    // The binary data is first assembled into the following four collections, then
    //   copied to its final raw-memory destination.
    UVector            *keyVec;
    UVector            *valueVec;
    UnicodeString      *stringTable;
    UVector            *stringLengthsTable;
    
    SPUStringPool      *stringPool;
    URegularExpression *fParseLine;
    URegularExpression *fParseHexNum;
    int32_t             fLineNum;

    ConfusabledataBuilder(SpoofImpl *spImpl, UErrorCode *status);
    ~ConfusabledataBuilder();
    void build(const char * confusables, int32_t confusablesLen, UErrorCode *status);

    // Add an entry to the key and value tables being built
    //   input:  data from SLTable, MATable, etc.
    //   outut:  entry added to keyVec and valueVec
    void addKeyEntry(UChar32     keyChar,     // The key character
                     UHashtable *table,       // The table, one of SATable, MATable, etc.
                     int32_t     tableFlag,   // One of USPOOF_SA_TABLE_FLAG, etc.
                     UErrorCode *status);

  public:
    static void buildConfusableData(SpoofImpl *spImpl, const char * confusables,
        int32_t confusablesLen, int32_t *errorType, UParseError *pe, UErrorCode *status);
};

#endif
