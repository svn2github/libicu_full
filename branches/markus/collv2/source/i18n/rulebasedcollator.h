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
    UCollationResult compareUpToTertiary(CollationIterator &left, CollationIterator &right,
                                         UErrorCode &errorCode);

    UCollationResult comparePrimaryAndCase(CollationIterator &left, CollationIterator &right,
                                           UErrorCode &errorCode);

    UCollationResult compareQuaternary(CollationIterator &left, CollationIterator &right,
                                       UErrorCode &errorCode);

    // TODO
    UColAttributeValue strength;
    UColAttributeValue caseLevel;
    UBool isFrenchSec;
    UBool withHiraganaQuaternary;
    uint32_t variableTop;
    uint32_t caseSwitch;
    uint32_t tertiaryMask;
    uint8_t *reorderTable;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __RULEBASEDCOLLATOR_H__
