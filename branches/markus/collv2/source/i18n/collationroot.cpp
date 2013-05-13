/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationroot.cpp
*
* created on: 2012dec17
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/udata.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatareader.h"
#include "collationroot.h"
#include "collationsettings.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "rulebasedcollator.h"
#include "ucln_in.h"

U_NAMESPACE_BEGIN

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_collation_root_cleanup();
U_CDECL_END

STATIC_TRI_STATE_SINGLETON(rootSingleton);

U_CDECL_BEGIN

static UBool U_CALLCONV uprv_collation_root_cleanup() {
    TriStateSingletonWrapper<CollationDataReader>(rootSingleton).deleteInstance();
    return TRUE;
}

U_CDECL_END

CollationDataReader *
CollationRoot::load(UErrorCode &errorCode) {
    const Normalizer2Impl *nfcImpl = Normalizer2Factory::getNFCImpl(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    LocalPointer<CollationDataReader> cdr(new CollationDataReader(*nfcImpl));
    if(cdr.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    cdr->memory = udata_openChoice(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll",
                                   "icu", "ucadata2",
                                   CollationDataReader::isAcceptable, NULL, &errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    const uint8_t *inBytes = reinterpret_cast<const uint8_t *>(udata_getMemory(cdr->memory));
    cdr->setData(NULL, inBytes, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    ucln_i18n_registerCleanup(UCLN_I18N_COLLATION_ROOT, uprv_collation_root_cleanup);
    return cdr.orphan();
}

void *
CollationRoot::createInstance(const void * /*context*/, UErrorCode &errorCode) {
    return CollationRoot::load(errorCode);
}

const CollationDataReader *
CollationRoot::getReader(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    return TriStateSingletonWrapper<CollationDataReader>(rootSingleton).
        getInstance(createInstance, NULL, errorCode);
}

const CollationData *
CollationRoot::getBaseData(UErrorCode &errorCode) {
    const CollationDataReader *reader = getReader(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return &reader->data;
}

const CollationSettings *
CollationRoot::getBaseSettings(UErrorCode &errorCode) {
    const CollationDataReader *reader = getReader(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return &reader->settings;
}

Collator *
CollationRoot::createCollator(UErrorCode &errorCode) {
    const CollationDataReader *reader = getReader(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return new RuleBasedCollator2(*reader);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
