/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File MEASFMTTEST.CPP
*
*******************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/measfmt.h"
#include "unicode/measure.h"
#include "unicode/measunit.h"
#include "unicode/tmutamt.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

class MeasureFormatTest : public IntlTest {
public:
    MeasureFormatTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestBasic();
    void TestGetAvailable();
    void TestExamplesInDocs();
};

void MeasureFormatTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite MeasureFormatTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasic);
    TESTCASE_AUTO(TestGetAvailable);
    TESTCASE_AUTO(TestExamplesInDocs);
    TESTCASE_AUTO_END;
}

void MeasureFormatTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureUnit *ptr1 = MeasureUnit::createArcMinute(status);
    MeasureUnit *ptr2 = MeasureUnit::createArcMinute(status);
    if (!(*ptr1 == *ptr2)) {
        errln("Expect == to work.");
    }
    if (*ptr1 != *ptr2) {
        errln("Expect != to work.");
    }
    MeasureUnit *ptr3 = MeasureUnit::createMeter(status);
    if (*ptr1 == *ptr3) {
        errln("Expect == to work.");
    }
    if (!(*ptr1 != *ptr3)) {
        errln("Expect != to work.");
    }
    MeasureUnit *ptr4 = (MeasureUnit *) ptr1->clone();
    if (*ptr1 != *ptr4) {
        errln("Expect clone to work.");
    }
    MeasureUnit stack;
    stack = *ptr1;
    if (*ptr1 != stack) {
        errln("Expect assignment to work.");
    }

    delete ptr1;
    delete ptr2;
    delete ptr3;
    delete ptr4;
}

void MeasureFormatTest::TestGetAvailable() {
    MeasureUnit *units = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int32_t totalCount = MeasureUnit::getAvailable(units, 0, status);
    while (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        delete [] units;
        units = new MeasureUnit[totalCount];
        totalCount = MeasureUnit::getAvailable(units, totalCount, status);
    }
    if (U_FAILURE(status)) {
        dataerrln("Failure creating format object - %s", u_errorName(status));
        delete [] units;
        return;
    }
    if (totalCount < 200) {
        errln("Expect at least 200 measure units including currencies.");
    }
    delete [] units;
    StringEnumeration *types = MeasureUnit::getAvailableTypes(status);
    if (U_FAILURE(status)) {
        dataerrln("Failure getting types - %s", u_errorName(status));
        delete types;
        return;
    }
    if (types->count(status) < 10) {
        errln("Expect at least 10 distinct unit types.");
    }
    units = NULL;
    int32_t unitCapacity = 0;
    int32_t unitCountSum = 0;
    for (
            const char* type = types->next(NULL, status);
            type != NULL;
            type = types->next(NULL, status)) {
        int32_t unitCount = MeasureUnit::getAvailable(type, units, unitCapacity, status);
        while (status == U_BUFFER_OVERFLOW_ERROR) {
            status = U_ZERO_ERROR;
            delete [] units;
            units = new MeasureUnit[unitCount];
            unitCapacity = unitCount;
            unitCount = MeasureUnit::getAvailable(type, units, unitCapacity, status);
        }
        if (U_FAILURE(status)) {
            dataerrln("Failure getting units - %s", u_errorName(status));
            delete [] units;
            delete types;
            return;
        }
        if (unitCount < 1) {
            errln("Expect at least one unit count per type.");
        }
        unitCountSum += unitCount;
    }
    if (unitCountSum != totalCount) {
        errln("Expected total unit count to equal sum of unit counts by type.");
    }
    delete [] units;
    delete types;
}

void MeasureFormatTest::TestExamplesInDocs() {
    UnicodeString buffer;
    FieldPosition pos(0);
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmtFr(Locale::getFrench(), UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating fmtFr.", status)) {
        return;
    }
    Measure measureC(23, MeasureUnit::createCelsius(status), status);
    if (!assertSuccess("Error creating measureC.", status)) {
        return;
    }
    fmtFr.formatMeasures(
            &measureC,
            1,
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting measureC.", status)) {
        return;
    }
    assertEquals(
            "formatting measureC",
            UnicodeString("23 \\u00B0C").unescape(),
            buffer);
    buffer.remove();

    Measure measureF(70, MeasureUnit::createFahrenheit(status), status);
    if (!assertSuccess("Error creating measureF.", status)) {
        return;
    }
    fmtFr.formatMeasures(
            &measureF,
            1,
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting measureF.", status)) {
        return;
    }
    assertEquals(
            "formatting measureF",
            UnicodeString("70 \\u00B0F").unescape(),
            buffer);
    buffer.remove();
    
    MeasureFormat fmtFrFull(
            Locale::getFrench(), UMEASFMT_WIDTH_WIDE, status);
    if (!assertSuccess("Error creating fmtFrFull.", status)) {
        return;
    }
    Measure poundsAndOunces[] = {
            Measure(70, MeasureUnit::createPound(status), status),
            Measure(5.3, MeasureUnit::createOunce(status), status)};
    if (!assertSuccess("Error creating poundsAndOunces.", status)) {
        return;
    }
    fmtFrFull.formatMeasures(
            poundsAndOunces,
            LENGTHOF(poundsAndOunces),
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting poundsAndOunces.", status)) {
        return;
    }
    assertEquals(
            "formatting poundsAndOunces",
            UnicodeString("70 pieds et 5,3 pouces"),
            buffer);
    buffer.remove();
    Measure poundAndOunce[] = {
            Measure(1, MeasureUnit::createPound(status), status),
            Measure(1, MeasureUnit::createOunce(status), status)};
    if (!assertSuccess("Error creating poundAndOunce.", status)) {
        return;
    }
    fmtFrFull.formatMeasures(
            poundAndOunce,
            LENGTHOF(poundAndOunce),
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting poundAndOunce.", status)) {
        return;
    }
    assertEquals(
            "formatting poundAndOunce",
            UnicodeString("1 pied et 1 pouce"),
            buffer);
    buffer.remove();

    MeasureFormat fmtFrNarrow(
            Locale::getFrench(), UMEASFMT_WIDTH_NARROW, status);
    if (!assertSuccess("Error creating fmtFrNarrow.", status)) {
        return;
    }
    Measure footAndInch[] = {
            Measure(1, MeasureUnit::createFoot(status), status),
            Measure(1, MeasureUnit::createInch(status), status)};
    if (!assertSuccess("Error creating footAndInch.", status)) {
        return;
    }
    fmtFrNarrow.formatMeasures(
            footAndInch,
            LENGTHOF(footAndInch),
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting footAndInch.", status)) {
        return;
    }
    assertEquals(
            "formatting footAndInch",
            UnicodeString("1\\u2032 1\\u2033").unescape(),
            buffer);
    buffer.remove();

    MeasureFormat fmtEn(Locale::getUS(), UMEASFMT_WIDTH_WIDE, status);
    if (!assertSuccess("Error creating fmtEn.", status)) {
        return;
    }
    Measure inchAndFeet[] = {
            Measure(1, MeasureUnit::createInch(status), status),
            Measure(2, MeasureUnit::createFoot(status), status)};
    if (!assertSuccess("Error creating inchAndFeet.", status)) {
        return;
    }
    fmtEn.formatMeasures(
            inchAndFeet,
            LENGTHOF(inchAndFeet),
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting inchAndFeet.", status)) {
        return;
    }
    assertEquals(
            "formatting inchAndFeet",
            UnicodeString("1 inch, 2 feet"),
            buffer);
    buffer.remove();
}


extern IntlTest *createMeasureFormatTest() {
    return new MeasureFormatTest();
}

#endif

