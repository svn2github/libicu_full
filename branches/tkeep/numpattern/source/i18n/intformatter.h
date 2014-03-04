/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/
#ifndef __INTFORMATTER_H
#define __INTFORMATTER_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class Formattable;
class FieldPosition;
class DecimalFormat;

class U_I18N_API IntFormatter : public UMemory {
public:
    IntFormatter();
    ~IntFormatter();
    UBool like(const DecimalFormat &df);
    UnicodeString &format(
            const Formattable &number,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;
private:
    int32_t fGroupingSize;
    int32_t fGroupingSize2;
    UnicodeString fGroupingSeparator;
    UnicodeString fPosPrefix;
    UnicodeString fNegPrefix;
    UnicodeString fPosSuffix;
    UnicodeString fNegSuffix;
    UChar fDigits[10];
    IntFormatter(const IntFormatter &other);
    IntFormatter &operator=(const IntFormatter &other);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif /* __INTFORMATTER_H */
