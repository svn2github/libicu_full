/*
**********************************************************************
*   Copyright (C) 1998-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "uspoofim.h"


U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SpoofImpl)

static const int SPOOF_MAGIC = 0x8345fdef; 
SpoofImpl::SpoofImpl(UErrorCode &status) {
	fMagic = SPOOF_MAGIC;
	fChecks = 0;
}

SpoofImpl::SpoofImpl() {
}

SpoofImpl::~SpoofImpl() {
	fMagic = 0;   // head off application errors by preventing use of
	              //    of deleted objects.
}

//
//  Incoming parameter check on Status and the SpoofChecker object
//    received from the C API.
//
SpoofImpl *SpoofImpl::validateThis(USpoofChecker *sc, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (sc == NULL) {
        return NULL;
    };
    SpoofImpl *This = (SpoofImpl *)sc;
    if (This->fMagic != SPOOF_MAGIC) {
        return NULL;
    }
    return This;
}

const SpoofImpl *SpoofImpl::validateThis(const USpoofChecker *sc, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (sc == NULL) {
        return NULL;
    };
    const SpoofImpl *This = (const SpoofImpl *)sc;
    if (This->fMagic != SPOOF_MAGIC) {
        return NULL;
    }
    return This;
}


U_NAMESPACE_END
