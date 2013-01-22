/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationroot.h
*
* created on: 2012dec17
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONROOT_H__
#define __COLLATIONROOT_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

struct CollationData;
class Collator;

/**
 * Collation root provider.
 */
class U_I18N_API CollationRoot {  // purely static
public:
    static const CollationData *getBaseData(UErrorCode &errorCode);
    static Collator *createCollator(UErrorCode &errorCode);

private:
    static CollationData *load(UErrorCode &errorCode);
    static void *createInstance(const void *context, UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONROOT_H__
