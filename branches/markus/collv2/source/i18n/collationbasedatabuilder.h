/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationbasedatabuilder.h
*
* created on: 2012aug11
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONBASEDATABUILDER_H__
#define __COLLATIONBASEDATABUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

/**
 * Low-level CollationBaseData builder.
 */
class U_I18N_API CollationBaseDataBuilder : public CollationDataBuilder {
public:
    CollationBaseDataBuilder(UErrorCode &errorCode);

    virtual ~CollationBaseDataBuilder();

    void initBase(UErrorCode &errorCode);

    /** Non-functional in this subclass. */
    virtual void initTailoring(const CollationData *b, UErrorCode &errorCode);

    virtual UBool isCompressibleLeadByte(uint32_t b) const;

    CollationBaseData *buildBaseData(UErrorCode &errorCode);

    /** Non-functional in this subclass. */
    virtual CollationData *buildTailoring(UErrorCode &errorCode);

private:
    void initHanRanges(UErrorCode &errorCode);
    void initHanCompat(UErrorCode &errorCode);

    // Linear FCD16 data table for U+0000..U+0EFF.
    uint16_t *fcd16_F00;
    // Flags for which primary-weight lead bytes are compressible.
    // NULL in a tailoring builder, consult the base instead.
    UBool *compressibleBytes;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONBASEDATABUILDER_H__
