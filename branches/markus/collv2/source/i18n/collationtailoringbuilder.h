/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoringbuilder.h
*
* created on: 2013mar05
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONTAILORINGBUILDER_H__
#define __COLLATIONTAILORINGBUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationsettings.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

/**
 * Low-level tailoring CollationData builder.
 */
class U_I18N_API CollationTailoringBuilder : public CollationDataBuilder {
public:
    CollationTailoringBuilder(UErrorCode &errorCode);

    virtual ~CollationTailoringBuilder();

    void init(const CollationData *b, const CollationSettings *bs, UErrorCode &errorCode);

    virtual void build(UErrorCode &errorCode);

private:
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONTAILORINGBUILDER_H__
