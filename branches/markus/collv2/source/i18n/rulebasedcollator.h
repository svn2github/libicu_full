/*
*******************************************************************************
* Copyright (C) 1996-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* rulebasedcollator.h
*
* created on: 2012feb14 with new and old collation code
* created by: Markus W. Scherer
*/

#ifndef __RULEBASEDCOLLATOR_H__
#define __RULEBASEDCOLLATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/ucol.h"

U_NAMESPACE_BEGIN

class CollationData;
class CollationIterator;

// TODO: Consider splitting off worker classes like CollationCompare & CollationSortkey.
// Pro: They would be small and focused.
// Con: Their methods would have to take a lot of parameters.

class U_I18N_API RuleBasedCollator : public Collator {
public:
private:
    EComparisonResult compareUpToTertiary(CollationIterator &left, CollationIterator &right,
                                          UErrorCode &errorCode);

    EComparisonResult comparePrimaryAndCase(CollationIterator &left, CollationIterator &right,
                                            UErrorCode &errorCode);

    EComparisonResult compareQuaternary(CollationIterator &left, CollationIterator &right,
                                        UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __RULEBASEDCOLLATOR_H__
