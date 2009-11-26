/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  n2builder.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov25
*   created by: Markus W. Scherer
*
* Builds Normalizer2Data and writes a binary .nrm file.
* For the file format see source/common/normalizer2impl.h.
*/

#include "n2builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/putil.h"
#include "unicode/udata.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "normalizer2impl.h"
#include "toolutil.h"
#include "unewdata.h"
#include "unormimp.h"
#include "utrie2.h"
#include "writesrc.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

/* file data ---------------------------------------------------------------- */

#if UCONFIG_NO_NORMALIZATION

/* dummy UDataInfo cf. udata.h */
static UDataInfo dataInfo = {
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    U_SIZEOF_UCHAR,
    0,

    { 0, 0, 0, 0 },                 /* dummy dataFormat */
    { 0, 0, 0, 0 },                 /* dummy formatVersion */
    { 0, 0, 0, 0 }                  /* dummy dataVersion */
};

#else

/* UDataInfo cf. udata.h */
static UDataInfo dataInfo={
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    U_SIZEOF_UCHAR,
    0,

    { 0x4e, 0x72, 0x6d, 0x32 }, /* dataFormat="Nrm2" */
    { 1, 0, 0, 0 },             /* formatVersion */
    { 5, 2, 0, 0 }              /* dataVersion (Unicode version) */
};

U_NAMESPACE_BEGIN

struct Norm {
    uint8_t cc;
};

Normalizer2DataBuilder::Normalizer2DataBuilder(UErrorCode &errorCode) {
    memset(unicodeVersion, 0, sizeof(unicodeVersion));
    normTrie=utrie2_open(0, 0, &errorCode);
    normMem=utm_open("gennorm2 normalization structs", 20000, 20000, sizeof(Norm));
    norms=allocNorm();  // unused Norm struct at index 0
}

Normalizer2DataBuilder::~Normalizer2DataBuilder() {
    utrie2_close(normTrie);
    utm_close(normMem);
}

void
Normalizer2DataBuilder::setUnicodeVersion(const char *v) {
    u_versionFromString(unicodeVersion, v);
    // TODO: do when writing the file: uprv_memcpy(dataInfo.dataVersion, unicodeVersion, 4);
}

Norm *Normalizer2DataBuilder::allocNorm() {
    Norm *p=(Norm *)utm_alloc(normMem);
    norms=(Norm *)utm_getStart(normMem);  // in case it got reallocated
    return p;
}

/* get an existing Norm unit */
Norm *Normalizer2DataBuilder::getNorm(UChar32 c) {
    uint32_t i=utrie2_get32(normTrie, c);
    if(i==0) {
        return NULL;
    }
    return norms+i;
}

/*
 * get or create a Norm unit;
 * get or create the intermediate trie entries for it as well
 */
Norm *Normalizer2DataBuilder::createNorm(UChar32 c) {
    uint32_t i=utrie2_get32(normTrie, c);
    if(i!=0) {
        return norms+i;
    } else {
        /* allocate Norm */
        Norm *p=allocNorm();
        IcuToolErrorCode errorCode("gennorm2/createNorm()");
        utrie2_set32(normTrie, c, (uint32_t)(p-norms), errorCode);
        return p;
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_NORMALIZATION */

/*
 * Hey, Emacs, please set the following:
 *
 * Local Variables:
 * indent-tabs-mode: nil
 * End:
 */
