/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2002-2007, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

//
//   regextst.cpp
//
//      ICU Regular Expressions test, part of intltest.
//

#include "intltest.h"

#include "v32test.h"
#include "uvectr32.h"
#include "uvector.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>


//---------------------------------------------------------------------------
//
//  Test class boilerplate
//
//---------------------------------------------------------------------------
UVector32Test::UVector32Test() 
{
}


UVector32Test::~UVector32Test()
{
}



void UVector32Test::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite UVector32Test: ");
    switch (index) {

        case 0: name = "UVector32_API";
            if (exec) UVector32_API(); 
            break;
        default: name = ""; 
            break; //needed to end loop
    }
}



//---------------------------------------------------------------------------
//
//      UVector32_API      Check for basic functionality of UVector32.
//
//---------------------------------------------------------------------------
void UVector32Test::UVector32_API() {

    UErrorCode  status = U_ZERO_ERROR;
    UVector32     *a;
    UVector32     *b;

    a = new UVector32(status);
    ASSERT_SUCCESS((status));
    delete a;

    status = U_ZERO_ERROR;
    a = new UVector32(2000, status);
    ASSERT_SUCCESS((status));
    delete a;

    //
    //  assign()
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    b = new UVector32(status);
    b->assign(*a, status);
    EXPECT_EQUALS((3, b->size()));
    EXPECT_EQUALS((20, b->elementAti(1)));
    EXPECT_SUCCESS((status));
    delete a;
    delete b;

    //
    //  operator == and != and equals()
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    b = new UVector32(status);
    EXPECT_TRUE((*b != *a));
    EXPECT_FALSE((*b == *a));
    EXPECT_FALSE((b->equals(*a)));
    b->assign(*a, status);
    EXPECT_TRUE((*b == *a));
    EXPECT_FALSE((*b != *a));
    EXPECT_TRUE((b->equals(*a)));
    b->addElement(666, status);
    EXPECT_TRUE((*b != *a));
    EXPECT_FALSE((*b == *a));
    EXPECT_FALSE((b->equals(*a)));
    EXPECT_SUCCESS((status));
    delete b;
    delete a;

    //
    //  addElement().   Covered by above tests.
    //

    //
    // setElementAt()
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    a->setElementAt(666, 1);
    EXPECT_EQUALS((10, a->elementAti(0)));
    EXPECT_EQUALS((666, a->elementAti(1)));
    EXPECT_EQUALS((3, a->size()));
    EXPECT_SUCCESS((status));
    delete a;

    //
    // insertElementAt()
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    a->insertElementAt(666, 1, status);
    EXPECT_EQUALS((10, a->elementAti(0)));
    EXPECT_EQUALS((666, a->elementAti(1)));
    EXPECT_EQUALS((20, a->elementAti(2)));
    EXPECT_EQUALS((30, a->elementAti(3)));
    EXPECT_EQUALS((4, a->size()));
    EXPECT_SUCCESS((status));
    delete a;

    //
    //  elementAti()    covered by above tests
    //

    //
    //  lastElementi
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    EXPECT_EQUALS((30, a->lastElementi()));
    EXPECT_SUCCESS((status));
    delete a;


    //
    //  indexOf
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    EXPECT_EQUALS((2, a->indexOf(30, 0)));
    EXPECT_EQUALS((-1, a->indexOf(40, 0)));
    EXPECT_EQUALS((0, a->indexOf(10, 0)));
    EXPECT_EQUALS((-1, a->indexOf(10, 1)));
    EXPECT_SUCCESS((status));
    delete a;

    
    //
    //  contains
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    EXPECT_TRUE((a->contains(10)));
    EXPECT_FALSE((a->contains(11)));
    EXPECT_TRUE((a->contains(20)));
    EXPECT_FALSE((a->contains(-10)));
    EXPECT_SUCCESS((status));
    delete a;


    //
    //  containsAll
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    b = new UVector32(status);
    EXPECT_TRUE((a->containsAll(*b)));
    b->addElement(2, status);
    EXPECT_FALSE((a->containsAll(*b)));
    b->setElementAt(10, 0);
    EXPECT_TRUE((a->containsAll(*b)));
    EXPECT_FALSE((b->containsAll(*a)));
    b->addElement(30, status);
    b->addElement(20, status);
    EXPECT_TRUE((a->containsAll(*b)));
    EXPECT_TRUE((b->containsAll(*a)));
    b->addElement(2, status);
    EXPECT_FALSE((a->containsAll(*b)));
    EXPECT_TRUE((b->containsAll(*a)));
    EXPECT_SUCCESS((status));
    delete a;
    delete b;

    //
    //  removeAll
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    b = new UVector32(status);
    a->removeAll(*b);
    EXPECT_EQUALS((3, a->size()));
    b->addElement(20, status);
    a->removeAll(*b);
    EXPECT_EQUALS((2, a->size()));
    EXPECT_TRUE((a->contains(10)));
    EXPECT_TRUE((a->contains(30)));
    b->addElement(10, status);
    a->removeAll(*b);
    EXPECT_EQUALS((1, a->size()));
    EXPECT_TRUE((a->contains(30)));
    EXPECT_SUCCESS((status));
    delete a;
    delete b;


    // retainAll
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    b = new UVector32(status);
    b->addElement(10, status);
    b->addElement(20, status);
    b->addElement(30, status);
    b->addElement(15, status);
    a->retainAll(*b);
    EXPECT_EQUALS((3, a->size()));
    b->removeElementAt(1);
    a->retainAll(*b);
    EXPECT_FALSE((a->contains(20)));
    EXPECT_EQUALS((2, a->size()));
    b->removeAllElements();
    EXPECT_EQUALS((0, b->size()));
    a->retainAll(*b);
    EXPECT_EQUALS((0, a->size()));
    EXPECT_SUCCESS((status));
    delete a;
    delete b;

    //
    //  removeElementAt   Tested above.
    //

    //
    //  removeAllElments   Tested above
    //

    //
    //  size()   tested above
    //

    //
    //  isEmpty
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    EXPECT_TRUE((a->isEmpty()));
    a->addElement(10, status);
    EXPECT_FALSE((a->isEmpty()));
    a->addElement(20, status);
    a->removeElementAt(0);
    EXPECT_FALSE((a->isEmpty()));
    a->removeElementAt(0);
    EXPECT_TRUE((a->isEmpty()));
    EXPECT_SUCCESS((status));
    delete a;


    //
    // ensureCapacity, expandCapacity
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    EXPECT_TRUE((a->isEmpty()));
    a->addElement(10, status);
    EXPECT_TRUE((a->ensureCapacity(5000, status)));
    EXPECT_TRUE((a->expandCapacity(20000, status)));
    EXPECT_SUCCESS((status));
    delete a;
    
    //
    // setSize
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    a->setSize(100);
    EXPECT_EQUALS((100, a->size()));
    EXPECT_EQUALS((10, a->elementAti(0)));
    EXPECT_EQUALS((20, a->elementAti(1)));
    EXPECT_EQUALS((30, a->elementAti(2)));
    EXPECT_EQUALS((0, a->elementAti(3)));
    a->setElementAt(666, 99);
    a->setElementAt(777, 100);
    EXPECT_EQUALS((666, a->elementAti(99)));
    EXPECT_EQUALS((0, a->elementAti(100)));
    a->setSize(2);
    EXPECT_EQUALS((20, a->elementAti(1)));
    EXPECT_EQUALS((0, a->elementAti(2)));
    EXPECT_EQUALS((2, a->size()));
    a->setSize(0);
    EXPECT_TRUE((a->empty()));
    EXPECT_EQUALS((0, a->size()));

    EXPECT_SUCCESS((status));
    delete a;

    //
    // containsNone
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    b = new UVector32(status);
    EXPECT_TRUE((a->containsNone(*b)));
    b->addElement(5, status);
    EXPECT_TRUE((a->containsNone(*b)));
    b->addElement(30, status);
    EXPECT_FALSE((a->containsNone(*b)));

    EXPECT_SUCCESS((status));
    delete a;
    delete b;

    //
    // sortedInsert
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->sortedInsert(30, status);
    a->sortedInsert(20, status);
    a->sortedInsert(10, status);
    EXPECT_EQUALS((10, a->elementAti(0)));
    EXPECT_EQUALS((20, a->elementAti(1)));
    EXPECT_EQUALS((30, a->elementAti(2)));

    EXPECT_SUCCESS((status));
    delete a;

    //
    // getBuffer
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    int32_t *buf = a->getBuffer();
    EXPECT_EQUALS((10, buf[0]));
    EXPECT_EQUALS((20, buf[1]));
    a->setSize(20000);
    int32_t *resizedBuf;
    resizedBuf = a->getBuffer();
    //TEST_ASSERT(buf != resizedBuf); // The buffer might have been realloc'd
    EXPECT_EQUALS((10, resizedBuf[0]));
    EXPECT_EQUALS((20, resizedBuf[1]));

    EXPECT_SUCCESS((status));
    delete a;


    //
    //  empty
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    EXPECT_TRUE((a->empty()));
    a->addElement(10, status);
    EXPECT_FALSE((a->empty()));
    a->addElement(20, status);
    a->removeElementAt(0);
    EXPECT_FALSE((a->empty()));
    a->removeElementAt(0);
    EXPECT_TRUE((a->empty()));
    EXPECT_SUCCESS((status));
    delete a;


    //
    //  peeki
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    EXPECT_EQUALS((10, a->peeki()));
    a->addElement(20, status);
    EXPECT_EQUALS((20, a->peeki()));
    a->addElement(30, status);
    EXPECT_EQUALS((30, a->peeki()));
    EXPECT_SUCCESS((status));
    delete a;


    //
    // popi
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->addElement(10, status);
    a->addElement(20, status);
    a->addElement(30, status);
    EXPECT_EQUALS((30, a->popi()));
    EXPECT_EQUALS((20, a->popi()));
    EXPECT_EQUALS((10, a->popi()));
    EXPECT_EQUALS((0, a->popi()));
    EXPECT_TRUE((a->isEmpty()));
    EXPECT_SUCCESS((status));
    delete a;

    //
    // push
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    EXPECT_EQUALS((10, a->push(10, status)));
    EXPECT_EQUALS((20, a->push(20, status)));
    EXPECT_EQUALS((30, a->push(30, status)));
    EXPECT_EQUALS((3, a->size()));
    EXPECT_EQUALS((30, a->popi()));
    EXPECT_EQUALS((20, a->popi()));
    EXPECT_EQUALS((10, a->popi()));
    EXPECT_TRUE((a->isEmpty()));
    EXPECT_SUCCESS((status));
    delete a;


    //
    // reserveBlock
    //
    status = U_ZERO_ERROR;
    a = new UVector32(status);
    a->ensureCapacity(1000, status);

    // TODO:

    EXPECT_SUCCESS((status));
    delete a;

}


