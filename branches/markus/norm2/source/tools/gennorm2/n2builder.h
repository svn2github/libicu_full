/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  n2builder.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov25
*   created by: Markus W. Scherer
*/

#ifndef __N2BUILDER_H__
#define __N2BUILDER_H__

#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/unistr.h"
#include "normalizer2impl.h"  // for IX_COUNT
#include "toolutil.h"
#include "utrie2.h"

#define DATA_TYPE "nrm"

U_NAMESPACE_BEGIN

extern UBool beVerbose, haveCopyright;

// TODO: move to toolutil library
class IcuToolErrorCode : public ErrorCode {
public:
    IcuToolErrorCode(const char *loc) : location(loc) {}
    virtual ~IcuToolErrorCode();
protected:
    virtual void handleFailure() const;
private:
    const char *location;
};

struct Norm;

class Normalizer2DataBuilder {
public:
    Normalizer2DataBuilder(UErrorCode &errorCode);
    ~Normalizer2DataBuilder();

    enum OverrideHandling {
        OVERRIDE_NONE,
        OVERRIDE_ANY,
        OVERRIDE_PREVIOUS
    };

    void setOverrideHandling(OverrideHandling oh);

    void setCC(UChar32 c, uint8_t cc);
    void setOneWayMapping(UChar32 c, const UnicodeString &m);
    void setRoundTripMapping(UChar32 c, const UnicodeString &m);
    void removeMapping(UChar32 c);

    void setUnicodeVersion(const char *v);

    void writeBinaryFile(const char *filename);

private:
    // TODO: no copy, assign, etc.

    friend class CompositionBuilder;
    friend class Decomposer;
    friend class ExtraDataWriter;

    uint8_t getCC(UChar32 c);

    Norm *allocNorm();
    Norm *getNorm(UChar32 c);
    Norm *createNorm(UChar32 c);
    Norm *checkNormForMapping(Norm *p, UChar32 c);

    void addComposition(UChar32 start, UChar32 end, uint32_t value);
    UBool decompose(UChar32 start, UChar32 end, uint32_t value);
    void reorder(Norm *p);
    void setHangulData();
    void writeMapping(UChar32 c, Norm *p, UnicodeString &dataString);
    void writeCompositions(UChar32 c, Norm *p, UnicodeString &dataString);
    void writeExtraData(UChar32 c, uint32_t value, ExtraDataWriter &writer);
    void processData();

    UTrie2 *normTrie;
    UToolMemory *normMem;
    Norm *norms;

    int32_t phase;
    OverrideHandling overrideHandling;

    int32_t indexes[Normalizer2Data::IX_COUNT];
    UTrie2 *norm16Trie;
    UnicodeString extraData;

    UVersionInfo unicodeVersion;
};

U_NAMESPACE_END

#endif  // __N2BUILDER_H__
