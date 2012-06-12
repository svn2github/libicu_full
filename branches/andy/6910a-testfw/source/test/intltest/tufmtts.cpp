/********************************************************************
 * Copyright (c) 2008-2012, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"
#include "unicode/tmunit.h"
#include "unicode/tmutamt.h"
#include "unicode/tmutfmt.h"
#include "tufmtts.h"
#include "unicode/ustring.h"

//TODO: put as compilation flag
//#define TUFMTTS_DEBUG 1

#ifdef TUFMTTS_DEBUG
#include <iostream>
#endif

void TimeUnitTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ ) {
    if (exec) logln("TestSuite TimeUnitTest");
    switch (index) {
        TESTCASE(0, testBasic);
        TESTCASE(1, testAPI);
        TESTCASE(2, testGreekWithFallback);
        TESTCASE(3, testGreekWithSanitization);
        default: name = ""; break;
    }
}

/**
 * Test basic
 */
void TimeUnitTest::testBasic() {
    const char* locales[] = {"en", "sl", "fr", "zh", "ar", "ru", "zh_Hant", "pa"};
    for ( unsigned int locIndex = 0; 
          locIndex < sizeof(locales)/sizeof(locales[0]); 
          ++locIndex ) {
        UErrorCode status = U_ZERO_ERROR;
        Locale loc(locales[locIndex]);
        TimeUnitFormat** formats = new TimeUnitFormat*[2];
        formats[UTMUTFMT_FULL_STYLE] = new TimeUnitFormat(loc, status);
        ASSERT_SUCCESS((status, "TimeUnitFormat(full) failed. Possibly missing data."));
        formats[UTMUTFMT_ABBREVIATED_STYLE] = new TimeUnitFormat(loc, UTMUTFMT_ABBREVIATED_STYLE, status);
        ASSERT_SUCCESS((status, "TimeUnitFormat(short)"));
#ifdef TUFMTTS_DEBUG
        std::cout << "locale: " << locales[locIndex] << "\n";
#endif
        for (int style = UTMUTFMT_FULL_STYLE; 
             style <= UTMUTFMT_ABBREVIATED_STYLE;
             ++style) {
          for (TimeUnit::UTimeUnitFields j = TimeUnit::UTIMEUNIT_YEAR; 
             j < TimeUnit::UTIMEUNIT_FIELD_COUNT; 
             j = (TimeUnit::UTimeUnitFields)(j+1)) {
#ifdef TUFMTTS_DEBUG
            std::cout << "time unit: " << j << "\n";
#endif
            double tests[] = {0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 5, 10, 100, 101.35};
            for (unsigned int i = 0; i < sizeof(tests)/sizeof(tests[0]); ++i) {
#ifdef TUFMTTS_DEBUG
                std::cout << "number: " << tests[i] << "\n";
#endif
                TimeUnitAmount* source = new TimeUnitAmount(tests[i], j, status);
                ASSERT_SUCCESS((status));
                UnicodeString formatted;
                Formattable formattable;
                formattable.adoptObject(source);
                formatted = ((Format*)formats[style])->format(formattable, formatted, status);
                ASSERT_SUCCESS((status));
#ifdef TUFMTTS_DEBUG
                char formatResult[1000];
                formatted.extract(0, formatted.length(), formatResult, "UTF-8");
                std::cout << "format result: " << formatResult << "\n";
#endif
                Formattable result;
                ((Format*)formats[style])->parseObject(formatted, result, status);
                ASSERT_SUCCESS((status));
                if (result != formattable) {
                    dataerrln("No round trip: ");
                }
                // other style parsing
                Formattable result_1;
                ((Format*)formats[1-style])->parseObject(formatted, result_1, status);
                ASSERT_SUCCESS((status));
                if (result_1 != formattable) {
                    dataerrln("No round trip: ");
                }
            }
          }
        }
        delete formats[UTMUTFMT_FULL_STYLE];
        delete formats[UTMUTFMT_ABBREVIATED_STYLE];
        delete[] formats;
    }
}


void TimeUnitTest::testAPI() {
    //================= TimeUnit =================
    UErrorCode status = U_ZERO_ERROR;

    TimeUnit* tmunit = TimeUnit::createInstance(TimeUnit::UTIMEUNIT_YEAR, status);
    ASSERT_SUCCESS((status));

    TimeUnit* another = (TimeUnit*)tmunit->clone();
    TimeUnit third(*tmunit);
    TimeUnit fourth = third;

    EXPECT_TRUE((*tmunit == *another, "orig and clone are equal"));
    EXPECT_TRUE((third == fourth, "copied and assigned are equal"));

    TimeUnit* tmunit_m = TimeUnit::createInstance(TimeUnit::UTIMEUNIT_MONTH, status);
    EXPECT_TRUE((*tmunit != *tmunit_m, "year != month"));

    TimeUnit::UTimeUnitFields field = tmunit_m->getTimeUnitField();
    EXPECT_TRUE((field == TimeUnit::UTIMEUNIT_MONTH, "field of month time unit is month"));
    
    delete tmunit;
    delete another;
    delete tmunit_m;
    //
    //================= TimeUnitAmount =================

    Formattable formattable((int32_t)2);
    TimeUnitAmount tma_long(formattable, TimeUnit::UTIMEUNIT_DAY, status);
    ASSERT_SUCCESS((status));

    formattable.setDouble(2);
    TimeUnitAmount tma_double(formattable, TimeUnit::UTIMEUNIT_DAY, status);
    ASSERT_SUCCESS((status));

    formattable.setDouble(3);
    TimeUnitAmount tma_double_3(formattable, TimeUnit::UTIMEUNIT_DAY, status);
    ASSERT_SUCCESS((status));

    TimeUnitAmount tma(2, TimeUnit::UTIMEUNIT_DAY, status);
    ASSERT_SUCCESS((status));

    TimeUnitAmount tma_h(2, TimeUnit::UTIMEUNIT_HOUR, status);
    ASSERT_SUCCESS((status));

    TimeUnitAmount second(tma);
    TimeUnitAmount third_tma = tma;
    TimeUnitAmount* fourth_tma = (TimeUnitAmount*)tma.clone();

    EXPECT_TRUE(((second == tma), "orig and copy are equal"));
    EXPECT_TRUE(((third_tma == *fourth_tma), "clone and assigned are equal"));
    EXPECT_TRUE(((tma_double != tma_double_3), "different if number diff"));
    EXPECT_TRUE(((tma_double != tma_long), "different if number type diff"));
    EXPECT_TRUE(((tma != tma_h), "different if time unit diff"));
    EXPECT_TRUE(((tma_double == tma), "same even different constructor"));

    EXPECT_TRUE((tma.getTimeUnitField() == TimeUnit::UTIMEUNIT_DAY, "getTimeUnitField"));
    delete fourth_tma;
    //
    //================= TimeUnitFormat =================
    //
    TimeUnitFormat* tmf_en = new TimeUnitFormat(Locale("en"), status);
    ASSERT_SUCCESS((status, "Possible Data Error"));
    TimeUnitFormat tmf_fr(Locale("fr"), status);
    ASSERT_SUCCESS((status));

    EXPECT_TRUE(((*tmf_en != tmf_fr), "TimeUnitFormat: en and fr diff"));

    TimeUnitFormat tmf_assign = *tmf_en;
    EXPECT_TRUE(((*tmf_en == tmf_assign), "TimeUnitFormat: orig and assign are equal"));

    TimeUnitFormat tmf_copy(tmf_fr);
    EXPECT_TRUE(((tmf_fr == tmf_copy), "TimeUnitFormat: orig and copy are equal"));

    TimeUnitFormat* tmf_clone = (TimeUnitFormat*)tmf_en->clone();
    EXPECT_TRUE(((*tmf_en == *tmf_clone), "TimeUnitFormat: orig and clone are equal"));
    delete tmf_clone;

    tmf_en->setLocale(Locale("fr"), status);
    ASSERT_SUCCESS((status));

    NumberFormat* numberFmt = NumberFormat::createInstance(
                                 Locale("fr"), status);
    ASSERT_SUCCESS((status));
    tmf_en->setNumberFormat(*numberFmt, status);
    ASSERT_SUCCESS((status));
    EXPECT_TRUE(((*tmf_en == tmf_fr), "TimeUnitFormat: setLocale"));

    delete tmf_en;

    TimeUnitFormat* en_long = new TimeUnitFormat(Locale("en"), UTMUTFMT_FULL_STYLE, status);
    ASSERT_SUCCESS((status));
    delete en_long;

    TimeUnitFormat* en_short = new TimeUnitFormat(Locale("en"), UTMUTFMT_ABBREVIATED_STYLE, status);
    ASSERT_SUCCESS((status));
    delete en_short;

    TimeUnitFormat* format = new TimeUnitFormat(status);
    format->setLocale(Locale("zh"), status);
    format->setNumberFormat(*numberFmt, status);
    ASSERT_SUCCESS((status));
    delete numberFmt;
    delete format;
}

/* @bug 7902
 * Tests for Greek Language.
 * This tests that requests for short unit names correctly fall back 
 * to long unit names for a locale where the locale data does not 
 * provide short unit names. As of CLDR 1.9, Greek is one such language.
 */
void TimeUnitTest::testGreekWithFallback() {
    UErrorCode status = U_ZERO_ERROR;

    const char* locales[] = {"el-GR", "el"};
    TimeUnit::UTimeUnitFields tunits[] = {TimeUnit::UTIMEUNIT_SECOND, TimeUnit::UTIMEUNIT_MINUTE, TimeUnit::UTIMEUNIT_HOUR, TimeUnit::UTIMEUNIT_DAY, TimeUnit::UTIMEUNIT_MONTH, TimeUnit::UTIMEUNIT_YEAR};
    UTimeUnitFormatStyle styles[] = {UTMUTFMT_FULL_STYLE, UTMUTFMT_ABBREVIATED_STYLE};
    const int numbers[] = {1, 7};

    const UChar oneSecond[] = {0x0031, 0x0020, 0x03b4, 0x03b5, 0x03c5, 0x03c4, 0x03b5, 0x03c1, 0x03cc, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03bf, 0};
    const UChar oneMinute[] = {0x0031, 0x0020, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03cc, 0};
    const UChar oneHour[] = {0x0031, 0x0020, 0x03ce, 0x03c1, 0x03b1, 0};
    const UChar oneDay[] = {0x0031, 0x0020, 0x03b7, 0x03bc, 0x03ad, 0x03c1, 0x03b1, 0};
    const UChar oneMonth[] = {0x0031, 0x0020, 0x03bc, 0x03ae, 0x03bd, 0x03b1, 0x03c2, 0};
    const UChar oneYear[] = {0x0031, 0x0020, 0x03ad, 0x03c4, 0x03bf, 0x03c2, 0};
    const UChar sevenSeconds[] = {0x0037, 0x0020, 0x03b4, 0x03b5, 0x03c5, 0x03c4, 0x03b5, 0x03c1, 0x03cc, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03b1, 0};
    const UChar sevenMinutes[] = {0x0037, 0x0020, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03ac, 0};
    const UChar sevenHours[] = {0x0037, 0x0020, 0x03ce, 0x03c1, 0x03b5, 0x03c2, 0};
    const UChar sevenDays[] = {0x0037, 0x0020, 0x03b7, 0x03bc, 0x03ad, 0x03c1, 0x03b5, 0x03c2, 0};
    const UChar sevenMonths[] = {0x0037, 0x0020, 0x03bc, 0x03ae, 0x03bd, 0x03b5, 0x3c2, 0};
    const UChar sevenYears[] = {0x0037, 0x0020, 0x03ad, 0x03c4, 0x03b7, 0};

    const UnicodeString oneSecondStr(oneSecond);
    const UnicodeString oneMinuteStr(oneMinute);
    const UnicodeString oneHourStr(oneHour);
    const UnicodeString oneDayStr(oneDay);
    const UnicodeString oneMonthStr(oneMonth);
    const UnicodeString oneYearStr(oneYear);
    const UnicodeString sevenSecondsStr(sevenSeconds);
    const UnicodeString sevenMinutesStr(sevenMinutes);
    const UnicodeString sevenHoursStr(sevenHours);
    const UnicodeString sevenDaysStr(sevenDays);
    const UnicodeString sevenMonthsStr(sevenMonths);
    const UnicodeString sevenYearsStr(sevenYears);

    const UnicodeString expected[] = {oneSecondStr, oneMinuteStr, oneHourStr, oneDayStr, oneMonthStr, oneYearStr,
                              oneSecondStr, oneMinuteStr, oneHourStr, oneDayStr, oneMonthStr, oneYearStr,
                              sevenSecondsStr, sevenMinutesStr, sevenHoursStr, sevenDaysStr, sevenMonthsStr, sevenYearsStr,
                              sevenSecondsStr, sevenMinutesStr, sevenHoursStr, sevenDaysStr, sevenMonthsStr, sevenYearsStr,
                              oneSecondStr, oneMinuteStr, oneHourStr, oneDayStr, oneMonthStr, oneYearStr,
                              oneSecondStr, oneMinuteStr, oneHourStr, oneDayStr, oneMonthStr, oneYearStr,
                              sevenSecondsStr, sevenMinutesStr, sevenHoursStr, sevenDaysStr, sevenMonthsStr, sevenYearsStr,
                              sevenSecondsStr, sevenMinutesStr, sevenHoursStr, sevenDaysStr, sevenMonthsStr, sevenYearsStr};

    int counter = 0;
    for ( unsigned int locIndex = 0;
        locIndex < sizeof(locales)/sizeof(locales[0]);
        ++locIndex ) {

        Locale l = Locale::createFromName(locales[locIndex]);

        for ( unsigned int numberIndex = 0;
            numberIndex < sizeof(numbers)/sizeof(int);
            ++numberIndex ) {

            for ( unsigned int styleIndex = 0;
                styleIndex < sizeof(styles)/sizeof(styles[0]);
                ++styleIndex ) {

                for ( unsigned int unitIndex = 0;
                    unitIndex < sizeof(tunits)/sizeof(tunits[0]);
                    ++unitIndex ) {

                    TimeUnitAmount *tamt = new TimeUnitAmount(numbers[numberIndex], tunits[unitIndex], status);
                    if (U_FAILURE(status)) {
                        dataerrln("generating TimeUnitAmount Object failed.");
#ifdef TUFMTTS_DEBUG
                        std::cout << "Failed to get TimeUnitAmount for " << tunits[unitIndex] << "\n";
#endif
                        return;
                    }

                    TimeUnitFormat *tfmt = new TimeUnitFormat(l, styles[styleIndex], status);
                    if (U_FAILURE(status)) {
                        dataerrln("generating TimeUnitAmount Object failed.");
#ifdef TUFMTTS_DEBUG
                       std::cout <<  "Failed to get TimeUnitFormat for " << locales[locIndex] << "\n";
#endif
                       return;
                    }

                    Formattable fmt;
                    UnicodeString str;

                    fmt.adoptObject(tamt);
                    str = ((Format *)tfmt)->format(fmt, str, status);
                    if (!EXPECT_SUCCESS((status, "formatting relative time failed"))) {
                        delete tfmt;
#ifdef TUFMTTS_DEBUG
                        std::cout <<  "Failed to format" << "\n";
#endif
                        return;
                    }

#ifdef TUFMTTS_DEBUG
                    char tmp[128];    //output
                    char tmp1[128];    //expected
                    int len = 0;
                    u_strToUTF8(tmp, 128, &len, str.getTerminatedBuffer(), str.length(), &status);
                    u_strToUTF8(tmp1, 128, &len, expected[counter].unescape().getTerminatedBuffer(), expected[counter].unescape().length(), &status);
                    std::cout <<  "Formatted string : " << tmp << " expected : " << tmp1 << "\n";
#endif
                    if (!EXPECT_EQUALS((expected[counter], str, 
                                       "formatted time string is not expected, locale: %s  style: %d  units: %d",
                                       locales[locIndex], (int)styles[styleIndex], (int)tunits[unitIndex]))) {
                        delete tfmt;
                        str.remove();
                        return;
                    }
                    delete tfmt;
                    str.remove();
                    ++counter;
                }
            }
        }
    }
}

// Test bug9042
void TimeUnitTest::testGreekWithSanitization() {
    
    UErrorCode status = U_ZERO_ERROR;
    Locale elLoc("el");
    NumberFormat* numberFmt = NumberFormat::createInstance(Locale("el"), status);
    ASSERT_SUCCESS((status));
    numberFmt->setMaximumFractionDigits(1);

    TimeUnitFormat* timeUnitFormat = new TimeUnitFormat(elLoc, status);
    ASSERT_SUCCESS((status));

    timeUnitFormat->setNumberFormat(*numberFmt, status);

    delete numberFmt;
    delete timeUnitFormat;
}


#endif
