/*
 ***************************************************************************
 * Copyright (C) 2008, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***************************************************************************
 *   file name:  uspoof_build.cpp
 *   encoding:   US-ASCII
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2008 Dec 8
 *   created by: Andy Heninger
 *
 *   Unicode Spoof Detection Data Builder
 *   These functions are kept in a separate file so that applications not needing
 *   the builder can more easily exclude them, typically by static linking.
 */

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/unorm.h"
#include "cmemory.h"
#include "uspoof_impl.h"

U_NAMESPACE_USE


// Regular Expression to parse a line from Confusables.txt
//   Capture Group 1:  the source char
//   Capture Group 2:  the replacement chars
//   Capture Group 3:  the table type, SL, SA, ML, or MA
static const char * confusableLine = "([0-9A-F]+)\\s+;([^;]+);\\s+(..)";

// Regexp for parsing a hex number out of a space-separated list of them.
//   Capture group 1 gets the number, with spaces removed.
static const char * parseHexNumber = "\\s*([0-9A-F]+)";


U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSource(const char *confusables,  int32_t confusablesLen,
                      const char *confusablesWholeScript, int32_t confusablesWholeScriptLen,
                      const char *xidModifications, int32_t xidModificationsLen,
                      int32_t *errType, UParseError *pe, UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return NULL;
    }
    SpoofImpl *This = new SpoofImpl(NULL, *status);

    
    return (USpoofChecker *)This;
}

