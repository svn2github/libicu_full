
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2000, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
 
#ifndef __TimeZoneTest__
#define __TimeZoneTest__
 
#include "unicode/utypes.h"
#include "caltztst.h"
class SimpleTimeZone;

/** 
 * Various tests for TimeZone
 **/
class TimeZoneTest: public CalendarTimeZoneTest {
    // IntlTest override
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par );
public: // package
    static const int32_t millisPerHour;
 
public:
    /**
     * Test the offset of the PRT timezone.
     */
    virtual void TestPRTOffset(void);
    /**
     * Regress a specific bug with a sequence of API calls.
     */
    virtual void TestVariousAPI518(void);
    /**
     * Test the call which retrieves the available IDs.
     */
    virtual void TestGetAvailableIDs913(void);

    /**
     * Generic API testing for API coverage.
     */
    virtual void TestGenericAPI(void);
    /**
     * Test the setStartRule/setEndRule API calls.
     */
    virtual void TestRuleAPI(void);

    /**
     * subtest used by TestRuleAPI
     **/
    void testUsingBinarySearch(SimpleTimeZone* tz, UDate min, UDate max, UDate expectedBoundary);


    /**
     *  Test short zone IDs for compliance
     */ 
    virtual void TestShortZoneIDs(void);


    /**
     *  Test parsing custom zones
     */ 
    virtual void TestCustomParse(void);
    
    /**
     *  Test new getDisplayName() API
     */ 
    virtual void TestDisplayName(void);

    void TestDSTSavings(void);
    void TestAlternateRules(void);


    static const UDate INTERVAL;

private:
    // internal functions
    static UnicodeString& formatMinutes(int32_t min, UnicodeString& rv);
};
 
#endif // __TimeZoneTest__
