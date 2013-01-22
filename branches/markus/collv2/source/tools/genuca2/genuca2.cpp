/*
*******************************************************************************
*
*   Copyright (C) 2000-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  genuca.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created at the end of XX century
*   created by: Vladimir Weinstein & Markus Scherer
*
*   This program reads the Franctional UCA table and generates
*   internal format for UCA table as well as inverse UCA table.
*   It then writes binary files containing the data: ucadata.icu & invuca.icu.
*/

#include <stdio.h>
#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "charstr.h"
#include "collation.h"
#include "collationbasedatabuilder.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "cstring.h"
#include "toolutil.h"
#include "ucol_bld.h"
#include "ucol_elm.h"
#include "unewdata.h"
#include "uoptions.h"
#include "uparse.h"
#include "writesrc.h"

#define U_NO_DEFAULT_INCLUDE_UTF_HEADERS 1

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_USE

static UBool beVerbose=FALSE, withCopyright=TRUE;

static UVersionInfo UCAVersion;

static const UDataInfo ucaDataInfo={
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    U_SIZEOF_UCHAR,
    0,

    { 0x55, 0x43, 0x6f, 0x6c },         // dataFormat="UCol"
    { 4, 0, 0, 0 },                     // formatVersion
    { 6, 3, 0, 0 }                      // dataVersion
};

static const UDataInfo invUcaDataInfo={
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    U_SIZEOF_UCHAR,
    0,

    { 0x49, 0x6e, 0x76, 0x43 },         // dataFormat="InvC"
    { 3, 0, 0, 0 },                     // formatVersion
    { 6, 3, 0, 0 }                      // dataVersion
};

static UCAElements le;

static uint32_t gVariableTop;  // TODO: store in the base builder, or otherwise make this not be a global variable

// returns number of characters read
static int32_t readElement(char **from, char *to, char separator, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return 0;
    }
    char buffer[1024];
    int32_t i = 0;
    for(;;) {
        char c = **from;
        if(c == separator || (separator == ' ' && c == '\t')) {
            break;
        }
        if (c == '\0') {
            return 0;
        }
        if(c != ' ') {
            *(buffer+i++) = c;
        }
        (*from)++;
    }
    (*from)++;
    *(buffer + i) = 0;
    strcpy(to, buffer);
    return i;
}

/* TODO: remove?!
static int32_t skipUntilWhiteSpace(char **from, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    int32_t count = 0;
    while (**from != ' ' && **from != '\t' && **from != '\0') {
        (*from)++;
        count++;
    }
    return count;
}
*/

static int32_t skipWhiteSpace(char **from, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }
    int32_t count = 0;
    while (**from == ' ' || **from == '\t') {
        (*from)++;
        count++;
    }
    return count;
}

static uint32_t parseWeight(char *s, uint32_t maxWeight, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || *s == 0) { return 0; }
    if(*s == '0' && s[1] == '0') {
        // leading 0 byte
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    char *end;
    unsigned long weight = uprv_strtoul(s, &end, 16);
    if(end == s || *end != 0 || weight > maxWeight) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    // Left-align the weight.
    if(weight != 0) {
        while(weight <= (maxWeight >> 8)) { weight <<= 8; }
    }
    return (uint32_t)weight;
}

static int64_t getSingleCEValue(const CollationDataBuilder &builder,
                                char *primary, char *secondary, char *tertiary, UErrorCode *status) {
    uint32_t p = parseWeight(primary, 0xffffffff, *status);
    uint32_t s = parseWeight(secondary, 0xffff, *status);
    uint32_t t = parseWeight(tertiary, 0xffff, *status);
    if(U_FAILURE(*status)) {
        return 0;
    }
    // Transform an implicit primary weight.
    if(0xe0000000 <= p && p <= 0xe4ffffff) {
        UChar32 c = uprv_uca_getCodePointFromRaw(uprv_uca_getRawFromImplicit(p));
        U_ASSERT(c >= 0);
        int64_t ce = builder.getSingleCE(c, *status);
// TODO: remove debug code -- printf("implicit p %08lx -> U+%04lx -> new p %08lx\n", (long)p, (long)c, (long)(uint32_t)(ce >> 32));
        p = (uint32_t)(ce >> 32);
    }
    return ((int64_t)p << 32) | (s << 16) | t;
}

static int32_t hex2num(char hex) {
    if(hex>='0' && hex <='9') {
        return hex-'0';
    } else if(hex>='a' && hex<='f') {
        return hex-'a'+10;
    } else if(hex>='A' && hex<='F') {
        return hex-'A'+10;
    } else {
        return 0;
    }
}

static const struct {
    const char *name;
    int32_t code;
} specialReorderTokens[] = {
    { "TERMINATOR", -2 },  // -2 means "ignore"
    { "LEVEL-SEPARATOR", -2 },
    { "FIELD-SEPARATOR", -2 },
    { "COMPRESS", -3 },
    // The standard name is "PUNCT" but FractionalUCA.txt uses the long form.
    { "PUNCTUATION", UCOL_REORDER_CODE_PUNCTUATION },
    { "IMPLICIT", USCRIPT_HAN },  // Implicit weights are usually for Han characters. Han & unassigned share a lead byte.
    { "TRAILING", -2 },  // We do not reorder trailing weights (those after implicits).
    { "SPECIAL", -2 }  // We must never reorder internal, special CE lead bytes.
};

int32_t getReorderCode(const char* name) {
    int32_t code = ucol_findReorderingEntry(name);
    if (code >= 0) {
        return code;
    }
    code = u_getPropertyValueEnum(UCHAR_SCRIPT, name);
    if (code >= 0) {
        return code;
    }
    for (int32_t i = 0; i < LENGTHOF(specialReorderTokens); ++i) {
        if (0 == strcmp(name, specialReorderTokens[i].name)) {
            return specialReorderTokens[i].code;
        }
    }
    return -1;  // Same as UCHAR_INVALID_CODE or USCRIPT_INVALID_CODE.
}

static UnicodeString *leadByteScripts = NULL;

UCAElements *readAnElement(FILE *data,
                           CollationBaseDataBuilder &builder,
                           UCAConstants *consts,
                           int64_t ces[32], int32_t &cesLength,
                           UErrorCode *status) {
    char buffer[2048], primary[100], secondary[100], tertiary[100];
    UChar leadByte[100], scriptCode[100];
    int32_t i = 0;
    char *pointer = NULL;
    char *commentStart = NULL;
    char *startCodePoint = NULL;
    char *endCodePoint = NULL;
    char *result = fgets(buffer, 2048, data);
    int32_t buflen = (int32_t)uprv_strlen(buffer);
    if(U_FAILURE(*status)) {
        return 0;
    }
    *primary = *secondary = *tertiary = '\0';
    *leadByte = *scriptCode = '\0';
    if(result == NULL) {
        if(feof(data)) {
            return NULL;
        } else {
            fprintf(stderr, "empty line but no EOF!\n");
            *status = U_INVALID_FORMAT_ERROR;
            return NULL;
        }
    }
    while(buflen>0 && (buffer[buflen-1] == '\r' || buffer[buflen-1] == '\n')) {
      buffer[--buflen] = 0;
    }

    if(buffer[0] == 0 || buffer[0] == '#') {
        return NULL; // just a comment, skip whole line
    }

    UCAElements *element = &le;
    memset(element, 0, sizeof(*element));

    enum ActionType {
      READCE,
      READPRIMARY,
      READFIRSTPRIMARY,  // TODO: remove
      READUNIFIEDIDEOGRAPH,
      READUCAVERSION,
      READLEADBYTETOSCRIPTS,
      IGNORE
    };

    // Directives.
    if(buffer[0] == '[') {
      uint32_t cnt = 0;
      static const struct {
        char name[128];
        uint32_t *what;
        ActionType what_to_do;
      } vt[]  = { {"[first tertiary ignorable",  consts->UCA_FIRST_TERTIARY_IGNORABLE,  READCE},
                  {"[last tertiary ignorable",   consts->UCA_LAST_TERTIARY_IGNORABLE,   READCE},
                  {"[first secondary ignorable", consts->UCA_FIRST_SECONDARY_IGNORABLE, READCE},
                  {"[last secondary ignorable",  consts->UCA_LAST_SECONDARY_IGNORABLE,  READCE},
                  {"[first primary ignorable",   consts->UCA_FIRST_PRIMARY_IGNORABLE,   READCE},
                  {"[last primary ignorable",    consts->UCA_LAST_PRIMARY_IGNORABLE,    READCE},
                  {"[first variable",            consts->UCA_FIRST_VARIABLE,            READCE},
                  {"[last variable",             consts->UCA_LAST_VARIABLE,             READCE},
                  {"[first regular",             consts->UCA_FIRST_NON_VARIABLE,        READCE},
                  {"[last regular",              consts->UCA_LAST_NON_VARIABLE,         READCE},
                  {"[first implicit",            consts->UCA_FIRST_IMPLICIT,            READCE},
                  {"[last implicit",             consts->UCA_LAST_IMPLICIT,             READCE},
                  {"[first trailing",            consts->UCA_FIRST_TRAILING,            READCE},
                  {"[last trailing",             consts->UCA_LAST_TRAILING,             READCE},

                  {"[Unified_Ideograph",            NULL, READUNIFIEDIDEOGRAPH},
                  {"[first_primary",                NULL, IGNORE},  // TODO: remove

                  {"[fixed top",                    NULL, IGNORE},
                  {"[fixed first implicit byte",    NULL, IGNORE},
                  {"[fixed last implicit byte",     NULL, IGNORE},
                  {"[fixed first trail byte",       NULL, IGNORE},
                  {"[fixed last trail byte",        NULL, IGNORE},
                  {"[fixed first special byte",     NULL, IGNORE},
                  {"[fixed last special byte",      NULL, IGNORE},
                  {"[variable top = ",              &gVariableTop,                      READPRIMARY},
                  {"[UCA version = ",               NULL,                               READUCAVERSION},
                  {"[top_byte",                     NULL,                               READLEADBYTETOSCRIPTS},
                  {"[reorderingTokens",             NULL,                               IGNORE},
                  {"[categories",                   NULL,                               IGNORE},
                  // TODO: Do not ignore these -- they are important for tailoring,
                  // for creating well-formed Collation Element Tables.
                  {"[first tertiary in secondary non-ignorable",                 NULL,                               IGNORE},
                  {"[last tertiary in secondary non-ignorable",                 NULL,                               IGNORE},
                  {"[first secondary in primary non-ignorable",                 NULL,                               IGNORE},
                  {"[last secondary in primary non-ignorable",                 NULL,                               IGNORE},
      };
      for (cnt = 0; cnt<LENGTHOF(vt); cnt++) {
        uint32_t vtLen = (uint32_t)uprv_strlen(vt[cnt].name);
        if(uprv_strncmp(buffer, vt[cnt].name, vtLen) == 0) {
            ActionType what_to_do = vt[cnt].what_to_do;
            if (what_to_do == IGNORE) { //vt[cnt].what_to_do == IGNORE
                return NULL;
            } else if(what_to_do == READPRIMARY) {
              pointer = buffer+vtLen;
              readElement(&pointer, primary, ']', status);
              *(vt[cnt].what) = parseWeight(primary, 0xffffffff, *status);
              if(U_FAILURE(*status)) {
                  fprintf(stderr, "Value of \"%s\" is not a primary weight\n", buffer);
                  return NULL;
              }
#if 0  // TODO: remove
            } else if(what_to_do == READFIRSTPRIMARY) {
                pointer = buffer+vtLen;
                skipWhiteSpace(&pointer, status);
                UBool firstInGroup = FALSE;
                if(uprv_strncmp(pointer, "group", 5) == 0) {
                    firstInGroup = TRUE;
                    pointer += 5;
                    skipWhiteSpace(&pointer, status);
                }
                char scriptName[100];
                int32_t length = readElement(&pointer, scriptName, ' ', status);
                if (length <= 0) {
                    fprintf(stderr, "Syntax error: unable to parse reorder code from line '%s'\n", buffer);
                    *status = U_INVALID_FORMAT_ERROR;
                    return NULL;
                }
                int32_t reorderCode = getReorderCode(scriptName);
                if (reorderCode < 0) {
                    fprintf(stderr, "Syntax error: unable to parse reorder code from line '%s'\n", buffer);
                    *status = U_INVALID_FORMAT_ERROR;
                    return NULL;
                }
                if (CollationData::scriptByteFromInt(reorderCode) < 0) {
                    fprintf(stderr, "reorder code %d for \"%s\" cannot be encoded by CollationData::scriptByteFromInt(reorderCode)\n",
                            reorderCode, scriptName);
                    *status = U_INTERNAL_PROGRAM_ERROR;
                    return NULL;
                }
                readElement(&pointer, primary, ']', status);
                uint32_t weight = parseWeight(primary, 0xffffffff, *status);
                if(U_FAILURE(*status)) {
                    fprintf(stderr, "Value of \"%s\" is not a primary weight\n", buffer);
                    return NULL;
                }
                builder.addFirstPrimary(reorderCode, firstInGroup, weight, *status);
                if(U_FAILURE(*status)) {
                    fprintf(stderr, "Failure setting data for \"%s\" - %s -- [first_primary] data out of order?\n",
                            buffer, u_errorName(*status));
                    return NULL;
                }
#endif
            } else if(what_to_do == READUNIFIEDIDEOGRAPH) {
                pointer = buffer+vtLen;
                UVector32 unihan(*status);
                if(U_FAILURE(*status)) { return NULL; }
                for(;;) {
                    skipWhiteSpace(&pointer, status);
                    if(*pointer == ']') { break; }
                    if(*pointer == 0) {
                        // Missing ] after ranges.
                        *status = U_INVALID_FORMAT_ERROR;
                        return NULL;
                    }
                    char *s = pointer;
                    while(*s != ' ' && *s != '\t' && *s != ']' && *s != '\0') { ++s; }
                    char c = *s;
                    *s = 0;
                    uint32_t start, end;
                    u_parseCodePointRange(pointer, &start, &end, status);
                    *s = c;
                    if(U_FAILURE(*status)) {
                        fprintf(stderr, "Syntax error: unable to parse one of the ranges from line '%s'\n", buffer);
                        *status = U_INVALID_FORMAT_ERROR;
                        return NULL;
                    }
                    unihan.addElement((UChar32)start, *status);
                    unihan.addElement((UChar32)end, *status);
                    pointer = s;
                }
                builder.initHanRanges(unihan.getBuffer(), unihan.size(), *status);
            } else if (what_to_do == READCE) {
              // TODO: combine & clean up the two CE parsers
#if 0  // TODO
              pointer = strchr(buffer+vtLen, '[');
              if(pointer) {
                pointer++;
                element->sizePrim[0]=readElement(&pointer, primary, ',', status) / 2;
                element->sizeSec[0]=readElement(&pointer, secondary, ',', status) / 2;
                element->sizeTer[0]=readElement(&pointer, tertiary, ']', status) / 2;
                vt[cnt].what[0] = getSingleCEValue(primary, secondary, tertiary, status);
                if(element->sizePrim[0] > 2 || element->sizeSec[0] > 1 || element->sizeTer[0] > 1) {
                  uint32_t CEi = 1;
                  uint32_t value = UCOL_CONTINUATION_MARKER; /* Continuation marker */
                    if(2*CEi<element->sizePrim[i]) {
                        value |= ((hex2num(*(primary+4*CEi))&0xF)<<28);
                        value |= ((hex2num(*(primary+4*CEi+1))&0xF)<<24);
                    }

                    if(2*CEi+1<element->sizePrim[i]) {
                        value |= ((hex2num(*(primary+4*CEi+2))&0xF)<<20);
                        value |= ((hex2num(*(primary+4*CEi+3))&0xF)<<16);
                    }

                    if(CEi<element->sizeSec[i]) {
                        value |= ((hex2num(*(secondary+2*CEi))&0xF)<<12);
                        value |= ((hex2num(*(secondary+2*CEi+1))&0xF)<<8);
                    }

                    if(CEi<element->sizeTer[i]) {
                        value |= ((hex2num(*(tertiary+2*CEi))&0x3)<<4);
                        value |= (hex2num(*(tertiary+2*CEi+1))&0xF);
                    }

                    CEi++;

                    vt[cnt].what[1] = value;
                    //element->CEs[CEindex++] = value;
                } else {
                  vt[cnt].what[1] = 0;
                }
              } else {
                fprintf(stderr, "Failed to read a CE from line %s\n", buffer);
              }
#endif
            } else if (what_to_do == READUCAVERSION) { //vt[cnt].what_to_do == READUCAVERSION
              u_versionFromString(UCAVersion, buffer+vtLen);
              if(beVerbose) {
                char uca[U_MAX_VERSION_STRING_LENGTH];
                u_versionToString(UCAVersion, uca);
                printf("UCA version %s\n", uca);
              }
              UVersionInfo UCDVersion;
              u_getUnicodeVersion(UCDVersion);
              if (UCAVersion[0] != UCDVersion[0] || UCAVersion[1] != UCDVersion[1]) {
                char uca[U_MAX_VERSION_STRING_LENGTH];
                char ucd[U_MAX_VERSION_STRING_LENGTH];
                u_versionToString(UCAVersion, uca);
                u_versionToString(UCDVersion, ucd);
                // Warning, not error, to permit bootstrapping during a version upgrade.
                fprintf(stderr, "warning: UCA version %s != UCD version %s (temporarily change the FractionalUCA.txt UCA version during Unicode version upgrade)\n", uca, ucd);
                // *status = U_INVALID_FORMAT_ERROR;
                // return NULL;
              }
            } else if (what_to_do == READLEADBYTETOSCRIPTS) { //vt[cnt].what_to_do == READLEADBYTETOSCRIPTS
                pointer = buffer + vtLen;
                skipWhiteSpace(&pointer, status);

                uint16_t leadByte = (hex2num(*pointer++) * 16);
                leadByte += hex2num(*pointer++);
                //printf("~~~~ processing lead byte = %02x\n", leadByte);
                skipWhiteSpace(&pointer, status);

                UnicodeString scripts;
                char scriptName[100];
                int32_t elementLength = 0;
                while ((elementLength = readElement(&pointer, scriptName, ' ', status)) > 0) {
                    if (scriptName[0] == ']') {
                        break;
                    }
                    int32_t reorderCode = getReorderCode(scriptName);
                    if (reorderCode == -3) {  // COMPRESS
                        builder.setCompressibleLeadByte(leadByte);
                        continue;
                    }
                    if (reorderCode == -2) {
                        continue;  // Ignore "TERMINATOR" etc.
                    }
                    if (reorderCode < 0 || 0xffff < reorderCode) {
                        printf("Syntax error: unable to parse reorder code from '%s'\n", scriptName);
                        *status = U_INVALID_FORMAT_ERROR;
                        return NULL;
                    }
                    scripts.append((UChar)reorderCode);
                }
                if(!scripts.isEmpty()) {
                    if(leadByteScripts == NULL) {
                        leadByteScripts = new UnicodeString[256];
                    }
                    leadByteScripts[leadByte] = scripts;
                }
            }
            return NULL;
        }
      }
      fprintf(stderr, "Warning: unrecognized option: %s\n", buffer);
      //*status = U_INVALID_FORMAT_ERROR;
      return NULL;
    }

    startCodePoint = buffer;
    endCodePoint = strchr(startCodePoint, ';');

    if(endCodePoint == 0) {
        fprintf(stderr, "error - line with no code point!\n");
        *status = U_INVALID_FORMAT_ERROR; /* No code point - could be an error, but probably only an empty line */
        return NULL;
    } else {
        *(endCodePoint) = 0;
    }

    char *pipePointer = strchr(buffer, '|');
    if (pipePointer != NULL) {
        // Read the prefix string which precedes the actual string.
        *pipePointer = 0;
        element->prefixSize =
            u_parseString(startCodePoint,
                          element->prefixChars, LENGTHOF(element->prefixChars),
                          NULL, status);
        if(U_FAILURE(*status)) {
            fprintf(stderr, "error - parsing of prefix \"%s\" failed: %s\n",
                    startCodePoint, u_errorName(*status));
            *status = U_INVALID_FORMAT_ERROR;
            return NULL;
        }
        element->prefix = element->prefixChars;
        startCodePoint = pipePointer + 1;
    }

    // Read the string which gets the CE(s) assigned.
    element->cSize =
        u_parseString(startCodePoint,
                      element->uchars, LENGTHOF(element->uchars),
                      NULL, status);
    if(U_FAILURE(*status)) {
        fprintf(stderr, "error - parsing of code point(s) \"%s\" failed: %s\n",
                startCodePoint, u_errorName(*status));
        *status = U_INVALID_FORMAT_ERROR;
        return NULL;
    }
    element->cPoints = element->uchars;

    startCodePoint = endCodePoint+1;

    commentStart = strchr(startCodePoint, '#');
    if(commentStart == NULL) {
        commentStart = strlen(startCodePoint) + startCodePoint;
    }

    i = 0;
    cesLength = 0;
    element->noOfCEs = 0;
    for(;;) {
        endCodePoint = strchr(startCodePoint, ']');
        if(endCodePoint == NULL || endCodePoint >= commentStart) {
            break;
        }
        pointer = strchr(startCodePoint, '[');
        pointer++;

        readElement(&pointer, primary, ',', status);
        readElement(&pointer, secondary, ',', status);
        readElement(&pointer, tertiary, ']', status);
        ces[cesLength++] = getSingleCEValue(builder, primary, secondary, tertiary, status);
        // TODO: max 31 CEs

        startCodePoint = endCodePoint+1;
        i++;
    }

    // we don't want any strange stuff after useful data!
    if (pointer == NULL) {
        /* huh? Did we get ']' without the '['? Pair your brackets! */
        *status=U_INVALID_FORMAT_ERROR;
    }
    else {
        while(pointer < commentStart)  {
            if(*pointer != ' ' && *pointer != '\t')
            {
                *status=U_INVALID_FORMAT_ERROR;
                break;
            }
            pointer++;
        }
    }
    if(element->cSize == 1 && element->cPoints[0] == 0xfffe) {
        // UCA 6.0 gives U+FFFE a special minimum weight using the
        // byte 02 which is the merge-sort-key separator and illegal for any
        // other characters.
    } else {
        // Rudimentary check for valid bytes in CE weights.
        // For a more comprehensive check see cintltst /tscoll/citertst/TestCEValidity
        for (i = 0; i < cesLength; ++i) {
            int64_t ce = ces[i];
            for (int j = 7; j >= 0; --j) {
                uint8_t b = (uint8_t)(ce >> (j * 8));
                if(j <= 1) { b &= 0x3f; }  // tertiary bytes use 6 bits
                if (b == 1) {
                    fprintf(stderr, "Warning: invalid UCA weight byte 01 for %s\n", buffer);
                    return NULL;
                }
                if ((j == 7 || j == 3 || j == 1) && b == 2) {
                    fprintf(stderr, "Warning: invalid UCA weight lead byte 02 for %s\n", buffer);
                    return NULL;
                }
                // Primary second bytes 03 and FF are compression terminators.
                // TODO: 02 & 03 are usable when the lead byte is not compressible.
                // 02 is unusable and 03 is the low compression terminator when the lead byte is compressible.
                if (j == 6 && b == 0xff) {
                    fprintf(stderr, "Warning: invalid UCA primary second weight byte %02X for %s\n",
                            b, buffer);
                    return NULL;
                }
            }
        }
    }

    if(U_FAILURE(*status)) {
        fprintf(stderr, "problem putting stuff in hash table %s\n", u_errorName(*status));
        *status = U_INTERNAL_PROGRAM_ERROR;
        return NULL;
    }

    return element;
}

static int32_t
diffThreeBytePrimaries(uint32_t p1, uint32_t p2, UBool isCompressible) {
    if((p1 & 0xffff0000) == (p2 & 0xffff0000)) {
        // Same first two bytes.
        return (int32_t)(p2 - p1) >> 8;
    } else {
        // Third byte: 254 bytes 02..FF
        int32_t linear1 = (int32_t)((p1 >> 8) & 0xff) - 2;
        int32_t linear2 = (int32_t)((p2 >> 8) & 0xff) - 2;
        int32_t factor;
        if(isCompressible) {
            // Second byte for compressible lead byte: 251 bytes 04..FE
            linear1 += 254 * ((int32_t)((p1 >> 16) & 0xff) - 4);
            linear2 += 254 * ((int32_t)((p2 >> 16) & 0xff) - 4);
            factor = 251 * 254;
        } else {
            // Second byte for incompressible lead byte: 254 bytes 02..FF
            linear1 += 254 * ((int32_t)((p1 >> 16) & 0xff) - 2);
            linear2 += 254 * ((int32_t)((p2 >> 16) & 0xff) - 2);
            factor = 254 * 254;
        }
        linear1 += factor * (int32_t)((p1 >> 24) & 0xff);
        linear2 += factor * (int32_t)((p2 >> 24) & 0xff);
        return linear2 - linear1;
    }
}

static void
parseFractionalUCA(const char *filename,
                   CollationBaseDataBuilder &builder,
                   UErrorCode *status)
{
    FILE *data = fopen(filename, "r");
    if(data == NULL) {
        fprintf(stderr, "Couldn't open file: %s\n", filename);
        *status = U_FILE_ACCESS_ERROR;
        return;
    }
    uint32_t line = 0;
    UCAElements *element = NULL;
    // TODO: Turn UCAConstants into fields in the inverse-base table.
    // Pass the inverse-base builder into the parsing function.
    UCAConstants consts;
    uprv_memset(&consts, 0, sizeof(consts));

    /* first set up constants for implicit calculation */
    uprv_uca_initImplicitConstants(status);

    // TODO: uprv_memset(inverseTable, 0xDA, sizeof(int32_t)*3*0xFFFF);

    if(U_FAILURE(*status))
    {
        fprintf(stderr, "Failed to initialize data structures for parsing FractionalUCA.txt - %s\n", u_errorName(*status));
        fclose(data);
        return;
    }

    UChar32 maxCodePoint = 0;
    int32_t numArtificialSecondaries = 0;
    int32_t numExtraSecondaryExpansions = 0;
    int32_t numExtraExpansionCEs = 0;
    int32_t numDiffTertiaries = 0;
    int32_t surrogateCount = 0;
    while(!feof(data)) {
        if(U_FAILURE(*status)) {
            fprintf(stderr, "Something returned an error %i (%s) while processing line %u of %s. Exiting...\n",
                *status, u_errorName(*status), (int)line, filename);
            exit(*status);
        }

        line++;
        if(beVerbose) {
          printf("%u ", (int)line);
        }
        int64_t ces[32];
        int32_t cesLength = 0;
        element = readAnElement(data, builder, &consts, ces, cesLength, status);
        if(element != NULL) {
            // we have read the line, now do something sensible with the read data!

#if 0  // TODO
            // if element is a contraction, we want to add it to contractions[]
            int32_t length = (int32_t)element->cSize;
            if(length > 1 && element->cPoints[0] != 0xFDD0) { // this is a contraction
              if(U16_IS_LEAD(element->cPoints[0]) && U16_IS_TRAIL(element->cPoints[1]) && length == 2) {
                surrogateCount++;
              } else {
                if(noOfContractions>=MAX_UCA_CONTRACTIONS) {
                  fprintf(stderr,
                          "\nMore than %d contractions. Please increase MAX_UCA_CONTRACTIONS in genuca.cpp. "
                          "Exiting...\n",
                          (int)MAX_UCA_CONTRACTIONS);
                  exit(U_BUFFER_OVERFLOW_ERROR);
                }
                if(length > MAX_UCA_CONTRACTION_LENGTH) {
                  fprintf(stderr,
                          "\nLine %d: Contraction of length %d is too long. Please increase MAX_UCA_CONTRACTION_LENGTH in genuca.cpp. "
                          "Exiting...\n",
                          (int)line, (int)length);
                  exit(U_BUFFER_OVERFLOW_ERROR);
                }
                UChar *t = &contractions[noOfContractions][0];
                u_memcpy(t, element->cPoints, length);
                t += length;
                for(; length < MAX_UCA_CONTRACTION_LENGTH; ++length) {
                    *t++ = 0;
                }
                noOfContractions++;
              }
            }
            else {
                // TODO (claireho): does this work? Need more tests
                // The following code is to handle the UCA pre-context rules
                // for L/l with middle dot. We share the structures for contractionCombos.
                // The format for pre-context character is
                // contractions[0]: codepoint in element->cPoints[0]
                // contractions[1]: '\0' to differentiate from a contraction
                // contractions[2]: prefix char
                if (element->prefixSize>0) {
                    if(length > 1 || element->prefixSize > 1) {
                        fprintf(stderr,
                                "\nLine %d: Character with prefix, "
                                "either too many characters or prefix too long.\n",
                                (int)line);
                        exit(U_INTERNAL_PROGRAM_ERROR);
                    }
                    if(noOfContractions>=MAX_UCA_CONTRACTIONS) {
                      fprintf(stderr,
                              "\nMore than %d contractions. Please increase MAX_UCA_CONTRACTIONS in genuca.cpp. "
                              "Exiting...\n",
                              (int)MAX_UCA_CONTRACTIONS);
                      exit(U_BUFFER_OVERFLOW_ERROR);
                    }
                    UChar *t = &contractions[noOfContractions][0];
                    t[0]=element->cPoints[0];
                    t[1]=0;
                    t[2]=element->prefixChars[0];
                    t += 3;
                    for(length = 3; length < MAX_UCA_CONTRACTION_LENGTH; ++length) {
                        *t++ = 0;
                    }
                    noOfContractions++;
                }
            }
#endif

#if 0  // TODO
            /* we're first adding to inverse, because addAnElement will reverse the order */
            /* of code points and stuff... we don't want that to happen */
            if((element->CEs[0] >> 24) != 2) {
                // Add every element except for the special minimum-weight character U+FFFE
                // which has 02 weights.
                // If we had 02 weights in the invuca table, then tailoring primary
                // after an ignorable would try to put a weight before 02 which is not valid.
                // We could fix this in a complicated way in the from-rule-string builder,
                // but omitting this special element from invuca is simple and effective.
                addToInverse(element, status);
            }
#endif
            if(!((int32_t)element->cSize > 1 && element->cPoints[0] == 0xFDD0)) {
                UnicodeString prefix(FALSE, element->prefixChars, (int32_t)element->prefixSize);
                UnicodeString s(FALSE, element->cPoints, (int32_t)element->cSize);

                UChar32 c = s.char32At(0);
                if(c > maxCodePoint) { maxCodePoint = c; }

                // We ignore the CEs for U+FFFD..U+FFFF.
                // CollationBaseDataBuilder::initBase() maps them to special CEs.
                // (U+FFFD & U+FFFF have higher primaries in v2 than in FractionalUCA.txt.)
                if(0xfffd <= c && c <= 0xffff) { continue; }

                builder.add(prefix, s, ces, cesLength, *status);

                uint32_t p = (uint32_t)(ces[0] >> 32);
                if(cesLength >= 2 && p != 0) {
                    int64_t prevCE = ces[0];
                    for(int32_t i = 1; i < cesLength; ++i) {
                        int64_t ce = ces[i];
                        if((uint32_t)(ce >> 32) == 0 && ((uint32_t)ce >> 16) > 0xdb99 && (uint32_t)(prevCE >> 32) != 0) {
                            ++numArtificialSecondaries;
                            if(cesLength == 2) {
                                ++numExtraSecondaryExpansions;
                                numExtraExpansionCEs += 2;
                            } else {
                                ++numExtraExpansionCEs;
                            }
                            if(((uint32_t)prevCE & 0x3f3f) != ((uint32_t)ce & 0x3f3f)) {
                                ++numDiffTertiaries;
                            }
                        }
                        prevCE = ce;
                    }
                }
            }
        }
    }

    // TODO: Remove analysis & output.
    printf("*** number of artificial secondary CEs:                 %6ld\n", (long)numArtificialSecondaries);
    printf("    number of 2-CE expansions that could be single CEs: %6ld\n", (long)numExtraSecondaryExpansions);
    printf("    number of extra expansion CEs:                      %6ld\n", (long)numExtraExpansionCEs);
    printf("    number of artificial sec. CEs w. diff. tertiaries:  %6ld\n", (long)numDiffTertiaries);

    int32_t numRanges = 0;
    int32_t numRangeCodePoints = 0;
    UChar32 rangeFirst = U_SENTINEL;
    UChar32 rangeLast = U_SENTINEL;
    uint32_t rangeFirstPrimary = 0;
    uint32_t rangeLastPrimary = 0;
    int32_t rangeStep = -1;

    // Detect ranges of characters in primary code point order,
    // with 3-byte primaries and
    // with consistent "step" differences between adjacent primaries.
    // TODO: Modify the FractionalUCA generator to use the same primary-weight incrementation.
    // Start at U+0180: No ranges for common Latin characters.
    // Go one beyond maxCodePoint in case a range ends there.
    for(UChar32 c = 0x180; c <= (maxCodePoint + 1); ++c) {
        UBool action;
        uint32_t p = builder.getLongPrimaryIfSingleCE(c);
        if(p != 0) {
            // p is a "long" (three-byte) primary.
            if(rangeFirst >= 0 && c == (rangeLast + 1) && p > rangeLastPrimary) {
                // Find the offset between the two primaries.
                int32_t step = diffThreeBytePrimaries(
                    rangeLastPrimary, p, builder.isCompressiblePrimary(p));
                if(rangeFirst == rangeLast && step >= 2) {
                    // c == rangeFirst + 1, store the "step" between range primaries.
                    rangeStep = step;
                    rangeLast = c;
                    rangeLastPrimary = p;
                    action = 0;  // continue range
                } else if(rangeStep == step) {
                    // Continue the range with the same "step" difference.
                    rangeLast = c;
                    rangeLastPrimary = p;
                    action = 0;  // continue range
                } else {
                    action = 1;  // maybe finish range, start a new one
                }
            } else {
                action = 1;  // maybe finish range, start a new one
            }
        } else {
            action = -1;  // maybe finish range, do not start a new one
        }
        if(action != 0 && rangeFirst >= 0) {
            // Finish a range.
            // Set offset CE32s for a long range, leave single CEs for a short range.
            UBool didSetRange = builder.maybeSetPrimaryRange(
                rangeFirst, rangeLast,
                rangeFirstPrimary, rangeStep, *status);
            if(U_FAILURE(*status)) {
                fprintf(stderr,
                        "failure setting code point order range U+%04lx..U+%04lx "
                        "%08lx..%08lx step %d - %s\n",
                        (long)rangeFirst, (long)rangeLast,
                        (long)rangeFirstPrimary, (long)rangeLastPrimary,
                        (int)rangeStep, u_errorName(*status));
            } else if(didSetRange) {
                int32_t rangeLength = rangeLast - rangeFirst + 1;
                /*printf("* set code point order range U+%04lx..U+%04lx [%d] "
                        "%08lx..%08lx step %d\n",
                        (long)rangeFirst, (long)rangeLast,
                        (int)rangeLength,
                        (long)rangeFirstPrimary, (long)rangeLastPrimary,
                        (int)rangeStep);*/
                ++numRanges;
                numRangeCodePoints += rangeLength;
            }
            rangeFirst = U_SENTINEL;
            rangeStep = -1;
        }
        if(action > 0) {
            // Start a new range.
            rangeFirst = rangeLast = c;
            rangeFirstPrimary = rangeLastPrimary = p;
        }
    }
    printf("** set %d ranges with %d code points\n", (int)numRanges, (int)numRangeCodePoints);

    // TODO: Probably best to work in two passes.
    // Pass 1 for reading all data, setting isCompressible flags (and reordering groups)
    // and finding ranges.
    // Then set the ranges in a newly initialized builder
    // for optimal compression (makes sure that adjacent blocks can overlap easily).
    // Then set all mappings outside the ranges.
    //
    // In the first pass, we could store mappings in a simple list,
    // with single-character/single-long-primary-CE mappings in a UTrie2;
    // or store the mappings in a temporary builder;
    // or we could just parse the input file again in the second pass.
    //
    // Ideally set/copy U+0000..U+017F before setting anything else,
    // then set default Han/Hangul, then set the ranges, then copy non-range mappings.
    // It should be easy to copy mappings from an un-built builder to a new one.
    // Add CollationDataBuilder::copyFrom(builder, code point, errorCode) -- copy contexts & expansions.

    // TODO: Analyse why CLDR root collation data is large.
    // Size of expansions for canonical decompositions?
    // Size of expansions for compatibility decompositions?
    // For the latter, consider storing some of them as specials,
    //   decompose at runtime?? Store unusual tertiary in special CE32?

    if(UCAVersion[0] == 0 && UCAVersion[1] == 0 && UCAVersion[2] == 0 && UCAVersion[3] == 0) {
        fprintf(stderr, "UCA version not specified. Cannot create data file!\n");
        fclose(data);
        return;
    }

    if (beVerbose) {
        printf("\nLines read: %u\n", (int)line);
        printf("Surrogate count: %i\n", (int)surrogateCount);
        printf("Raw data breakdown:\n");
        /*printf("Compact array stage1 top: %i, stage2 top: %i\n", t->mapping->stage1Top, t->mapping->stage2Top);*/
        // TODO: printf("Number of contractions: %u\n", (int)noOfContractions);
        // TODO: printf("Contraction image size: %u\n", (int)t->image->contractionSize);
        // TODO: printf("Expansions size: %i\n", (int)t->expansions->position);
    }


    /* produce canonical closure for table */
#if 0  // TODO
    /* do the closure */
    UnicodeSet closed;
    int32_t noOfClosures = uprv_uca_canonicalClosure(t, NULL, &closed, status);
    if(noOfClosures != 0) {
        fprintf(stderr, "Warning: %i canonical closures occured!\n", (int)noOfClosures);
        UnicodeString pattern;
        std::string utf8;
        closed.toPattern(pattern, TRUE).toUTF8String(utf8);
        fprintf(stderr, "UTF-8 pattern string: %s\n", utf8.c_str());
    }

    /* test */
    UCATableHeader *myData = uprv_uca_assembleTable(t, status);  

    if (beVerbose) {
        printf("Compacted data breakdown:\n");
        /*printf("Compact array stage1 top: %i, stage2 top: %i\n", t->mapping->stage1Top, t->mapping->stage2Top);*/
        printf("Number of contractions: %u\n", (int)noOfContractions);
        printf("Contraction image size: %u\n", (int)t->image->contractionSize);
        printf("Expansions size: %i\n", (int)t->expansions->position);
    }

    if(U_FAILURE(*status)) {
        fprintf(stderr, "Error creating table: %s\n", u_errorName(*status));
        fclose(data);
        return -1;
    }

    /* populate the version info struct with version info*/
    myData->version[0] = UCOL_BUILDER_VERSION;
    myData->version[1] = UCAVersion[0];
    myData->version[2] = UCAVersion[1];
    myData->version[3] = UCAVersion[2];
    /*TODO:The fractional rules version should be taken from FractionalUCA.txt*/
    // Removed this macro. Instead, we use the fields below
    //myD->version[1] = UCOL_FRACTIONAL_UCA_VERSION;
    //myD->UCAVersion = UCAVersion; // out of FractionalUCA.txt
    uprv_memcpy(myData->UCAVersion, UCAVersion, sizeof(UVersionInfo));
    u_getUnicodeVersion(myData->UCDVersion);

    writeOutData(myData, &consts, &leadByteConstants, contractions, noOfContractions, outputDir, copyright, status);
    uprv_free(myData);

    InverseUCATableHeader *inverse = assembleInverseTable(status);
    uprv_memcpy(inverse->UCAVersion, UCAVersion, sizeof(UVersionInfo));
    writeOutInverseData(inverse, outputDir, copyright, status);
    uprv_free(inverse);
#endif

    fclose(data);

    return;
}

static uint8_t trieBytes[300 * 1024];
static uint16_t unsafeBwdSetWords[30 * 1024];

static void
buildAndWriteBaseData(CollationBaseDataBuilder &builder,
                      const char *path, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }

    if(leadByteScripts != NULL) {
        uint32_t firstLead = Collation::MERGE_SEPARATOR_BYTE + 1;
        do {
            // Find the range of lead bytes with this set of scripts.
            const UnicodeString &firstScripts = leadByteScripts[firstLead];
            if(firstScripts.isEmpty()) {
                fprintf(stderr, "[top_byte 0x%02X] has no reorderable scripts\n", (int)firstLead);
                errorCode = U_INVALID_FORMAT_ERROR;
                return;
            }
            uint32_t lead = firstLead;
            for(;;) {
                ++lead;
                const UnicodeString &scripts = leadByteScripts[lead];
                // The scripts should either be the same or disjoint.
                // We do not test if all reordering groups have disjoint sets of scripts.
                if(scripts.isEmpty() || firstScripts.indexOf(scripts[0]) < 0) { break; }
                if(scripts != firstScripts) {
                    fprintf(stderr,
                            "[top_byte 0x%02X] includes script %d from [top_byte 0x%02X] "
                            "but not all scripts match\n",
                            (int)firstLead, scripts[0], (int)lead);
                    errorCode = U_INVALID_FORMAT_ERROR;
                    return;
                }
            }
            // lead is one greater than the last lead byte with the same set of scripts as firstLead.
            if(firstScripts.indexOf(USCRIPT_HAN) >= 0) {
                // Extend the Hani range to the end of what this implementation uses.
                // FractionalUCA.txt assumes a different algorithm for implicit primary weights,
                // and different high-lead byte ranges.
                lead = Collation::UNASSIGNED_IMPLICIT_BYTE;
            }
            builder.addReorderingGroup(firstLead, lead - 1, firstScripts, errorCode);
            if(U_FAILURE(errorCode)) { return; }
            firstLead = lead;
        } while(firstLead < Collation::UNASSIGNED_IMPLICIT_BYTE);
        delete[] leadByteScripts;
    }

    LocalPointer<CollationData> cd(builder.buildBaseData(errorCode));
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "builder.buildBaseData() failed: %s\n",
                u_errorName(errorCode));
        return;
    }
    cd->variableTop = gVariableTop;  // TODO

    int32_t indexes[CollationData::IX_COUNT]={
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    indexes[CollationData::IX_INDEXES_LENGTH] = CollationData::IX_COUNT;
    indexes[CollationData::IX_OPTIONS] = cd->numericPrimary | cd->options;

    printf("*** CLDR root collation part sizes ***\n");
    int32_t totalSize = (int32_t)sizeof(indexes);

    indexes[CollationData::IX_TRIE_OFFSET] = totalSize;
    int32_t trieSize = builder.serializeTrie(trieBytes, LENGTHOF(trieBytes), errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "builder.serializeTrie()=%ld failed: %s\n",
                (long)trieSize, u_errorName(errorCode));
        return;
    }
    printf("  trie size:                    %6ld\n", (long)trieSize);
    totalSize += trieSize;

    // cePadding should be 0 due to the way compactIndex2(UNewTrie2 *trie)
    // currently works (trieSize should be a multiple of 8 bytes).
    int32_t cePadding = (totalSize & 7) == 0 ? 0 : 4;
    totalSize += cePadding;

    indexes[CollationData::IX_RESERVED5_OFFSET] = totalSize;
    indexes[CollationData::IX_CES_OFFSET] = totalSize;
    int32_t length = builder.lengthOfCEs();
    printf("  CEs:              %6ld *8 = %6ld\n", (long)length, (long)length * 8);
    totalSize += length * 8;

    indexes[CollationData::IX_RESERVED7_OFFSET] = totalSize;
    indexes[CollationData::IX_CE32S_OFFSET] = totalSize;
    length = builder.lengthOfCE32s();
    printf("  CE32s:            %6ld *4 = %6ld\n", (long)length, (long)length * 4);
    totalSize += length * 4;

    indexes[CollationData::IX_RESERVED9_OFFSET] = totalSize;
    indexes[CollationData::IX_REORDER_CODES_OFFSET] = totalSize;
    indexes[CollationData::IX_RESERVED11_OFFSET] = totalSize;
    indexes[CollationData::IX_CONTEXTS_OFFSET] = totalSize;
    length = builder.lengthOfContexts();
    printf("  contexts:         %6ld *2 = %6ld\n", (long)length, (long)length * 2);
    totalSize += length * 2;

    indexes[CollationData::IX_UNSAFE_BWD_OFFSET] = totalSize;
    int32_t unsafeBwdSetLength = builder.serializeUnsafeBackwardSet(
        unsafeBwdSetWords, LENGTHOF(unsafeBwdSetWords), errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "builder.serializeUnsafeBackwardSet()=%ld failed: %s\n",
                (long)unsafeBwdSetLength, u_errorName(errorCode));
        return;
    }
    printf("  unsafeBwdSet:     %6ld *2 = %6ld\n", (long)unsafeBwdSetLength, (long)unsafeBwdSetLength * 2);
    totalSize += unsafeBwdSetLength * 2;

    indexes[CollationData::IX_SCRIPTS_OFFSET] = totalSize;
    length = cd->scriptsLength;
    printf("  scripts data:     %6ld *2 = %6ld\n", (long)length, (long)length * 2);
    totalSize += length * 2;

    indexes[CollationData::IX_RESERVED15_OFFSET] = totalSize;
    indexes[CollationData::IX_COMPRESSIBLE_BYTES_OFFSET] = totalSize;
    printf("  compressibleBytes:               256\n");
    totalSize += 256;

    indexes[CollationData::IX_REORDER_TABLE_OFFSET] = totalSize;
    indexes[CollationData::IX_RESERVED18_OFFSET] = totalSize;
    indexes[CollationData::IX_TOTAL_SIZE] = totalSize;
    printf("*** CLDR root collation size:   %6ld\n", (long)totalSize);

    UNewDataMemory *pData=udata_create(path, "icu", "ucadata2", &ucaDataInfo,
                                       withCopyright ? U_COPYRIGHT_STRING : NULL, &errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "genuca: udata_create(%s, ucadata.icu) failed - %s\n",
                path, u_errorName(errorCode));
        return;
    }

    indexes[CollationData::IX_JAMO_CES_START] = cd->jamoCEs - cd->ces;

    udata_writeBlock(pData, indexes, sizeof(indexes));
    udata_writeBlock(pData, trieBytes, trieSize);
    udata_writePadding(pData, cePadding);
    udata_writeBlock(pData, cd->ces, builder.lengthOfCEs() * 8);
    udata_writeBlock(pData, cd->ce32s, builder.lengthOfCE32s() * 4);
    udata_writeBlock(pData, cd->contexts, builder.lengthOfContexts() * 2);
    udata_writeBlock(pData, unsafeBwdSetWords, unsafeBwdSetLength * 2);
    udata_writeBlock(pData, cd->scripts, cd->scriptsLength * 2);
    udata_writeBlock(pData, cd->compressibleBytes, 256);

    long dataLength = udata_finish(pData, &errorCode);
    if(U_FAILURE(errorCode)) {
        fprintf(stderr, "genuca: error %s writing the output file\n", u_errorName(errorCode));
        return;
    }

    if(dataLength != (long)totalSize) {
        fprintf(stderr,
                "udata_finish(ucadata.icu) reports %ld bytes written but should be %ld\n",
                dataLength, (long)totalSize);
        errorCode=U_INTERNAL_PROGRAM_ERROR;
    }
}

/**
 * Adds each lead surrogate to the bmp set if any of the 1024
 * associated supplementary code points is in the supp set.
 * These can be one and the same set.
 */
static void
setLeadSurrogatesForAssociatedSupplementary(UnicodeSet &bmp, const UnicodeSet &supp) {
    UChar32 c = 0x10000;
    for(UChar lead = 0xd800; lead < 0xdc00; ++lead, c += 0x400) {
        if(supp.containsSome(c, c + 0x3ff)) {
            bmp.add(lead);
        }
    }
}

static int32_t
makeBMPFoldedBitSet(const UnicodeSet &set, uint8_t index[0x800], uint32_t bits[256],
                    UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    bits[0] = 0;  // no bits set
    bits[1] = 0xffffffff;  // all bits set
    int32_t bitsLength = 2;
    int32_t i = 0;
    for(UChar32 c = 0; c <= 0xffff; c += 0x20, ++i) {
        if(set.containsNone(c, c + 0x1f)) {
            index[i] = 0;
        } else if(set.contains(c, c + 0x1f)) {
            index[i] = 1;
        } else {
            uint32_t b = 0;
            for(int32_t j = 0; j <= 0x1f; ++j) {
                if(set.contains(c + j)) {
                    b |= (uint32_t)1 << j;
                }
            }
            int32_t k;
            for(k = 2;; ++k) {
                if(k == bitsLength) {
                    // new bit combination
                    if(bitsLength == 256) {
                        errorCode = U_BUFFER_OVERFLOW_ERROR;
                        return 0;
                    }
                    bits[bitsLength++] = b;
                    break;
                }
                if(bits[k] == b) {
                    // duplicate bit combination
                    break;
                }
            }
            index[i] = k;
        }
    }
    return bitsLength;
}

// TODO: Make preparseucd.py write fcd_data.h mapping code point ranges to FCD16 values,
// use that rather than properties APIs.
// Then consider moving related logic for the unsafeBwdSet back from the loader into this builder.

/**
 * Builds data for the FCD check fast path.
 * For details see the CollationFCD class comments.
 */
static void
buildAndWriteFCDData(const char *path, UErrorCode &errorCode) {
    UnicodeSet lcccSet(UNICODE_STRING_SIMPLE("[[:^lccc=0:][\\udc00-\\udfff]]"), errorCode);
    UnicodeSet tcccSet(UNICODE_STRING_SIMPLE("[:^tccc=0:]"), errorCode);
    setLeadSurrogatesForAssociatedSupplementary(tcccSet, tcccSet);
    // The following supp(lccc)->lead(tccc) should be unnecessary
    // after the previous supp(tccc)->lead(tccc)
    // because there should not be any characters with lccc!=0 and tccc=0.
    // It is safe and harmless.
    setLeadSurrogatesForAssociatedSupplementary(tcccSet, lcccSet);
    setLeadSurrogatesForAssociatedSupplementary(lcccSet, lcccSet);
    uint8_t lcccIndex[0x800], tcccIndex[0x800];
    uint32_t lcccBits[256], tcccBits[256];
    int32_t lcccBitsLength = makeBMPFoldedBitSet(lcccSet, lcccIndex, lcccBits, errorCode);
    int32_t tcccBitsLength = makeBMPFoldedBitSet(tcccSet, tcccIndex, tcccBits, errorCode);
    printf("@@@ lcccBitsLength=%d -> %d bytes\n", lcccBitsLength, 0x800 + lcccBitsLength * 4);
    printf("@@@ tcccBitsLength=%d -> %d bytes\n", tcccBitsLength, 0x800 + tcccBitsLength * 4);

    if(U_FAILURE(errorCode)) { return; }

    FILE *f=usrc_create(path, "collationfcd.cpp",
                        "icu/tools/unicode/c/genuca/genuca.cpp");
    if(f==NULL) {
        errorCode=U_FILE_ACCESS_ERROR;
        return;
    }
    fputs("#include \"unicode/utypes.h\"\n\n", f);
    fputs("#if !UCONFIG_NO_COLLATION\n\n", f);
    fputs("#include \"collationfcd.h\"\n\n", f);
    fputs("U_NAMESPACE_BEGIN\n\n", f);
    usrc_writeArray(f,
        "const uint8_t CollationFCD::lcccIndex[%ld]={\n",
        lcccIndex, 8, 0x800,
        "\n};\n\n");
    usrc_writeArray(f,
        "const uint32_t CollationFCD::lcccBits[%ld]={\n",
        lcccBits, 32, lcccBitsLength,
        "\n};\n\n");
    usrc_writeArray(f,
        "const uint8_t CollationFCD::tcccIndex[%ld]={\n",
        tcccIndex, 8, 0x800,
        "\n};\n\n");
    usrc_writeArray(f,
        "const uint32_t CollationFCD::tcccBits[%ld]={\n",
        tcccBits, 32, tcccBitsLength,
        "\n};\n\n");
    fputs("U_NAMESPACE_END\n\n", f);
    fputs("#endif  // !UCONFIG_NO_COLLATION\n", f);
    fclose(f);
}

static void
parseAndWriteCollationRootData(
        const char *fracUCAPath,
        const char *binaryDataPath,
        const char *sourceCodePath,
        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    CollationBaseDataBuilder builder(errorCode);
    builder.initBase(errorCode);
    parseFractionalUCA(fracUCAPath, builder, &errorCode);
    // TODO: remove artificial "tailoring", replace with real tailoring
    // Set U+FFF1 to a tertiary CE.
    // It must have uppercase bits.
    // Its tertiary weight must be higher than that of any primary or secondary CE.
    UnicodeString prefix;
    UnicodeString s((UChar)0xfff1);
    int64_t ce = 0x8000 | 0x3ff0;
    builder.add(prefix, s, &ce, 1, errorCode);
    // Set U+FFF2 to the next tertiary CE after a gap.
    s.setTo((UChar)0xfff2);
    ce += 2;
    builder.add(prefix, s, &ce, 1, errorCode);
    // end artificial tailoring
    buildAndWriteBaseData(builder, binaryDataPath, errorCode);
    buildAndWriteFCDData(sourceCodePath, errorCode);
}

// ------------------------------------------------------------------------- ***

enum {
    HELP_H,
    HELP_QUESTION_MARK,
    VERBOSE,
    COPYRIGHT
};

static UOption options[]={
    UOPTION_HELP_H,
    UOPTION_HELP_QUESTION_MARK,
    UOPTION_VERBOSE,
    UOPTION_COPYRIGHT
};

extern "C" int
main(int argc, char* argv[]) {
    U_MAIN_INIT_ARGS(argc, argv);

    argc=u_parseArgs(argc, argv, LENGTHOF(options), options);

    /* error handling, printing usage message */
    if(argc<0) {
        fprintf(stderr,
            "error in command line argument \"%s\"\n",
            argv[-argc]);
    }
    if( argc<2 ||
        options[HELP_H].doesOccur || options[HELP_QUESTION_MARK].doesOccur
    ) {
        /*
         * Broken into chunks because the C89 standard says the minimum
         * required supported string length is 509 bytes.
         */
        fprintf(stderr,
            "Usage: %s [-options] path/to/ICU/src/root\n"
            "\n"
            "Reads path/to/ICU/src/root/source/data/unidata/FractionalUCA.txt and\n"
            "writes source and binary data files with the collation root data.\n"
            "\n",
            argv[0]);
        fprintf(stderr,
            "Options:\n"
            "\t-h or -? or --help  this usage text\n"
            "\t-v or --verbose     verbose output\n"
            "\t-c or --copyright   include a copyright notice\n");
        return argc<0 ? U_ILLEGAL_ARGUMENT_ERROR : U_ZERO_ERROR;
    }

    beVerbose=options[VERBOSE].doesOccur;
    withCopyright=options[COPYRIGHT].doesOccur;

    IcuToolErrorCode errorCode("genuca2");

    CharString icuSrcRoot(argv[1], errorCode);

    CharString icuSource(icuSrcRoot, errorCode);
    icuSource.appendPathPart("source", errorCode);

    CharString icuSourceData(icuSource, errorCode);
    icuSourceData.appendPathPart("data", errorCode);

    CharString fracUCAPath(icuSourceData, errorCode);
    fracUCAPath.appendPathPart("unidata", errorCode);
    fracUCAPath.appendPathPart("FractionalUCA.txt", errorCode);

    CharString sourceDataInColl(icuSourceData, errorCode);
    sourceDataInColl.appendPathPart("in", errorCode);
    sourceDataInColl.appendPathPart("coll", errorCode);

    CharString sourceI18n(icuSource, errorCode);
    sourceI18n.appendPathPart("i18n", errorCode);

    errorCode.assertSuccess();

    parseAndWriteCollationRootData(
        fracUCAPath.data(),
        sourceDataInColl.data(),
        sourceI18n.data(),
        errorCode);

    return errorCode;
}
