/*
*******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utrace.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003aug06
*   created by: Markus W. Scherer
*
*   Definitions for ICU tracing/logging.
*
*/

#ifndef __UTRACE_H__
#define __UTRACE_H__

#include <stdarg.h>
#include "unicode/utypes.h"

U_CDECL_BEGIN

/**
 * Trace severity levels.  Higher levels increase the verbosity of the trace output.
 * @see utrace_setLevel
 * @draft ICU 2.8
 */

enum UTraceLevel {
    UTRACE_OFF=-1,
    UTRACE_ERROR=0,
    UTRACE_WARNING=3,
    UTRACE_OPEN_CLOSE=5,
    UTRACE_INFO=7,
    UTRACE_VERBOSE=9
};
typedef enum UTraceLevel UTraceLevel;


/**
 * Setter for the trace level.
 * @param traceLevel A UTraceLevel value.
 * @draft ICU 2.8
 */
U_CAPI void U_EXPORT2
utrace_setLevel(int32_t traceLevel);

/**
 * Getter for the trace level.
 * @param traceLevel A UTraceLevel value.
 * @draft ICU 2.8
 */
U_CAPI int32_t U_EXPORT2
utrace_getLevel();

/* Trace function pointers types  ----------------------------- */

typedef void U_CALLCONV
UTraceEntry(const void *context, int32_t fnNumber);

typedef void U_CALLCONV
UTraceExit(const void *context, int32_t fnNumber, 
           int32_t retType, va_list args);

typedef void U_CALLCONV
UTraceData(const void *context, int32_t fnNumber, int32_t level,
           const char *fmt, va_list args);

/**
  *  Set ICU Tracing functions.  Installs application-provided tracing
  *  functions into ICU.  After doing this, subsequent ICU operations
  *  will call back to the installed functions, providing a trace
  *  of the use of ICU.
  *  @param context an uninterpretted pointer.  Whatever is passed in
  *                 here will in turn be passed to each of the tracing
  *                 functions UTraceEntry, UTraceExit and UTraceData.
  *                 ICU does not use or alter this pointer.
  *  @param e       Callback function to be called on entry to a 
  *                 a traced ICU function.
  *  @param x       Callback function to be called on exit from a
  *                 traced ICU function.
  *  @param d       Callback function to be called from within a 
  *                 traced ICU function, for the purpose of providing
  *                 data to the trace.
  *
  *  @draft ICU 2.8
  */
U_CAPI void U_EXPORT2
utrace_setFunctions(const void *context,
                    UTraceEntry *e, UTraceExit *x, UTraceData *d);

/**
  * Get the currently installed ICU tracing functions. 
  * @param context  The currently installed tracing context.
  * @param e        The currently installed UTraceEntry function.
  * @param x        The currently installed UTraceExit function.
  * @param d        The currently installed UTraceData function.
  * @draft ICU 2.8
  */
U_CAPI void U_EXPORT2
utrace_getFunctions(const void **context,
                    UTraceEntry **e, UTraceExit **x, UTraceData **d);



/*
 *
 * ICU trace format string syntax
 *
 * Format Strings are passed to UTraceData functions, and define the
 * number and types of the trace data being passed on each call.
 *
 * The UTraceData function, which is supplied by the application,
 * not by ICU, can either forward the trace data (passed via
 * varargs) and the format string back to ICU for formatting into
 * a displayable string, or it can interpret the format itself,
 * and do as it wishes with the trace data.
 *
 *
 * Goals for the format string
 * - basic data output
 * - easy to use for trace programmer
 * - sufficient provision for data types for trace output readability
 * - well-defined types and binary portable APIs
 *
 * Non-goals
 * - printf compatibility
 * - fancy formatting
 * - argument reordering and other internationalization features
 *
 * ICU trace format strings contain plain text with argument inserts,
 * much like standard printf format strings.
 * Each insert begins with a '%', then optionally contains a 'v',
 * then exactly one type character.
 * Two '%' in a row represent a '%' instead of an insert.
 * The trace format strings need not have \n at the end.
 *
 *
 * Types
 * -----
 *
 * Type characters:
 * - c A char character in the default codepage.
 * - s A NULL-terminated char * string in the default codepage.
 * - S A UChar * string.  Requires two params, (ptr, length).  Length=-1 for null term.
 * - b A byte (8-bit integer).
 * - h A 16-bit integer.  Also a 16 bit Unicode code unit.
 * - d A 32-bit integer.  Also a 20 bit Unicode code point value. 
 * - l A 64-bit integer.
 * - p A data pointer.
 *
 * Vectors
 * -------
 *
 * If the 'v' is not specified, then one item of the specified type
 * is passed in.
 * If the 'v' (for "vector") is specified, then a vector of items of the
 * specified type is passed in, via a pointer to the first item
 * and an int32_t value for the length of the vector.
 * Length==-1 means zero or NULL termination.  Works for vectors of all types.
 *
 * Note:  %vS is a vector of (UChar *) strings.  The strings must
 *        be null terminated as there is no way to provide a
 *        separate length parameter for each string.  The length
 *        parameter (required for all vectors) is the number of
 *        strings, not the length of the strings.
 *
 * Examples
 * --------
 *
 * These examples show the parameters that will be passed to an application's
 *   UTraceData() function for various formats.
 *
 * - the precise formatting is up to the application!
 * - the examples use type casts for arguments only to _show_ the types of
 *   arguments without needing variable declarations in the examples;
 *   the type casts will not be necessary in actual code
 *
 * UTraceDataFunc(context, fnNumber, level,
 *              "There is a character %c in the string %s.",   // Format String 
 *              (char)c, (const char *)s);                     // varargs parameters
 * ->   There is a character 0x42 'B' in the string "Bravo".
 *
 * UTraceDataFunc(context, fnNumber, level,
 *              "Vector of bytes %vb vector of chars %vc",
 *              (const uint8_t *)bytes, (int32_t)bytesLength,
 *              (const char *)chars, (int32_t)charsLength);
 * ->  Vector of bytes
 *      42 63 64 3f [4]
 *     vector of chars
 *      "Bcd?"[4]
 *
 * UTraceDataFunc(context, fnNumber, level,
 *              "An int32_t %d and a whole bunch of them %vd",
 *              (int32_t)-5, (const int32_t *)ints, (int32_t)intsLength);
 * ->   An int32_t 0xfffffffb and a whole bunch of them
 *      fffffffb 00000005 0000010a [3]
 *
 */



/**
  *  Trace output Formatter.  An application's UTraceData tracing functions may call
  *                 back to this function to format the trace output in a
  *                 human readable form.  Note that a UTraceData function is not
  *                 required to format the data;  it could, for example, save it in
  *                 in the raw form it was received (more compact), leaving
  *                 formatting for a later trace analyis tool.
  *  @param outBuf  pointer to a buffer to receive the formatted output.  Output
  *                 will be null terminated if there is space in the buffer -
  *                 if the length of the requested output < the output buffer size.
  *  @param capacity  Length of the output buffer.
  *  @param indent  Number of spaces to indent the output.  Intended to allow
  *                 data displayed from nested functions to be indented for readability.
  *  @param fmt     Format specification for the data to output
  *  @param args    Data to be formatted.
  *  @return        Length of formatted output, including the terminating NULL.
  *                 If buffer capacity is insufficient, the required capacity is returned. 
  *  @draft ICU 2.8
  */
U_CAPI int32_t U_EXPORT2
utrace_format(char *outBuf, int32_t capacity,
              int32_t indent, const char *fmt,  va_list args);


/** 
 *   Traced Function Exit return types.  
 *   Flags indicating the number and types of varargs included in a call
 *   to a UTraceExit function.
 *   Bits 0-3:  The function return type.  First variable param.
 *   Bit    4:  Flag for presence of U_ErrorCode status param.
 *   @draft ICU 2.8
 */
enum UTraceExitVal {
    UTRACE_EXITV_NONE   = 0,
    UTRACE_EXITV_I32    = 1,
    UTRACE_EXITV_PTR    = 2,
    UTRACE_EXITV_BOOL   = 3,
    UTRACE_EXITV_MASK   = 0xf,
    UTRACE_EXITV_STATUS = 0x10
};
typedef enum UTraceExitVal UTraceExitVal;


/**
  *  Trace formatter for UTraceExit, function exit tracing.
  *  UTraceExit may optionally receive two data items:  a function return value
  *  and a UErrorCode status value.
  *
  *  @param outBuf   pointer to a buffer to receive the formatted output.Output
  *                 will be null terminated if there is space in the buffer -
  *                 if the length of the requested output < the output buffer size.
  *  @param capacity  Length of the output buffer.
  *  @param indent  Number of spaces to indent the output.  Intended to allow
  *                 data displayed from nested functions to be indented for readability.
  *  @param fnNumber  An index specifying the function that is exiting.
  *  @param argType Flags specifying the number and types of data values.
  *  @param args    Data to be formatted.
  *  @return        Length of formatted output, including the terminating NULL.
  *                 If buffer capacity is insufficient, the required capacity is returned. 
  *  @draft ICU 2.8
  */

U_CAPI void U_EXPORT2
utrace_formatExit(char *outBuf, int32_t capacity, int32_t indent, 
                                  int32_t fnNumber, int32_t argtype, va_list args);

/* Trace function numbers --------------------------------------------------- */

/**
 * Get the name of a function from its trace function number.
 *
 * @param fnNumber The trace number for an ICU function.
 * @return The name string for the function.
 *
 * @see UTraceFunctionNumber
 * @draft ICU 2.8
 */
U_CAPI const char * U_EXPORT2
utrace_functionName(int32_t fnNumber);

/**
 *  These are the ICU functions that will be traced when tracing is enabled.
 */
enum UTraceFunctionNumber {
    UTRACE_FUNCTION_START=0,
    UTRACE_U_INIT=UTRACE_FUNCTION_START,
    UTRACE_U_CLEANUP,
    UTRACE_FUNCTION_LIMIT,

    UTRACE_CONVERSION_START=0x1000,
    UTRACE_UCNV_OPEN=UTRACE_CONVERSION_START,
    UTRACE_UCNV_CLOSE,
    UTRACE_UCNV_FLUSH_CACHE,
    UTRACE_CONVERSION_LIMIT,

    UTRACE_COLLATION_START=0x2000,
    UTRACE_UCOL_OPEN=UTRACE_COLLATION_START,
    UTRACE_UCOL_CLOSE,
    UTRACE_UCOL_STRCOLL,
    UTRACE_UCOL_GET_SORTKEY,
    UTRACE_UCOL_GETLOCALE,
    UTRACE_UCOL_NEXTSORTKEYPART,
    UTRACE_UCOL_STRCOLLITER,
    UTRACE_COLLATION_LIMIT
};
typedef enum UTraceFunctionNumber UTraceFunctionNumber;

U_CDECL_END

#endif
