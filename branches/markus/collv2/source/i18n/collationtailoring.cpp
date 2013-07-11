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

#include "unicode/udata.h"
#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "cmemory.h"
#include "collationdata.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "uhash.h"
#include "umutex.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

CollationTailoring::CollationTailoring(const CollationSettings *baseSettings)
        : data(NULL), ownedData(NULL),
          builder(NULL), memory(NULL),
          trie(NULL), unsafeBackwardSet(NULL),
          reorderCodes(NULL),
          refCount(0) {
    if(baseSettings != NULL) {
        settings.options = baseSettings->options;
        settings.variableTop = baseSettings->variableTop;
        U_ASSERT(baseSettings->reorderCodesLength == 0);
        U_ASSERT(baseSettings->reorderTable == NULL);
    }
    version[0] = version[1] = version[2] = version[3] = 0;
    maxExpansionsSingleton.fInstance = NULL;
}

CollationTailoring::~CollationTailoring() {
    delete ownedData;
    delete builder;
    udata_close(memory);
    utrie2_close(trie);
    delete unsafeBackwardSet;
    uprv_free(reorderCodes);
    uhash_close(static_cast<UHashtable *>(maxExpansionsSingleton.fInstance));
}

void
CollationTailoring::addRef() const {
    umtx_atomic_inc(&refCount);
}

void
CollationTailoring::removeRef() const {
    if(umtx_atomic_dec(&refCount) == 0) {
        delete this;
    }
}

UBool
CollationTailoring::ensureOwnedData(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    if(ownedData == NULL) {
        const Normalizer2Impl *nfcImpl = Normalizer2Factory::getNFCImpl(errorCode);
        if(U_FAILURE(errorCode)) { return FALSE; }
        ownedData = new CollationData(*nfcImpl);
        if(ownedData == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return FALSE;
        }
    }
    data = ownedData;
    return TRUE;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
