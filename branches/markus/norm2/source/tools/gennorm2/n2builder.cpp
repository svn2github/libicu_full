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
#include <vector>
#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/putil.h"
#include "unicode/udata.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
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

class HangulIterator {
public:
    struct Range {
        UChar32 start, limit;
        uint16_t norm16;
    };

    HangulIterator() : rangeIndex(0) {}
    const Range *nextRange() {
        if(rangeIndex<LENGTHOF(ranges)) {
            return ranges+rangeIndex++;
        } else {
            return NULL;
        }
    }
    void reset() { rangeIndex=0; }
private:
    static const Range ranges[4];
    int32_t rangeIndex;
};

const HangulIterator::Range HangulIterator::ranges[4]={
    { JAMO_L_BASE, JAMO_L_BASE+JAMO_L_COUNT, 1 },
    { JAMO_V_BASE, JAMO_V_BASE+JAMO_V_COUNT, Normalizer2Data::JAMO_VT },
    { JAMO_T_BASE+1, JAMO_T_BASE+JAMO_T_COUNT, Normalizer2Data::JAMO_VT },  // +1: not U+11A7
    { HANGUL_BASE, HANGUL_BASE+HANGUL_COUNT, 0 },  // will become minYesNo
};

struct CompositionPair {
    CompositionPair(UChar32 t, UChar32 c) : trail(t), composite(c) {}
    UChar32 trail, composite;
};

struct Norm {
    enum MappingType { NONE, REMOVED, ROUND_TRIP, ONE_WAY };

    UnicodeString *mapping;
    int32_t mappingPhase;
    MappingType mappingType;

    std::vector<CompositionPair> *compositions;
    uint8_t cc;
    UBool combinesBack;
};

Normalizer2DataBuilder::Normalizer2DataBuilder(UErrorCode &errorCode) :
        phase(0), overrideHandling(OVERRIDE_PREVIOUS) {
    memset(unicodeVersion, 0, sizeof(unicodeVersion));
    normTrie=utrie2_open(0, 0, &errorCode);
    normMem=utm_open("gennorm2 normalization structs", 10000, 200000, sizeof(Norm));
    norms=allocNorm();  // unused Norm struct at index 0
}

Normalizer2DataBuilder::~Normalizer2DataBuilder() {
    utrie2_close(normTrie);
    int32_t count=utm_countItems(normMem);
    for(int32_t i=0; i<count; ++i) {
        delete norms[i].mapping;
        delete norms[i].compositions;
    }
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

Norm *Normalizer2DataBuilder::checkNormForMapping(Norm *p, UChar32 c) {
    if(p!=NULL) {
        if(p->mappingType!=Norm::NONE) {
            if( overrideHandling==OVERRIDE_NONE ||
                (overrideHandling==OVERRIDE_PREVIOUS && p->mappingPhase==phase)
            ) {
                fprintf(stderr,
                        "error in gennorm2 phase %d: "
                        "not permitted to override mapping for U+%04lX from phase %d\n",
                        (int)phase, (long)c, (int)p->mappingPhase);
                exit(U_INVALID_FORMAT_ERROR);
            }
            delete p->mapping;
            p->mapping=NULL;
        }
        p->mappingPhase=phase;
    }
    return p;
}

void Normalizer2DataBuilder::setOverrideHandling(OverrideHandling oh) {
    overrideHandling=oh;
    ++phase;
}

void Normalizer2DataBuilder::setCC(UChar32 c, uint8_t cc) {
    createNorm(c)->cc=cc;
}

void Normalizer2DataBuilder::setOneWayMapping(UChar32 c, const UnicodeString &m) {
    Norm *p=checkNormForMapping(createNorm(c), c);
    p->mapping=new UnicodeString(m);
    p->mappingType=Norm::ONE_WAY;
}

// TODO: check for well-formed UTF-16 mappings
// TODO: check that a mapping does not contain the original character

void Normalizer2DataBuilder::setRoundTripMapping(UChar32 c, const UnicodeString &m) {
    int32_t numCP=u_countChar32(m.getBuffer(), m.length());
    if(numCP!=2) {
        fprintf(stderr,
                "error in gennorm2 phase %d: "
                "illegal round-trip mapping from U+%04lX to %d!=2 code points\n",
                (int)phase, (long)c, (int)numCP);
        exit(U_INVALID_FORMAT_ERROR);
    }
    Norm *p=checkNormForMapping(createNorm(c), c);
    p->mapping=new UnicodeString(m);
    p->mappingType=Norm::ROUND_TRIP;
}

void Normalizer2DataBuilder::removeMapping(UChar32 c) {
    Norm *p=checkNormForMapping(getNorm(c), c);
    if(p!=NULL) {
        p->mappingType=Norm::REMOVED;
    }
}

U_CDECL_BEGIN

static UBool U_CALLCONV
addComposition(const void *context, UChar32 start, UChar32 end, uint32_t value) {
    return ((Normalizer2DataBuilder *)context)->addComposition(start, end, value);
}

U_CDECL_END

UBool
Normalizer2DataBuilder::addComposition(UChar32 start, UChar32 end, uint32_t value) {
    if(norms[value].mappingType==Norm::ROUND_TRIP) {
        if(start!=end) {
            fprintf(stderr,
                    "gennorm2 error: same round-trip mapping for "
                    "more than 1 code point U+%04lX..U+%04lX\n",
                    (long)start, (long)end);
            exit(U_INVALID_FORMAT_ERROR);
        }
        // setRoundTripMapping() ensured that there are exactly two code points.
        const UChar *m=norms[value].mapping->getBuffer();
        int32_t i=0, length=norms[value].mapping->length();
        UChar32 lead, trail;
        U16_NEXT(m, i, length, lead);
        U16_NEXT(m, i, length, trail);
        // Flag for trailing character.
        createNorm(trail)->combinesBack=TRUE;
        // Insert (trail, composite) pair into compositions list for the lead character.
        CompositionPair pair(trail, start);
        Norm *leadNorm=createNorm(lead);
        std::vector<CompositionPair> *compositions=leadNorm->compositions;
        if(compositions==NULL) {
            compositions=leadNorm->compositions=new std::vector<CompositionPair>;
            compositions->push_back(pair);
        } else {
            // Insertion sort, and check for duplicate trail characters.
            std::vector<CompositionPair>::iterator it;
            for(it=compositions->begin(); it!=compositions->end(); ++it) {
                if(trail==it->trail) {
                    fprintf(stderr,
                            "gennorm2 error: same round-trip mapping for "
                            "more than 1 code point (e.g., U+%04lX) to U+%04lX + U+%04lX\n",
                            (long)start, (long)lead, (long)trail);
                    exit(U_INVALID_FORMAT_ERROR);
                }
                if(trail<it->trail) {
                    break;
                }
            }
            compositions->insert(it, pair);
        }
    }
    return TRUE;
}

void Normalizer2DataBuilder::makeCompositions() {
    utrie2_enum(normTrie, NULL, icu::addComposition, this);
}

void Normalizer2DataBuilder::setHangulData() {
    HangulIterator hi;
    const HangulIterator::Range *range;
    // Check that none of the Hangul/Jamo code points have data.
    while((range=hi.nextRange())!=NULL) {
        for(UChar32 c=range->start; c<range->limit; ++c) {
            Norm *p=getNorm(c);
            if( p!=NULL && (p->cc!=0 || p->mappingType>Norm::REMOVED ||
                            p->compositions!=NULL || p->combinesBack)
            ) {
                fprintf(stderr,
                        "gennorm2 error: "
                        "illegal mapping or ccc data for Hangul or Jamo U+%04lX\n",
                        (long)c);
                exit(U_INVALID_FORMAT_ERROR);
            }
        }
    }
    // Set data for algorithmic runtime handling.
    IcuToolErrorCode errorCode("gennorm2/setHangulData()");
    hi.reset();
    while((range=hi.nextRange())!=NULL) {
        uint16_t norm16=range->norm16;
        if(norm16==0) {
            norm16=2;  // TODO: minYesNo
        }
        utrie2_setRange32(normTrie, range->start, range->limit-1, norm16, TRUE, errorCode);
        errorCode.assertSuccess();
    }
}

void Normalizer2DataBuilder::writeBinaryFile(const char *filename) {
    makeCompositions();
    setHangulData();
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
