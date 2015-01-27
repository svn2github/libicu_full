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
 *
 * PluralAffix is essentially a map of DigitAffix objects keyed by plural
 * variant. The 'other' plural variant is the default and always has some
 * value. The rest of the variants are optional. Querying for a variant that
 * is not set always returns the 'other' variant.
 */
class U_I18N_API PluralAffix : public UMemory {
public:
    /**
     * The 'other' variant set to empty string with no field annotations.
     */
    PluralAffix();
    PluralAffix(const PluralAffix &other);
    PluralAffix &operator=(const PluralAffix &other);
    ~PluralAffix();

    /**
     * Sets a particular plural variant while overwritting anything that
     * may have been previously stored for that variant. The set variant
     * has no field annotations.
     * @param variant "one", "two", "few", ...
     * @param value the value for the plural variant
     * @param status Any error returned here.
     */
    UBool setVariant(
            const char *variant,
            const UnicodeString &value,
            UErrorCode &status);
    /**
     * Remove all plural variants and make the 'other' variant be the
     * empty string with no field annotations.
     */
    void remove();

    /**
     * Append value to all set variants. If fieldId present, value is that
     * field type.
     */
    void appendUChar(UChar value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append value to all set variants. If fieldId present, value is that
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
     * If the particular variant is not set, returns the 'other' variant
     * which is always set.
     */
    const DigitAffix &getByVariant(const char *variant) const;

    /**
     * Get the DigitAffix for the other variant which is always set.
     */
    const DigitAffix &getOtherVariant() const { return *affixes[0]; }

    /**
     * Returns TRUE if this instance has variants besides "other"
     */
    UBool hasMultipleVariants() const;
private:
    DigitAffix otherAffix;
    DigitAffix *affixes[6];
};


U_NAMESPACE_END

#endif  // __PLURALAFFIX_H__
