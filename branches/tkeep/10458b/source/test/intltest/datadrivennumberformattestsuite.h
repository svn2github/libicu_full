/************************************************************************
 * COPYRIGHT:
 * Copyright (c) 2015, International Business Machines Corporation
 * and others. All Rights Reserved.
 ************************************************************************/

#ifndef _DATADRIVENNUMBERFORMATTESTSUITE_H__
#define _DATADRIVENNUMBERFORMATTESTSUITE_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "numberformattesttuple.h"

struct UCHARBUF;
class IntlTest;

U_NAMESPACE_BEGIN

/**
 * Performs various in-depth test on NumberFormat
 **/
class DataDrivenNumberFormatTestSuite : public UObject {

 public:
     DataDrivenNumberFormatTestSuite(IntlTest *test)
     : fTest(test), fFileLineNumber(0) { }
     void run(const char *fileName, UBool runAllTests);
 protected:
    virtual UBool isFormatPass(
            const NumberFormatTestTuple &tuple,
            UnicodeString &appendErrorMessage,
            UErrorCode &status);
 private:
    IntlTest *fTest;
    UnicodeString fFileLine;
    int32_t fFileLineNumber;
    UnicodeString fFileTestName;
    NumberFormatTestTuple fTuple;

    void setTupleField(UErrorCode &);
    int32_t splitBy(
            UnicodeString *columnValues,
            int32_t columnValueCount,
            UChar delimiter);
    void showError(const char *message);
    void showFailure(const UnicodeString &message);
    void showLineInfo();
    UBool breaksC();
    UBool readLine(UCHARBUF *f, UErrorCode &);
    UBool isPass(
            const NumberFormatTestTuple &tuple,
            UnicodeString &appendErrorMessage,
            UErrorCode &status);
};

U_NAMESPACE_END

#endif // _DATADRIVENNUMBERFORMATTESTSUITE_
