/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitaffix.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __DIGITAFFIX_H__
#define __DIGITAFFIX_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"

U_NAMESPACE_BEGIN

class FieldPositionHandler;

class U_I18N_API DigitAffix : public UMemory {
public:
void remove();
void append(const UnicodeString &value, int32_t fieldId=UNUM_FIELD_COUNT);
UnicodeString &format(FieldPositionHandler &handler, UnicodeString &appendTo) const;
int32_t countChar32() const { return fAffix.countChar32(); }
private:
UnicodeString fAffix;
UnicodeString fAnnotations;
};


U_NAMESPACE_END

#endif  // __DIGITAFFIX_H__
