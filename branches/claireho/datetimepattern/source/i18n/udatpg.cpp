/*
*******************************************************************************
*
*   Copyright (C) 2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udatpg.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007jul30
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udatpg.h"
#include "unicode/uenum.h"
#include "unicode/dtptngen.h"

U_NAMESPACE_USE

U_DRAFT UDateTimePatternGenerator * U_EXPORT2
udatpg_open(const char *locale, UErrorCode *pErrorCode) {
    if(locale==NULL) {
        return (UDateTimePatternGenerator *)DateTimePatternGenerator::createInstance(*pErrorCode);
    } else {
        return (UDateTimePatternGenerator *)DateTimePatternGenerator::createInstance(Locale(locale), *pErrorCode);
    }
}

U_DRAFT UDateTimePatternGenerator * U_EXPORT2
udatpg_openEmpty(UErrorCode *pErrorCode) {
    // TODO(markus): createEmptyInstance
    return (UDateTimePatternGenerator *)DateTimePatternGenerator::createInstance(*pErrorCode);
}

U_DRAFT void U_EXPORT2
udatpg_close(UDateTimePatternGenerator *dtpg) {
    delete (DateTimePatternGenerator *)dtpg;
}

U_DRAFT UDateTimePatternGenerator * U_EXPORT2
udatpg_clone(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    // TODO(markus): Remove error code from clone().
    return (UDateTimePatternGenerator *)(((DateTimePatternGenerator *)dtpg)->clone(*pErrorCode));
}

U_DRAFT int32_t U_EXPORT2
udatpg_getBestPattern(const UDateTimePatternGenerator *dtpg,
                      const UChar *skeleton, int32_t length,
                      UChar *bestPattern, int32_t capacity,
                      UErrorCode *pErrorCode) {
    // TODO(markus): Should getBestPattern() take a UErrorCode?
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    UnicodeString skeletonString((UBool)(length<0), skeleton, length);
    UnicodeString result=((DateTimePatternGenerator *)dtpg)->getBestPattern(skeletonString);
    return result.extract(bestPattern, capacity, *pErrorCode);
}

#endif
