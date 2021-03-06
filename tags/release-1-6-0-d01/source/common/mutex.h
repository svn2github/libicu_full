/*
*****************************************************************************************
*
*   Copyright (C) 1997-1999, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*****************************************************************************************
*/
//------------------------------------------------------------------------------
// File:     mutex.h
//
// Lightweight C++ wrapper for umtx_ C mutex functions
//
// Author:   Alan Liu  1/31/97
// History:
// 06/04/97   helena         Updated setImplementation as per feedback from 5/21 drop.
// 04/07/1999  srl               refocused as a thin wrapper
//
//------------------------------------------------------------------------------
#ifndef MUTEX_H
#define MUTEX_H
#include "unicode/utypes.h"

#include "umutex.h"

//------------------------------------------------------------------------------
// Code within this library which accesses protected data
// should instantiate a Mutex object while doing so. You should make your own 
// private mutex where possible.

// For example:
// 
// UMTX myMutex;
// 
// int InitializeMyMutex()
// {
//    umtx_init( &myMutex );
//    return 0;
// }
// 
// static int initializeMyMutex = InitializeMyMutex();
//
// void Function(int arg1, int arg2)
// {
//    static Object* foo; // Shared read-write object
//    Mutex mutex(&myMutex);  // or no args for the global lock
//    foo->Method();
//    // When 'mutex' goes out of scope and gets destroyed here, the lock is released
// }
//
// Note:  Do NOT use the form 'Mutex mutex();' as that merely forward-declares a function
//        returning a Mutex. This is a common mistake which silently slips through the
//        compiler!!
//

class U_COMMON_API Mutex
{
public:
  inline Mutex(UMTX *mutex = NULL);
  inline ~Mutex();

private:
  UMTX   *fMutex;
};

inline Mutex::Mutex(UMTX *mutex)
  : fMutex(mutex)
{
  umtx_lock(fMutex);
}

inline Mutex::~Mutex()
{
  umtx_unlock(fMutex);
}

#endif //_MUTEX_
//eof








