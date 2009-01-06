/*
 ***************************************************************************
 * Copyright (C) 2008-2009, International Business Machines Corporation
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
 *   Builder-related functions are kept in separate files so that applications not needing
 *   the builder can more easily exclude them, typically by means of static linking.
 *
 *   There are three relatively independent sets of Spoof data,
 *      Confusables,
 *      Whole Script Confusables
 *      ID character extensions.
 *
 *   The data tables for each are built separately, each from its own definitions
 */

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/unorm.h"
#include "unicode/uregex.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "uspoof_impl.h"
#include "uhash.h"
#include "uvector.h"
#include "uassert.h"
#include "uarrsort.h"
#include "uspoof_buildconf.h"


#include <stdio.h>   // DEBUG

U_NAMESPACE_USE

// Forward Declarations

static void buildConfusableWSData(SpoofImpl *This, const char *confusablesWholeScript, 
    int32_t confusablesWholeScriptLen, UErrorCode *status);


static void  buildXidData(SpoofImpl *This, const char *xidModifications,
    int32_t confusablesWholeScriptLen, UErrorCode *status);



static void buildConfusableWSData(SpoofImpl* /* This*/ , const char* /* confusablesWholeScript */,
    int32_t /* confusablesWholeScriptLen */, UErrorCode * /* status */) {
}


static void  buildXidData(SpoofImpl * /* This */, const char * /* xidModifications */,
    int32_t /* confusablesWholeScriptLen */, UErrorCode * /* status */) {
}



// The main data building function

U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSource(const char *confusables,  int32_t confusablesLen,
                      const char *confusablesWholeScript, int32_t confusablesWholeScriptLen,
                      const char *xidModifications, int32_t xidModificationsLen,
                      int32_t *errorType, UParseError *pe, UErrorCode *status) {

    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (errorType!=NULL) {
        *errorType = 0;
    }
    if (pe != NULL) {
        pe->line = 0;
        pe->offset = 0;
        pe->preContext[0] = 0;
        pe->postContext[0] = 0;
    }

    // Set up a shell of a spoof detector, with empty data.
    SpoofData *newSpoofData = new SpoofData(*status);
    SpoofImpl *This = new SpoofImpl(newSpoofData, *status);

    // Compile the binary data from the source (text) format.
    ConfusabledataBuilder::buildConfusableData(This, confusables, confusablesLen, errorType, pe, status);
    buildConfusableWSData(This, confusablesWholeScript, confusablesWholeScriptLen, status);
    buildXidData(This, xidModifications, xidModificationsLen, status);

    if (U_FAILURE(*status)) {
        delete This;
        This = NULL;
    }
    return (USpoofChecker *)This;
}

