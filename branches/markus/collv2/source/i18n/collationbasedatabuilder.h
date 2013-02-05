/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
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
 * Low-level base CollationData builder.
 */
class U_I18N_API CollationBaseDataBuilder : public CollationDataBuilder {
public:
    CollationBaseDataBuilder(UErrorCode &errorCode);

    virtual ~CollationBaseDataBuilder();

    void initBase(UErrorCode &errorCode);

    /** Non-functional in this subclass. */
    virtual void initTailoring(const CollationData *b, UErrorCode &errorCode);

    /**
     * Sets the Han ranges as ranges of offset CE32s.
     * Note: Unihan extension A sorts after the other BMP ranges.
     * See http://www.unicode.org/reports/tr10/#Implicit_Weights
     *
     * @param ranges array of ranges of [:Unified_Ideograph:] in collation order,
     *               as (start, end) code point pairs
     * @param length number of code points (not pairs)
     * @param errorCode in/out error code
     */
    void initHanRanges(const UChar32 ranges[], int32_t length, UErrorCode &errorCode);

    void setNumericPrimary(uint32_t np) { numericPrimary = np; }

    virtual UBool isCompressibleLeadByte(uint32_t b) const;

    void setCompressibleLeadByte(uint32_t b);

    void addReorderingGroup(uint32_t firstByte, uint32_t lastByte,
                            const UnicodeString &groupScripts,
                            UErrorCode &errorCode);

    CollationData *buildBaseData(UErrorCode &errorCode);

    /** Non-functional in this subclass. */
    virtual CollationData *buildTailoring(UErrorCode &errorCode);

private:
    // Flags for which primary-weight lead bytes are compressible.
    UBool compressibleBytes[256];
    UnicodeString scripts;
    uint32_t numericPrimary;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONBASEDATABUILDER_H__
