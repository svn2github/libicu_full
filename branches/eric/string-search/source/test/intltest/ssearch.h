/*
 **********************************************************************
 *   Copyright (C) 2005-2008, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __SSEARCH_H
#define __SSEARCH_H

#include "unicode/utypes.h"
#include "unicode/unistr.h"

#include "intltest.h"

//
//  Test of the function usearch_search()
//
//     See srchtest.h for the tests for the rest of the string search functions.
//
class SSearchTest: public IntlTest {
public:
  
    SSearchTest();
    virtual ~SSearchTest();

    virtual void runIndexedTest(int32_t index, UBool exec, const char* &name, char* par = NULL );

    virtual void searchTest();
    virtual void searchTime();
    virtual void offsetTest();
    virtual void monkeyTest();

private:
    virtual const char *getPath(char buffer[2048], const char *filename);
};

#endif
