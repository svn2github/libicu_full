/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoringdatabuilder.h
*
* created on: 2013mar05
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONTAILORINGDATABUILDER_H__
#define __COLLATIONTAILORINGDATABUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"

U_NAMESPACE_BEGIN

/**
 * Low-level tailoring CollationData builder.
 */
class U_I18N_API CollationTailoringDataBuilder : public CollationDataBuilder {
public:
    CollationTailoringDataBuilder(UErrorCode &errorCode);

    virtual ~CollationTailoringDataBuilder();

    void init(const CollationData *b, UErrorCode &errorCode);

    virtual void build(CollationData &data, UErrorCode &errorCode);

    /**
     * Looks up CEs for s and appends them to the ces array.
     * s must be in NFD form.
     * Does not write completely ignorable CEs.
     * Does not write beyond Collation::MAX_EXPANSION_LENGTH.
     * @return incremented cesLength
     */
    int32_t getCEs(const UnicodeString &s, int64_t ces[], int32_t cesLength) const;

private:
    uint32_t getCE32FromBasePrefix(const UnicodeString &s, uint32_t ce32, int32_t i) const;

    uint32_t getCE32FromBaseContraction(const UnicodeString &s,
                                        uint32_t ce32, int32_t sIndex,
                                        UnicodeSet &consumed) const;

    static int32_t appendCE(int64_t ces[], int32_t cesLength, int64_t ce) {
        if(ce != 0) {
            if(cesLength < Collation::MAX_EXPANSION_LENGTH) {
                ces[cesLength] = ce;
            }
            ++cesLength;
        }
        return cesLength;
    }
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONTAILORINGDATABUILDER_H__
