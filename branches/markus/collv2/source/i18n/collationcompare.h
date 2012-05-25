/*
*******************************************************************************
* Copyright (C) 1996-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationcompare.h
*
* created on: 2012feb14 with new and old collation code
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONCOMPARE_H__
#define __COLLATIONCOMPARE_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"

U_NAMESPACE_BEGIN

class CollationIterator;

class U_I18N_API CollationCompare /* not : public UObject because all methods are static */ {
public:
private:
    static UCollationResult compareUpToTertiary(CollationIterator &left, CollationIterator &right,
                                                UErrorCode &errorCode);

    static UCollationResult comparePrimaryAndCase(CollationIterator &left, CollationIterator &right,
                                                  UErrorCode &errorCode);

    static UCollationResult compareQuaternary(CollationIterator &left, CollationIterator &right,
                                              UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONCOMPARE_H__
