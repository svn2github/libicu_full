/*
**********************************************************************
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   12/09/99    aliu        Ported from Java.
**********************************************************************
*/

#ifndef COLLATIONTHAITEST_H
#define COLLATIONTHAITEST_H

#include "tscoll.h"

class CollationThaiTest : public IntlTestCollator {
    Collator* coll; // Thai collator

public:

    CollationThaiTest();
    virtual ~CollationThaiTest();

    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );
    
private:

    /**
     * Read the external dictionary file, which is already in proper
     * sorted order, and confirm that the collator compares each line as
     * preceding the following line.
     */
    void TestDictionary(void);
    
    /**
     * Odd corner conditions taken from "How to Sort Thai Without Rewriting Sort",
     * by Doug Cooper, http://seasrc.th.net/paper/thaisort.zip
     */
    void TestCornerCases(void);
    
private:

    void compareArray(const Collator& c, const char* tests[],
                      int32_t testsLength);

    int8_t sign(int32_t i);
    
    /**
     * Set a UnicodeString corresponding to the given string.  Use
     * UnicodeString and the default converter, unless we see the sequence
     * "\\u", in which case we interpret the subsequent escape.
     */
    UnicodeString& parseChars(UnicodeString& result,
                              const char* chars);
};

#endif
