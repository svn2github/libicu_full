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

class U_I18N_API RuleBasedCollator2 : public Collator {
public:
private:
    // TODO
    CollationData *data;  // == defaultData, or cloned & modified via setting attributes
    CollationData *defaultData;
    uint8_t *reorderTable;  // NULL unless different from defaultData->reorderTable
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __RULEBASEDCOLLATOR_H__
