/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  unistr_titlecase_brkiter.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:2
*
*   created on: 2011may30
*   created by: Markus W. Scherer
*
*   Titlecasing functions that are based on BreakIterator
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/brkiter.h"
#include "unicode/ubrk.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "ustr_imp.h"

// Duplicate of unistr_case.cpp/caseMap() except this version only handles titlecasing
// while the other one handles all other case mappings.
// Duplicated and separated for simpler dependencies.
UnicodeString &
UnicodeString::caseMap(BreakIterator *titleIter, const char *locale, uint32_t options) {
  if(isEmpty() || !isWritable()) {
    // nothing to do
    return *this;
  }

  const UCaseProps *csp=ucase_getSingleton();

  // We need to allocate a new buffer for the internal string case mapping function.
  // This is very similar to how doReplace() keeps the old array pointer
  // and deletes the old array itself after it is done.
  // In addition, we are forcing cloneArrayIfNeeded() to always allocate a new array.
  UChar oldStackBuffer[US_STACKBUF_SIZE];
  UChar *oldArray;
  int32_t oldLength;

  if(fFlags&kUsingStackBuffer) {
    // copy the stack buffer contents because it will be overwritten
    u_memcpy(oldStackBuffer, fUnion.fStackBuffer, fShortLength);
    oldArray = oldStackBuffer;
    oldLength = fShortLength;
  } else {
    oldArray = getArrayStart();
    oldLength = length();
  }

  int32_t capacity;
  if(oldLength <= US_STACKBUF_SIZE) {
    capacity = US_STACKBUF_SIZE;
  } else {
    capacity = oldLength + 20;
  }
  int32_t *bufferToDelete = 0;
  if(!cloneArrayIfNeeded(capacity, capacity, FALSE, &bufferToDelete, TRUE)) {
    return *this;
  }

  // Case-map, and if the result is too long, then reallocate and repeat.
  UErrorCode errorCode;
  int32_t newLength;
  do {
    errorCode = U_ZERO_ERROR;
    newLength = ustr_toTitle(csp, getArrayStart(), getCapacity(),
                             oldArray, oldLength,
                             (UBreakIterator *)titleIter, locale, options, &errorCode);
    setLength(newLength);
  } while(errorCode==U_BUFFER_OVERFLOW_ERROR && cloneArrayIfNeeded(newLength, newLength, FALSE));

  if (bufferToDelete) {
    uprv_free(bufferToDelete);
  }
  if(U_FAILURE(errorCode)) {
    setToBogus();
  }
  return *this;
}

UnicodeString &
UnicodeString::toTitle(BreakIterator *titleIter) {
  return caseMap(titleIter, Locale::getDefault().getName(), 0);
}

UnicodeString &
UnicodeString::toTitle(BreakIterator *titleIter, const Locale &locale) {
  return caseMap(titleIter, locale.getName(), 0);
}

UnicodeString &
UnicodeString::toTitle(BreakIterator *titleIter, const Locale &locale, uint32_t options) {
  return caseMap(titleIter, locale.getName(), options);
}

#endif  // !UCONFIG_NO_BREAK_ITERATION
