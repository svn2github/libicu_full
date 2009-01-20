/*
******************************************************************************
*
*   Copyright (C) 2008-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_buildwsconf.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan05  (refactoring earlier files)
*   created by: Andy Heninger
*
*   Internal classes for compililing confusable data into its binary (runtime) form.
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
#include "uspoof_buildwsconf.h"


#include <stdio.h>       // TODO:  debug.  remove.
U_NAMESPACE_USE



WSConfusableDataBuilder::WSConfusableDataBuilder() {
}

// Regular expression for parsing a line from the Unicode file confusablesWholeScript.txt
// Example Lines:
//   006F          ; Latn; Deva; A #      (o)  LATIN SMALL LETTER O
//   0048..0049    ; Latn; Grek; A #  [2] (H..I)  LATIN CAPITAL LETTER H..LATIN CAPITAL LETTER I
//    |               |     \    |
//    |               |     |    |---- Which table, Any Case or Lower Case (A or L)
//    |               |     |----------Target script.   We need this.
//    |               |----------------Src script.  Should match the script of the source
//    |                                code points.  Beyond checking that, we don't keep it.
//    |--------------------------------Src code points or range.
//
// The expression will match _all_ lines, including erroneous lines.
// The result of the parse is returned via the contents of the (match) groups.
static const char *parseExp = 
        
        "(?m)"                                         // Multi-line mode
        "^([ \\t]*(?:#.*?)?)$"                         // A blank or comment line.  Matches Group 1.
        "|^(?:"                                         //   OR
        "\\s*([0-9A-F]{4,})(?:..([0-9A-F]{4,}))?\\s*;" // Code point range.  Groups 2 and 3.
        "\\s*([A-Za-z]+)\\s*;"                         // The source script.  Group 4.
        "\\s*([A-Za-z]+)\\s*;"                         // The target script.  Group 5.
        "\\s*(A|L)"                                    // The table A or L.   Group 6
        "[ \\t]*(?:#.*?)?"                             // Trailing commment
        ")$|"                                          //   OR
        "^(.*?)$";                                      // An error line.      Group 7.
                                                       //   (Any line not matching the preceding
                                                       //    parts of the expression.is an error)
void WSConfusableDataBuilder::buildWSConfusableData(SpoofImpl *spImpl, const char * confusablesWS,
          int32_t confusablesWSLen, UParseError *pe, UErrorCode &status) 
{
    if (U_FAILURE(status)) {
        return;
    }
    URegularExpression *parseRegexp = NULL;
    UChar              *input       = NULL;
    int32_t             lineNum     = 0;
    
    // Convert the user input data from UTF-8 to UChar (UTF-16)
    int32_t inputLen = 0;
    u_strFromUTF8(NULL, 0, &inputLen, confusablesWS, confusablesWSLen, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        return;
    }
    status = U_ZERO_ERROR;
    input = static_cast<UChar *>(uprv_malloc((inputLen+1) * sizeof(UChar)));
    if (input == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        goto cleanup;
    }
    u_strFromUTF8(input, inputLen+1, NULL, confusablesWS, confusablesWSLen, &status);

    parseRegexp = uregex_openC(parseExp, 0, NULL, &status);
    
    // Zap any Byte Order Mark at the start of input.  Changing it to a space is benign
    //   given the syntax of the input.
    if (*input == 0xfeff) {
        *input = 0x20;
    }

    // Parse the input, one line per iteration of this loop.
    uregex_setText(parseRegexp, input, inputLen, &status);
    while (uregex_findNext(parseRegexp, &status)) {
        lineNum++;
        UChar  line[200];
        uregex_group(parseRegexp, 0, line, 200, &status);
        if (uregex_start(parseRegexp, 1, &status) >= 0) {
            // this was a blank or comment line.
            continue;
        }
        if (uregex_start(parseRegexp, 7, &status) >= 0) {
            // input file syntax error.
            status = U_PARSE_ERROR;
            goto cleanup;
        }
        if (U_FAILURE(status)) {
            goto cleanup;
        }
    }

cleanup:
    if (U_FAILURE(status)) {
        pe->line = lineNum;
    }
    uregex_close(parseRegexp);
    uprv_free(input);
    return;
}
