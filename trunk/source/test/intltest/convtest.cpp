/*
*******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  convtest.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jul15
*   created by: Markus W. Scherer
*
*   Test file for data-driven conversion tests.
*/

#include "unicode/utypes.h"
#include "unicode/ucnv.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "unicode/ures.h"
#include "convtest.h"
#include "tstdtmod.h"
#include <string.h>
#include <stdlib.h>

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

enum {
    // characters used in test data for callbacks
    SUB_CB='?',
    SKIP_CB='0',
    STOP_CB='.',
    ESC_CB='&'
};

void
ConversionTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if (exec) logln("TestSuite StringCaseTest: ");
    switch (index) {
        case 0: name="TestToUnicode"; if (exec) TestToUnicode(); break;
        case 1: name="TestFromUnicode"; if (exec) TestFromUnicode(); break;
        default: name=""; break; //needed to end loop
    }
}

// test data interface ----------------------------------------------------- ***

void
ConversionTest::TestToUnicode() {
    ConversionCase cc;
    char charset[100], cbopt[4];
    const char *option;
    UnicodeString s, unicode;
    int32_t offsetsLength;
    UConverterToUCallback callback;

    TestLog testLog;
    TestDataModule *dataModule;
    TestData *testData;
    const DataMap *testCase;
    UErrorCode errorCode;
    int32_t i;

    errorCode=U_ZERO_ERROR;
    dataModule=TestDataModule::getTestDataModule("conversion", testLog, errorCode);
    if(U_SUCCESS(errorCode)) {
        testData=dataModule->createTestData("toUnicode", errorCode);
        if(U_SUCCESS(errorCode)) {
            for(i=0; testData->nextCase(testCase, errorCode); ++i) {
                cc.caseNr=i;

                s=testCase->getString("charset", errorCode);
                s.extract(0, 0x7fffffff, charset, sizeof(charset), "");
                cc.charset=charset;

                cc.bytes=testCase->getBinary(cc.bytesLength, "bytes", errorCode);
                unicode=testCase->getString("unicode", errorCode);
                cc.unicode=unicode.getBuffer();
                cc.unicodeLength=unicode.length();

                offsetsLength=0;
                cc.offsets=testCase->getIntVector(offsetsLength, "offsets", errorCode);
                if(offsetsLength==0) {
                    cc.offsets=NULL;
                } else if(offsetsLength!=unicode.length()) {
                    errln("toUnicode[%d] unicode[%d] and offsets[%d] must have the same length",
                            i, unicode.length(), offsetsLength);
                    errorCode=U_ILLEGAL_ARGUMENT_ERROR;
                }

                cc.finalFlush= 0!=testCase->getInt28("flush", errorCode);
                cc.fallbacks= 0!=testCase->getInt28("fallbacks", errorCode);

                s=testCase->getString("errorCode", errorCode);
                if(s==UNICODE_STRING("invalid", 7)) {
                    cc.outErrorCode=U_INVALID_CHAR_FOUND;
                } else if(s==UNICODE_STRING("illegal", 7)) {
                    cc.outErrorCode=U_ILLEGAL_CHAR_FOUND;
                } else if(s==UNICODE_STRING("truncated", 9)) {
                    cc.outErrorCode=U_TRUNCATED_CHAR_FOUND;
                } else {
                    cc.outErrorCode=U_ZERO_ERROR;
                }

                s=testCase->getString("callback", errorCode);
                s.extract(0, 0x7fffffff, cbopt, sizeof(cbopt), "");
                cc.cbopt=cbopt;
                switch(cbopt[0]) {
                case SUB_CB:
                    callback=UCNV_TO_U_CALLBACK_SUBSTITUTE;
                    break;
                case SKIP_CB:
                    callback=UCNV_TO_U_CALLBACK_SKIP;
                    break;
                case STOP_CB:
                    callback=UCNV_TO_U_CALLBACK_STOP;
                    break;
                case ESC_CB:
                    callback=UCNV_TO_U_CALLBACK_ESCAPE;
                    break;
                default:
                    callback=NULL;
                    break;
                }
                option=callback==NULL ? cbopt : cbopt+1;
                if(*option==0) {
                    option=NULL;
                }

                cc.invalidChars=testCase->getBinary(cc.invalidLength, "invalidChars", errorCode);

                if(U_FAILURE(errorCode)) {
                    errln("error parsing conversion/toUnicode test case %d - %s",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                } else {
                    ToUnicodeCase(cc, callback, option);
                }
            }
            delete testData;
        }
        delete dataModule;
    }
}

void
ConversionTest::TestFromUnicode() {
    ConversionCase cc;
    char charset[100], cbopt[4];
    const char *option;
    UnicodeString s, unicode, invalidUChars;
    int32_t offsetsLength;
    UConverterFromUCallback callback;

    TestLog testLog;
    TestDataModule *dataModule;
    TestData *testData;
    const DataMap *testCase;
    const UChar *p;
    UErrorCode errorCode;
    int32_t i, length;

    errorCode=U_ZERO_ERROR;
    dataModule=TestDataModule::getTestDataModule("conversion", testLog, errorCode);
    if(U_SUCCESS(errorCode)) {
        testData=dataModule->createTestData("fromUnicode", errorCode);
        if(U_SUCCESS(errorCode)) {
            for(i=0; testData->nextCase(testCase, errorCode); ++i) {
                cc.caseNr=i;

                s=testCase->getString("charset", errorCode);
                s.extract(0, 0x7fffffff, charset, sizeof(charset), "");
                cc.charset=charset;

                unicode=testCase->getString("unicode", errorCode);
                cc.unicode=unicode.getBuffer();
                cc.unicodeLength=unicode.length();
                cc.bytes=testCase->getBinary(cc.bytesLength, "bytes", errorCode);

                offsetsLength=0;
                cc.offsets=testCase->getIntVector(offsetsLength, "offsets", errorCode);
                if(offsetsLength==0) {
                    cc.offsets=NULL;
                } else if(offsetsLength!=cc.bytesLength) {
                    errln("fromUnicode[%d] bytes[%d] and offsets[%d] must have the same length",
                            i, cc.bytesLength, offsetsLength);
                    errorCode=U_ILLEGAL_ARGUMENT_ERROR;
                }

                cc.finalFlush= 0!=testCase->getInt28("flush", errorCode);
                cc.fallbacks= 0!=testCase->getInt28("fallbacks", errorCode);

                s=testCase->getString("errorCode", errorCode);
                if(s==UNICODE_STRING("invalid", 7)) {
                    cc.outErrorCode=U_INVALID_CHAR_FOUND;
                } else if(s==UNICODE_STRING("illegal", 7)) {
                    cc.outErrorCode=U_ILLEGAL_CHAR_FOUND;
                } else if(s==UNICODE_STRING("truncated", 9)) {
                    cc.outErrorCode=U_TRUNCATED_CHAR_FOUND;
                } else {
                    cc.outErrorCode=U_ZERO_ERROR;
                }

                s=testCase->getString("callback", errorCode);

                // read NUL-separated subchar first, if any
                length=u_strlen(p=s.getTerminatedBuffer());
                if(++length<s.length()) {
                    // copy the subchar from Latin-1 characters
                    // start after the NUL
                    p+=length;
                    length=s.length()-length;
                    if(length>=sizeof(cc.subchar)) {
                        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
                    } else {
                        for(i=0; i<length; ++i) {
                            cc.subchar[i]=(char)p[i];
                        }
                        // NUL-terminate the subchar
                        cc.subchar[i]=0;
                    }

                    // remove the NUL and subchar from s
                    s.truncate(u_strlen(s.getBuffer()));
                } else {
                    // no subchar
                    cc.subchar[0]=0;
                }

                s.extract(0, 0x7fffffff, cbopt, sizeof(cbopt), "");
                cc.cbopt=cbopt;
                switch(cbopt[0]) {
                case SUB_CB:
                    callback=UCNV_FROM_U_CALLBACK_SUBSTITUTE;
                    break;
                case SKIP_CB:
                    callback=UCNV_FROM_U_CALLBACK_SKIP;
                    break;
                case STOP_CB:
                    callback=UCNV_FROM_U_CALLBACK_STOP;
                    break;
                case ESC_CB:
                    callback=UCNV_FROM_U_CALLBACK_ESCAPE;
                    break;
                default:
                    callback=NULL;
                    break;
                }
                option=callback==NULL ? cbopt : cbopt+1;
                if(*option==0) {
                    option=NULL;
                }

                invalidUChars=testCase->getString("invalidUChars", errorCode);
                cc.invalidUChars=invalidUChars.getBuffer();
                cc.invalidLength=invalidUChars.length();

                if(U_FAILURE(errorCode)) {
                    errln("error parsing conversion/fromUnicode test case %d - %s",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                } else {
                    FromUnicodeCase(cc, callback, option);
                }
            }
            delete testData;
        }
        delete dataModule;
    }
}

// output helpers ---------------------------------------------------------- ***

static inline char
hexDigit(uint8_t digit) {
    return digit<=9 ? (char)('0'+digit) : (char)('a'-10+digit);
}

static char *
printBytes(const uint8_t *bytes, int32_t length, char *out) {
    uint8_t b;

    if(length>0) {
        b=*bytes++;
        --length;
        *out++=hexDigit((uint8_t)(b>>4));
        *out++=hexDigit((uint8_t)(b&0xf));
    }

    while(length>0) {
        b=*bytes++;
        --length;
        *out++=' ';
        *out++=hexDigit((uint8_t)(b>>4));
        *out++=hexDigit((uint8_t)(b&0xf));
    }
    *out++=0;
    return out;
}

static char *
printUnicode(const UChar *unicode, int32_t length, char *out) {
    UChar32 c;
    int32_t i;

    for(i=0; i<length;) {
        if(i>0) {
            *out++=' ';
        }
        U16_NEXT(unicode, i, length, c);
        // write 4..6 digits
        if(c>=0x100000) {
            *out++='1';
        }
        if(c>=0x10000) {
            *out++=hexDigit((uint8_t)((c>>16)&0xf));
        }
        *out++=hexDigit((uint8_t)((c>>12)&0xf));
        *out++=hexDigit((uint8_t)((c>>8)&0xf));
        *out++=hexDigit((uint8_t)((c>>4)&0xf));
        *out++=hexDigit((uint8_t)(c&0xf));
    }
    *out++=0;
    return out;
}

static char *
printOffsets(const int32_t *offsets, int32_t length, char *out) {
    int32_t i, o, d;

    if(offsets==NULL) {
        length=0;
    }

    for(i=0; i<length; ++i) {
        if(i>0) {
            *out++=' ';
        }
        o=offsets[i];

        // print all offsets with 2 characters each (-x, -9..99, xx)
        if(o<-9) {
            *out++='-';
            *out++='x';
        } else if(o<0) {
            *out++='-';
            *out++=(char)('0'-o);
        } else if(o<=99) {
            *out++=(d=o/10)==0 ? ' ' : (char)('0'+d);
            *out++=(char)('0'+o%10);
        } else /* o>99 */ {
            *out++='x';
            *out++='x';
        }
    }
    *out++=0;
    return out;
}

// toUnicode test worker functions ----------------------------------------- ***

static int32_t
stepToUnicode(ConversionCase &cc, UConverter *cnv,
              UChar *result, int32_t resultCapacity,
              int32_t *resultOffsets, /* also resultCapacity */
              int32_t step,
              UErrorCode *pErrorCode) {
    const char *source, *sourceLimit, *bytesLimit;
    UChar *target, *targetLimit, *resultLimit;
    UBool flush;

    source=(const char *)cc.bytes;
    target=result;
    bytesLimit=source+cc.bytesLength;
    resultLimit=result+resultCapacity;

    if(step>=0) {
        // call ucnv_toUnicode() with in/out buffers no larger than (step) at a time
        // move only one buffer (in vs. out) at a time to be extra mean
        // step==0 performs bulk conversion and generates offsets

        // initialize the partial limits for the loop
        if(step==0) {
            // use the entire buffers
            sourceLimit=bytesLimit;
            targetLimit=resultLimit;
            flush=cc.finalFlush;
        } else {
            // start with empty partial buffers
            sourceLimit=source;
            targetLimit=target;
            flush=FALSE;

            // output offsets only for bulk conversion
            resultOffsets=NULL;
        }

        for(;;) {
            // resetting the opposite conversion direction must not affect this one
            ucnv_resetFromUnicode(cnv);

            // convert
            ucnv_toUnicode(cnv,
                &target, targetLimit,
                &source, sourceLimit,
                resultOffsets,
                flush, pErrorCode);

            // check pointers and errors
            if(*pErrorCode==U_BUFFER_OVERFLOW_ERROR) {
                if(target!=targetLimit) {
                    // buffer overflow must only be set when the target is filled
                    *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                    break;
                } else if(targetLimit==resultLimit) {
                    // not just a partial overflow
                    break;
                }

                // the partial target is filled, set a new limit, reset the error and continue
                targetLimit=(resultLimit-target)>=step ? target+step : resultLimit;
                *pErrorCode=U_ZERO_ERROR;
            } else if(U_FAILURE(*pErrorCode)) {
                // some other error occurred, done
                break;
            } else {
                if(source!=sourceLimit) {
                    // when no error occurs, then the input must be consumed
                    *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                    break;
                }

                if(sourceLimit==bytesLimit) {
                    // we are done
                    break;
                }

                // the partial conversion succeeded, set a new limit and continue
                sourceLimit=(bytesLimit-source)>=step ? source+step : bytesLimit;
                flush=(UBool)(cc.finalFlush && sourceLimit==bytesLimit);
            }
        }
    } else /* step<0 */ {
        // step==-1 or -2: call ucnv_toUnicode() and ucnv_getNextUChar() alternatingly
        // step==-3: call only ucnv_getNextUChar()
        UChar32 c;

        // end the loop by getting an index out of bounds error
        for(;;) {
            // resetting the opposite conversion direction must not affect this one
            ucnv_resetFromUnicode(cnv);

            // convert
            if((step&1)!=0 /* odd: -1 or -3 */) {
                sourceLimit=source; // use sourceLimit not as a real limit
                                    // but to remember the pre-getNextUChar source pointer
                c=ucnv_getNextUChar(cnv, &source, bytesLimit, pErrorCode);

                // check pointers and errors
                if(*pErrorCode==U_INDEX_OUTOFBOUNDS_ERROR) {
                    if(source!=bytesLimit) {
                        *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                    } else {
                        *pErrorCode=U_ZERO_ERROR;
                    }
                    break;
                } else if(U_FAILURE(*pErrorCode)) {
                    break;
                }
                // source may not move if c is from previous overflow

                if(target==resultLimit) {
                    *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
                    break;
                }
                if(c<=0xffff) {
                    *target++=(UChar)c;
                } else {
                    *target++=U16_LEAD(c);
                    if(target==resultLimit) {
                        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
                        break;
                    }
                    *target++=U16_TRAIL(c);
                }

                // alternate between -1 and -2 but leave -3 alone
                if(step==-1) {
                    step=-2;
                }
            } else /* step==-2 */ {
                // allow only one UChar output
                targetLimit=target<resultLimit ? target+1 : resultLimit;

                // as with ucnv_getNextUChar(), we always flush and never output offsets
                ucnv_toUnicode(cnv,
                    &target, targetLimit,
                    &source, bytesLimit,
                    NULL, TRUE, pErrorCode);

                // check pointers and errors
                if(*pErrorCode==U_BUFFER_OVERFLOW_ERROR) {
                    if(target!=targetLimit) {
                        // buffer overflow must only be set when the target is filled
                        *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                        break;
                    } else if(targetLimit==resultLimit) {
                        // not just a partial overflow
                        break;
                    }

                    // the partial target is filled, set a new limit and continue
                    *pErrorCode=U_ZERO_ERROR;
                } else if(U_FAILURE(*pErrorCode)) {
                    // some other error occurred, done
                    break;
                } else {
                    if(source!=bytesLimit) {
                        // when no error occurs, then the input must be consumed
                        *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                        break;
                    }

                    // we are done (flush==TRUE) but we continue, to get the index out of bounds error above
                }

                step=-1;
            }
        }
    }

    return (int32_t)(target-result);
}

UBool
ConversionTest::ToUnicodeCase(ConversionCase &cc, UConverterToUCallback callback, const char *option) {
    UConverter *cnv;
    UErrorCode errorCode;

    // open the converter
    errorCode=U_ZERO_ERROR;
    cnv=ucnv_open(cc.charset, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("toUnicode[%d](%s cb=\"%s\" fb=%d flush=%d) ucnv_open() failed - %s",
                cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, u_errorName(errorCode));
        return FALSE;
    }

    // set the callback
    if(callback!=NULL) {
        ucnv_setToUCallBack(cnv, callback, option, NULL, NULL, &errorCode);
        if(U_FAILURE(errorCode)) {
            errln("toUnicode[%d](%s cb=\"%s\" fb=%d flush=%d) ucnv_setToUCallBack() failed - %s",
                    cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, u_errorName(errorCode));
            ucnv_close(cnv);
            return FALSE;
        }
    }

    int32_t resultOffsets[200];
    UChar result[200];
    int32_t resultLength;

    static const struct {
        int32_t step;
        const char *name;
    } steps[]={
        { 0, "bulk" }, // must be first for offsets to be checked
        { 1, "step=1" },
        { 3, "step=3" },
        { 7, "step=7" },
        { -3, "getNext" },
        { -1, "getNext+toU" },
        { -2, "toU+getNext" }
    };
    int32_t i, step;

    for(i=0; i<LENGTHOF(steps); ++i) {
        step=steps[i].step;
        if(step!=0) {
            // bulk test is first, then offsets are not checked any more
            cc.offsets=NULL;
        }
        errorCode=U_ZERO_ERROR;
        resultLength=stepToUnicode(cc, cnv,
                                result, LENGTHOF(result),
                                step==0 ? resultOffsets : NULL,
                                step, &errorCode);
        if(!checkToUnicode(
                cc, cnv, steps[i].name,
                result, resultLength,
                cc.offsets!=NULL ? resultOffsets : NULL,
                errorCode)
        ) {
            return FALSE;
        }
        ucnv_resetToUnicode(cnv);
    }

    ucnv_close(cnv);
    return TRUE;
}

UBool
ConversionTest::checkToUnicode(ConversionCase &cc, UConverter *cnv, const char *name,
                               const UChar *result, int32_t resultLength,
                               const int32_t *resultOffsets,
                               UErrorCode resultErrorCode) {
    char resultInvalidChars[8];
    int8_t resultInvalidLength;
    UErrorCode errorCode;

    const char *msg;

    // reset the message; NULL will mean "ok"
    msg=NULL;

    errorCode=U_ZERO_ERROR;
    resultInvalidLength=sizeof(resultInvalidChars);
    ucnv_getInvalidChars(cnv, resultInvalidChars, &resultInvalidLength, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("toUnicode[%d](%s cb=\"%s\" fb=%d flush=%d %s) ucnv_getInvalidChars() failed - %s",
                cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, name, u_errorName(errorCode));
        return FALSE;
    }

    // check everything that might have gone wrong
    if(cc.unicodeLength!=resultLength) {
        msg="wrong result length";
    } else if(0!=u_memcmp(cc.unicode, result, cc.unicodeLength)) {
        msg="wrong result string";
    } else if(cc.offsets!=NULL && 0!=memcmp(cc.offsets, resultOffsets, cc.unicodeLength*sizeof(*cc.offsets))) {
        msg="wrong offsets";
    } else if(cc.outErrorCode!=resultErrorCode) {
        msg="wrong error code";
    } else if(cc.invalidLength!=resultInvalidLength) {
        msg="wrong length of last invalid input";
    } else if(0!=memcmp(cc.invalidChars, resultInvalidChars, cc.invalidLength)) {
        msg="wrong last invalid input";
    }

    if(msg==NULL) {
        return TRUE;
    } else {
        char buffer[2000]; // one buffer for all strings
        char *s, *bytesString, *unicodeString, *resultString,
            *offsetsString, *resultOffsetsString,
            *invalidCharsString, *resultInvalidCharsString;

        bytesString=s=buffer;
        s=printBytes(cc.bytes, cc.bytesLength, bytesString);
        s=printUnicode(cc.unicode, cc.unicodeLength, unicodeString=s);
        s=printUnicode(result, resultLength, resultString=s);
        s=printOffsets(cc.offsets, cc.unicodeLength, offsetsString=s);
        s=printOffsets(resultOffsets, resultLength, resultOffsetsString=s);
        s=printBytes(cc.invalidChars, cc.invalidLength, invalidCharsString=s);
        s=printBytes((uint8_t *)resultInvalidChars, resultInvalidLength, resultInvalidCharsString=s);

        if((s-buffer)>sizeof(buffer)) {
            errln("toUnicode[%d](%s cb=\"%s\" fb=%d flush=%d %s) fatal error: checkToUnicode() test output buffer overflow writing %d chars\n",
                    cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, name, (int)(s-buffer));
            exit(1);
        }

        errln("toUnicode[%d](%s cb=\"%s\" fb=%d flush=%d %s) failed: %s\n"
              "  bytes <%s>[%d]\n"
              " expected <%s>[%d]\n"
              "  result  <%s>[%d]\n"
              " offsets         <%s>\n"
              "  result offsets <%s>\n"
              " error code expected %s got %s\n"
              "  invalidChars expected <%s> got <%s>\n",
              cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, name, msg,
              bytesString, cc.bytesLength,
              unicodeString, cc.unicodeLength,
              resultString, resultLength,
              offsetsString,
              resultOffsetsString,
              u_errorName(cc.outErrorCode), u_errorName(resultErrorCode),
              invalidCharsString, resultInvalidCharsString);

        return FALSE;
    }
}

// fromUnicode test worker functions --------------------------------------- ***

static int32_t
stepFromUnicode(ConversionCase &cc, UConverter *cnv,
                char *result, int32_t resultCapacity,
                int32_t *resultOffsets, /* also resultCapacity */
                int32_t step,
                UErrorCode *pErrorCode) {
    const UChar *source, *sourceLimit, *unicodeLimit;
    char *target, *targetLimit, *resultLimit;
    UBool flush;

    source=cc.unicode;
    target=result;
    unicodeLimit=source+cc.unicodeLength;
    resultLimit=result+resultCapacity;

    // call ucnv_fromUnicode() with in/out buffers no larger than (step) at a time
    // move only one buffer (in vs. out) at a time to be extra mean
    // step==0 performs bulk conversion and generates offsets

    // initialize the partial limits for the loop
    if(step==0) {
        // use the entire buffers
        sourceLimit=unicodeLimit;
        targetLimit=resultLimit;
        flush=cc.finalFlush;
    } else {
        // start with empty partial buffers
        sourceLimit=source;
        targetLimit=target;
        flush=FALSE;

        // output offsets only for bulk conversion
        resultOffsets=NULL;
    }

    for(;;) {
        // resetting the opposite conversion direction must not affect this one
        ucnv_resetToUnicode(cnv);

        // convert
        ucnv_fromUnicode(cnv,
            &target, targetLimit,
            &source, sourceLimit,
            resultOffsets,
            flush, pErrorCode);

        // check pointers and errors
        if(*pErrorCode==U_BUFFER_OVERFLOW_ERROR) {
            if(target!=targetLimit) {
                // buffer overflow must only be set when the target is filled
                *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                break;
            } else if(targetLimit==resultLimit) {
                // not just a partial overflow
                break;
            }

            // the partial target is filled, set a new limit, reset the error and continue
            targetLimit=(resultLimit-target)>=step ? target+step : resultLimit;
            *pErrorCode=U_ZERO_ERROR;
        } else if(U_FAILURE(*pErrorCode)) {
            // some other error occurred, done
            break;
        } else {
            if(source!=sourceLimit) {
                // when no error occurs, then the input must be consumed
                *pErrorCode=U_INTERNAL_PROGRAM_ERROR;
                break;
            }

            if(sourceLimit==unicodeLimit) {
                // we are done
                break;
            }

            // the partial conversion succeeded, set a new limit and continue
            sourceLimit=(unicodeLimit-source)>=step ? source+step : unicodeLimit;
            flush=(UBool)(cc.finalFlush && sourceLimit==unicodeLimit);
        }
    }

    return (int32_t)(target-result);
}

UBool
ConversionTest::FromUnicodeCase(ConversionCase &cc, UConverterFromUCallback callback, const char *option) {
    UConverter *cnv;
    UErrorCode errorCode;

    // open the converter
    errorCode=U_ZERO_ERROR;
    cnv=ucnv_open(cc.charset, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("fromUnicode[%d](%s cb=\"%s\" fb=%d flush=%d) ucnv_open() failed - %s",
                cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, u_errorName(errorCode));
        return FALSE;
    }

    // set the callback
    if(callback!=NULL) {
        ucnv_setFromUCallBack(cnv, callback, option, NULL, NULL, &errorCode);
        if(U_FAILURE(errorCode)) {
            errln("fromUnicode[%d](%s cb=\"%s\" fb=%d flush=%d) ucnv_setFromUCallBack() failed - %s",
                    cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, u_errorName(errorCode));
            ucnv_close(cnv);
            return FALSE;
        }
    }

    // set the fallbacks flag
    // TODO change with Jitterbug 2401, then add a similar call for toUnicode too
    ucnv_setFallback(cnv, cc.fallbacks);

    // set the subchar
    int32_t length;

    if((length=strlen(cc.subchar))!=0) {
        ucnv_setSubstChars(cnv, cc.subchar, (int8_t)length, &errorCode);
        if(U_FAILURE(errorCode)) {
            errln("fromUnicode[%d](%s cb=\"%s\" fb=%d flush=%d) ucnv_setSubChars() failed - %s",
                    cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, u_errorName(errorCode));
            ucnv_close(cnv);
            return FALSE;
        }
    }

    int32_t resultOffsets[200];
    char result[200];
    int32_t resultLength;

    static const struct {
        int32_t step;
        const char *name;
    } steps[]={
        { 0, "bulk" }, // must be first for offsets to be checked
        { 1, "step=1" },
        { 3, "step=3" },
        { 7, "step=7" }
    };
    int32_t i, step;

    for(i=0; i<LENGTHOF(steps); ++i) {
        step=steps[i].step;
        if(step!=0) {
            // bulk test is first, then offsets are not checked any more
            cc.offsets=NULL;
        }
        errorCode=U_ZERO_ERROR;
        resultLength=stepFromUnicode(cc, cnv,
                                result, LENGTHOF(result),
                                step==0 ? resultOffsets : NULL,
                                step, &errorCode);
        if(!checkFromUnicode(
                cc, cnv, steps[i].name,
                (uint8_t *)result, resultLength,
                cc.offsets!=NULL ? resultOffsets : NULL,
                errorCode)
        ) {
            return FALSE;
        }
        ucnv_resetFromUnicode(cnv);
    }

    ucnv_close(cnv);
    return TRUE;
}

UBool
ConversionTest::checkFromUnicode(ConversionCase &cc, UConverter *cnv, const char *name,
                                 const uint8_t *result, int32_t resultLength,
                                 const int32_t *resultOffsets,
                                 UErrorCode resultErrorCode) {
    UChar resultInvalidUChars[8];
    int8_t resultInvalidLength;
    UErrorCode errorCode;

    const char *msg;

    // reset the message; NULL will mean "ok"
    msg=NULL;

    errorCode=U_ZERO_ERROR;
    resultInvalidLength=LENGTHOF(resultInvalidUChars);
    ucnv_getInvalidUChars(cnv, resultInvalidUChars, &resultInvalidLength, &errorCode);
    if(U_FAILURE(errorCode)) {
        errln("fromUnicode[%d](%s cb=\"%s\" fb=%d flush=%d %s) ucnv_getInvalidUChars() failed - %s",
                cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, name, u_errorName(errorCode));
        return FALSE;
    }

    // check everything that might have gone wrong
    if(cc.bytesLength!=resultLength) {
        msg="wrong result length";
    } else if(0!=memcmp(cc.bytes, result, cc.bytesLength)) {
        msg="wrong result string";
    } else if(cc.offsets!=NULL && 0!=memcmp(cc.offsets, resultOffsets, cc.bytesLength*sizeof(*cc.offsets))) {
        msg="wrong offsets";
    } else if(cc.outErrorCode!=resultErrorCode) {
        msg="wrong error code";
    } else if(cc.invalidLength!=resultInvalidLength) {
        msg="wrong length of last invalid input";
    } else if(0!=u_memcmp(cc.invalidUChars, resultInvalidUChars, cc.invalidLength)) {
        msg="wrong last invalid input";
    }

    if(msg==NULL) {
        return TRUE;
    } else {
        char buffer[2000]; // one buffer for all strings
        char *s, *unicodeString, *bytesString, *resultString,
            *offsetsString, *resultOffsetsString,
            *invalidCharsString, *resultInvalidUCharsString;

        unicodeString=s=buffer;
        s=printUnicode(cc.unicode, cc.unicodeLength, unicodeString);
        s=printBytes(cc.bytes, cc.bytesLength, bytesString=s);
        s=printBytes(result, resultLength, resultString=s);
        s=printOffsets(cc.offsets, cc.bytesLength, offsetsString=s);
        s=printOffsets(resultOffsets, resultLength, resultOffsetsString=s);
        s=printUnicode(cc.invalidUChars, cc.invalidLength, invalidCharsString=s);
        s=printUnicode(resultInvalidUChars, resultInvalidLength, resultInvalidUCharsString=s);

        if((s-buffer)>sizeof(buffer)) {
            errln("fromUnicode[%d](%s cb=\"%s\" fb=%d flush=%d %s) fatal error: checkFromUnicode() test output buffer overflow writing %d chars\n",
                    cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, name, (int)(s-buffer));
            exit(1);
        }

        errln("fromUnicode[%d](%s cb=\"%s\" fb=%d flush=%d %s) failed: %s\n"
              "  unicode <%s>[%d]\n"
              " expected <%s>[%d]\n"
              "  result  <%s>[%d]\n"
              " offsets         <%s>\n"
              "  result offsets <%s>\n"
              " error code expected %s got %s\n"
              "  invalidChars expected <%s> got <%s>\n",
              cc.caseNr, cc.charset, cc.cbopt, cc.fallbacks, cc.finalFlush, name, msg,
              unicodeString, cc.unicodeLength,
              bytesString, cc.bytesLength,
              resultString, resultLength,
              offsetsString,
              resultOffsetsString,
              u_errorName(cc.outErrorCode), u_errorName(resultErrorCode),
              invalidCharsString, resultInvalidUCharsString);

        return FALSE;
    }
}
