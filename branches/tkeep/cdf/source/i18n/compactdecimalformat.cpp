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

#include "charstr.h"
#include "cstring.h"
#include "digitlst.h"
#include "mutex.h"
#include "unicode/compactdecimalformat.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/ures.h"
#include "ucln_in.h"
#include "uhash.h"
#include "umutex.h"
#include "uresimp.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

// Maps locale name to CDFLocaleData struct.
static UHashtable* gCompactDecimalData = NULL;
static UMutex gCompactDecimalMetaLock = U_MUTEX_INITIALIZER;

U_NAMESPACE_BEGIN

static const int32_t MAX_DIGITS = 15;
static const char gOther[] = "other";
static const char gLatnTag[] = "latn";
static const char gNumberElementsTag[] = "NumberElements";
static const char gDecimalFormatTag[] = "decimalFormat";
static const char gPatternsShort[] = "patternsShort";
static const char gPatternsLong[] = "patternsLong";

static const UChar u_0 = 0x30;
static const UChar u_sq = 0x27;

static const UChar kZero[] = {u_0};

// Used to unescape single quotes.
enum QuoteState {
  OUTSIDE,
  INSIDE_EMPTY,
  INSIDE_FULL
};

// CDFUnit represents a prefix-suffix pair for a particular variant
// and log10 value.
struct CDFUnit {
  UnicodeString prefix;
  UnicodeString suffix;
  inline CDFUnit() : prefix(), suffix() {
    prefix.setToBogus();
  }
  inline ~CDFUnit() {}
  inline UBool isSet() const {
    return !prefix.isBogus();
  }
  inline void markAsSet() {
    prefix.remove();
  }
};

// CDFLocaleStyleData contains formatting data for a particular locale
// and style.
class CDFLocaleStyleData {
 public:
  // What to divide by for each log10 value when formatting. These values
  // will be powers of 10. For English, would be:
  // 1, 1, 1, 1000, 1000, 1000, 1000000, 1000000, 1000000, 1000000000 ...
  double divisors[MAX_DIGITS];
  // Maps plural variants to CDFUnit[MAX_DIGITS] arrays.
  // To format a number x,
  // first compute log10(x). Compute displayNum = (x / divisors[log10(x)]).
  // Compute the plural variant for displayNum
  // (e.g zero, one, two, few, many, other).
  // Compute cdfUnits = unitsByVariant[pluralVariant].
  // Prefix and suffix to use at cdfUnits[log10(x)]
  UHashtable* unitsByVariant;
  inline CDFLocaleStyleData() : unitsByVariant(NULL) {}
  ~CDFLocaleStyleData();
  // Init initializes this object.
  void Init(UErrorCode& status);
  inline UBool isBogus() const {
    return unitsByVariant == NULL;
  }
  void setToBogus();
 private:
  CDFLocaleStyleData(const CDFLocaleStyleData&);
  CDFLocaleStyleData& operator=(const CDFLocaleStyleData&);
};

// CDFLocaleData contains formatting data for a particular locale.
struct CDFLocaleData {
  CDFLocaleStyleData shortData;
  CDFLocaleStyleData longData;
  inline CDFLocaleData() : shortData(), longData() { }
  inline ~CDFLocaleData() { }
  // Init initializes this object.
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

static void U_CALLCONV deleteCDFUnits(void* ptr) {
  delete [] (icu::CDFUnit*) ptr;
}

static void U_CALLCONV deleteCDFLocaleData(void* ptr) {
  delete (icu::CDFLocaleData*) ptr;
}

U_CDECL_END

U_NAMESPACE_BEGIN

static UBool divisors_equal(const double* lhs, const double* rhs);
static const CDFLocaleStyleData* getCDFLocaleStyleData(const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status);

static const CDFLocaleStyleData* extractDataByStyleEnum(const CDFLocaleData& data, UNumberCompactStyle style, UErrorCode& status);
static CDFLocaleData* loadCDFLocaleData(const Locale& inLocale, UErrorCode& status);
static void initCDFLocaleData(const Locale& inLocale, CDFLocaleData* result, UErrorCode& status);
static void initCDFLocaleStyleData(const UResourceBundle* localeBundle, const char* numberingSystem, const char* style, CDFLocaleStyleData* result, UErrorCode& status);
static void populatePower10(const UResourceBundle* power10Bundle, CDFLocaleStyleData* result, UErrorCode& status);
static int32_t populatePrefixSuffix(const char* variant, int32_t log10Value, const UChar* formatStr, int32_t formatStrLen, UHashtable* result, UErrorCode& status);
static UBool onlySpaces(UnicodeString u);
static void fixQuotes(const UChar* start, int32_t len, UnicodeString* result);
static void fillInMissing(CDFLocaleStyleData* result);
static int32_t computeLog10(double x, UBool inRange);
static CDFUnit* createCDFUnit(const char* variant, int32_t log10Value, UHashtable* table, UErrorCode& status);
static const CDFUnit* getCDFUnitFallback(const UHashtable* table, const UnicodeString& variant, int32_t log10Value);
static int32_t unicodeToCString(const UnicodeString& src, char* buffer, int32_t bufferLen);

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
  CompactDecimalFormat* result =
      new CompactDecimalFormat(*decfmt, data->unitsByVariant, data->divisors, pluralRules.getAlias());
  if (result == NULL) {
    status = U_MEMORY_ALLOCATION_ERROR;
    return NULL;
  }
  pluralRules.orphan();
  result->setMaximumSignificantDigits(2);
  result->setSignificantDigitsUsed(TRUE);
  result->setGroupingUsed(FALSE);
  return result;
}

// TODO: _pluralRules will be NULL on out of memory error.
// Should rest of this class gracefully handle _pluralRules being NULL?
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
  DigitList orig, rounded;
  orig.set(number);
  UBool isNegative;
  UErrorCode status = U_ZERO_ERROR;
  _round(orig, rounded, isNegative, status);
  if (U_FAILURE(status)) {
    return appendTo;
  }
  double roundedDouble = rounded.getDouble();
  if (isNegative) {
    roundedDouble = -roundedDouble;
  }
  int32_t baseIdx = computeLog10(roundedDouble, TRUE);
  double numberToFormat = roundedDouble / _divisors[baseIdx];
  UnicodeString variant = _pluralRules->select(numberToFormat);
  if (isNegative) {
    numberToFormat = -numberToFormat;
  }
  const CDFUnit* unit = getCDFUnitFallback(_unitsByVariant, variant, baseIdx);
  appendTo += unit->prefix;
  DecimalFormat::format(numberToFormat, appendTo, pos);
  appendTo += unit->suffix;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    double /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
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
    int64_t /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    const StringPiece& /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    const DigitList& /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(const DigitList& /* number */,
                             UnicodeString& appendTo,
                             FieldPosition& /* pos */,
                             UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

void
CompactDecimalFormat::parse(
    const UnicodeString& /* text */,
    Formattable& /* result */,
    ParsePosition& /* parsePosition */) const {
}

void
CompactDecimalFormat::parse(
    const UnicodeString& /* text */,
    Formattable& /* result */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
}

CurrencyAmount*
CompactDecimalFormat::parseCurrency(
    const UnicodeString& /* text */,
    ParsePosition& /* pos */) const {
  return NULL;
}

void CDFLocaleStyleData::Init(UErrorCode& status) {
  if (unitsByVariant != NULL) {
    return;
  }
  unitsByVariant = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
  if (U_FAILURE(status)) {
    return;
  }
  uhash_setKeyDeleter(unitsByVariant, uprv_free);
  uhash_setValueDeleter(unitsByVariant, deleteCDFUnits);
}

CDFLocaleStyleData::~CDFLocaleStyleData() {
  setToBogus();
}

void CDFLocaleStyleData::setToBogus() {
  if (unitsByVariant != NULL) {
    uhash_close(unitsByVariant);
    unitsByVariant = NULL;
  }
}

void CDFLocaleData::Init(UErrorCode& status) {
  shortData.Init(status);
  if (U_FAILURE(status)) {
    return;
  }
  longData.Init(status);
}

// Helper method for operator=
static UBool divisors_equal(const double* lhs, const double* rhs) {
  for (int32_t i = 0; i < MAX_DIGITS; i++) {
    if (lhs[i] != rhs[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

// getCDFLocaleStyleData returns pointer to formatting data for given locale and 
// style within the global cache. On cache miss, getCDFLocaleStyleData loads
// the data from CLDR into the global cache before returning the pointer. If a
// UNUM_LONG data is requested for a locale, and that locale does not have
// UNUM_LONG data, getCDFLocaleStyleData will fall back to UNUM_SHORT data for
// that locale.
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
      if (!data.longData.isBogus()) {
        return &data.longData;
      }
      return &data.shortData;
    default:
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return NULL;
  }
}

// loadCDFLocaleData loads formatting data from CLDR for a given locale. The
// caller owns the returned pointer.
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

  initCDFLocaleData(inLocale, result, status);
  if (U_FAILURE(status)) {
    delete result;
    return NULL;
  }
  return result;
}

// initCDFLocaleData initializes result with data from CLDR.
// inLocale is the locale, the CLDR data is stored in result.
// First it will try to load data using locale specific numbering system
// for styles UNUM_SHORT and UNUM_LONG. If that fails for a particular style,
// it falls back to the latn numbering system for that style. If UNUM_LONG
// data cannot be loaded, it is marked bogus in result; if UNUM_SHORT
// data cannot be loaded, that is a failure.
static void initCDFLocaleData(const Locale& inLocale, CDFLocaleData* result, UErrorCode& status) {
  LocalPointer<NumberingSystem> ns(NumberingSystem::createInstance(inLocale, status));
  if (U_FAILURE(status)) {
    return;
  }
  const char* numberingSystemName = ns->getName();
  UResourceBundle* rb = ures_open(NULL, inLocale.getName(), &status);
  rb = ures_getByKeyWithFallback(rb, gNumberElementsTag, rb, &status);
  if (U_FAILURE(status)) {
    ures_close(rb);
    return;
  }
  UErrorCode ec = U_ZERO_ERROR;
  initCDFLocaleStyleData(
      rb, numberingSystemName, gPatternsLong, &result->longData, ec);
  if (U_FAILURE(ec)) {
    result->longData.setToBogus();
  }
  initCDFLocaleStyleData(
      rb, numberingSystemName, gPatternsShort, &result->shortData, status);
  ures_close(rb);
  if (U_FAILURE(status)) {
    return;
  }
}

// initCDFLocaleStyleData loads formatting data for a numbering system
// and style from CLDR.
// bundle is the resouce for the locale; style is the style: gPatternsShort
// or gPatternsLong. Loaded data stored in result.
static void initCDFLocaleStyleData(const UResourceBundle* numberElementsBundle, const char* numberingSystem, const char* style, CDFLocaleStyleData* result, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return;
  }
  UErrorCode ec = U_ZERO_ERROR;
  UResourceBundle* rb = ures_getByKeyWithFallback(numberElementsBundle, numberingSystem, NULL, &ec);
  rb = ures_getByKeyWithFallback(rb, style, rb, &ec);
  rb = ures_getByKeyWithFallback(rb, gDecimalFormatTag, rb, &ec);
  if (ec == U_MISSING_RESOURCE_ERROR && uprv_strcmp(numberingSystem, gLatnTag) != 0) {
    rb = ures_getByKeyWithFallback(numberElementsBundle, gLatnTag, rb, &status);
    rb = ures_getByKeyWithFallback(rb, style, rb, &status);
    rb = ures_getByKeyWithFallback(rb, gDecimalFormatTag, rb, &status);
  } else {
    status = ec;
  }
  if (U_FAILURE(status)) {
    ures_close(rb);
    return;
  }
  // Iterate through all the powers of 10.
  int32_t size = ures_getSize(rb);
  UResourceBundle* power10 = NULL;
  for (int32_t i = 0; i < size; ++i) {
    power10 = ures_getByIndex(rb, i, power10, &status);
    if (U_FAILURE(status)) {
      ures_close(power10);
      ures_close(rb);
      return;
    }
    populatePower10(power10, result, status);
    if (U_FAILURE(status)) {
      ures_close(power10);
      ures_close(rb);
      return;
    }
  }
  ures_close(power10);
  ures_close(rb);
  fillInMissing(result);
}

// populatePower10 grabs data for a particular power of 10 from CLDR.
// The loaded data is stored in result.
static void populatePower10(const UResourceBundle* power10Bundle, CDFLocaleStyleData* result, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return;
  }
  double power10 = uprv_strtod(ures_getKey(power10Bundle), NULL);
  int32_t log10Value = computeLog10(power10, FALSE);
  // Silently ignore divisors that are too big.
  if (log10Value == MAX_DIGITS) {
    return;
  }
  int32_t size = ures_getSize(power10Bundle);
  int32_t numZeros = 0;
  UBool otherVariantDefined = FALSE;
  UResourceBundle* variantBundle = NULL;
  // Iterate over all the plural variants for the power of 10
  for (int i = 0; i < size; ++i) {
    variantBundle = ures_getByIndex(power10Bundle, i, variantBundle, &status);
    if (U_FAILURE(status)) {
      ures_close(variantBundle);
      return;
    }
    const char* variant = ures_getKey(variantBundle);
    int32_t resLen;
    const UChar* formatStr = ures_getString(variantBundle, &resLen, &status);
    if (U_FAILURE(status)) {
      ures_close(variantBundle);
      return;
    }
    if (uprv_strcmp(variant, gOther) == 0) {
      otherVariantDefined = TRUE;
    }
    int nz = populatePrefixSuffix(
        variant, log10Value, formatStr, resLen, result->unitsByVariant, status);
    if (U_FAILURE(status)) {
      ures_close(variantBundle);
      return;
    }
    if (nz != numZeros) {
      // We expect all format strings to have the same number of 0's
      // left of the decimal point.
      if (numZeros != 0) {
        status = U_INTERNAL_PROGRAM_ERROR;
        ures_close(variantBundle);
        return;
      }
      numZeros = nz;
    }
  }
  ures_close(variantBundle);
  // We expect to find an OTHER variant for each power of 10.
  if (!otherVariantDefined) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return;
  }
  long divisor = power10;
  for (int i = 1; i < numZeros; i++) {
    divisor /= 10.0;
  }
  result->divisors[log10Value] = divisor;
}

// populatePrefixSuffix Adds a specific prefix-suffix pair to result for a
// given variant and log10 value.
// variant is 'zero', 'one', 'two', 'few', 'many', or 'other'.
// formatStr is the format string from which the prefix and suffix are
// extracted. It is usually of form 'Pefix 000 suffix'.
// populatePrefixSuffix returns the number of 0's found in formatStr
// before the decimal point.
// In the special case that formatStr contains only spaces for prefix
// and suffix, populatePrefixSuffix returns log10Value + 1.
static int32_t populatePrefixSuffix(
    const char* variant, int32_t log10Value, const UChar* formatStr, int32_t formatStrLen, UHashtable* result, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return 0;
  }
  const UChar* firstIdx = u_strFindFirst(formatStr, formatStrLen, kZero, LENGTHOF(kZero));
  // We must have 0's in format string.
  if (firstIdx == NULL) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return 0;
  }
  const UChar* lastIdx = u_strFindLast(firstIdx, formatStrLen - (firstIdx - formatStr), kZero, LENGTHOF(kZero));
  CDFUnit* unit = createCDFUnit(variant, log10Value, result, status);
  if (U_FAILURE(status)) {
    return 0;
  }
  // Everything up to first 0 is the prefix
  fixQuotes(formatStr, firstIdx - formatStr, &unit->prefix);
  // Everything beyond the last 0 is the suffix
  fixQuotes(lastIdx + 1, formatStrLen - (lastIdx - formatStr) - 1, &unit->suffix);

  // If there is effectively no prefix or suffix, ignore the actual number of
  // 0's and act as if the number of 0's matches the size of the number.
  if (onlySpaces(unit->prefix) && onlySpaces(unit->suffix)) {
    return log10Value + 1;
  }

  // Calculate number of zeros before decimal point
  const UChar* i = firstIdx + 1;
  while (i <= lastIdx && *i == u_0) {
    ++i;
  }
  return (i - firstIdx);
}

static UBool onlySpaces(UnicodeString u) {
  return u.trim().length() == 0;
}

// fixQuotes appends a UChar array to a UnicodeString unescaping single
// quotes as it goes. Don''t -> Don't. Letter 'j' -> Letter j.
// start is the beginning of the UChar array. len is the length of the
// array. UChar array appended to result.
static void fixQuotes(const UChar* start, int32_t len, UnicodeString* result) {
  const UChar* end = start + len;
  QuoteState state = OUTSIDE;
  for (const UChar* ptr = start; ptr < end; ++ptr) {
    if (*ptr == u_sq) {
      if (state == INSIDE_EMPTY) {
        result->append(*ptr);
      }
    } else {
      result->append(*ptr);
    }

    // Update state
    switch (state) {
      case OUTSIDE:
        state = *ptr == u_sq ? INSIDE_EMPTY : OUTSIDE;
        break;
      case INSIDE_EMPTY:
      case INSIDE_FULL:
        state = *ptr == u_sq ? OUTSIDE : INSIDE_FULL;
        break;
      default:
        break;
    }
  }
}

// fillInMissing ensures that the data in result is complete.
// result data is complete if for each variant in result, there exists
// a prefix-suffix pair for each log10 value and there also exists
// a divisor for each log10 value.
//
// First this function figures out for which log10 values, the other
// variant already had data. These are the same log10 values defined
// in CLDR. 
//
// For each log10 value not defined in CLDR, it uses the divisor for
// the last defined log10 value or 1.
//
// Then for each variant, it does the following. For each log10
// value not defined in CLDR, copy the prefix-suffix pair from the
// previous log10 value. If log10 value is defined in CLDR but is
// missing from given variant, copy the prefix-suffix pair for that
// log10 value from the 'other' variant.
static void fillInMissing(CDFLocaleStyleData* result) {
  const CDFUnit* otherUnits =
      (const CDFUnit*) uhash_get(result->unitsByVariant, gOther);
  UBool definedInCLDR[MAX_DIGITS];
  double lastDivisor = 1.0;
  for (int i = 0; i < MAX_DIGITS; ++i) {
    if (!otherUnits[i].isSet()) {
      result->divisors[i] = lastDivisor;
      definedInCLDR[i] = FALSE;
    } else {
      lastDivisor = result->divisors[i];
      definedInCLDR[i] = TRUE;
    }
  }
  // Iterate over each variant.
  int32_t pos = -1;
  const UHashElement* element = uhash_nextElement(result->unitsByVariant, &pos);
  for (;element != NULL; element = uhash_nextElement(result->unitsByVariant, &pos)) {
    CDFUnit* units = (CDFUnit*) element->value.pointer;
    for (int32_t i = 0; i < MAX_DIGITS; ++i) {
      if (definedInCLDR[i]) {
        if (!units[i].isSet()) {
          units[i] = otherUnits[i];
        }
      } else {
        if (i == 0) {
          units[0].markAsSet();
        } else {
          units[i] = units[i - 1];
        }
      }
    }
  }
}

// computeLog10 computes floor(log10(x)). If inRange is TRUE, the biggest
// value computeLog10 will return MAX_DIGITS -1 even for
// numbers > 10^MAX_DIGITS. If inRange is FALSE, computeLog10 will return
// up to MAX_DIGITS.
static int32_t computeLog10(double x, UBool inRange) {
  int32_t result = 0;
  int32_t max = inRange ? MAX_DIGITS - 1 : MAX_DIGITS;
  while (x >= 10.0) {
    x /= 10.0;
    ++result;
    if (result == max) {
      break;
    }
  }
  return result;
}

// createCDFUnit returns a pointer to the prefix-suffix pair for a given
// variant and log10 value within table. If no such prefix-suffix pair is
// stored in table, one is created within table before returning pointer.
static CDFUnit* createCDFUnit(const char* variant, int32_t log10Value, UHashtable* table, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  CDFUnit *cdfUnit = (CDFUnit*) uhash_get(table, variant);
  if (cdfUnit == NULL) {
    cdfUnit = new CDFUnit[MAX_DIGITS];
    if (cdfUnit == NULL) {
      status = U_MEMORY_ALLOCATION_ERROR;
      return NULL;
    }
    uhash_put(table, uprv_strdup(variant), cdfUnit, &status);
    if (U_FAILURE(status)) {
      return NULL;
    }
  }
  CDFUnit* result = &cdfUnit[log10Value];
  result->markAsSet();
  return result;
}

// getCDFUnitFallback returns a pointer to the prefix-suffix pair for a given
// variant and log10 value within table. If the given variant doesn't exist, it
// falls back to the OTHER variant. Therefore, this method will always return
// some non-NULL value.
static const CDFUnit* getCDFUnitFallback(const UHashtable* table, const UnicodeString& variant, int32_t log10Value) {
  char cvariant[80];
  unicodeToCString(variant, cvariant, LENGTHOF(cvariant));
  const CDFUnit *cdfUnit = (const CDFUnit*) uhash_get(table, cvariant);
  if (cdfUnit == NULL) {
    cdfUnit = (const CDFUnit*) uhash_get(table, gOther);
  }
  return &cdfUnit[log10Value];
}

// unicodeToCString converts a UnicodeString to a null terminated char* and returns
// the length. Note that this function cannot return more than bufferLen - 1.
static int32_t unicodeToCString(const UnicodeString& src, char* buffer, int32_t bufferLen) {
  int32_t len = src.length();
  if (len > bufferLen - 1) {
    len = bufferLen - 1;
  }
  u_UCharsToChars(src.getBuffer(), buffer, len);
  buffer[len] = 0;
  return len;
}

U_NAMESPACE_END
#endif
