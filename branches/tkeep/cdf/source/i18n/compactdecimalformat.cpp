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
#include "unicode/plurrule.h"
#include "uhash.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CompactDecimalFormat)

static UBool divisors_equal(const double* lhs, const double* rhs);
static const int32_t MAX_DIGITS = 15;

CompactDecimalFormat::CompactDecimalFormat(
    const DecimalFormat& decimalFormat,
    const UHashtable* unitsByVariant,
    const double* divisors,
    PluralRules* pluralRules)
  : DecimalFormat(decimalFormat), _unitsByVariant(unitsByVariant), _divisors(divisors), _pluralRules(pluralRules) {
}

CompactDecimalFormat::CompactDecimalFormat(const CompactDecimalFormat& source)
    : DecimalFormat(source), _unitsByVariant(source._unitsByVariant), _divisors(source._divisors), _pluralRules(source._pluralRules->clone()) {
}

CompactDecimalFormat* U_EXPORT2
CompactDecimalFormat::createInstance(
    const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status) {
  LocalPointer<DecimalFormat> decfmt((DecimalFormat*) NumberFormat::makeInstance(inLocale, UNUM_DECIMAL, true, status));
  if (U_FAILURE(status)) {
    return NULL;
  }
  // TODO: Implement
  return new CompactDecimalFormat(*decfmt, NULL, NULL, NULL);
}

CompactDecimalFormat&
CompactDecimalFormat::operator=(const CompactDecimalFormat& rhs) {
  if (this != &rhs) {
    DecimalFormat::operator=(rhs);
    _unitsByVariant = rhs._unitsByVariant;
    _divisors = rhs._divisors;
    delete _pluralRules;
    _pluralRules = rhs._pluralRules->clone();
  }
  return *this;
}

CompactDecimalFormat::~CompactDecimalFormat() {
  delete _pluralRules;
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
  return uhash_equals(_unitsByVariant, that._unitsByVariant) && divisors_equal(_divisors, that._divisors) && (*_pluralRules == *that._pluralRules);
}

UnicodeString&
CompactDecimalFormat::format(
    double number,
    UnicodeString& appendTo,
    FieldPosition& pos) const {
  // TODO: implement
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
  // TODO: implement
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

static UBool divisors_equal(const double* lhs, const double* rhs) {
  for (int32_t i = 0; i < MAX_DIGITS; i++) {
    if (lhs[i] != rhs[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

U_NAMESPACE_END

#endif
