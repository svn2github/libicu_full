/*
**********************************************************************
* Copyright (c) 2004-2011, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 20, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/measfmt.h"
#include "unicode/numfmt.h"
#include "currfmt.h"

U_NAMESPACE_BEGIN

MeasureFormat::MeasureFormat(
        const Locale &locale, UMeasureFormatWidth width, UErrorCode &status) {
    
}

MeasureFormat::MeasureFormat(
            const Locale &locale,
            UMeasureFormatWidth width,
            NumberFormat *nfToAdopt,
            UErrorCode &status) {
      delete nfToAdopt;
}

MeasureFormat::MeasureFormat(const MeasureFormat &other) : Format(other) {
}

MeasureFormat &MeasureFormat::operator=(const MeasureFormat &rhs) {
    if (this == &rhs) {
        return *this;
    }
    return *this;
}

MeasureFormat::MeasureFormat() {}

MeasureFormat::~MeasureFormat() {}

UBool MeasureFormat::operator==(const Format &other) const {
    const MeasureFormat *rhs = dynamic_cast<const MeasureFormat *>(&other);
    if (rhs == NULL) {
        return FALSE;
    }
    return TRUE;
}

Format *MeasureFormat::clone() const {
    return new MeasureFormat(*this);
}

UnicodeString &MeasureFormat::format(
        const Formattable &obj,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    return appendTo;
}

void MeasureFormat::parseObject(
        const UnicodeString &source,
        Formattable &reslt,
        ParsePosition& pos) const {
}

UnicodeString &MeasureFormat::formatMeasures(
        const Measure *measures,
        int32_t measureCount,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    return appendTo;
}

MeasureFormat* U_EXPORT2 MeasureFormat::createCurrencyFormat(const Locale& locale,
                                                   UErrorCode& ec) {
    CurrencyFormat* fmt = NULL;
    if (U_SUCCESS(ec)) {
        fmt = new CurrencyFormat(locale, ec);
        if (U_FAILURE(ec)) {
            delete fmt;
            fmt = NULL;
        }
    }
    return fmt;
}

MeasureFormat* U_EXPORT2 MeasureFormat::createCurrencyFormat(UErrorCode& ec) {
    if (U_FAILURE(ec)) {
        return NULL;
    }
    return MeasureFormat::createCurrencyFormat(Locale::getDefault(), ec);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
