/*
*******************************************************************************
* Copyright (C) 1997-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File COMPACTDECIMALFORMAT.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/compactdecimalformat.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CompactDecimalFormat)

CompactDecimalFormat::CompactDecimalFormat(const DecimalFormat& decimalFormat) : DecimalFormat(decimalFormat) {
}

CompactDecimalFormat::CompactDecimalFormat(const CompactDecimalFormat& source)
    : DecimalFormat(source) {
      // TODO: implement
}

CompactDecimalFormat* U_EXPORT2
CompactDecimalFormat::createInstance(
    const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status) {
  NumberFormat* fmt = NumberFormat::makeInstance(inLocale, UNUM_DECIMAL, status, true);
  if (U_FAILURE(status)) {
    return NULL;
  }
  LocalPointer<DecimalFormat> decfmt((DecimalFormat*) fmt);
  return new CompactDecimalFormat(*decfmt);
}

CompactDecimalFormat&
CompactDecimalFormat::operator=(const CompactDecimalFormat& rhs) {
  if (this != &rhs) {
    DecimalFormat::operator=(rhs);
    // TODO: implement
  }
  return *this;
}

CompactDecimalFormat::~CompactDecimalFormat() {
  // TODO: implement
}


Format*
CompactDecimalFormat::clone(void) const {
  return new CompactDecimalFormat(*this);
}

UBool
CompactDecimalFormat::operator==(const Format& that) const {
  if (this == &that) {
    return TRUE;
  }
  return (DecimalFormat::operator==(that) && eqHelper((const CompactDecimalFormat&) that));
}

UBool
CompactDecimalFormat::eqHelper(const CompactDecimalFormat& that) const {
  // TODO: implement
  return TRUE;
}

UnicodeString&
CompactDecimalFormat::format(
    double number,
    UnicodeString& appendTo,
    FieldPosition& pos) const {
  return DecimalFormat::format(number, appendTo, pos);
}

UnicodeString&
CompactDecimalFormat::format(
    double number,
    UnicodeString& appendTo,
    FieldPositionIterator* posIter,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    int64_t number,
    UnicodeString& appendTo,
    FieldPosition& pos) const {
  return DecimalFormat::format(number, appendTo, pos);
}

UnicodeString&
CompactDecimalFormat::format(
    int64_t number,
    UnicodeString& appendTo,
    FieldPositionIterator* posIter,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    const StringPiece& number,
    UnicodeString& appendTo,
    FieldPositionIterator* posIter,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    const DigitList& number,
    UnicodeString& appendTo,
    FieldPositionIterator* posIter,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(const DigitList &number,
                             UnicodeString& appendTo,
                             FieldPosition& pos,
                             UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

void
CompactDecimalFormat::parse(
    const UnicodeString& text,
    Formattable& result,
    ParsePosition& parsePosition) const {
}

void
CompactDecimalFormat::parse(
    const UnicodeString& text,
    Formattable& result,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
}

CurrencyAmount*
CompactDecimalFormat::parseCurrency(
    const UnicodeString& text,
    ParsePosition& pos) const {
  return NULL;
}

U_NAMESPACE_END

#endif
