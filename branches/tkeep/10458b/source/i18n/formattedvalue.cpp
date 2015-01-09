/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "formattedvalue.h"

U_NAMESPACE_BEGIN

FormattedValue::~FormattedValue() {
}

int32_t
FormattedValue::countChar32() const {
    UnicodeString appendTo;
    format(appendTo);
    return appendto.countChar32();
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
