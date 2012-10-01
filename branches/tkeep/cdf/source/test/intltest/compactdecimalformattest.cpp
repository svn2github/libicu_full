#include <stdio.h>
#include <stdlib.h>

#include "intltest.h"
#include "unicode/compactdecimalformat.h"
#include "unicode/unum.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

typedef struct ExpectedResult {
  double value;
  const char *expected;
} ExpectedResult;

static const char *kShortStr = "Short";
static const char *kLongStr = "Long";

static ExpectedResult kEnglishShort[] = {
  {0.0, "0.0"},
  {0.17, "0.2"},
  {1, "1"},
  {1234, "1.2K"},
  {12345, "12K"},
  {123456, "120K"},
  {1234567, "1.2M"},
  {12345678, "12M"},
  {123456789, "120M"},
  {1234567890, "1.2B"},
  {12345678901, "12B"},
  {123456789012, "120B"},
  {1234567890123, "1.2T"},
  {12345678901234, "12T"},
  {123456789012345, "120T"},
  {1234567890123456, "1200T"}};

class CompactDecimalFormatTest : public IntlTest {
public:
    CompactDecimalFormatTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestEnglishShort();
    void CheckLocale(
        const Locale& locale, UNumberCompactStyle style,
        const ExpectedResult* expectedResult, int32_t expectedResultSize);
    void CheckExpectedResult(
        const CompactDecimalFormat* cdf, const ExpectedResult* expectedResult,
        const char* description);
    static const char *StyleStr(UNumberCompactStyle style);
};

void CompactDecimalFormatTest::runIndexedTest(
    int32_t index, UBool exec, const char *&name, char *) {
  switch(index) {
    case 0:
      name = "TestEnglishShort";
      if (exec) {
        TestEnglishShort();
      }
      break;
    default: name = ""; break;
  }
}

void CompactDecimalFormatTest::TestEnglishShort() {
  UErrorCode error;
  if (CompactDecimalFormat::createInstance(Locale::getUS(), UNUM_SHORT, error) != NULL) {
    errln("Test failed!");
  }
  Locale locale("en");
  CheckLocale(locale, UNUM_SHORT, kEnglishShort, LENGTHOF(kEnglishShort));
}

void CompactDecimalFormatTest::CheckLocale(const Locale& locale, UNumberCompactStyle style, const ExpectedResult* expectedResults, int32_t expectedResultSize) {
  UErrorCode status = U_ZERO_ERROR;
  CompactDecimalFormat* cdf = CompactDecimalFormat::createInstance(locale, style, status);
  if (U_FAILURE(status)) {
    errln("Unable to create format object - %s", u_errorName(status));
  }
  if (cdf == NULL) {
    errln("Got NULL pointer back for format object.");
  }
  char description[256];
  sprintf(description,"%s - %s", locale.getName(), StyleStr(style));
  for (int32_t i = 0; i < expectedResultSize; i++) {
    CheckExpectedResult(cdf, &expectedResults[i], description);
  }
  delete cdf;
}

void CompactDecimalFormatTest::CheckExpectedResult(
    const CompactDecimalFormat* cdf, const ExpectedResult* expectedResult, const char* description) {
  UnicodeString actual;
  cdf->format(expectedResult->value, actual);
  UnicodeString expected(expectedResult->expected, -1, US_INV);
  expected = expected.unescape();
  if (actual != expected) {
    errln(UnicodeString("Fail: Expected: ") + expected
          + UnicodeString(" Got: ") + actual
          + UnicodeString(" for: ") + UnicodeString(description));
  }
}

const char *CompactDecimalFormatTest::StyleStr(UNumberCompactStyle style) {
  if (style == UNUM_SHORT) {
    return kShortStr;
  }
  return kLongStr;
}

extern IntlTest *createCompactDecimalFormatTest() {
  return new CompactDecimalFormatTest();
}
