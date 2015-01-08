/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File NUMBERFORMAT2TEST.CPP
*
*******************************************************************************
*/
#include "unicode/utypes.h"

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "digitformatter.h"
#include "digitinterval.h"
#include "digitlst.h"
#include "digitgrouping.h"
#include "unicode/localpointer.h"

class NumberFormat2Test : public IntlTest {
public:
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestDigitInterval();
    void verifyInterval(const DigitInterval &, int32_t minInclusive, int32_t maxExclusive);
    void TestGroupingUsed();
    void TestGrouping();
    void TestDigitListInterval();
    void TestDigitFormatter();
};

void NumberFormat2Test::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite ScientificNumberFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestDigitInterval);
    TESTCASE_AUTO(TestGroupingUsed);
    TESTCASE_AUTO(TestGrouping);
    TESTCASE_AUTO(TestDigitListInterval);
    TESTCASE_AUTO(TestDigitFormatter);
    TESTCASE_AUTO_END;
}

void NumberFormat2Test::TestDigitInterval() {
    DigitInterval all;
    DigitInterval threeInts;
    DigitInterval fourFrac;
    threeInts.setDigitsLeft(3);
    fourFrac.setDigitsRight(4);
    verifyInterval(all, INT32_MIN, INT32_MAX);
    verifyInterval(threeInts, INT32_MIN, 3);
    verifyInterval(fourFrac, -4, INT32_MAX);
    {
        DigitInterval result(threeInts);
        result.shrinkToFitWithin(fourFrac);
        verifyInterval(result, -4, 3);
    }
    {
        DigitInterval result(threeInts);
        result.expandToContain(fourFrac);
        verifyInterval(result, INT32_MIN, INT32_MAX);
    }
    {
        DigitInterval result(threeInts);
        result.setDigitsLeft(0);
        verifyInterval(result, INT32_MIN, 0);
        result.setDigitsLeft(-1);
        verifyInterval(result, INT32_MIN, INT32_MAX);
    }
    {
        DigitInterval result(fourFrac);
        result.setDigitsRight(0);
        verifyInterval(result, 0, INT32_MAX);
        result.setDigitsRight(-1);
        verifyInterval(result, INT32_MIN, INT32_MAX);
    }
}

void NumberFormat2Test::verifyInterval(
        const DigitInterval &interval,
        int32_t minInclusive, int32_t maxExclusive) {
    assertEquals("", minInclusive, interval.getSmallestInclusive());
    assertEquals("", maxExclusive, interval.getLargestExclusive());
}

void NumberFormat2Test::TestGroupingUsed() {
    {
        DigitGrouping grouping;
        assertFalse("", grouping.isGroupingUsed());
    }
    {
        DigitGrouping grouping;
        grouping.fGrouping = 2;
        assertTrue("", grouping.isGroupingUsed());
    }
}

void NumberFormat2Test::TestGrouping() {
    {
        DigitGrouping grouping;
        assertEquals("", 0, grouping.fGrouping);
        assertEquals("", 0, grouping.fGrouping2);
        assertEquals("", 0, grouping.fMinGrouping);
        assertEquals("", 0, grouping.getSeparatorCount(10));
        assertEquals("", 0, grouping.getSeparatorCount(0));
    }
    {
        DigitGrouping grouping;
        grouping.fGrouping = 3;
        assertEquals("", 0, grouping.getSeparatorCount(0));
        assertEquals("", 0, grouping.getSeparatorCount(3));
        assertEquals("", 1, grouping.getSeparatorCount(4));
        assertEquals("", 1, grouping.getSeparatorCount(6));
        assertEquals("", 2, grouping.getSeparatorCount(7));
    }
    {
        DigitGrouping grouping;
        grouping.fGrouping = 3;
        grouping.fGrouping2 = 2;
        assertEquals("", 0, grouping.getSeparatorCount(0));
        assertEquals("", 0, grouping.getSeparatorCount(3));
        assertEquals("", 1, grouping.getSeparatorCount(4));
        assertEquals("", 2, grouping.getSeparatorCount(6));
        assertEquals("", 2, grouping.getSeparatorCount(7));
        assertEquals("", 3, grouping.getSeparatorCount(8));
    }
    {
        DigitGrouping grouping;
        grouping.fGrouping = 3;
        grouping.fMinGrouping = 2;
        assertEquals("", 0, grouping.getSeparatorCount(0));
        assertEquals("", 0, grouping.getSeparatorCount(4));
        assertEquals("", 1, grouping.getSeparatorCount(5));
        assertEquals("", 1, grouping.getSeparatorCount(6));
        assertEquals("", 2, grouping.getSeparatorCount(7));
        assertEquals("", 2, grouping.getSeparatorCount(9));
        assertEquals("", 3, grouping.getSeparatorCount(10));
    }
}

void NumberFormat2Test::TestDigitListInterval() {
    DigitInterval result;
    DigitList digitList;
    {
        digitList.set(12345);
        verifyInterval(digitList.getSmallestInterval(result), 0, 5);
    }
    {
        digitList.set(1000.00);
        verifyInterval(digitList.getSmallestInterval(result), 0, 4);
    }
    {
        digitList.set(43.125);
        verifyInterval(digitList.getSmallestInterval(result), -3, 2);
    }
    {
        digitList.set(.0078125);
        verifyInterval(digitList.getSmallestInterval(result), -7, 0);
    }
}

void NumberFormat2Test::TestDigitFormatter() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    DigitFormatter formatter(symbols);
    DigitList digits;
    DigitInterval interval;
    {
        DigitGrouping grouping;
        UnicodeString appendTo;
        digits.set(8192);
        assertEquals(
                "",
                "8192",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        FALSE,
                        appendTo));
        appendTo.remove();
        assertEquals(
                "",
                "8192.",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        TRUE,
                        appendTo));

        // Turn on grouping
        grouping.fGrouping = 3;
        appendTo.remove();
        assertEquals(
                "",
                "8,192",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        FALSE,
                        appendTo));

        // turn on min grouping which will suppress grouping
        grouping.fMinGrouping = 2;
        appendTo.remove();
        assertEquals(
                "",
                "8192",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        FALSE,
                        appendTo));

        // adding one more digit will enable grouping once again.
        digits.set(43560);
        appendTo.remove();
        assertEquals(
                "",
                "43,560",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        FALSE,
                        appendTo));
    }
    {
        DigitGrouping grouping;
        UnicodeString appendTo;
        digits.set(31415926.0078125);
        assertEquals(
                "",
                "31415926.0078125",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        FALSE,
                        appendTo));

        // Turn on grouping with secondary.
        grouping.fGrouping = 2;
        grouping.fGrouping2 = 3;
        appendTo.remove();
        assertEquals(
                "",
                "314,159,26.0078125",
                formatter.format(
                        digits,
                        grouping,
                        digits.getSmallestInterval(interval),
                        FALSE,
                        appendTo));

        // Pad with zeros by widening interval.
        interval.setDigitsLeft(9);
        interval.setDigitsRight(10);
        appendTo.remove();
        assertEquals(
                "",
                "0,314,159,26.0078125000",
                formatter.format(
                        digits,
                        grouping,
                        interval,
                        FALSE,
                        appendTo));

    }
}

extern IntlTest *createNumberFormat2Test() {
    return new NumberFormat2Test();
}

#endif /* !UCONFIG_NO_FORMATTING */
