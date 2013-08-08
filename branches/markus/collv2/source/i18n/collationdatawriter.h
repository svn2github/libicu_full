/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatawriter.h
*
* created on: 2013aug06
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATAWRITER_H__
#define __COLLATIONDATAWRITER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

struct CollationData;
struct CollationSettings;

class CollationDataBuilder;

/**
 * Collation-related code for tools & demos.
 */
class U_I18N_API CollationDataWriter /* all static */ {
public:
    static int32_t write(UBool isBase,  const UVersionInfo dataVersion,
                         const CollationDataBuilder *dataBuilder,
                         const CollationData &data, const CollationSettings &settings,
                         const void *rootElements, int32_t rootElementsLength,
                         int32_t indexes[], uint8_t *dest, int32_t capacity,
                         UErrorCode &errorCode);

private:
    CollationDataWriter();  // no constructor

    static void copyData(const int32_t indexes[], int32_t startIndex,
                         const void *src, uint8_t *dest);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATAWRITER_H__
