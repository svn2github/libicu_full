/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitaffixesandpadding.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __DIGITAFFIXESANDPADDING_H__
#define __DIGITAFFIXESANDPADDING_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "pluralaffix.h"

U_NAMESPACE_BEGIN

class DigitList;
class ValueFormatter;
class UnicodeString;
class FieldPositionHandler;
class PluralRules;

class U_I18N_API DigitAffixesAndPadding : public UMemory {
public:
enum EPadPosition {
    kPadBeforePrefix,
    kPadAfterPrefix,
    kPadBeforeSuffix,
    kPadAfterSuffix
};

PluralAffix fPositivePrefix;
PluralAffix fPositiveSuffix;
PluralAffix fNegativePrefix;
PluralAffix fNegativeSuffix;
EPadPosition fPadPosition;
UChar32 fPadChar;
int32_t fWidth;

DigitAffixesAndPadding()
        : fPadPosition(kPadBeforePrefix), fPadChar(0x2a), fWidth(0) { }

UBool needsPluralRules() const;

UnicodeString &format(
        DigitList &value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo) const;
private:
UnicodeString &appendPadding(int32_t paddingCount, UnicodeString &appendTo) const;

};


U_NAMESPACE_END

#endif  // __DIGITAFFIXANDPADDING_H__
