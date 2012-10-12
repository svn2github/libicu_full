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

#include "cstring.h"
#include "mutex.h"
#include "unicode/compactdecimalformat.h"
#include "unicode/plurrule.h"
#include "ucln_in.h"
#include "uhash.h"
#include "umutex.h"

static UHashtable* gCompactDecimalData = NULL;
static UMutex gCompactDecimalMetaLock = U_MUTEX_INITIALIZER;

U_NAMESPACE_BEGIN

static const int32_t MAX_DIGITS = 15;

struct CDFUnit {
  UnicodeString prefix;
  UnicodeString suffix;
};

class CDFLocaleStyleData {
 public:
  double divisors[MAX_DIGITS];
  UHashtable* unitsByVariant;
  CDFLocaleStyleData() {}
  ~CDFLocaleStyleData();
  void Init(UErrorCode& status);
 private:
  CDFLocaleStyleData(const CDFLocaleStyleData&);
  CDFLocaleStyleData& operator=(const CDFLocaleStyleData&);
};

struct CDFLocaleData {
  CDFLocaleStyleData shortData;
  CDFLocaleStyleData longData;
  void Init(UErrorCode& status);
};

U_NAMESPACE_END

U_CDECL_BEGIN

static UBool U_CALLCONV cdf_cleanup(void) {
  if (gCompactDecimalData != NULL) {
    uhash_close(gCompactDecimalData);
    gCompactDecimalData = NULL;
  }
  return TRUE;
}

static void U_CALLCONV deleteCDFUnit(void* ptr) {
  delete (icu::CDFUnit*) ptr;
}

static void U_CALLCONV deleteCDFLocaleData(void* ptr) {
  delete (icu::CDFLocaleData*) ptr;
}

U_CDECL_END

U_NAMESPACE_BEGIN

static UBool divisors_equal(const double* lhs, const double* rhs);

static const CDFLocaleStyleData* extractDataByStyleEnum(const CDFLocaleData& data, UNumberCompactStyle style, UErrorCode& status);
static CDFLocaleData* loadCDFLocaleData(const Locale& inLocale, UErrorCode& status);
static const CDFLocaleStyleData* getCDFLocaleStyleData(const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status);

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CompactDecimalFormat)

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
  LocalPointer<PluralRules> pluralRules(PluralRules::forLocale(inLocale, status));
  if (U_FAILURE(status)) {
    return NULL;
  }
  const CDFLocaleStyleData* data = getCDFLocaleStyleData(inLocale, style, status);
  if (U_FAILURE(status)) {
    return NULL;
  }
  return new CompactDecimalFormat(*decfmt, data->unitsByVariant, data->divisors, pluralRules.orphan());
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

static const CDFLocaleStyleData* getCDFLocaleStyleData(const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  // Make sure our cache exists.
  UBool needed;
  UMTX_CHECK(&gCompactDecimalMetaLock, (gCompactDecimalData == NULL), needed);
  if (needed) {
    Mutex lock(&gCompactDecimalMetaLock);
    if (gCompactDecimalData == NULL) {
      gCompactDecimalData = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
      if (U_FAILURE(status)) {
        return NULL;
      }
      uhash_setKeyDeleter(gCompactDecimalData, uprv_free);
      uhash_setValueDeleter(gCompactDecimalData, deleteCDFLocaleData);
      ucln_i18n_registerCleanup(UCLN_I18N_CDFINFO, cdf_cleanup);
    }
  }
  CDFLocaleData* result = NULL;
  const char* key = inLocale.getName();
  {
    Mutex lock(&gCompactDecimalMetaLock);
    result = (CDFLocaleData*) uhash_get(gCompactDecimalData, key);
  }
  if (result) {
    return extractDataByStyleEnum(*result, style, status);
  }

  result = loadCDFLocaleData(inLocale, status);
  if (U_FAILURE(status)) {
    return NULL;
  }

  {
    Mutex lock(&gCompactDecimalMetaLock);
    CDFLocaleData* temp = (CDFLocaleData*) uhash_get(gCompactDecimalData, key);
    if (temp) {
      delete result;
      result = temp;
    } else {
      uhash_put(gCompactDecimalData, uprv_strdup(key), (void*) result, &status);
      if (U_FAILURE(status)) {
        return NULL;
      }
    }
  }
  return extractDataByStyleEnum(*result, style, status);
}

static const CDFLocaleStyleData* extractDataByStyleEnum(const CDFLocaleData& data, UNumberCompactStyle style, UErrorCode& status) {
  switch (style) {
    case UNUM_SHORT:
      return &data.shortData;
    case UNUM_LONG:
      return &data.longData;
    default:
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return NULL;
  }
}

static CDFLocaleData* loadCDFLocaleData(const Locale& inLocale, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  CDFLocaleData* result = new CDFLocaleData;
  if (result == NULL) {
    status = U_MEMORY_ALLOCATION_ERROR;
    return NULL;
  }
  result->Init(status);
  if (U_FAILURE(status)) {
    delete result;
    return NULL;
  }

  // TODO: Load the data from CLDR here.
  return result;
}

void CDFLocaleStyleData::Init(UErrorCode& status) {
  unitsByVariant = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
  if (U_FAILURE(status)) {
    return;
  }
  uhash_setKeyDeleter(unitsByVariant, uprv_free);
  uhash_setValueDeleter(unitsByVariant, deleteCDFUnit);
}

CDFLocaleStyleData::~CDFLocaleStyleData() {
  if (unitsByVariant != NULL) {
    uhash_close(unitsByVariant);
  }
}

void CDFLocaleData::Init(UErrorCode& status) {
  shortData.Init(status);
  if (U_FAILURE(status)) {
    return;
  }
  longData.Init(status);
}

U_NAMESPACE_END
#endif
