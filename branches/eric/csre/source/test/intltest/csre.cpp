/*
 * Copyright (C) 2008, International Business Machines Corporation and Others.
 * All rights reserved.
 */

#include "unicode/utypes.h"
#include "unicode/crematch.h"

#include "csre.h"

struct CSRE
{
    CSREMatcher *matcher;
    UnicodeString *pattern;
    UnicodeString *text;
};

U_CAPI CSRE * U_EXPORT2
csre_open(UCollator *coll,
          const UChar *pattern, int32_t patternLength,
          const UChar *target, int32_t targetLength,
          UErrorCode *status)
{
    CSRE *csre = new CSRE;

    csre->pattern = new UnicodeString(pattern, patternLength);
    csre->text    = new UnicodeString(target,  targetLength);

    csre->matcher = new CSREMatcher(coll, *csre->pattern, *csre->text, 0, *status);

    return csre;
}

U_CAPI void U_EXPORT2
csre_close(CSRE *csre)
{

    delete csre->matcher;
    delete csre->text;
    delete csre->pattern;
    delete csre;
}

U_CAPI UBool U_EXPORT2
csre_find(CSRE *csre)
{
    return csre->matcher->find();
}

U_CAPI int32_t U_EXPORT2
csre_start(CSRE *csre, UErrorCode *status)
{
    return csre->matcher->start(*status);
}

U_CAPI int32_t U_EXPORT2
csre_end(CSRE *csre, UErrorCode *status)
{
    return csre->matcher->end(*status);
}
