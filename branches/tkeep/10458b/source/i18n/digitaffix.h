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

/**
 * A prefix or suffix of a formatted number.
 */
class U_I18N_API DigitAffix : public UMemory {
public:

    /**
     * Makes this affix be the empty string.
     */
    void remove();

    /**
     * Append value to this affix. If fieldId is present, the appended
     * string is considered to be the type fieldId.
     */
    void append(const UnicodeString &value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Formats this affix.
     */
    UnicodeString &format(
            FieldPositionHandler &handler, UnicodeString &appendTo) const;
    int32_t countChar32() const { return fAffix.countChar32(); }

    /**
     * Returns this affix as a unicode string.
     */
    const UnicodeString & toString() const { return fAffix; }
private:
    UnicodeString fAffix;
    UnicodeString fAnnotations;
};


U_NAMESPACE_END

#endif  // __DIGITAFFIX_H__
