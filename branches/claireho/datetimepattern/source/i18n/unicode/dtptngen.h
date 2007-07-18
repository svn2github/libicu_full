/*
*******************************************************************************
* Copyright (C) 2007-, International Business Machines Corporation and
* others. All Rights Reserved.
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File DTPTNGEN.H
*
* Modification History:
*
*   Date        Name        Description
*******************************************************************************
*/

#ifndef DTPTNGEN_H
#define DTPTNGEN_H

#include "hash.h"
#include "unicode/utypes.h"
#include "unicode/format.h"
#include "unicode/datefmt.h"
#include "unicode/calendar.h"
#include "unicode/ustring.h"
#include "unicode/locid.h"
#include "unicode/udat.h"

U_NAMESPACE_BEGIN

#define MAX_PATTERN_ENTRIES 52
#define MAX_CLDR_FIELD_LEN  60
#define MAX_DT_TOKEN        50
#define MAX_RESOURCE_FIELD  11
#define MAX_AVAILABLE_FORMATS  12
#define TYPE_LIMIT    16
#define BASE_CONFLICT 1
#define NO_CONFLICT   0
#define CONFLICT      2
#define NONE          0
#define EXTRA_FIELD   0x10000
#define MISSING_FIELD  0x1000
#define SINGLE_QUOTE      ((UChar)0x0027)
#define FORWARDSLASH      ((UChar)0x002F)
#define BACKSLASH         ((UChar)0x005C)
#define SPACE             ((UChar)0x0020)
#define QUOTATION_MARK    ((UChar)0x0022)
#define ASTERISK          ((UChar)0x002A)
#define PLUSSITN          ((UChar)0x002B)
#define COMMA             ((UChar)0x002C)
#define HYPHEN            ((UChar)0x002D)
#define DOT               ((UChar)0x002E)
#define COLON             ((UChar)0x003A)
#define CAP_A             ((UChar)0x0041)
#define CAP_C             ((UChar)0x0043)
#define CAP_D             ((UChar)0x0044)
#define CAP_E             ((UChar)0x0045)
#define CAP_F             ((UChar)0x0046)
#define CAP_G             ((UChar)0x0047)
#define CAP_H             ((UChar)0x0048)
#define CAP_L             ((UChar)0x004C)
#define CAP_M             ((UChar)0x004D)
#define CAP_O             ((UChar)0x004F)
#define CAP_Q             ((UChar)0x0051)
#define CAP_S             ((UChar)0x0053)
#define CAP_T             ((UChar)0x0054)
#define CAP_V             ((UChar)0x0056)
#define CAP_W             ((UChar)0x0057)
#define CAP_Y             ((UChar)0x0059)
#define CAP_Z             ((UChar)0x005A)
#define LOWLINE           ((UChar)0x005F)
#define LOW_A             ((UChar)0x0061)
#define LOW_C             ((UChar)0x0063)
#define LOW_D             ((UChar)0x0064)
#define LOW_E             ((UChar)0x0065)
#define LOW_F             ((UChar)0x0066)
#define LOW_G             ((UChar)0x0067)
#define LOW_H             ((UChar)0x0068)
#define LOW_I             ((UChar)0x0069)
#define LOW_K             ((UChar)0x006B)
#define LOW_L             ((UChar)0x006C)
#define LOW_M             ((UChar)0x006D)
#define LOW_N             ((UChar)0x006E)
#define LOW_O             ((UChar)0x006F)
#define LOW_P             ((UChar)0x0070)
#define LOW_Q             ((UChar)0x0071)
#define LOW_R             ((UChar)0x0072)
#define LOW_S             ((UChar)0x0073)
#define LOW_T             ((UChar)0x0074)
#define LOW_U             ((UChar)0x0075)
#define LOW_V             ((UChar)0x0076)
#define LOW_W             ((UChar)0x0077)
#define LOW_Y             ((UChar)0x0079)
#define LOW_Z             ((UChar)0x007A)
#define DT_SHORT          -0x101
#define DT_LONG           -0x102
#define DT_NUMERIC         0x100
#define DT_NARROW         -0x100
#define DT_DELTA           0x10


class DateFormatSymbols;
class DateFormat;
class Hashtable;

typedef enum DtField {
    DT_ERA,
    DT_YEAR,
    DT_QUARTER,
    DT_MONTH,
    DT_WEEK_OF_YEAR,
    DT_WEEK_OF_MONTH,
    DT_WEEKDAY,
    DT_DAY_OF_YEAR, 
    DT_DAY_OF_WEEK_IN_MONTH,
    DT_DAY,
    DT_DAYPERIOD,
    DT_HOUR,
    DT_MINUTE,
    DT_SECOND,
    DT_FRACTIONAL_SECOND,
    DT_ZONE,
} DtField;



typedef struct dtTypeElem {
    UChar             patternChar;
    DtField           field;
    int32_t           type;
    int32_t           minLen;
    int32_t           weight;
}dtTypeElem;

typedef struct PatternInfo {
    int32_t  status;
    UnicodeString conflictingPattern;
}PatternInfo; 


class U_I18N_API PtnSkeleton : public UObject {
public:
    int32_t type[TYPE_LIMIT];
    UnicodeString original[TYPE_LIMIT];
    UnicodeString baseOriginal[TYPE_LIMIT];
    
    PtnSkeleton();
    PtnSkeleton(PtnSkeleton& other);
    UBool equals(PtnSkeleton& other);
    virtual ~PtnSkeleton();

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     *
    */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;

};


class U_I18N_API PtnElem : public UObject {
public:
    UnicodeString *basePattern;
    PtnSkeleton   *skeleton;
    UnicodeString *pattern;
    PtnElem       *next;
    
    PtnElem();
    virtual ~PtnElem();
    
    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     *
    */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;
    
}; 

class U_I18N_API FormatParser : public UObject{
public:
    UnicodeString items[MAX_DT_TOKEN];
    int32_t  itemNumber;
    
    FormatParser();
    virtual ~FormatParser();
    /**
     * Set the string to parse
     * @param string
     * @return this, for chaining
     * @deprecated
     * @internal
     */
    void set(UnicodeString &patternString);
    /**
     * Check string 's' is a quoted literal
     * @param string with quoted literals
     * @param starting index of items and return the next index of items of end quoted literal
     * @return TRUE if s is a quote literal, FALSE if s is not a quote literal.
     * @deprecated
     * @internal
     */    
    UBool isQuoteLiteral(UnicodeString s);
    /**
     *  produce a quoted literal
     * @param string with quoted literals
     * @param starting index of items and return the next index of items of end quoted literal
     * @return none
     * @deprecated
     * @internal
     */    
    void getQuoteLiteral(UnicodeString& quote, int32_t *itemIndex);
    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     *
    */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;
    int32_t getCanonicalIndex(UnicodeString s);
    UBool isPatternSeparator(UnicodeString& field);

private:
   typedef enum TokenStatus {
       START,
       ADD_TOKEN,
       SYNTAX_ERROR,
       DONE
   } ToeknStatus;
   
 
   TokenStatus status;
   
   virtual TokenStatus setTokens(UnicodeString &pattern, int32_t startPos, int32_t *len);
};


class DistanceInfo : public UObject {
public:
    int32_t missingFieldMask;
    int32_t extraFieldMask;
    
    DistanceInfo() {};
    virtual ~DistanceInfo() {};
    void clear() { missingFieldMask = extraFieldMask = 0; };
    void setTo(DistanceInfo& other);
    void addMissing(int32_t field) { missingFieldMask |= (1<<field); };
    void addExtra(int32_t field) { extraFieldMask |= (1<<field); };
    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     *
    */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;
};

class U_I18N_API DateTimeMatcher: public UObject {
public:
    PtnSkeleton skeleton;
    
    void getBasePattern(UnicodeString &basePattern);
    void set(const UnicodeString& pattern, FormatParser& fp);
    void set(const UnicodeString& pattern, FormatParser& fp, PtnSkeleton& skeleton);
    void copyFrom(PtnSkeleton& skeleton);
    void copyFrom();
    PtnSkeleton* getSkeleton();
    UBool equals(DateTimeMatcher* other);
    int32_t getDistance(DateTimeMatcher& other, int32_t includeMask, DistanceInfo& distanceInfo);
    DateTimeMatcher(); 
    virtual ~DateTimeMatcher() {};
    int32_t getFieldMask();
   /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     */
    static UClassID U_EXPORT2 getStaticClassID();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;
    
};

class U_I18N_API PatternMap : public UObject{
public:
    PtnElem *boot[MAX_PATTERN_ENTRIES];
    PatternMap(); 
    virtual  ~PatternMap();
    void  add(UnicodeString& basePattern, PtnSkeleton& skeleton, UnicodeString& value, UErrorCode& status);
    UErrorCode status;
    UnicodeString* getPatternFromBasePattern(UnicodeString& basePattern);
    UnicodeString* getPatternFromSkeleton(PtnSkeleton& skeleton);
    void copyFrom(const PatternMap& other, UErrorCode& status);
    UBool equals(const PatternMap& other);
    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     *
    */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;
    
private:
    UBool isDupAllowed;
    PtnElem*  getDuplicateElem(UnicodeString &basePattern, PtnSkeleton& skeleton, PtnElem *baseElem);
}; // end  PatternMap 

class U_I18N_API PatternMapIterator : public UObject {
public:
    PatternMapIterator();  
    virtual ~PatternMapIterator();
    void set(PatternMap& patternMap);
    UBool hasNext();
    DateTimeMatcher& next();
    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     *
    */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;
private:
    int32_t bootIndex;
    PtnElem *nodePtr;
    DateTimeMatcher *matcher;
    PatternMap *patternMap;
};


/**
 * This class provides flexible generation of date format patterns, like "yy-MM-dd". The user can build up the generator
 * by adding successive patterns. Once that is done, a query can be made using a "skeleton", which is a pattern which just
 * includes the desired fields and lengths. The generator will return the "best fit" pattern corresponding to that skeleton.
 * <p>The main method people will use is getBestPattern(String skeleton),
 * since normally this class is pre-built with data from a particular locale. However, generators can be built directly from other data as well.
 * <p><i>Issue: may be useful to also have a function that returns the list of fields in a pattern, in order, since we have that internally.
 * That would be useful for getting the UI order of field elements.</i>
 * @draft ICU 3.8
 * @provisional This API might change or be removed in a future release.
**/
class U_I18N_API DateTimePatternGenerator : public UObject {
public:
   /**
     * Construct a flexible generator according to default locale.
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    static DateTimePatternGenerator* U_EXPORT2 createInstance(UErrorCode& err);
    
   /**
     * Construct a flexible generator according to data for a given locale.
     * @param uLocale
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    static DateTimePatternGenerator* U_EXPORT2 createInstance(const Locale& other, UErrorCode& err );

     /**
      * Clones DateTimePatternGenerator object.  Clients are responsible for deleting 
      * the DateTimePatternGenerator object cloned.
      * @draft ICU 3.8
      * @provisional This API might change or be removed in a future release.
      */
    DateTimePatternGenerator* clone(UErrorCode & status);

     /**
      * Return true if another object is semantically equal to this one.
      *
      * @param other    the DateTimePatternGenerator object to be compared with.
      * @return         true if other is semantically equal to this. 
      * @draft ICU 3.8
      * @provisional This API might change or be removed in a future release.
      */
    UBool operator==(const DateTimePatternGenerator& other);      
    
    /**
     * Utility to return a unique skeleton from a given pattern. For example,
     * both "MMM-dd" and "dd/MMM" produce the skeleton "MMMdd".
     * 
     * @param pattern     Input pattern, such as "dd/MMM"
     * @param retSkeleton return skeleton, such as "MMMdd"
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void getSkeleton(UnicodeString pattern, UnicodeString& retSkeleton);
    
    /**
     * Utility to return a unique base skeleton from a given pattern. This is
     * the same as the skeleton, except that differences in length are minimized
     * so as to only preserve the difference between string and numeric form. So
     * for example, both "MMM-dd" and "d/MMM" produce the skeleton "MMMd"
     * (notice the single d).
     * 
     * @param pattern  Input pattern, such as "dd/MMM"
     * @param result   return skeleton, such as "Md"
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void getBaseSkeleton(UnicodeString pattern, UnicodeString &result);
    
    /**
     * Adds a pattern to the generator. If the pattern has the same skeleton as
     * an existing pattern, aClass providing date formattingnd the override parameter is set, then the previous
     * value is overriden. Otherwise, the previous value is retained. In either
     * case, the conflicting information is returned in PatternInfo.
     * <p>
     * Note that single-field patterns (like "MMM") are automatically added, and
     * don't need to be added explicitly!
     * 
     * @param pattern    Input pattern, such as "dd/MMM"
     * @param override   when existing values are to be overridden use true, otherwise use false.
     * @param returnInfo return conflicting information in PatternInfo.  The value of status is 
     *                   CONFLICT, BASE_CONFLICT or NO_CONFLICT.
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
     void add(UnicodeString pattern, UBool override, PatternInfo& returnInfo);
    
   /**
     * An AppendItem format is a pattern used to append a field if there is no
     * good match. For example, suppose that the input skeleton is "GyyyyMMMd",
     * and there is no matching pattern internally, but there is a pattern
     * matching "yyyyMMMd", say "d-MM-yyyy". Then that pattern is used, plus the
     * G. The way these two are conjoined is by using the AppendItemFormat for G
     * (era). So if that value is, say "{0}, {1}" then the final resulting
     * pattern is "d-MM-yyyy, G".
     * <p>
     * There are actually three available variables: {0} is the pattern so far,
     * {1} is the element we are adding, and {2} is the name of the element.
     * <p>
     * This reflects the way that the CLDR data is organized.
     * 
     * @param field  such as ERA
     * @param value  pattern, such as "{0}, {1}"
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void setAppendItemFormats(int32_t field, UnicodeString value);
         
    /**
     * Getter corresponding to setAppendItemFormats. Values below 0 or at or
     * above TYPE_LIMIT are illegal arguments.
     * 
     * @param field
     * @param appendItemFormat return append pattern for field
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void getAppendItemFormats(int32_t field, UnicodeString &appendItemFormat);
 
   /**
     * Sets the names of fields, eg "era" in English for ERA. These are only
     * used if the corresponding AppendItemFormat is used, and if it contains a
     * {2} variable.
     * <p>
     * This reflects the way that the CLDR data is organized.
     * 
     * @param field
     * @param value
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void setAppendItemNames(int32_t field, UnicodeString value);
    
    /**
     * Getter corresponding to setAppendItemNames. Values below 0 or at or above
     * TYPE_LIMIT are illegal arguments.
     * 
     * @param field
     * @param appendItemName return name for field
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void getAppendItemNames(int32_t field, UnicodeString& appendItemName);
           
    /**
     * The date time format is a message format pattern used to compose date and
     * time patterns. The default value is "{0} {1}", where {0} will be replaced
     * by the date pattern and {1} will be replaced by the time pattern.
     * <p>
     * This is used when the input skeleton contains both date and time fields,
     * but there is not a close match among the added patterns. For example,
     * suppose that this object was created by adding "dd-MMM" and "hh:mm", and
     * its datetimeFormat is the default "{0} {1}". Then if the input skeleton
     * is "MMMdhmm", there is not an exact match, so the input skeleton is
     * broken up into two components "MMMd" and "hmm". There are close matches
     * for those two skeletons, so the result is put together with this pattern,
     * resulting in "d-MMM h:mm".
     * 
     * @param dateTimeFormat
     *            message format pattern, here {0} will be replaced by the date
     *            pattern and {1} will be replaced by the time pattern.
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void setDateTimeFormat(UnicodeString& dtFormat);
    
     /**
     * Getter corresponding to setDateTimeFormat.
     * @param return pattern
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void getDateTimeFormat(UnicodeString& dtFormat);
    
   /**
     * Return the best pattern matching the input skeleton. It is guaranteed to
     * have all of the fields in the skeleton.
     * 
     * @param patternForm
     *            The patternForm is a pattern containing only the variable fields.
     *            For example, "MMMdd" and "mmhh" are patternForms.
     * @param bestPattern
     *            The best pattern found from the given patternForm.
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
     void getBestPattern(const UnicodeString& patternForm, UnicodeString& bestPattern);
    
        
    /**
     * Adjusts the field types (width and subtype) of a pattern to match what is
     * in a skeleton. That is, if you supply a pattern like "d-M H:m", and a
     * skeleton of "MMMMddhhmm", then the input pattern is adjusted to be
     * "dd-MMMM hh:mm". This is used internally to get the best match for the
     * input skeleton, but can also be used externally.
     * 
     * @param pattern
     *        input pattern
     * @param skeleton
     * @param result
     *        pattern adjusted to match the skeleton fields widths and subtypes.
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
     void replaceFieldTypes(UnicodeString pattern, UnicodeString skeleton, UnicodeString& result);
     
     /**
      * Return a list of all the skeletons (in canonical form) from this class,
      * and the patterns that they map to.
      * 
      * @param result
      *            an output PatternMap in which to place the mapping from skeleton to
      *            pattern. If the input value is null, then a PatternMap is allocated.
      *            <p>
      *            <i>Issue: an alternate API would be to just return a list of
      *            the skeletons, and then have a separate routine to get from
      *            skeleton to pattern.</i>
      * @param status
      *            return U_ZERO_ERROR or U_MEMORY_ALLOCATION_ERROR
      *            
      * @draft ICU 3.8
      * @provisional This API might change or be removed in a future release.
      */
     void getSkeletons(Hashtable* result, UErrorCode& status);
     
     /**
      * Return a list of all the base skeletons (in canonical form) from this class
      * @param arraySize
      *          The resultArray size.
      * @param resultArray
      *          The result array to store the base skeletons.
      * @param numberSkeleton
      *          Number of skeletons returned.   
      * @param status
      *          U_ZERO_ERROR or U_BUFFER_OVERFLOW_ERROR.
      * @draft ICU 3.6
      * @provisional This API might change or be removed in a future release.
      */
     void getBaseSkeletons(int32_t maxArraySize, UnicodeString* resultArray, int32_t *numberSkeleton, UErrorCode& status);
     
    /**
     * The decimal value is used in formatting fractions of seconds. If the
     * skeleton contains fractional seconds, then this is used with the
     * fractional seconds. For example, suppose that the input pattern is
     * "hhmmssSSSS", and the best matching pattern internally is "H:mm:ss", and
     * the decimal string is ",". Then the resulting pattern is modified to be
     * "H:mm:ss,SSSS"
     * 
     * @param decimal
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void setDecimal(UnicodeString decimal);
    
    /**
     * Getter corresponding to setDecimal.
     * @param result
     *        string corresponding to the decimal point
     * @draft ICU 3.8
     * @provisional This API might change or be removed in a future release.
     */
    void getDecimal(UnicodeString& result);
    
    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 3.8
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 3.8
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Destructor.
     */
    virtual ~DateTimePatternGenerator();
protected :  
    /**
     * constructor.
     * @draft ICU 3.8
     */
    DateTimePatternGenerator(UErrorCode & status); 
    
    /**
     * constructor.
     * @draft ICU 3.8
     */
    DateTimePatternGenerator(const Locale& locale, UErrorCode & status); 
    
    /**
     * Copy constructor.
     * @param DateTimePatternGenerator copy 
     * @draft ICU 3.8
     */
    DateTimePatternGenerator(const DateTimePatternGenerator& other); 
    
    /**
     * Copy constructor.
     * @draft ICU 3.8
     */
    DateTimePatternGenerator(const DateTimePatternGenerator&, UErrorCode & status);    
    
    /**
      * Default assignment operator.
      */
    DateTimePatternGenerator& operator=(const DateTimePatternGenerator& other);
     
private :    

    Locale pLocale;  // pattern locale
    FormatParser fp;
    DateTimeMatcher dtMatcher;
    DistanceInfo distanceInfo;
    PatternMap patternMap;
    UnicodeString appendItemFormats[TYPE_LIMIT];
    UnicodeString appendItemNames[TYPE_LIMIT];
    UnicodeString dateTimeFormat;
    UnicodeString decimal;
    DateTimeMatcher *skipMatcher;
    Hashtable *fAvailableFormatKeyHash;
    UnicodeString hackPattern;
    UErrorCode status;
    
    static const int32_t FRACTIONAL_MASK = 1<<DT_FRACTIONAL_SECOND;
    static const int32_t SECOND_AND_FRACTIONAL_MASK = (1<<DT_SECOND) | (1<<DT_FRACTIONAL_SECOND);
    
    void initData(const Locale &locale);
    void addCanonicalItems();
    void addICUPatterns(const Locale& locale);
    void hackTimes(PatternInfo& returnInfo, UnicodeString& hackPattern);
    void addCLDRData(const Locale& locale);
    void initHashtable(UErrorCode& err);
    void setDateTimeFromCalendar(const Locale& locale);
    void setDecimalSymbols(const Locale& locale);
    int32_t getAppendFormatNumber(const char* field); 
    int32_t getAppendNameNumber(const char* field);
    void getAppendName(int32_t field, UnicodeString& value);
    int32_t getCanonicalIndex(UnicodeString &field);
    UnicodeString* getBestRaw(DateTimeMatcher source, int32_t includeMask, DistanceInfo& missingFields);
    UnicodeString adjustFieldTypes(UnicodeString& pattern, UBool fixFractionalSeconds);
    UnicodeString getBestAppending(int32_t missingFields);
    int32_t getTopBitNumber(int32_t foundMask);
    void setAvailableFormat(const char* key, UErrorCode& err);
    UBool isAvailableFormatSet(const char* key);
    void copyHashtable(Hashtable *other);
    UErrorCode getStatus() {  return status; } ;
} ;// end class DateTimePatternGenerator 



U_NAMESPACE_END
#endif
