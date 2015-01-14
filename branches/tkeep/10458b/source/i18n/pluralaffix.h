/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* pluralaffix.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __PLURALAFFIX_H__
#define __PLURALAFFIX_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "digitaffix.h"
#include "unicode/unum.h"

U_NAMESPACE_BEGIN

class FieldPositionHandler;

/**
 * A plural aware prefix or suffix of a formatted number.
 */
class U_I18N_API PluralAffix : public UMemory {
public:
    PluralAffix();
    PluralAffix(const PluralAffix &other);
    PluralAffix &operator=(const PluralAffix &other);
    ~PluralAffix();
    /**
     * Sets a particular plural variant the set variant has no field type.
     * @param variant "one", "two", "few", ...
     */
    UBool setVariant(
            const char *variant,
            const UnicodeString &value,
            UErrorCode &status);
    /**
     * Make all plural variants be the empty string.
     */
    void remove();

    /**
     * Append value to all variants. If fieldId present, value is that
     * field type.
     */
    void append(const UnicodeString &value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append each variant in rhs to the corresponding variant in this
     * instance. Each variant appended from rhs is of type fieldId.
     */
    UBool append(
            const PluralAffix &rhs,
            int32_t fieldId,
            UErrorCode &status);
    /**
     * Get the DigitAffix for a paricular variant such as "zero", "one", ...
     */
    const DigitAffix &getByVariant(const char *variant) const;

    /**
     * Get the DigitAffix for the other variant.
     */
    const DigitAffix &getOtherVariant() const { return *affixes[0]; }
private:
    DigitAffix otherAffix;
    DigitAffix *affixes[6];
};


U_NAMESPACE_END

#endif  // __PLURALAFFIX_H__
