/*
*******************************************************************************
* Copyright (C) 2006-, 
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File DTFMTPTN.CPP
*
* Modification History:
*
*   Date        Name        Description
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "cpputils.h"
#include "ucln_in.h"
#include "mutex.h"
#include "cmemory.h"
#include "cstring.h"
#include "locbased.h"
#include "gregoimp.h"
#include "hash.h"
#include "uresimp.h" 
#include "unicode/datefmt.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/msgfmt.h"
#include "unicode/smpdtfmt.h"
#include "unicode/translit.h"
#include "unicode/udat.h"
#include "unicode/ures.h"
#include "unicode/rep.h"

// TODO remove comment
//#if defined( U_DEBUG_CALSVC ) || defined (U_DEBUG_CAL)
#include <stdio.h>
//#endif

U_NAMESPACE_BEGIN

// *****************************************************************************
// class DateTimePatternGenerator
// *****************************************************************************
// TODO : move to header file as a privae data
static const UChar Canonical_Items[] = {
    // GyQMwWedDFHmsSv
    CAP_G, LOW_Y, CAP_Q, CAP_M, LOW_W, CAP_W, LOW_E, LOW_D, CAP_D, CAP_F,
    CAP_H, LOW_M, LOW_S, CAP_S, LOW_V, 0
};

static const dtTypeElem dtTypes[] = {
    // patternChar, field, type, minLen, weight
    {CAP_G, DT_ERA, DT_SHORT, 1, 3,},
    {CAP_G, DT_ERA, DT_LONG, 4},
    {LOW_Y, DT_YEAR, DT_NUMERIC, 1, 20},
    {CAP_Y, DT_YEAR, DT_NUMERIC + DT_DELTA, 1, 20},
    {LOW_U, DT_YEAR, DT_NUMERIC + 2*DT_DELTA, 1, 20},
    {CAP_Q, DT_QUARTER, DT_NUMERIC, 1, 2},
    {CAP_Q, DT_QUARTER, DT_SHORT, 3},
    {CAP_Q, DT_QUARTER, DT_LONG, 4},
    {CAP_M, DT_MONTH, DT_NUMERIC, 1, 2},
    {CAP_M, DT_MONTH, DT_SHORT, 3},
    {CAP_M, DT_MONTH, DT_LONG, 4},
    {CAP_M, DT_MONTH, DT_NARROW, 5},
    {CAP_L, DT_MONTH, DT_NUMERIC + DT_DELTA, 1, 2},
    {CAP_L, DT_MONTH, DT_SHORT - DT_DELTA, 3},
    {CAP_L, DT_MONTH, DT_LONG - DT_DELTA, 4},
    {CAP_L, DT_MONTH, DT_NARROW - DT_DELTA, 5},
    {LOW_W, DT_WEEK_OF_YEAR, DT_NUMERIC, 1, 2},
    {CAP_W, DT_WEEK_OF_MONTH, DT_NUMERIC + DT_DELTA, 1},
    {LOW_E, DT_WEEKDAY, DT_NUMERIC + DT_DELTA, 1, 2},
    {LOW_E, DT_WEEKDAY, DT_SHORT - DT_DELTA, 3},
    {LOW_E, DT_WEEKDAY, DT_LONG - DT_DELTA, 4},
    {LOW_E, DT_WEEKDAY, DT_NARROW - DT_DELTA, 5},
    {CAP_E, DT_WEEKDAY, DT_SHORT, 1, 3},
    {CAP_E, DT_WEEKDAY, DT_LONG, 4},
    {CAP_E, DT_WEEKDAY, DT_NARROW, 5},
    {LOW_C, DT_WEEKDAY, DT_NUMERIC + 2*DT_DELTA, 1, 2},
    {LOW_C, DT_WEEKDAY, DT_SHORT - 2*DT_DELTA, 3},
    {LOW_C, DT_WEEKDAY, DT_LONG - 2*DT_DELTA, 4},
    {LOW_C, DT_WEEKDAY, DT_NARROW - 2*DT_DELTA, 5},
    {LOW_D, DT_DAY, DT_NUMERIC, 1, 2},
    {CAP_D, DT_DAY_OF_YEAR, DT_NUMERIC + DT_DELTA, 1, 3},
    {CAP_F, DT_DAY_OF_WEEK_IN_MONTH, DT_NUMERIC + 2*DT_DELTA, 1},
    {LOW_G, DT_DAY, DT_NUMERIC + 3*DT_DELTA, 1, 20}, // really internal use, so we d'ont care
    {LOW_A, DT_DAYPERIOD, DT_SHORT, 1},
    {CAP_H, DT_HOUR, DT_NUMERIC + 10*DT_DELTA, 1, 2}, // 24 hour
    {LOW_K, DT_HOUR, DT_NUMERIC + 11*DT_DELTA, 1, 2},
    {LOW_H, DT_HOUR, DT_NUMERIC, 1, 2}, // 12 hour
    {LOW_K, DT_HOUR, DT_NUMERIC + DT_DELTA, 1, 2},
    {LOW_M, DT_MINUTE, DT_NUMERIC, 1, 2},
    {LOW_S, DT_SECOND, DT_NUMERIC, 1, 2},
    {CAP_S, DT_FRACTIONAL_SECOND, DT_NUMERIC + DT_DELTA, 1, 1000},
    {CAP_A, DT_SECOND, DT_NUMERIC + 2*DT_DELTA, 1, 1000},
    {LOW_V, DT_ZONE, DT_SHORT - 2*DT_DELTA, 1},
    {LOW_V, DT_ZONE, DT_LONG - 2*DT_DELTA, 4},
    {LOW_Z, DT_ZONE, DT_SHORT, 1, 3},
    {LOW_Z, DT_ZONE, DT_LONG, 4},
    {CAP_Z, DT_ZONE, DT_SHORT - DT_DELTA, 1, 3},
    {CAP_Z, DT_ZONE, DT_LONG - DT_DELTA, 4},
    {'\0'} , // last row of dtTypes[]
 };    

static const char* CLDR_FIELD_APPEND[] = {
    "Era", "Year", "Quarter", "Month", "Week", "*", "Day-Of-Week", "Day", "*", "*", "*", 
    "Hour", "Minute", "Second", "*", "Timezone"
};

static const char* CLDR_FIELD_NAME[] = {
    "era", "year", "quarter", "month", "week", "*", "weekday", "day", "*", "*", "dayperiod", 
    "hour", "minute", "second", "*", "zone"
};

static const char* Resource_Fields[] = {
    "day", "dayperiod", "era", "hour", "minute", "month", "second", "week", 
    "weekday", "year", "zone" };

static const char* CLDR_AVAILABLE_FORMAT[MAX_AVAILABLE_FORMATS] = {
        "Ed", "EMMMd", "H", "HHmm", "HHmmss", "Md", "MMMMd", "MMyy", "Qyy", 
        "mmss", "yyMMM", "yyyy",
};  // binary ascending order

static Transliterator *fromHex=NULL;

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateTimePatternGenerator)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PtnSkeleton)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PtnElem)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateTimeMatcher)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PatternMap)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PatternMapIterator)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FormatParser)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DistanceInfo)

DateTimePatternGenerator*  U_EXPORT2
DateTimePatternGenerator::createInstance(UErrorCode& status) {
    return createInstance(Locale::getDefault(), status);
}

DateTimePatternGenerator* U_EXPORT2
DateTimePatternGenerator::createInstance(const Locale& locale, UErrorCode& status) {
    return new DateTimePatternGenerator(locale, status);
}

DateTimePatternGenerator::DateTimePatternGenerator(UErrorCode &status) : UObject() {
    initData(Locale::getDefault());
    status = getStatus();
}
DateTimePatternGenerator::DateTimePatternGenerator(const Locale& locale, UErrorCode &status) : UObject() {
    initData(locale);
    status = getStatus();
}

DateTimePatternGenerator::DateTimePatternGenerator(const DateTimePatternGenerator& other, UErrorCode &status) 
: UObject() {
    *this=other;
    status = getStatus();
}

DateTimePatternGenerator::DateTimePatternGenerator(const DateTimePatternGenerator& other) 
: UObject() {
    *this=other;
}

DateTimePatternGenerator&
DateTimePatternGenerator::operator=(const DateTimePatternGenerator& other) {
    pLocale = other.pLocale;
    fp = other.fp;
    dtMatcher = other.dtMatcher;
    distanceInfo = other.distanceInfo;
    dateTimeFormat = other.dateTimeFormat;
    decimal = other.decimal;
    skipMatcher = other.skipMatcher;
    for (int32_t i=0; i< TYPE_LIMIT; ++i ) {
        appendItemFormats[i] = other.appendItemFormats[i];
        appendItemNames[i] = other.appendItemNames[i];
    }
    
    patternMap.copyFrom(other.patternMap, status);
    fAvailableFormatKeyHash=NULL;
    copyHashtable(other.fAvailableFormatKeyHash);
    return *this;
}


UBool 
DateTimePatternGenerator::operator==(const DateTimePatternGenerator& other) {
    if (this == &other) {
        return TRUE;
    }
    if ((pLocale==other.pLocale) && (patternMap.equals(other.patternMap))) {
        return TRUE;
    }
    else {
        return FALSE;
    }
}

DateTimePatternGenerator::~DateTimePatternGenerator() {
    if (fAvailableFormatKeyHash!=NULL) {
        delete fAvailableFormatKeyHash;
        fAvailableFormatKeyHash=NULL;
    }
}

void
DateTimePatternGenerator::initData(const Locale& locale) {
    status = U_ZERO_ERROR;
    skipMatcher = NULL;
    fAvailableFormatKeyHash=NULL;
    
    fromHex=Transliterator::createInstance(UNICODE_STRING_SIMPLE("Hex-Any"), UTRANS_FORWARD, status); 
    // DateTimePatternGenerator* result = new DateTimePatternGenerator();
    addCanonicalItems();
    addICUPatterns(locale);
    addCLDRData(locale);
    setDateTimeFromCalendar(locale);
    setDecimalSymbols(locale);
} // DateTimePatternGenerator::initData

void 
DateTimePatternGenerator::getSkeleton(UnicodeString pattern, UnicodeString& result) {
    PtnSkeleton *ptrSkeleton;
    dtMatcher.set(pattern, fp);
    ptrSkeleton=dtMatcher.getSkeleton();
    result="";
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        if (ptrSkeleton->original[i].length()!=0) {
            result += ptrSkeleton->original[i];
        }
    }
}

void 
DateTimePatternGenerator::getBaseSkeleton(UnicodeString pattern, UnicodeString& result) {
    PtnSkeleton *ptrSkeleton;
    dtMatcher.set(pattern, fp);
    ptrSkeleton=dtMatcher.getSkeleton();
    result="";
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        if (ptrSkeleton->baseOriginal[i].length()!=0) {
            result += ptrSkeleton->baseOriginal[i];
        }
    }
}

void
DateTimePatternGenerator::addICUPatterns(const Locale& locale) {
    PatternInfo returnInfo;
    UnicodeString dfPattern;
    
    // Load with ICU patterns
    for (DateFormat::EStyle i=DateFormat::kFull; i<=DateFormat::kShort; i=static_cast<DateFormat::EStyle>(i+1)) {
        SimpleDateFormat* df = (SimpleDateFormat*)DateFormat::createDateInstance(i, locale);
        UnicodeString newPattern=df->toPattern(dfPattern);
    printf("\n ICU Date format:");
    for (int32_t j=0; j < newPattern.length(); ++j) {
        printf("%c", newPattern.charAt(j));
    }
        add(df->toPattern(dfPattern), FALSE, returnInfo);
        df = (SimpleDateFormat*)DateFormat::createTimeInstance(i, locale);
        add(df->toPattern(dfPattern), FALSE, returnInfo);
        newPattern=df->toPattern(dfPattern);
    printf("\n ICU Time format:");
    for (int32_t j=0; j < newPattern.length(); ++j) {
        printf("%c", newPattern.charAt(j));
    }
        // HACK for hh:ss 
        if ( i==DateFormat::kMedium ) {
            hackPattern = df->toPattern(hackPattern);
        }
    }
    
}

void
DateTimePatternGenerator::hackTimes(PatternInfo &returnInfo, UnicodeString& hackPattern) {
    fp.set(hackPattern);
    UnicodeString mmss;
    UBool gotMm=FALSE;
    for (int32_t i=0; i<fp.itemNumber; ++i) {
        UnicodeString field = fp.items[i];
        if ( fp.isQuoteLiteral(field) ) {
            if ( gotMm ) {
               UnicodeString quoteLiteral;
               fp.getQuoteLiteral(quoteLiteral, &i);
               mmss += quoteLiteral;
            }
        }
        else {
            if (fp.isPatternSeparator(field) && gotMm) {
                mmss+=field;
            }
            else {
                UChar ch=field.charAt(0);
                if (ch==LOW_M) {
                    gotMm=TRUE;
                    mmss+=field;
                }
                else {
                    if (ch==LOW_S) {
                        if (!gotMm) {
                            break;
                        }
                        mmss+= field;
                        add(mmss, FALSE, returnInfo);
                        break;
                    }
                    else {
                        if (gotMm || ch==LOW_Z || ch==CAP_Z || ch==LOW_V || ch==CAP_V) {
                            break;
                        }
                    }
                }
            }
        }
    }
}

void
DateTimePatternGenerator::addCLDRData(const Locale& locale) {
    // Load with Resource Bundle
    //UResourceBundle *rb = ures_open(DateTimePatternsTag, locale.getName(), &err);
    //TODO make sure the package is NULL
    UErrorCode err = U_ZERO_ERROR;
    UResourceBundle *rb = ures_open(NULL, locale.getName(), &err);
    UResourceBundle *gregorianBundle=NULL;
    UResourceBundle *patBundle=NULL;
    int32_t len=0;
    UnicodeString rbPattern, value, field;
    PatternInfo returnInfo;
    const char *key=NULL;
    
    // Initialize appendItems
    UChar itemFormat[]= {0x7B, 0x30, 0x7D, 0x20, 0x251C, 0x7B, 0x32, 0x7D, 0x3A,
        0x20, 0x7B, 0x31, 0x7D, 0x2524, 0};  // {0} \u251C{2}: {1}\u2524
    UnicodeString defaultItemFormat = UnicodeString(itemFormat);
    
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        appendItemFormats[i] = defaultItemFormat;
        appendItemNames[i]=CAP_F;
        appendItemNames[i]+=(i+0x30);
        appendItemNames[i]+="";
    }
    
    
    err = U_ZERO_ERROR;
    rb = ures_open(NULL, locale.getName(), &err);
    rb = ures_getByKey(rb, "calendar", rb, &err);
    rb = ures_getByKey(rb, "gregorian", rb, &err);
    patBundle = ures_getByKeyWithFallback(rb, "appendItems", rb, &err);
    key=NULL;
    UnicodeString itemKey;
    while (U_SUCCESS(err)) {
        rbPattern = ures_getNextUnicodeString(patBundle, &key, &err);
        if (rbPattern.length()==0 ) {
            break;  // no more pattern
        }
        else {
            //printf("\n ResourceBundle - AppendItems:  key:%s", key);
            printf("\n ResourceBundle - AppendItems:  key:%s", key);
            for (int32_t i=0; i<rbPattern.length(); ++i)
               printf("%c", rbPattern.charAt(i));
            setAppendItemFormats(getAppendFormatNumber(key), rbPattern);
        }
    }
    
    err = U_ZERO_ERROR; 
    rb = ures_open(NULL, locale.getName(), &err);
    rb = ures_getByKey(rb, "calendar", rb, &err);
    rb = ures_getByKey(rb, "gregorian", rb, &err);
    gregorianBundle = ures_getByKeyWithFallback(rb, "fields",rb, &err);
    UResourceBundle *fieldBundle=NULL;
    
    key=NULL;
    err = U_ZERO_ERROR;
    patBundle = ures_getByKeyWithFallback(gregorianBundle, "day", gregorianBundle, &err);
    fieldBundle = ures_getByKeyWithFallback(patBundle, "dn", patBundle, &err);
    rbPattern = ures_getNextUnicodeString(fieldBundle, &key, &err);
    
    key=NULL;
    err = U_ZERO_ERROR;
    rb = ures_open(NULL, locale.getName(), &err);
    rb = ures_getByKey(rb, "calendar", rb, &err);
    rb = ures_getByKey(rb, "gregorian", rb, &err);
    rb = ures_getByKeyWithFallback(rb, "fields", rb, &err);
    for (int32_t i=0; i<MAX_RESOURCE_FIELD; ++i) {
        err = U_ZERO_ERROR;
        patBundle = ures_getByKeyWithFallback(rb, Resource_Fields[i], patBundle, &err);
        fieldBundle = ures_getByKeyWithFallback(patBundle, "dn", fieldBundle, &err);
        rbPattern = ures_getNextUnicodeString(fieldBundle, &key, &err);
        if (rbPattern.length()==0 ) {
            continue;  // no more pattern
        }
        else {
            printf("\n ResourceBundle - Fields:");
            for (int32_t j=0; j<rbPattern.length(); ++j)
                printf("%c", rbPattern.charAt(j));
            setAppendItemNames(getAppendNameNumber(Resource_Fields[i]), rbPattern);
        }
    }
    ures_close(rb);
    
    // add available formats
    err = U_ZERO_ERROR;
    initHashtable(err);
    rb = ures_open(NULL, locale.getName(), &err);
    rb = ures_getByKey(rb, "calendar", rb, &err);
    rb = ures_getByKey(rb, "gregorian", rb, &err);
    patBundle = ures_getByKeyWithFallback(rb, "availableFormats", patBundle, &err);
    if (U_SUCCESS(err)) {
        int32_t numberKeys = ures_getSize(patBundle);  
        printf ("\n available formats from current locale:%s", locale.getName());
        int32_t len;
        const UChar *retPattern;
        key=NULL;
        for(int32_t i=0; i<numberKeys; ++i) {
            retPattern=ures_getNextString(patBundle, &len, &key, &err);
            UnicodeString format=UnicodeString(retPattern);
            UnicodeString retKey=UnicodeString(key);
            setAvailableFormat(key, err);
            add(format, FALSE, returnInfo);
            printf("\n Available Format: Key=>");
            for (int32_t j=0; j<retKey.length(); ++j)
                printf("%c", retKey.charAt(j));
            printf("   format=>");
            for (int32_t j=0; j<format.length(); ++j)
                printf("%c", format.charAt(j));
        }
    }
    ures_close(rb); 
    err = U_ZERO_ERROR;
    char parentLocale[50];
    const char *curLocaleName=locale.getName();
    int32_t localeNameLen=0;
    uprv_strcpy(parentLocale, curLocaleName);
    while((localeNameLen=uloc_getParent(parentLocale, parentLocale, 50, &err))>=0 ) {
        rb = ures_open(NULL, parentLocale, &err);
        rb = ures_getByKey(rb, "calendar", rb, &err);
        rb = ures_getByKey(rb, "gregorian", rb, &err);
        patBundle = ures_getByKeyWithFallback(rb, "availableFormats", patBundle, &err);
        if (U_SUCCESS(err)) {
            int32_t numberKeys = ures_getSize(patBundle);  
            int32_t len;
            const UChar *retPattern;
            key=NULL;
            
            printf ("\n available formats from parent locale:%s", parentLocale);
            for(int32_t i=0; i<numberKeys; ++i) {
                retPattern=ures_getNextString(patBundle, &len, &key, &err);
                UnicodeString format=UnicodeString(retPattern);
                UnicodeString retKey=UnicodeString(key);
                if ( !isAvailableFormatSet(key) ) {
                    setAvailableFormat(key, err);
                    add(format, FALSE, returnInfo);
                    printf("\n Available Format: Key=>");
                    for (int32_t j=0; j<retKey.length(); ++j)
                        printf("%c", retKey.charAt(j));
                    printf("   format=>");
                    for (int32_t j=0; j<format.length(); ++j)
                        printf("%c", format.charAt(j));
                }
            }
        }
        if (localeNameLen==0) {
            break;
        }
    }
    
    
    ures_close(rb);
    //ures_close(gregorianBundle);
    
    
    if (hackPattern.length()>0) {
        hackTimes(returnInfo, hackPattern);
    }
}

void 
DateTimePatternGenerator::initHashtable(UErrorCode& err) {
    if (fAvailableFormatKeyHash!=NULL) {
        return;
    }
    if ((fAvailableFormatKeyHash = new Hashtable(FALSE, err))!=NULL) {
        return;
    }
}


void
DateTimePatternGenerator::setAppendItemFormats(int32_t field, UnicodeString value) {
    Mutex mutex;
    appendItemFormats[field] = value;
}

void
DateTimePatternGenerator::getAppendItemFormats(int32_t field, UnicodeString& appendItemFormat) {
    appendItemFormat = appendItemFormats[field];
}

void
DateTimePatternGenerator::setAppendItemNames(int32_t field, UnicodeString value) {
    Mutex mutex;
    if ((field >=0)&&(field<=TYPE_LIMIT)) {
        appendItemNames[field] = value;
    }
}

void
DateTimePatternGenerator:: getAppendItemNames(int32_t field, UnicodeString& appendItemName) {
    appendItemName = appendItemNames[field];
}

void
DateTimePatternGenerator::getAppendName(int32_t field, UnicodeString& value) {
    value = SINGLE_QUOTE;
    value += appendItemNames[field];
    value += SINGLE_QUOTE;
}

UnicodeString
DateTimePatternGenerator::getBestPattern(const UnicodeString& patternForm) {
    UnicodeString *bestPattern=NULL;
    UnicodeString resultPattern, dtFormat;
    UErrorCode err = U_ZERO_ERROR;
    
    int32_t dateMask=(1<<DT_DAYPERIOD) - 1;
    int32_t timeMask=(1<<TYPE_LIMIT) - 1 - dateMask;
    
    printf("\n\n TestPattern:");
    for (int32_t i=0; i < patternForm.length(); ++i) {
        printf("%c", patternForm.charAt(i));
    }
    dtMatcher.set(patternForm, fp);
    bestPattern=getBestRaw(dtMatcher, -1, distanceInfo);
    printf("\n 1st BestPattern:");
    for (int32_t i=0; i < bestPattern->length(); ++i) {
        printf("%c", bestPattern->charAt(i));
    }
    if ( distanceInfo.missingFieldMask==0 && distanceInfo.extraFieldMask==0 ) {
        resultPattern = adjustFieldTypes(*bestPattern, FALSE);
        
    printf("\n resultPattern:");
    for (int32_t i=0; i < resultPattern.length(); ++i) {
        printf("%c", resultPattern.charAt(i));
    }
        return resultPattern;
    }
    int32_t neededFields = dtMatcher.getFieldMask();
    UnicodeString datePattern=getBestAppending(neededFields & dateMask);
    UnicodeString timePattern=getBestAppending(neededFields & timeMask);
    
    if (datePattern.length()==0) {
        if (timePattern.length()==0) {
            resultPattern="";
            return resultPattern;
        }
        else {
            resultPattern=timePattern;
            return resultPattern;
        }
    }
    if (timePattern.length()==0) {
        resultPattern=datePattern;
        return resultPattern;
    }
    resultPattern="";
    getDateTimeFormat(dtFormat);
    Formattable dateTimeObject[] = { datePattern, timePattern };
    resultPattern = MessageFormat::format(dtFormat, dateTimeObject, 2, resultPattern, err );
    return resultPattern;
}

void 
DateTimePatternGenerator::replaceFieldTypes(UnicodeString pattern, UnicodeString skeleton, UnicodeString& result) {
    
    dtMatcher.set(skeleton, fp);
    result = adjustFieldTypes(pattern, FALSE);
    return;
}

void 
DateTimePatternGenerator::setDecimal(UnicodeString decimal) {
    this->decimal = decimal;
}

void 
DateTimePatternGenerator::getDecimal(UnicodeString& result) {
    result = decimal;
}

void 
DateTimePatternGenerator::addCanonicalItems() {
    PatternInfo  patternInfo;
   
    for (int32_t i=0; i<TYPE_LIMIT; i++) {
        add(UnicodeString(Canonical_Items[i]), FALSE, patternInfo);
    }
}

void
DateTimePatternGenerator::setDateTimeFormat(UnicodeString& dtFormat) {
    if ( dtFormat.charAt(0) == '{' ) {
        // This code is hard coded because there is no tag for DateTimeFormat
        // under DateTimePatterns section.
        dateTimeFormat = dtFormat;
    }
}


void
DateTimePatternGenerator::getDateTimeFormat(UnicodeString& dtFormat) {
    dtFormat = dateTimeFormat;
}

void 
DateTimePatternGenerator::setDateTimeFromCalendar(const Locale& locale) {
    UErrorCode err=U_ZERO_ERROR;
    UResourceBundle *patBundle;
    UnicodeString rbPattern;
    const char *key=NULL; 
    
     // Set the datetime pattern
    CalendarData calData(locale, NULL, err );

    // load the first data item
    patBundle = calData.getByKey("DateTimePatterns", err);
    UnicodeString dtFormat;
    while (U_SUCCESS(err)) {
        dtFormat = ures_getNextUnicodeString(patBundle, &key, &err);
        if (rbPattern.length()==0 ) {
            break;  // no more pattern
        }
        setDateTimeFormat(dtFormat);
    };
    
}

void 
DateTimePatternGenerator::setDecimalSymbols(const Locale& locale) {
    UErrorCode err;
    
    DecimalFormatSymbols dfs = DecimalFormatSymbols(locale, err);
    decimal = dfs.getSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
    return;
}

void
DateTimePatternGenerator::add(
    UnicodeString pattern, 
    UBool override, 
    PatternInfo& returnInfo) {
 
    const UChar oldFormat[3]={0x5C, LOW_U, 0 }; // "\\u" 
    UErrorCode err;      
    UnicodeString basePattern;
    PtnSkeleton   skeleton;
    
    //TODO: lock the tree
    if (pattern.indexOf(oldFormat, 3, 0) > 0 ) {
        UnicodeString oldPattern(pattern);
    }
    //static Transliterator *fromHex=Transliterator::createInstance(fHex, UTRANS_FORWARD, err); 
    fromHex->transliterate(pattern);
    DateTimeMatcher matcher;
    matcher.set(pattern, fp, skeleton);
    matcher.getBasePattern(basePattern);
    UnicodeString *duplicatePattern = patternMap.getPatternFromSkeleton(skeleton);   
    if (duplicatePattern != NULL ) {
        returnInfo.status = BASE_CONFLICT;
        returnInfo.conflictingPattern = *duplicatePattern;
        if (!override) {
            return;
        }
    }
    patternMap.add(basePattern, skeleton, pattern, err);
    returnInfo.status = NO_CONFLICT;
    returnInfo.conflictingPattern = "\0";
    return;
}


int32_t 
DateTimePatternGenerator::getAppendFormatNumber(const char* field) {
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        if (uprv_strcmp(CLDR_FIELD_APPEND[i], field)==0) {
            return i;
        }
    }
    return -1;
}

int32_t 
DateTimePatternGenerator::getAppendNameNumber(const char* field) {
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        if (uprv_strcmp(CLDR_FIELD_NAME[i],field)==0) {
            return i;
        }
    }
    return -1;
}

UnicodeString*
DateTimePatternGenerator::getBestRaw(DateTimeMatcher source, 
                                     int32_t includeMask, 
                                     DistanceInfo& missingFields) {
    int32_t bestDistance = 0x7fffffff;
    DistanceInfo tempInfo;
    UnicodeString *bestPattern=NULL;
    
    bestPattern = '\0';
    PatternMapIterator it;
    for (it.set(patternMap); it.hasNext(); ) {
        DateTimeMatcher trial = it.next();
        if (trial.equals(skipMatcher)) {
            continue;
        }
        int32_t distance=source.getDistance(trial, includeMask, tempInfo);
        UnicodeString* tempPattern=patternMap.getPatternFromSkeleton(*trial.getSkeleton());
        /*
            printf("\n Distance:%d  tempPattern: ", distance);
            for (int32_t i=0; i<tempPattern->length(); ++i) {
                printf("%c", tempPattern->charAt(i));
            }
         */
        
        if (distance<bestDistance) {
            bestDistance=distance;
            bestPattern=patternMap.getPatternFromSkeleton(*trial.getSkeleton());
            missingFields.setTo(tempInfo);
            /*
            printf("\n Distance:%d  bestPattern: ", distance);
            for (int32_t i=0; i<bestPattern->length(); ++i) {
                printf("%c", bestPattern->charAt(i));
            }
            */
            if (distance==0) {
                break;
            }
        }
    }
    
    return bestPattern;
}

UnicodeString
DateTimePatternGenerator::adjustFieldTypes(UnicodeString& pattern, 
                                           //DateTimeMatcher& inputRequest, 
                                           UBool fixFractionalSeconds) {
    UnicodeString newPattern="";
    fp.set(pattern);    
    for (int32_t i=0; i < fp.itemNumber; i++) {
        UnicodeString field = fp.items[i];
        if ( fp.isQuoteLiteral(field) ) {
            
            UnicodeString quoteLiteral="";
            fp.getQuoteLiteral(quoteLiteral, &i);
            newPattern += quoteLiteral;
        }
        else {
    
            int32_t canonicalIndex = fp.getCanonicalIndex(field);
            if (canonicalIndex < 0) {
                if (fp.isPatternSeparator(field)) {
                    newPattern+=field;
                }
                continue; //don't adjust
            }
            const dtTypeElem *row = &dtTypes[canonicalIndex];
            int32_t typeValue = row->field;
            if (fixFractionalSeconds && typeValue == DT_SECOND) {
                UnicodeString newField=dtMatcher.skeleton.original[DT_FRACTIONAL_SECOND];
                field = field + decimal + newField;
            }
            else {
                if (dtMatcher.skeleton.type[typeValue]!=0) {
                    UnicodeString newField=dtMatcher.skeleton.original[typeValue];
                    if (typeValue!= DT_HOUR) {
                        field=newField;
                    }
                    else {
                        if (field.length()!=newField.length()) {
                            UChar c=field.charAt(0);
                            field="";
                            for (int32_t i=newField.length(); i>0; --i) {
                                field+=c;
                            }
                        }
                    }
                }
                newPattern+=field;
            }
        }
    }
    printf("\n test return:");
    for (int32_t i=0; i<newPattern.length(); ++i) {
        printf("%c", newPattern.charAt(i));
    }
    return newPattern;
}

UnicodeString
DateTimePatternGenerator::getBestAppending(int32_t missingFields) {
    UnicodeString  resultPattern, tempPattern, formattedPattern;
    UErrorCode err=U_ZERO_ERROR;
    int32_t lastMissingFieldMask=0;
    if (missingFields!=0) {
        resultPattern=UnicodeString("");
        tempPattern = *getBestRaw(dtMatcher, missingFields, distanceInfo);
        resultPattern = adjustFieldTypes(tempPattern, FALSE);
        printf("\n Adjust Field Types length:%d  field:", resultPattern.length());
        for (int32_t k=0; k<resultPattern.length(); ++k ) {
            printf("%c", resultPattern.charAt(k));
        }
        
        while (distanceInfo.missingFieldMask!=0) { // precondition: EVERY single field must work!
            if ( lastMissingFieldMask == distanceInfo.missingFieldMask ) {
                break;  // cannot find the proper missing field
            } 
            if (((distanceInfo.missingFieldMask & SECOND_AND_FRACTIONAL_MASK)==FRACTIONAL_MASK) &&
                ((missingFields & SECOND_AND_FRACTIONAL_MASK) == SECOND_AND_FRACTIONAL_MASK)) {
                resultPattern = adjustFieldTypes(resultPattern, FALSE);
                resultPattern = tempPattern;
                distanceInfo.missingFieldMask &= ~FRACTIONAL_MASK;
                continue;
            }
            int32_t startingMask = distanceInfo.missingFieldMask;
            tempPattern = *getBestRaw(dtMatcher, distanceInfo.missingFieldMask, distanceInfo);
            tempPattern = adjustFieldTypes(tempPattern, FALSE);
            int32_t foundMask=startingMask& ~distanceInfo.missingFieldMask;
            int32_t topField=getTopBitNumber(foundMask);
            UnicodeString appendName;
            getAppendName(topField, appendName);
        printf("\n after getAppendName: topField:%d len:%d name:", topField, appendName.length());
        for (int32_t k=0; k<appendName.length(); ++k ) {
            printf("%c", appendName.charAt(k));
        }
            const Formattable formatPattern[] = {
                resultPattern,
                tempPattern,
                appendName
            };
        printf("\n Item Format:");
        for (int32_t k=0; k<appendItemFormats[topField].length(); ++k ) {
            printf("%c", appendItemFormats[topField].charAt(k));
        }
        printf("\n Adjust field:");
        for (int32_t k=0; k<resultPattern.length(); ++k ) {
            printf("%c", resultPattern.charAt(k));
        }
        printf("\n Best raw :");
        for (int32_t k=0; k<tempPattern.length(); ++k ) {
            printf("%c", tempPattern.charAt(k));
        }
        printf("\n Append  name: len:%d  name:", appendName.length());
        for (int32_t k=0; k<appendName.length(); ++k ) {
            printf("%c", appendName.charAt(k));
        }
            
           
            formattedPattern = MessageFormat::format(appendItemFormats[topField], formatPattern, 3, resultPattern, err);
            printf("\n Message format result: len:%d  newPattern:", resultPattern.length());
        for (int32_t k=0; k<resultPattern.length(); ++k ) {
            printf("%c", resultPattern.charAt(k));
        }
            lastMissingFieldMask = distanceInfo.missingFieldMask;
        }
    } 
    return formattedPattern;
}

int32_t
DateTimePatternGenerator::getTopBitNumber(int32_t foundMask) {
    int32_t i=0;
    while (foundMask!=0) {
        foundMask >>=1;  
        ++i;
    }
    return i-1;
}

void
DateTimePatternGenerator::setAvailableFormat(const char* key, UErrorCode& err) {
    
    fAvailableFormatKeyHash->puti(key, 1, err);
}

UBool
DateTimePatternGenerator::isAvailableFormatSet(const char* key) {
    int32_t i=0;
    
    i=fAvailableFormatKeyHash->geti(key);
    if ( i==1 ) {
        return TRUE;
    }
    else{
        return FALSE;
    }
}

void
DateTimePatternGenerator::copyHashtable(Hashtable *other) {
    
    if (fAvailableFormatKeyHash !=NULL) {
        delete fAvailableFormatKeyHash;
    }
    initHashtable(status);
    if(U_FAILURE(status)){
        return;
    }
    int32_t pos = -1;
    const UHashElement* elem = NULL;
    // walk through the hash table and create a deep clone 
    while((elem = other->nextElement(pos))!= NULL){
        const UHashTok otherKeyTok = elem->key;
        UnicodeString* otherKey = (UnicodeString*)otherKeyTok.pointer;
        UnicodeString *key = new UnicodeString(*otherKey);
        fAvailableFormatKeyHash->puti(*key, 1, status);
        if(U_FAILURE(status)){
            return;
        }
    } 
}


DateTimePatternGenerator*
DateTimePatternGenerator::clone(UErrorCode& status) {
    return new DateTimePatternGenerator(*this, status);
}

PatternMap::PatternMap() {
   for (int32_t i=0; i < MAX_PATTERN_ENTRIES; ++i ) {
      boot[i]=NULL;
   }
   isDupAllowed = TRUE;
} 

void
PatternMap::copyFrom(const PatternMap& other, UErrorCode& status) {
    this->isDupAllowed = other.isDupAllowed;
    for (int32_t bootIndex=0; bootIndex<MAX_PATTERN_ENTRIES; ++bootIndex ) {
        PtnElem *curElem, *otherElem, *prevElem=NULL;
        otherElem = other.boot[bootIndex];
        while (otherElem!=NULL) {
            if ((curElem=this->boot[bootIndex]= new PtnElem()) == NULL ) {
                // out of memory
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            curElem->basePattern = new UnicodeString(*(otherElem->basePattern));
            curElem->pattern = new UnicodeString(*(otherElem->pattern));
            
            if ((curElem->skeleton=new PtnSkeleton(*(otherElem->skeleton))) == NULL ) {
                // out of memory
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            
            if (prevElem!=NULL) {
                prevElem->next=curElem;
            }
            curElem->next=NULL;
            prevElem = curElem;
            otherElem = otherElem->next;
        }
        
    }
}  


PatternMap::~PatternMap() {
   printf("\n Begining ....~PatternMap(): %p", this);
   for (int32_t i=0; i < MAX_PATTERN_ENTRIES; ++i ) {
       if (boot[i]!=NULL ) {
           delete boot[i];
           boot[i]=NULL;
       }
   }
   printf("\nEnd ... ~PatternMap(): %p", this);
}  // PatternMap destructor 

void
PatternMap::add( UnicodeString& basePattern,
                 PtnSkeleton& skeleton,  
                 UnicodeString& value,// mapped pattern value 
                 UErrorCode &status) { 
    UChar baseChar = basePattern.charAt(0);
    PtnElem *curElem, *nextElem, *baseElem;

    // the baseChar must be A-Z or a-z 
    if ((baseChar >= CAP_A) && (baseChar <= CAP_Z)) {
        baseElem = boot[baseChar-CAP_A];
    }
    else {
        if ((baseChar >=LOW_A) && (baseChar <= LOW_Z)) {
            baseElem = boot[26+baseChar-LOW_A];
         }
         else {
             // TODO return syntax error
             return;
         }
    }

    if (baseElem == NULL) {
        if ((curElem = new PtnElem()) == NULL ) {
            // out of memory
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        } 
        if (baseChar >= LOW_A) {
            Mutex mutex;
            if ( boot[26 + (baseChar-LOW_A)]==NULL ) {
                boot[26 + (baseChar-LOW_A)] = curElem;
            }
            else {
                uprv_free(curElem);
                curElem=NULL;
                baseElem = boot[26 + (baseChar-LOW_A)];
            }
        }
        else {
            Mutex mutex;
            if (boot[baseChar-CAP_A] == NULL ) {
                boot[baseChar-CAP_A] = curElem;
            }
            else {
                uprv_free(curElem);
                curElem=NULL;
                baseElem = boot[baseChar-CAP_A];
            }
        }
        if ( curElem != NULL ) {
            curElem->basePattern = new UnicodeString(basePattern);
            curElem->pattern = new UnicodeString(value);
            PtnSkeleton* pNewSkeleton = curElem->skeleton = new PtnSkeleton;

            for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
                pNewSkeleton->type[i] = skeleton.type[i];
                pNewSkeleton->original[i] = skeleton.original[i];
                pNewSkeleton->baseOriginal[i] = skeleton.baseOriginal[i];
            }
            curElem->next = NULL;
        }
    }
    if ( baseElem != NULL ) { 
        curElem = getDuplicateElem(basePattern, skeleton, baseElem);

        if (curElem == NULL) {
            // add new element to the list.
            curElem = baseElem;
            {
                Mutex mutex;
                while( curElem -> next != NULL )
                {
                    curElem = curElem->next;
                } 
                if ((curElem->next = new PtnElem()) == NULL ) {
                    // out of memory
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return;
                } 
                curElem=curElem->next;
            }
            curElem->basePattern = new UnicodeString(basePattern);
            curElem->pattern = new UnicodeString(value);
            PtnSkeleton* pNewSkeleton = curElem->skeleton = new PtnSkeleton;
            for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
                pNewSkeleton->type[i] = skeleton.type[i];
                pNewSkeleton->original[i] = skeleton.original[i];
                pNewSkeleton->baseOriginal[i] = skeleton.baseOriginal[i];
            }
            curElem->next = NULL;
        }
        else {
            // Pattern exists in the list already.
            if ( !isDupAllowed ) {
                return;
            }
            // Overwrite the value.
            *(curElem->pattern)= value;
        }
    }
}  // PatternMap::add

// Find the pattern from the given basePattern string.
UnicodeString *
PatternMap::getPatternFromBasePattern(UnicodeString& basePattern) { // key to search for
   UChar baseChar = basePattern.charAt(0);
   PtnElem *curElem, baseElem;

   // the baseChar must be A-Z or a-z 
   if ( (baseChar >= CAP_A) && (baseChar <= CAP_Z) ) {
     curElem = boot[26 + (baseChar-CAP_A)];
   }
   else {
      if ( (baseChar >=LOW_A) && (baseChar <= LOW_Z) ) {
        curElem = boot[baseChar-LOW_A];
      }
      else
         return NULL;
   }
   
   if ( curElem == NULL ) {
     return NULL;  // no match
   }

   do  {
     if ( basePattern.compare(*(curElem->basePattern))==0 ) {
        return curElem->pattern;
     }
     curElem=curElem->next;   
   }while (curElem != NULL);

   return NULL;  
}  // PatternMap::getFromBasePattern


// Find the pattern from the given skeleton.
UnicodeString *
PatternMap::getPatternFromSkeleton(PtnSkeleton& skeleton) { // key to search for
   PtnElem *curElem, *nextElem, baseElem;

   // find boot entry
   UChar baseChar='\0';
   for (int32_t i=0; i<TYPE_LIMIT; ++i) {
       if (skeleton.baseOriginal[i].length() !=0 ) {
           baseChar = skeleton.baseOriginal[i].charAt(0);
           break;
       }
   }
   
   // the baseChar must be A-Z or a-z 
   if ( (baseChar >= CAP_A) && (baseChar <= CAP_Z) ) {
     curElem = boot[baseChar-CAP_A];
   }
   else {
      if ( (baseChar >=LOW_A) && (baseChar <= LOW_Z) ) {
        curElem = boot[26+baseChar-LOW_A];
      }
      else
         return NULL;
   }
   
   if ( curElem == NULL ) {
     return NULL;  // no match
   }

   do  {
       int32_t i=0;
       for (i=0; i<TYPE_LIMIT; ++i) {
           if (curElem->skeleton->baseOriginal[i].compare(skeleton.baseOriginal[i]) != 0 )
           {
               break;
           }
       }
       if (i == TYPE_LIMIT) {
           return curElem->pattern;
       }
       curElem=curElem->next;   
   }while (curElem != NULL);

   return NULL;  
}  // PatternMap::getFromBasePattern


UBool
PatternMap::equals(const PatternMap& other) {
    if ( this==&other ) {
        return TRUE;
    }
    for (int32_t bootIndex=0; bootIndex<MAX_PATTERN_ENTRIES; ++bootIndex ) {
        if ( boot[bootIndex]==other.boot[bootIndex] ) {
            continue;
        }
        if ( (boot[bootIndex]==NULL)||(other.boot[bootIndex]==NULL) ) {
            return FALSE;
        }
        PtnElem *otherElem = other.boot[bootIndex];
        PtnElem *myElem = boot[bootIndex];
        while ((otherElem!=NULL) || (myElem!=NULL)) {
            if ( myElem == otherElem ) {
                break;
            }
            if ((otherElem==NULL) || (myElem==NULL)) {
                return FALSE;
            }
            if ( (*(myElem->basePattern) != *(otherElem->basePattern)) ||
                 (*(myElem->pattern) != *(otherElem->pattern) ) ) {
                return FALSE;
            }
            if ((myElem->skeleton!=otherElem->skeleton)&&
                !myElem->skeleton->equals(*(otherElem->skeleton))) {
                return FALSE;
            }
            myElem = myElem->next;
            otherElem=otherElem->next;
        }
    }
    return TRUE;
}  

// find any key existing in the mapping table already.
// return TRUE if there is an existing key, otherwise return FALSE.
PtnElem* 
PatternMap::getDuplicateElem(
            UnicodeString &basePattern, 
            PtnSkeleton &skeleton,
            PtnElem *baseElem)  {
   PtnElem *curElem;
   
   if ( baseElem == (PtnElem *)NULL )  {
         return (PtnElem*)NULL;
   }
   else {
         curElem = baseElem;
   }
   do {
     if ( basePattern.compare(*(curElem->basePattern))==0 ) {
        UBool isEqual=TRUE;
        for (int32_t i=0; i<TYPE_LIMIT; ++i) {
            if (curElem->skeleton->type[i] != skeleton.type[i] ) {
                isEqual=FALSE;
                break;
            }
        }
        if (isEqual) {
            return curElem;
        }
     }
     curElem = curElem->next;
   } while( curElem != (PtnElem *)NULL );
   
   // end of the list
   return (PtnElem*)NULL;  
  
}  // PatternMap::getDuplicateElem

DateTimeMatcher::DateTimeMatcher(void) {
}


void
DateTimeMatcher::set(const UnicodeString& pattern, FormatParser& fp) {
    PtnSkeleton skeleton;
    return set(pattern, fp, skeleton);
}

void
DateTimeMatcher::set(const UnicodeString& patternForm, FormatParser& fp, PtnSkeleton& skeleton) {
    
    const UChar oldFormat[3]={0x5C, 0x75, 0}; // "\\u"   
    const UChar repeatedPatterns[6]={CAP_G, CAP_E, LOW_Z, LOW_V, CAP_Q, 0}; // "GEzvQ"
    UnicodeString repeatedPattern=UnicodeString(repeatedPatterns);
        
    //TODO: lock the tree, how about oldPattern
    if (patternForm.indexOf(oldFormat, 3, 0) > 0 ) {
        UnicodeString oldPattern(patternForm);
    }
    UnicodeString pattern(patternForm);
    
    fromHex->transliterate(pattern);
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        skeleton.type[i]=NONE; 
    }
    fp.set(pattern);
    for (int32_t i=0; i < fp.itemNumber; i++) {
        UnicodeString field = fp.items[i];
        if ( field.charAt(0) == LOW_A ) {
            continue;  // skip 'a'
        }
    
        int32_t canonicalIndex = fp.getCanonicalIndex(field);
        if (canonicalIndex < 0 ) {
            continue;
            // Error
            // TODO set UErrorCode
        }
        const dtTypeElem *row = &dtTypes[canonicalIndex];
        int32_t typeValue = row->field;
        skeleton.original[typeValue]=field;
        UChar repeatChar = row->patternChar;
        int32_t repeatCount = row->minLen > 3 ? 3: row->minLen;
        while (repeatCount-- > 0) {
            skeleton.baseOriginal[typeValue] += repeatChar;
        }
        int16_t subTypeValue = row->type;
        if ( row->type > 0) {
            subTypeValue += field.length();  // why?
        }
        skeleton.type[typeValue] = (char)subTypeValue;
    }
    
    {
        Mutex mutex;
        for (int32_t i=0; i<TYPE_LIMIT; ++i) {
            this->skeleton.type[i] = skeleton.type[i];
            this->skeleton.baseOriginal[i] =  skeleton.baseOriginal[i];
            this->skeleton.original[i] = skeleton.original[i];
        }
    }
    
    return;
}

void
DateTimeMatcher::getBasePattern(UnicodeString &result ) {
    result.remove(); // Reset the result first.
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        if (skeleton.baseOriginal[i].length()!=0) {
            result += skeleton.baseOriginal[i];
        }
    }
    return;
}

int32_t 
DateTimeMatcher::getDistance(DateTimeMatcher& other, int32_t includeMask, DistanceInfo& distanceInfo) {
    int32_t result=0;
    distanceInfo.clear();
    for (int32_t i=0; i<TYPE_LIMIT; ++i ) {
        int32_t myType = (includeMask&(1<<i))==0 ? 0 : skeleton.type[i];
        int32_t otherType = other.skeleton.type[i];
        if (myType==otherType) {
            continue;
        }
        if (myType==0) {// and other is not
            result += EXTRA_FIELD;
            distanceInfo.addExtra(i);
        }
        else {
            if (otherType==0) {
                result += MISSING_FIELD;
                distanceInfo.addMissing(i);
            }
            else {
                result += abs(myType - otherType); 
                //printf("\n myType:%d  otherType:%d r=%d", myType, otherType, result);
            }
        }
        
    }
    return result;
}

void
DateTimeMatcher::copyFrom(PtnSkeleton& skeleton) {
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        this->skeleton.type[i]=skeleton.type[i];
        this->skeleton.original[i]=skeleton.original[i];
        this->skeleton.baseOriginal[i]=skeleton.baseOriginal[i];
    }
    return;
}

void 
DateTimeMatcher::copyFrom() {
    // same as clear
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        this->skeleton.type[i]=0;
        this->skeleton.original[i]="";
        this->skeleton.baseOriginal[i]="";
    }
    return;
}

UBool
DateTimeMatcher::equals(DateTimeMatcher* other) {
    if (other==NULL) {
        return FALSE;
    }
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        if (this->skeleton.original[i]!=other->skeleton.original[i] ) {
            return FALSE;
        }
    }
    return TRUE;
}

int32_t
DateTimeMatcher::getFieldMask() {
    int32_t result=0;
    
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        if (skeleton.type[i]!=0) {
            result |= (1<<i);
        }
    }
    return result;
}

//TODO : remove this API if the DateTimePatternGenerator() is a subclass of DateTimeMatcher
PtnSkeleton*
DateTimeMatcher::getSkeleton() {
    return &skeleton;
}

FormatParser::FormatParser () {
    status = START;
    itemNumber=0;
    printf("\nFormatParser():%p", this);
}


FormatParser::~FormatParser () {
    printf("\n~FormatParser():%p", this);
}


// Find the next token with the starting position and length
// Note: the startPos may 
FormatParser::TokenStatus
FormatParser::setTokens(UnicodeString &pattern, int32_t startPos, int32_t *len) {
    int32_t  curLoc = startPos;
    if ( curLoc >= pattern.length()) {
        return DONE;
    }
    // check the current char is between A-Z or a-z
    do {
        UChar c=pattern.charAt(curLoc);
        if ( (c>=CAP_A && c<=CAP_Z) || (c>=LOW_A && c<=LOW_Z) ) {
           curLoc++;
        }
        else {
               startPos = curLoc;
               *len=1;
               return ADD_TOKEN;
        }
        
        if ( pattern.charAt(curLoc)!= pattern.charAt(startPos) ) {
            break;  // not the same token
        }
    } while(curLoc <= pattern.length());
    *len = curLoc-startPos;
    return ADD_TOKEN;
}

void
FormatParser::set(UnicodeString &pattern) {
    int32_t startPos=0;
    TokenStatus result=START;
    int32_t len=0;
    itemNumber =0;
    
    do {
        result = setTokens( pattern, startPos, &len );
        if ( result == ADD_TOKEN )
        {
            items[itemNumber++] = UnicodeString(pattern, startPos, len );
            startPos += len;
        }
        else {
            break;
        }  
    } while (result==ADD_TOKEN && itemNumber < MAX_DT_TOKEN);
}

int
FormatParser::getCanonicalIndex(UnicodeString s) {
    int32_t len = s.length();
    UChar ch = s.charAt(0);
    int32_t i=0;
    
    while (dtTypes[i].patternChar!='\0') {
        if ( dtTypes[i].patternChar!=ch ) {
            ++i;
            continue;
        }
        if (dtTypes[i].patternChar!=dtTypes[i+1].patternChar) {
            return i;
        }
        if (dtTypes[i+1].minLen <= len) {
            ++i;
            continue;
        }
        return i;
    }
    return -1;
}

UBool 
FormatParser::isQuoteLiteral(UnicodeString s) { 
    if ((s.charAt(0)==SINGLE_QUOTE)||(s.charAt(0)==FORWARDSLASH)||(s.charAt(0)==BACKSLASH) ||
        (s.charAt(0)==SPACE) ||(s.charAt(0)==COMMA) ||(s.charAt(0)==HYPHEN) ||(s.charAt(0)==DOT) ) {
        return TRUE;
    }
    else {
        return FALSE;
    }
}

// This function aussumes the current itemIndex points to the quote literal.
// Please call isQuoteLiteral prior to this function.
void
FormatParser::getQuoteLiteral(UnicodeString& quote, int32_t *itemIndex) {
    int32_t i=*itemIndex;
    if ((items[i].charAt(0)==FORWARDSLASH) || (items[i].charAt(0)==BACKSLASH) || (items[i].charAt(0)==SPACE) ||
        (items[i].charAt(0)==COMMA) ||(items[i].charAt(0)==HYPHEN) ||(items[i].charAt(0)==DOT)) {
        quote += items[i];
        return;
    }
    if (items[i].charAt(0)==SINGLE_QUOTE) {
        quote += items[i];
        ++i;
    }
    while ( i < itemNumber ) {
        if ( items[i].charAt(0)==SINGLE_QUOTE ) {
            if ( items[i+1].charAt(0)==SINGLE_QUOTE ) {
                // two single quotes e.g. 'o''clock'
                quote += items[++i];
                continue;
            }
            else {
                quote += items[i];
                break;
            }
            
        }
        else {
            quote += items[i];
        }
        ++i;
    }
    *itemIndex=i;
    return;
}

UBool
FormatParser::isPatternSeparator(UnicodeString& field) {
    for (int32_t i=0; i<field.length(); ++i ) {
        UChar c= field.charAt(i);
        if ( (c==SINGLE_QUOTE) || (c==BACKSLASH) || (c==SPACE) || (c==COLON) ||
             (c==QUOTATION_MARK) || (c==COMMA) || (c==HYPHEN) ) {
            continue;
        }
        else {
            return FALSE;
        }
    }
    return TRUE;
}

void
DistanceInfo::setTo(DistanceInfo &other) {
    missingFieldMask = other.missingFieldMask;
    extraFieldMask= other.extraFieldMask;
    return;
}

PatternMapIterator::PatternMapIterator() {
    bootIndex = 0;
    nodePtr = NULL;
    patternMap=NULL;
    matcher= new DateTimeMatcher();
}


PatternMapIterator::~PatternMapIterator() {
    //delete matcher;
}

void
PatternMapIterator::set(PatternMap& patternMap) {
    /*
    for (int32_t i=0; i<MAX_PATTERN_ENTRIES; ++i ) {
        this->boot[i]=patternMap.boot[i];
    }
    */
    this->patternMap=&patternMap;
}

UBool
PatternMapIterator::hasNext() {
    int32_t headIndex=bootIndex;
    PtnElem *curPtr=nodePtr;
    
    if (patternMap==NULL) {
        return FALSE;
    }
    while ( headIndex < MAX_PATTERN_ENTRIES ) {
        if ( curPtr != NULL ) {
            if ( curPtr->next != NULL ) {
                return TRUE;
            }
            else {
                headIndex++;
                curPtr=NULL;
                continue;
            }
        }
        else {
            if ( patternMap->boot[headIndex] != NULL ) {
                return TRUE;
            }
            else {
                headIndex++;
                continue;
            }
        }
        
    }
    return FALSE;
}

DateTimeMatcher&
PatternMapIterator::next() {   
    while ( bootIndex < MAX_PATTERN_ENTRIES ) {
        if ( nodePtr != NULL ) {
            if ( nodePtr->next != NULL ) {
                nodePtr = nodePtr->next;
                break;
            }
            else {
                bootIndex++;
                nodePtr=NULL;
                continue;
            }
        }
        else {
            if ( patternMap->boot[bootIndex] != NULL ) {
                nodePtr = patternMap->boot[bootIndex];
                break;
            }
            else {
                bootIndex++;
                continue;
            }
        }
    }
    if (nodePtr!=NULL) {
        matcher->copyFrom(*nodePtr->skeleton);
    }
    else {
        matcher->copyFrom();
    }
    return *matcher;
}

PtnSkeleton::PtnSkeleton() : UObject() {
}


PtnSkeleton::PtnSkeleton(PtnSkeleton& other) : UObject() {
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        this->type[i]=other.type[i];
        this->original[i]=other.original[i];
        this->baseOriginal[i]=other.baseOriginal[i];
    }
}

UBool
PtnSkeleton::equals(PtnSkeleton& other)  {
    for (int32_t i=0; i<TYPE_LIMIT; ++i) {
        if ( (type[i]!= other.type[i]) || 
             (original[i]!=other.original[i]) ||
             (baseOriginal[i]!=other.baseOriginal[i]) ) {
            return FALSE;
        }
    }
    return TRUE;
}

PtnSkeleton::~PtnSkeleton() {
}

PtnElem::PtnElem() : UObject() {
    basePattern=NULL;
    pattern=NULL;
    skeleton=NULL;
    next=NULL;
}

PtnElem::~PtnElem() {

    if (next!=NULL) {
        delete next;
    }
    delete basePattern;
    delete pattern;
    delete skeleton;
}


U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
