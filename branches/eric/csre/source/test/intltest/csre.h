/*
 * Copyright (C) 2008, International Business Machines Corporation and Others.
 * All rights reserved.
 */

#ifndef _CSRE_H
#define _CSRE_H

#include "unicode/utypes.h"
#include "unicode/ucol.h"

struct CSRE;
typedef struct CSRE CSRE;

U_CAPI CSRE * U_EXPORT2
csre_open(UCollator *coll,
                const UChar *pattern, int32_t patternLength,
                const UChar *target, int32_t targetLength,
                UErrorCode *status);

U_CAPI void U_EXPORT2
csre_close(CSRE *csre);


U_CAPI UBool U_EXPORT2
csre_find(CSRE *csre);

U_CAPI int32_t U_EXPORT2
csre_start(CSRE *csre, UErrorCode *status);

U_CAPI int32_t U_EXPORT2
csre_end(CSRE *csre, UErrorCode *status);

#endif /* _CSRE_H */
