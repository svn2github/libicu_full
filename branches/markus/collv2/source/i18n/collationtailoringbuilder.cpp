/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoringbuilder.cpp
*
* created on: 2013mar05
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/localpointer.h"
#include "unicode/ucharstriebuilder.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationtailoringbuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

CollationTailoringBuilder::CollationTailoringBuilder(UErrorCode &errorCode)
        : CollationDataBuilder(errorCode) {}

CollationTailoringBuilder::~CollationTailoringBuilder() {
}

void
CollationTailoringBuilder::init(const CollationData *b, const CollationSettings *bs,
                                UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    if(b == NULL || bs == NULL) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    base = b;
    baseSettings = bs;
    settings = *bs;

    // For a tailoring, the default is to fall back to the base.
    trie = utrie2_open(Collation::MIN_SPECIAL_CE32, Collation::FFFD_CE32, &errorCode);

    unsafeBackwardSet = *b->unsafeBackwardSet;

    if(U_FAILURE(errorCode)) { return; }
    // TODO
}

void
CollationTailoringBuilder::build(UErrorCode &errorCode) {
    buildMappings(errorCode);
    data.numericPrimary = base->numericPrimary;
    data.compressibleBytes = base->compressibleBytes;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
