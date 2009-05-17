/*
*******************************************************************************
*
*   Copyright (C) 2000-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File reslist.h
*
* Modification History:
*
*   Date        Name        Description
*   02/21/00    weiv        Creation.
*******************************************************************************
*/

#ifndef RESLIST_H
#define RESLIST_H

#define KEY_SPACE_SIZE 65536
#define RESLIST_MAX_INT_VECTOR 2048

#include "unicode/utypes.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "uresdata.h"
#include "cmemory.h"
#include "cstring.h"
#include "unewdata.h"
#include "ustr.h"
#include "uhash.h"

U_CDECL_BEGIN

/* experimental compression */
#define LAST_WINDOW_LIMIT 0x20000

typedef struct KeyMapEntry {
    int32_t oldpos, newpos;
} KeyMapEntry;

/* How do we store string values? */
enum {
    kStringsUTF16v1,    /* formatVersion 1: int length + UChars + NUL + padding to 4 bytes */
    kStringsUTF16v2,    /* formatVersion 2: optional length in 1..3 UChars + UChars + NUL */
    kStringsCompact     /* compact encoding, requires runtime decompacting and cache */
};

/* Resource bundle root table */
struct SRBRoot {
  struct SResource *fRoot;
  char *fLocale;
  int32_t fMaxTableLength;
  /* size of root's children, not counting keys or compact-string bytes */
  uint32_t fSizeExceptRoot;
  UBool noFallback; /* see URES_ATT_NO_FALLBACK */
  UBool fIsDoneParsing; /* set while processing & writing */
  int8_t fStringsForm; /* default kStringsUTF16v1 */

  char *fKeys;
  KeyMapEntry *fKeyMap;
  int32_t fKeyPoint;
  int32_t fKeysCapacity;
  int32_t fKeysCount;

  UHashtable *fStringSet;
  UChar *fStringsUTF16;
  int32_t fStringsUTF16Length;

  uint8_t *fStringBytes;
  int32_t fStringBytesCapacity;
  int32_t fStringBytesLength;

  /* experimental compression */
  int32_t fStringsCount, fStringsSize;
  int32_t fPotentialSize, fMiniCount, fMiniReduction;
  int32_t fIncompressibleStrings;
  int32_t fCompressedPadding;
  int32_t fKeysReduction;
  int32_t fStringDedupReduction;
  int32_t fWindowCounts[LAST_WINDOW_LIMIT>>5];
  int32_t fWindows[3];
  uint32_t fInfixUTF16Size;

  struct SResource *fShortestString, *fLongestString;

  const char *fPoolBundleKeys;
  int32_t fPoolBundleKeysBottom, fPoolBundleKeysTop;
  int32_t fPoolBundleKeysCount;
};

struct SRBRoot *bundle_open(const struct UString* comment, UErrorCode *status);
void bundle_write(struct SRBRoot *bundle, const char *outputDir, const char *outputPkg, char *writtenFilename, int writtenFilenameLen, UErrorCode *status);

/* write a java resource file */
void bundle_write_java(struct SRBRoot *bundle, const char *outputDir, const char* outputEnc, char *writtenFilename, 
                       int writtenFilenameLen, const char* packageName, const char* bundleName, UErrorCode *status);

/* write a xml resource file */
/* commented by Jing*/
/* void bundle_write_xml(struct SRBRoot *bundle, const char *outputDir,const char* outputEnc, 
                  char *writtenFilename, int writtenFilenameLen,UErrorCode *status); */

/* added by Jing*/
void bundle_write_xml(struct SRBRoot *bundle, const char *outputDir,const char* outputEnc, const char* rbname,
                  char *writtenFilename, int writtenFilenameLen, const char* language, const char* package, UErrorCode *status);

void bundle_close(struct SRBRoot *bundle, UErrorCode *status);
void bundle_setlocale(struct SRBRoot *bundle, UChar *locale, UErrorCode *status);
int32_t bundle_addtag(struct SRBRoot *bundle, const char *tag, UErrorCode *status);

const char *
bundle_getKeyBytes(struct SRBRoot *bundle, int32_t *pLength);

int32_t
bundle_addKeyBytes(struct SRBRoot *bundle, const char *keyBytes, int32_t length, UErrorCode *status);

void
bundle_compactKeys(struct SRBRoot *bundle, UErrorCode *status);

/* Various resource types */

/*
 * Return a unique pointer to a dummy object,
 * for use in non-error cases when no resource is to be added to the bundle.
 * (NULL is used in error cases.)
 */
struct SResource* res_none(void);

struct SResTable {
    uint32_t fCount;
    UBool fIs32Bit;
    struct SResource *fFirst;
    struct SRBRoot *fRoot;
};

struct SResource* table_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status);
void table_add(struct SResource *table, struct SResource *res, int linenumber, UErrorCode *status);

struct SResArray {
    uint32_t fCount;
    struct SResource *fFirst;
    struct SResource *fLast;
};

struct SResource* array_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status);
void array_add(struct SResource *array, struct SResource *res, UErrorCode *status);

enum {
    MAX_INFIXES = 2
};

struct SResString {
    struct SResource *fSame;  /* used for duplicates */
    struct SResource *fShorter, *fLonger;  /* used for finding infixes */
    struct SResource *fInfixes[MAX_INFIXES];
    UChar *fChars;
    int32_t fLength;
    uint32_t fRes;  /* resource item for this string */
    int32_t fInfixIndexes[MAX_INFIXES];
    int8_t fInfixDepth; /* depth of indexes used inside this string */
    UBool fIsInfix;
    UBool fWriteUTF16;
    int8_t fNumCharsForLength;
};

struct SResource *string_open(struct SRBRoot *bundle, char *tag, const UChar *value, int32_t len, const struct UString* comment, UErrorCode *status);

struct SResource *alias_open(struct SRBRoot *bundle, char *tag, UChar *value, int32_t len, const struct UString* comment, UErrorCode *status);

struct SResIntVector {
    uint32_t fCount;
    uint32_t *fArray;
};

struct SResource* intvector_open(struct SRBRoot *bundle, char *tag,  const struct UString* comment, UErrorCode *status);
void intvector_add(struct SResource *intvector, int32_t value, UErrorCode *status);

struct SResInt {
    uint32_t fValue;
};

struct SResource *int_open(struct SRBRoot *bundle, char *tag, int32_t value, const struct UString* comment, UErrorCode *status);

struct SResBinary {
    uint32_t fLength;
    uint8_t *fData;
    char* fFileName; /* file name for binary or import binary tags if any */
};

struct SResource *bin_open(struct SRBRoot *bundle, const char *tag, uint32_t length, uint8_t *data, const char* fileName, const struct UString* comment, UErrorCode *status);

/* Resource place holder */

struct SResource {
    UResType fType;
    int32_t  fKey;  /* Index into bundle->fKeys, or -1 if no key. */
    int      line;  /* used internally to report duplicate keys in tables */
    struct SResource *fNext; /*This is for internal chaining while building*/
    struct UString fComment;
    union {
        struct SResTable fTable;
        struct SResArray fArray;
        struct SResString fString;
        struct SResIntVector fIntVector;
        struct SResInt fIntValue;
        struct SResBinary fBinaryValue;
    } u;
};

const char *
res_getKeyString(const struct SRBRoot *bundle, const struct SResource *res, char temp[8]);

void res_close(struct SResource *res);
void setIncludeCopyright(UBool val);
UBool getIncludeCopyright(void);

U_CDECL_END
#endif /* #ifndef RESLIST_H */
