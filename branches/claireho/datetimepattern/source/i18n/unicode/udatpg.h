/*
*******************************************************************************
*
*   Copyright (C) 2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udatpg.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007jul30
*   created by: Markus W. Scherer
*/

#ifndef __UDATPG_H__
#define __UDATPG_H__

#include "unicode/utypes.h"

/**
 * \file
 * \brief C API: Wrapper for DateTimePatternGenerator (unicode/dtptngen.h).
 *
 * TODO(markus): Copy API doc from Java or C++.
 */

/**
 * Opaque type for a date/time pattern generator object.
 * @draft ICU 3.8
 */
typedef void *UDateTimePatternGenerator;

#ifndef U_HIDE_DRAFT_API

/**
 * Field number constants for udatpg_getAppendItemFormats() and similar functions.
 * These constants are separate from UDateFormatField despite semantic overlap
 * because some fields are merged for the date/time pattern generator.
 * @draft ICU 3.8
 */
typedef enum UDateTimePatternField {
    /** @draft ICU 3.8 */
    UDATPG_ERA_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_YEAR_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_QUARTER_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_MONTH_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_WEEK_OF_YEAR_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_WEEK_OF_MONTH_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_WEEKDAY_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_DAY_OF_YEAR_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_DAY_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_DAYPERIOD_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_HOUR_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_MINUTE_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_SECOND_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_FRACTIONAL_SECOND_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_ZONE_FIELD,
    /** @draft ICU 3.8 */
    UDATPG_FIELD_COUNT
} UDateTimePatternField;

/**
 * Status return values from udatpg_addPattern().
 * @draft ICU 3.8
 */
typedef enum UDateTimePatternConflict {
    /** @draft ICU 3.8 */
    UDATPG_NO_CONFLICT,
    /** @draft ICU 3.8 */
    UDATPG_BASE_CONFLICT,
    /** @draft ICU 3.8 */
    UDATPG_CONFLICT,
    /** @draft ICU 3.8 */
    UDATPG_CONFLICT_COUNT
} UDateTimePatternConflict;

#endif

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT UDateTimePatternGenerator * U_EXPORT2
udatpg_open(const char *locale, UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT UDateTimePatternGenerator * U_EXPORT2
udatpg_openEmpty(UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT void U_EXPORT2
udatpg_close(UDateTimePatternGenerator *dtpg);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT UDateTimePatternGenerator * U_EXPORT2
udatpg_clone(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT int32_t U_EXPORT2
udatpg_getBestPattern(const UDateTimePatternGenerator *dtpg,
                      const UChar *skeleton, int32_t length,
                      UChar *bestPattern, int32_t capacity,
                      UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @draft ICU 3.8
 */
U_DRAFT int32_t U_EXPORT2
udatpg_getSkeleton(UDateTimePatternGenerator *dtpg,
                   const UChar *pattern, int32_t length,
                   UChar *skeleton, int32_t capacity,
                   UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @draft ICU 3.8
 */
U_DRAFT int32_t U_EXPORT2
udatpg_getBaseSkeleton(UDateTimePatternGenerator *dtpg,
                       const UChar *pattern, int32_t length,
                       UChar *skeleton, int32_t capacity,
                       UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT UDateTimePatternConflict U_EXPORT2
udatpg_addPattern(UDateTimePatternGenerator *dtpg,
                  const UChar *pattern, int32_t length,
                  UBool override,
                  UChar *conflictingPattern, int32_t capacity, int32_t *pLength,
                  UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT void U_EXPORT2
udatpg_setAppendItemFormats(UDateTimePatternGenerator *dtpg,
                            UDateTimePatternField field,
                            const UChar *value, int32_t length,
                            UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT const UChar * U_EXPORT2
udatpg_getAppendItemFormats(const UDateTimePatternGenerator *dtpg,
                            UDateTimePatternField field,
                            int32_t *pLength,
                            UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT void U_EXPORT2
udatpg_setAppendItemNames(UDateTimePatternGenerator *dtpg,
                          UDateTimePatternField field,
                          const UChar *value, int32_t length,
                          UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT const UChar * U_EXPORT2
udatpg_getAppendItemNames(const UDateTimePatternGenerator *dtpg,
                          UDateTimePatternField field,
                          int32_t *pLength,
                          UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT void U_EXPORT2
udatpg_setDateTimeFormat(const UDateTimePatternGenerator *dtpg,
                         const UChar *dtFormat, int32_t length,
                         UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT const UChar * U_EXPORT2
udatpg_getDateTimeFormat(const UDateTimePatternGenerator *dtpg,
                         int32_t *pLength
                         UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT void U_EXPORT2
udatpg_setDecimal(UDateTimePatternGenerator *dtpg,
                  const UChar *decimal, int32_t length,
                  UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT const UChar * U_EXPORT2
udatpg_getDecimal(const UDateTimePatternGenerator *dtpg,
                  int32_t *pLength,
                  UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT int32_t U_EXPORT2
udatpg_replaceFieldTypes(const UDateTimePatternGenerator *dtpg,
                         const UChar *pattern, int32_t patternLength,
                         const UChar *skeleton, int32_t skeletonLength,
                         UChar *dest, int32_t destCapacity,
                         UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT UEnumeration * U_EXPORT2
udatpg_openSkeletons(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT UEnumeration * U_EXPORT2
udatpg_openBaseSkeletons(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode);

/**
 * TODO(markus): Copy Java or C++ API doc.
 * @draft ICU 3.8
 */
U_DRAFT int32_t U_EXPORT2
udatpg_getPatternForSkeleton(const UDateTimePatternGenerator *dtpg,
                             const UChar *skeleton, int32_t length,
                             UChar *pattern, int32_t capacity,
                             UErrorCode *pErrorCode);

#endif
