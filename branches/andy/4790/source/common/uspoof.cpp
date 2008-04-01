/*
***************************************************************************
* Copyright (C) 2008, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*   file name:  uspoof.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2008Feb13
*   created by: Andy Heninger
*
*   Unicode Spoof Detection
*/
#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "uspoofim.h"

U_NAMESPACE_BEGIN



USpoofChecker * U_EXPORT2
uspoof_open(UErrorCode *status) {
	if (U_FAILURE(*status)) {
	    return NULL;
	}
    SpoofImpl *si = new SpoofImpl();
	if (U_FAILURE(*status)) {
		delete si;
		si = NULL;
	}
	return (USpoofChecker *)si;
}

U_DRAFT void U_EXPORT2
uspoof_close(USpoofChecker *sc, UErrorCode *status) {
	SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
    delete This;
}

    
U_DRAFT void U_EXPORT2
uspoof_setChecks(USpoofChecker *sc, int32_t checks, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
	if (This == NULL) {
		return;
	}

    // Verify that the requested checks are all ones (bits) that 
    //   are acceptable, known values.
    if (checks && ~USPOOF_ALL_CHECKS) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    This->fChecks = checks;
}


U_DRAFT int32_t U_EXPORT2
uspoof_getChecks(const USpoofChecker *sc, UErrorCode *status) {
    const SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
	if (This == NULL) {
		return 0;
	}
    return This->fChecks;
}

U_DRAFT void U_EXPORT2
uspoof_setAllowedLocales(USpoofChecker *sc, const char *localesList, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
	if (This == NULL) {
		return;
	}
    // TODO:
}


U_DRAFT void U_EXPORT2
uspoof_setAllowedChars(USpoofChecker *sc, const USet *chars, UErrorCode *status) {
    SpoofImpl *This = SpoofImpl::validateThis(sc, *status);
	if (This == NULL) {
		return;
	}

    // Cast the USet to a UnicodeSet.  Slightly dicey - relies on knowing that
    //    a USet * is actually a UnicodeSet.  But we have both USet and
    //    UnicodeSet interfaces, and need a common internal representation.
    UnicodeSet *uniset = (UnicodeSet *)chars;

    if (uniset->isBogus()) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    // Clone & freeze the caller's set
    UnicodeSet *clonedSet = (UnicodeSet *)uniset->clone();
    if (clonedSet == NULL || clonedSet->isBogus()) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    clonedSet->freeze();

    // Any existing set is frozen, meaning it can't be reused.
    //   Just delete it and create another.
    delete This->fAllowedCharsSet;
    This->fAllowedCharsSet = clonedSet;

    // Set the bit that enables the allowed chars test
    This->fChecks |= USPOOF_CHAR_LIMIT;
}

U_NAMESPACE_END
