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
#include "uspoof_buildwsconf.h"


#include <stdio.h>       // TODO:  debug.  remove.
U_NAMESPACE_USE



WSConfusableDataBuilder::WSConfusableDataBuilder() {
    fAnyCaseTrie = 0;
    fLowerCaseTrie = 0;
}


WSConfusableDataBuilder::~WSConfusableDataBuilder() {
    utrie2_close(fAnyCaseTrie);
    utrie2_close(fLowerCaseTrie);
}


// Regular expression for parsing a line from the Unicode file confusablesWholeScript.txt
// Example Lines:
//   006F          ; Latn; Deva; A #      (o)  LATIN SMALL LETTER O
//   0048..0049    ; Latn; Grek; A #  [2] (H..I)  LATIN CAPITAL LETTER H..LATIN CAPITAL LETTER I
//    |               |     |    |
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
        "|^(?:"                                        //   OR
        "\\s*([0-9A-F]{4,})(?:..([0-9A-F]{4,}))?\\s*;" // Code point range.  Groups 2 and 3.
        "\\s*([A-Za-z]+)\\s*;"                         // The source script.  Group 4.
        "\\s*([A-Za-z]+)\\s*;"                         // The target script.  Group 5.
        "\\s*(?:(A)|(L))"                              // The table A or L.   Group 6 or 7
        "[ \\t]*(?:#.*?)?"                             // Trailing commment
        ")$|"                                          //   OR
        "^(.*?)$";                                      // An error line.      Group 8.
                                                       //   (Any line not matching the preceding
                                                       //    parts of the expression.is an error)


// Extract a regular expression match group into a char * string.
//    The group must contain only invariant characters.
//    Used for script names
// 
static void extractGroup(
    URegularExpression *e, int32_t group, char *destBuf, int32_t destCapacity, UErrorCode &status) {

    UChar ubuf[50];
    ubuf[0] = 0;
    destBuf[0] = 0;
    int32_t len = uregex_group(e, group, ubuf, 50, &status);
    if (U_FAILURE(status) || len == -1 || len >= destCapacity) {
        return;
    }
    UnicodeString s(FALSE, ubuf, len);   // Aliasing constructor
    s.extract(0, len, destBuf, destCapacity, US_INV);
}

void WSConfusableDataBuilder::buildWSConfusableData(SpoofImpl *spImpl, const char * confusablesWS,
          int32_t confusablesWSLen, UParseError *pe, UErrorCode &status) 
{
    if (U_FAILURE(status)) {
        return;
    }
    URegularExpression *parseRegexp = NULL;
    UChar              *input       = NULL;
    int32_t             lineNum     = 0;
    UVector            *scriptSets  = NULL;

    WSConfusableDataBuilder *This = new WSConfusableDataBuilder();
    if (This == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    This->fAnyCaseTrie = utrie2_open(0, 0, &status);
    This->fAnyCaseTrie = utrie2_open(0, 0, &status);
    
    
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
        if (uregex_start(parseRegexp, 8, &status) >= 0) {
            // input file syntax error.
            status = U_PARSE_ERROR;
            goto cleanup;
        }
        if (U_FAILURE(status)) {
            goto cleanup;
        }

        // Pick up the start and optional range end code points from the parsed line.
        UChar32  startCodePoint = SpoofImpl::ScanHex(
            input, uregex_start(parseRegexp, 2, &status), uregex_end(parseRegexp, 2, &status), status);
        UChar32  endCodePoint = startCodePoint;
        if (uregex_start(parseRegexp, 3, &status) >=0) {
            endCodePoint = SpoofImpl::ScanHex(
                input, uregex_start(parseRegexp, 3, &status), uregex_end(parseRegexp, 3, &status), status);
        }

        // Extract the two script names from the source line.  We need these in an 8 bit
        //   default encoding (will be EBCDIC on IBM mainframes) in order to pass them on
        //   to the ICU u_getPropertyValueEnum() function.  Ugh.
        char  srcScriptName[20];
        char  targScriptName[20];
        extractGroup(parseRegexp, 4, srcScriptName, sizeof(srcScriptName), status);
        extractGroup(parseRegexp, 5, targScriptName, sizeof(targScriptName), status);
        UScriptCode srcScript  =
            static_cast<UScriptCode>(u_getPropertyValueEnum(UCHAR_SCRIPT, srcScriptName));
        UScriptCode targScript =
            static_cast<UScriptCode>(u_getPropertyValueEnum(UCHAR_SCRIPT, targScriptName));
        if (U_FAILURE(status)) {
            goto cleanup;
        }
        if (srcScript == USCRIPT_INVALID_CODE || targScript == USCRIPT_INVALID_CODE) {
            status = U_INVALID_FORMAT_ERROR;
            goto cleanup;
        }

        // select the table - (A) any case or (L) lower case only
        UTrie2 *table = This->fAnyCaseTrie;
        if (uregex_start(parseRegexp, 7, &status) >= 0) {
            table = This->fLowerCaseTrie;
        }

        // Build the set of scripts containing confusable characters for
        //   the code point(s) specified in this input line.
        // Sanity check that the script of the source code point is the same
        //   as the source script indicated in the input file.  Failure of this check is
        //   an error in the input file.
        //   
        UChar32 cp;
        for (cp=startCodePoint; cp<=endCodePoint; cp++) {
            int32_t setIndex = utrie2_get32(table, cp);
            ScriptSet *sset = NULL;
            if (setIndex >= 0) {
                U_ASSERT(setIndex < scriptSets->size());
                sset = static_cast<ScriptSet *>(scriptSets->elementAt(setIndex));
            } else {
                sset = new ScriptSet();
                if (sset == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    goto cleanup;
                }
                setIndex = scriptSets->size();
                scriptSets->addElement(sset, status);
                utrie2_set32(table, cp, setIndex, &status);
            }
            sset->add(targScript);

            if (U_FAILURE(status)) {
                goto cleanup;
            }
            UScriptCode cpScript = uscript_getScript(cp, &status);
            if (cpScript != srcScript) {
                status = U_INVALID_FORMAT_ERROR;
                goto cleanup;
            }
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



ScriptSet::ScriptSet() {
    for (int32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
    }
}

ScriptSet::~ScriptSet() {
}

UBool ScriptSet::operator == (const ScriptSet &other) {
    for (int32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        if (bits[i] != other.bits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

void ScriptSet::add(UScriptCode script) {
    int32_t index = script / 32;
    int32_t bit   = 1 << (script & 31);
    U_ASSERT(index < sizeof(bits)*4);
    bits[index] |= bit;
}


void ScriptSet::Union(const ScriptSet &other) {
    for (int32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] |= other.bits[i];
    }
}



