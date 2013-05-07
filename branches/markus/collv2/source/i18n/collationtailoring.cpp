/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoring.cpp
*
* created on: 2013mar12
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collationdata.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "umutex.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

CollationTailoring::CollationTailoring()
        : data(NULL),
          refCount(1),
          isDataOwned(FALSE) {
    version[0] = version[1] = version[2] = version[3] = 0;
}

CollationTailoring::~CollationTailoring() {
    if(isDataOwned && data != NULL) {
        // TODO: delete owned data
    }
}

void
CollationTailoring::addRef() {
    umtx_atomic_inc(&refCount);
}

void
CollationTailoring::removeRef() {
    if(umtx_atomic_dec(&refCount) == 0) {
        delete this;
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
