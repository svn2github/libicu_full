/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2002-2009, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

//
//   dcfmtest.cpp
//
//     Decimal Formatter tests, data driven. 
//

#include "intltest.h"
#if !UCONFIG_NO_REGULAR_EXPRESSIONS

#include "unicode/regex.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/unistr.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "cmemory.h"
#include "dcfmtest.h"
#include "util.h"
#include "cstring.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <iostream>

//---------------------------------------------------------------------------
//
//  Test class boilerplate
//
//---------------------------------------------------------------------------
DecimalFormatTest::DecimalFormatTest()
{
}


DecimalFormatTest::~DecimalFormatTest()
{
}



void DecimalFormatTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite DecimalFormatTest: ");
    switch (index) {

#if !UCONFIG_NO_FILE_IO
        case 0: name = "DataDrivenTests";
            if (exec) DataDrivenTests();
            break;
#else
        case 0: name = "skip";
            break;
#endif

        default: name = "";
            break; //needed to end loop
    }
}


//---------------------------------------------------------------------------
//
//   Error Checking / Reporting macros used in all of the tests.
//
//---------------------------------------------------------------------------
#define DF_CHECK_STATUS {if (U_FAILURE(status)) \
    {dataerrln("DecimalFormatTest failure at line %d.  status=%s", \
    __LINE__, u_errorName(status)); return 0;}}

#define DF_ASSERT(expr) {if ((expr)==FALSE) {errln("DecimalFormatTest failure at line %d.\n", __LINE__);};}

#define DF_ASSERT_FAIL(expr, errcode) {UErrorCode status=U_ZERO_ERROR; (expr);\
if (status!=errcode) {dataerrln("DecimalFormatTest failure at line %d.  Expected status=%s, got %s", \
    __LINE__, u_errorName(errcode), u_errorName(status));};}

#define DF_CHECK_STATUS_L(line) {if (U_FAILURE(status)) {errln( \
    "DecimalFormatTest failure at line %d, from %d.  status=%d\n",__LINE__, (line), status); }}

#define DF_ASSERT_L(expr, line) {if ((expr)==FALSE) { \
    errln("DecimalFormatTest failure at line %d, from %d.", __LINE__, (line)); return;}}



//
//  InvariantStringPiece
//    Wrap a StringPiece around the extracted invariant data of a UnicodeString.
//    The data is guaranteed to be nul terminated.  (This is not true of StringPiece
//    in general, but is true of InvariantStringPiece)
//
class InvariantStringPiece: public StringPiece {
  public:
    InvariantStringPiece(const UnicodeString &s);
    ~InvariantStringPiece() {};
  private:
    MaybeStackArray<char, 20>  buf;
};

InvariantStringPiece::InvariantStringPiece(const UnicodeString &s) {
    int32_t  len = s.length();
    if (len+1 > buf.getCapacity()) {
        buf.resize(len+1);
    }
    // Buffer size is len+1 so that s.extract() will nul-terminate the string.
    s.extract(0, len, buf.getAlias(), len+1, US_INV);
    this->set(buf, len);
}


//  UnicodeStringPiece
//    Wrap a StringPiece around the extracted (to the default charset) data of
//    a UnicodeString.  The extracted data is guaranteed to be nul terminated.
//    (This is not true of StringPiece in general, but is true of UnicodeStringPiece)
//
class UnicodeStringPiece: public StringPiece {
  public:
    UnicodeStringPiece(const UnicodeString &s);
    ~UnicodeStringPiece() {};
  private:
    MaybeStackArray<char, 20>  buf;
};

UnicodeStringPiece::UnicodeStringPiece(const UnicodeString &s) {
    int32_t  len = s.length();
    int32_t  capacity = buf.getCapacity();
    int32_t requiredCapacity = s.extract(0, len, buf.getAlias(), capacity) + 1;
    if (capacity < requiredCapacity) {
        buf.resize(requiredCapacity);
        capacity = requiredCapacity;
        s.extract(0, len, buf.getAlias(), capacity);
    }
    this->set(buf, requiredCapacity - 1);
}



//---------------------------------------------------------------------------
//
//      DataDrivenTests  
//             The test cases are in a separate data file,
//
//---------------------------------------------------------------------------

// Translate a Formattable::type enum value to a string, for error message formatting.
static const char *formattableType(Formattable::Type typ) {
    static const char *types[] = {"kDate",
                                  "kDouble",
                                  "kLong",
                                  "kString",
                                  "kArray",
                                  "kInt64",
                                  "kObject"
                                  };
    if (typ<0 || typ>Formattable::kObject) {
        return "Unknown";
    }
    return types[typ];
}

const char *
DecimalFormatTest::getPath(char *buffer, const char *filename) {
    UErrorCode status=U_ZERO_ERROR;
    const char *testDataDirectory = IntlTest::getSourceTestData(status);
    DF_CHECK_STATUS;

    strcpy(buffer, testDataDirectory);
    strcat(buffer, filename);
    return buffer;
}

void DecimalFormatTest::DataDrivenTests() {
    char tdd[2048];
    const char *srcPath;
    UErrorCode  status  = U_ZERO_ERROR;
    int32_t     lineNum = 0;

    //
    //  Open and read the test data file.
    //
    srcPath=getPath(tdd, "dcfmtest.txt");
    if(srcPath==NULL) {
        return; /* something went wrong, error already output */
    }

    int32_t    len;
    UChar *testData = ReadAndConvertFile(srcPath, len, status);
    if (U_FAILURE(status)) {
        return; /* something went wrong, error already output */
    }

    //
    //  Put the test data into a UnicodeString
    //
    UnicodeString testString(FALSE, testData, len);

    RegexMatcher    parseLineMat(UNICODE_STRING_SIMPLE(
            "(?i)\\s*parse\\s+"
            "([a-z_]+)\\s+"              // Capture group 1: Locale
            "\"([^\"]*)\"\\s+"              // Capture group 2: input
            "\"([^\"]*)\"\\s+"          // Capture group 3: expected parsed decimal
            "([ild])"                    // Capture group 4: expected parsed type
            "\\s*(?:#.*)?"),             // Trailing comment
         0, status);

    RegexMatcher    formatLineMat(UNICODE_STRING_SIMPLE(
            "(?i)\\s*format\\s+"
            "([a-z_]+)\\s+"              // Capture group 1: Locale
            "\"([^\"]*)\"\\s+"           // Capture group 2: input
            "\"([^\"]*)\"\\s+"           // Capture group 3: pattern
            "\"([^\"]*)\"\\s+"           // Capture group 4: expected output
            "\\s*(?:#.*)?"),             // Trailing comment
         0, status);

    RegexMatcher    commentMat    (UNICODE_STRING_SIMPLE("\\s*(#.*)?$"), 0, status);
    RegexMatcher    lineMat(UNICODE_STRING_SIMPLE("(?m)^(.*?)$"), testString, 0, status);

    if (U_FAILURE(status)){
        dataerrln("Construct RegexMatcher() error.");
        delete [] testData;
        return;
    }

    //
    //  Loop over the test data file, once per line.
    //
    while (lineMat.find()) {
        lineNum++;
        if (U_FAILURE(status)) {
            errln("File dcfmtest.txt, line %d: ICU Error \"%s\"", lineNum, u_errorName(status));
        }

        status = U_ZERO_ERROR;
        UnicodeString testLine = lineMat.group(1, status);
        // std::string s;
        // std::cout << lineNum << ": " << testLine.toUTF8String(s) << std::endl;
        printf("%s\n", UnicodeStringPiece(testLine).data());
        if (testLine.length() == 0) {
            continue;
        }

        //
        // Parse the test line.  Skip blank and comment only lines.
        // Separate out the three main fields - pattern, flags, target.
        //

        commentMat.reset(testLine);
        if (commentMat.lookingAt(status)) {
            // This line is a comment, or blank.
            continue;
        }


        //
        //  Handle "parse" test case line from file
        //
        parseLineMat.reset(testLine);
        if (parseLineMat.lookingAt(status)) {
            execParseTest(lineNum,
                          parseLineMat.group(1, status),    // locale
                          parseLineMat.group(2, status),    // input
                          parseLineMat.group(3, status),    // Expected Decimal String
                          parseLineMat.group(4, status),    // Expected Type
                          status
                          );
            continue;
        }

        //
        //  Handle "format" test case line
        //
        formatLineMat.reset(testLine);
        if (formatLineMat.lookingAt(status)) {
            // TODO:
            continue;
        }

        //
        //  Line is not a recognizable test case.
        //
        errln("Badly formed test case at line %d.", lineNum);

    }

    delete [] testData;
}



void DecimalFormatTest::execParseTest(int32_t lineNum,
                                     const UnicodeString &locale,
                                     const UnicodeString &inputText,
                                     const UnicodeString &expectedDecimal,
                                     const UnicodeString &expectedType,
                                     UErrorCode &status) {
    
    if (U_FAILURE(status)) {
        return;
    }
    Locale loc = Locale::createCanonical(InvariantStringPiece(locale).data());

    DecimalFormatSymbols symbols(loc, status);
    UnicodeString pattern = UNICODE_STRING_SIMPLE("####");
    DecimalFormat format(pattern, symbols, status);
    Formattable   result;
    if (U_FAILURE(status)) {
        errln("file dcfmtest.txt, line %d: %s error creating the formatter.",
            lineNum, u_errorName(status));
        return;
    }

    ParsePosition pos;
    int32_t expectedParseEndPosition = inputText.length();

    return;   // TODO remove
    format.parse(inputText, result, pos);

    if (expectedParseEndPosition != pos.getIndex()) {
        errln("file dcfmtest.txt, line %d: Expected parse position afeter parsing: %d.  "
              "Actual parse position: %d", expectedParseEndPosition, pos.getIndex());
        return;
    }

    char   expectedTypeC[2];
    expectedType.extract(0, 1, expectedTypeC, 2, US_INV);
    Formattable::Type expectType = Formattable::kDate;
    switch (expectedTypeC[0]) {
      case 'd': expectType = Formattable::kDouble; break;
      case 'i': expectType = Formattable::kLong;   break;
      case 'l': expectType = Formattable::kInt64;  break;
      default:
          errln("file dcfmtest.tx, line %d: unrecongized expected type \"%s\"",
              lineNum, InvariantStringPiece(expectedType).data());
          return;
    }
    if (result.getType() != expectType) {
        errln("file dcfmtest.txt, line %d: expectedParseType(%s) != actual parseType(%s)",
             lineNum, formattableType(expectType), formattableType(result.getType()));
        return;
    }

    StringPiece decimalResult = result.getDecimalNumber(status);
    if (U_FAILURE(status)) {
        errln("File %s, line %d: error %s.  Line in file dcfmtest.txt:  %d:",
            __FILE__, __LINE__, u_errorName(status), lineNum);
        return;
    }

    InvariantStringPiece expectedResults(expectedDecimal);
    if (decimalResult != expectedResults) {
        errln("file dcfmtest.txt, line %d: expected \"%s\", got \"%s\"",
            lineNum, expectedResults.data(), decimalResult.data());
    }
    
    return;
}


//-------------------------------------------------------------------------------
//      
//  Read a text data file, convert it from UTF-8 to UChars, and return the data
//    in one big UChar * buffer, which the caller must delete.
//
//    (Lightly modified version of a similar function in regextst.cpp)
//
//--------------------------------------------------------------------------------
UChar *DecimalFormatTest::ReadAndConvertFile(const char *fileName, int32_t &ulen,
                                     UErrorCode &status) {
    UChar       *retPtr  = NULL;
    char        *fileBuf = NULL;
    const char  *fileBufNoBOM = NULL;
    FILE        *f       = NULL;

    ulen = 0;
    if (U_FAILURE(status)) {
        return retPtr;
    }

    //
    //  Open the file.
    //
    f = fopen(fileName, "rb");
    if (f == 0) {
        dataerrln("Error opening test data file %s\n", fileName);
        status = U_FILE_ACCESS_ERROR;
        return NULL;
    }
    //
    //  Read it in
    //
    int32_t            fileSize;
    int32_t            amtRead;
    int32_t            amtReadNoBOM;

    fseek( f, 0, SEEK_END);
    fileSize = ftell(f);
    fileBuf = new char[fileSize];
    fseek(f, 0, SEEK_SET);
    amtRead = fread(fileBuf, 1, fileSize, f);
    if (amtRead != fileSize || fileSize <= 0) {
        errln("Error reading test data file.");
        goto cleanUpAndReturn;
    }

    //
    // Look for a UTF-8 BOM on the data just read.
    //    The test data file is UTF-8.
    //    The BOM needs to be there in the source file to keep the Windows & 
    //    EBCDIC machines happy, so force an error if it goes missing.  
    //    Many Linux editors will silently strip it.
    //
    fileBufNoBOM = fileBuf + 3;
    amtReadNoBOM = amtRead - 3;
    if (fileSize<3 || uprv_strncmp(fileBuf, "\xEF\xBB\xBF", 3) != 0) {
        errln("Test data file %s is missing its BOM", fileName);
        fileBufNoBOM = fileBuf;
        amtReadNoBOM = amtRead;
    }

    //
    // Find the length of the input in UTF-16 UChars
    //  (by preflighting the conversion)
    //
    u_strFromUTF8(NULL, 0, &ulen, fileBufNoBOM, amtReadNoBOM, &status);

    //
    // Convert file contents from UTF-8 to UTF-16
    //
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        // Buffer Overflow is expected from the preflight operation.
        status = U_ZERO_ERROR;
        retPtr = new UChar[ulen+1];
        u_strFromUTF8(retPtr, ulen+1, NULL, fileBufNoBOM, amtReadNoBOM, &status);
    }

cleanUpAndReturn:
    fclose(f);
    delete[] fileBuf;
    if (U_FAILURE(status)) {
        errln("ICU Error \"%s\"\n", u_errorName(status));
        delete retPtr;
        retPtr = NULL;
    };
    return retPtr;
}

#endif  /* !UCONFIG_NO_REGULAR_EXPRESSIONS  */

