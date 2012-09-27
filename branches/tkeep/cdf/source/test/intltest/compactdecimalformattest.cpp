#include <stdlib.h>

#include "intltest.h"
#include "unicode/compactdecimalformat.h"
#include "unicode/unum.h"

class CompactDecimalFormatTest : public IntlTest {
public:
    CompactDecimalFormatTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestHelloWorld();
};

void CompactDecimalFormatTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *par) {
  switch(index) {
    case 0:
      name = "TestHelloWorld";
      if (exec) {
        TestHelloWorld();
      }
      break;
    default: name = ""; break;
  }
}

void CompactDecimalFormatTest::TestHelloWorld() {
  UErrorCode error;
  if (CompactDecimalFormat::createInstance(Locale::getUS(), UNUM_SHORT, error) != NULL) {
    errln("Test failed!");
  }
}

extern IntlTest *createCompactDecimalFormatTest() {
  return new CompactDecimalFormatTest();
}
