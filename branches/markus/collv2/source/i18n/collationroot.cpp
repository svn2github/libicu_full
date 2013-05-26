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
#include "collationtailoring.h"
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
    TriStateSingletonWrapper<CollationTailoring>(rootSingleton).deleteInstance();
    return TRUE;
}

U_CDECL_END

CollationTailoring *
CollationRoot::load(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    LocalPointer<CollationTailoring> t(new CollationTailoring());
    if(t.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    t->memory = udata_openChoice(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll",
                                 "icu", "ucadata2",
                                 CollationDataReader::isAcceptable, NULL, &errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    const uint8_t *inBytes = static_cast<const uint8_t *>(udata_getMemory(t->memory));
    CollationDataReader::read(NULL, inBytes, *t, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    ucln_i18n_registerCleanup(UCLN_I18N_COLLATION_ROOT, uprv_collation_root_cleanup);
    t->refCount = 1;  // The rootSingleton takes ownership.
    return t.orphan();
}

void *
CollationRoot::createInstance(const void * /*context*/, UErrorCode &errorCode) {
    return CollationRoot::load(errorCode);
}

const CollationTailoring *
CollationRoot::getRoot(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    return TriStateSingletonWrapper<CollationTailoring>(rootSingleton).
        getInstance(createInstance, NULL, errorCode);
}

const CollationData *
CollationRoot::getData(UErrorCode &errorCode) {
    const CollationTailoring *root = getRoot(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return root->data;
}

const CollationSettings *
CollationRoot::getSettings(UErrorCode &errorCode) {
    const CollationTailoring *root = getRoot(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return &root->settings;
}

// TODO: poor layering, move to RuleBasedCollator?
Collator *
CollationRoot::createCollator(UErrorCode &errorCode) {
    const CollationTailoring *root = getRoot(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    Collator *rootCollator = new RuleBasedCollator2(root);
    if(rootCollator == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return rootCollator;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
