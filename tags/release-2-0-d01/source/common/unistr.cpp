/*
******************************************************************************
* Copyright (C) 1999-2001, International Business Machines Corporation and   *
* others. All Rights Reserved.                                               *
******************************************************************************
*
* File unistr.cpp
*
* Modification History:
*
*   Date        Name        Description
*   09/25/98    stephen     Creation.
*   04/20/99    stephen     Overhauled per 4/16 code review.
*   07/09/99    stephen     Renamed {hi,lo},{byte,word} to icu_X for HP/UX
*   11/18/99    aliu        Added handleReplaceBetween() to make inherit from
*                           Replaceable.
*   06/25/01    grhoten     Removed the dependency on iostream
******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/locid.h"
#include "cstring.h"
#include "cmemory.h"
#include "unicode/ustring.h"
#include "mutex.h"
#include "unicode/unistr.h"
#include "unicode/unicode.h"
#include "unicode/ucnv.h"
#include "uhash.h"
#include "ustr_imp.h"

#if 0

#if U_IOSTREAM_SOURCE >= 199711
#include <iostream>
using namespace std;
#elif U_IOSTREAM_SOURCE >= 198506
#include <iostream.h>
#endif

//DEBUGGING
void
print(const UnicodeString& s,
      const char *name)
{
  UChar c;
  cout << name << ":|";
  for(int i = 0; i < s.length(); ++i) {
    c = s[i];
    if(c>= 0x007E || c < 0x0020)
      cout << "[0x" << hex << s[i] << "]";
    else
      cout << (char) s[i];
  }
  cout << '|' << endl;
}

void
print(const UChar *s,
      int32_t len,
      const char *name)
{
  UChar c;
  cout << name << ":|";
  for(int i = 0; i < len; ++i) {
    c = s[i];
    if(c>= 0x007E || c < 0x0020)
      cout << "[0x" << hex << s[i] << "]";
    else
      cout << (char) s[i];
  }
  cout << '|' << endl;
}
// END DEBUGGING
#endif

// Local function definitions for now

// need to copy areas that may overlap
static
inline void
us_arrayCopy(const UChar *src, int32_t srcStart,
         UChar *dst, int32_t dstStart, int32_t count)
{
  if(count>0) {
    uprv_memmove(dst+dstStart, src+srcStart, (size_t)(count*sizeof(*src)));
  }
}

U_NAMESPACE_BEGIN

//========================================
// Constructors
//========================================
UnicodeString::UnicodeString()
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{}

UnicodeString::UnicodeString(int32_t capacity, UChar32 c, int32_t count)
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(0),
    fFlags(0)
{
  if(count <= 0) {
    // just allocate and do not do anything else
    allocate(capacity);
  } else {
    // count > 0, allocate and fill the new string with count c's
    int32_t unitCount = UTF_CHAR_LENGTH(c), length = count * unitCount;
    if(capacity < length) {
      capacity = length;
    }
    if(allocate(capacity)) {
      int32_t i = 0;

      // fill the new string with c
      if(unitCount == 1) {
        // fill with length UChars
        while(i < length) {
          fArray[i++] = (UChar)c;
        }
      } else {
        // get the code units for c
        UChar units[UTF_MAX_CHAR_LENGTH];
        UTF_APPEND_CHAR_UNSAFE(units, i, c);

        // now it must be i==unitCount
        i = 0;

        // for Unicode, unitCount can only be 1, 2, 3, or 4
        // 1 is handled above
        while(i < length) {
          int32_t unitIdx = 0;
          while(unitIdx < unitCount) {
            fArray[i++]=units[unitIdx++];
          }
        }
      }
    }
    fLength = length;
  }
}

UnicodeString::UnicodeString(UChar ch)
  : fLength(1),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  fStackBuffer[0] = ch;
}

UnicodeString::UnicodeString(UChar32 ch)
  : fLength(1),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  UTextOffset i = 0;
  UTF_APPEND_CHAR(fStackBuffer, i, US_STACKBUF_SIZE, ch);
  fLength = i;
}

UnicodeString::UnicodeString(const UChar *text)
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  doReplace(0, 0, text, 0, u_strlen(text));
}

UnicodeString::UnicodeString(const UChar *text,
                             int32_t textLength)
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  doReplace(0, 0, text, 0, textLength);
}

UnicodeString::UnicodeString(UBool isTerminated,
                             const UChar *text,
                             int32_t textLength)
  : fLength(textLength),
    fCapacity(isTerminated ? textLength + 1 : textLength),
    fArray((UChar *)text),
    fFlags(kReadonlyAlias)
{
  if(text == 0 || textLength < -1 || textLength == -1 && !isTerminated) {
    setToBogus();
  } else if(textLength == -1) {
    // text is terminated, or else it would have failed the above test
    fLength = u_strlen(text);
    fCapacity = fLength + 1;
  }
}

UnicodeString::UnicodeString(UChar *buff,
                             int32_t bufLength,
                             int32_t buffCapacity)
  : fLength(bufLength),
    fCapacity(buffCapacity),
    fArray(buff),
    fFlags(kWritableAlias)
{
  if(buff == 0 || bufLength < 0 || bufLength > buffCapacity) {
    setToBogus();
  }
}

UnicodeString::UnicodeString(const char *codepageData,
                             const char *codepage)
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  if(codepageData != 0) {
    doCodepageCreate(codepageData, (int32_t)uprv_strlen(codepageData), codepage);
  }
}


UnicodeString::UnicodeString(const char *codepageData,
                             int32_t dataLength,
                             const char *codepage)
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  if(codepageData != 0) {
    doCodepageCreate(codepageData, dataLength, codepage);
  }
}

UnicodeString::UnicodeString(const char *src, int32_t srcLength,
                             UConverter *cnv,
                             UErrorCode &errorCode)
  : fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  if(U_SUCCESS(errorCode)) {
    // check arguments
    if(srcLength<-1 || (srcLength!=0 && src==0)) {
      errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    } else {
      // get input length
      if(srcLength==-1) {
        srcLength=(int32_t)uprv_strlen(src);
      }
      if(srcLength>0) {
        if(cnv!=0) {
          // use the provided converter
          ucnv_resetToUnicode(cnv);
          doCodepageCreate(src, srcLength, cnv, errorCode);
        } else {
          // use the default converter
          cnv=u_getDefaultConverter(&errorCode);
          doCodepageCreate(src, srcLength, cnv, errorCode);
          u_releaseDefaultConverter(cnv);
        }
      }
    }

    if(U_FAILURE(errorCode)) {
      setToBogus();
    }
  }
}

UnicodeString::UnicodeString(const UnicodeString& that)
  : Replaceable(),
    fLength(0),
    fCapacity(US_STACKBUF_SIZE),
    fArray(fStackBuffer),
    fFlags(kShortString)
{
  *this = that;
}

//========================================
// array allocation
//========================================

UBool
UnicodeString::allocate(int32_t capacity) {
  if(capacity <= US_STACKBUF_SIZE) {
    fArray = fStackBuffer;
    fCapacity = US_STACKBUF_SIZE;
    fFlags = kShortString;
  } else {
    // count bytes for the refCounter and the string capacity, and
    // round up to a multiple of 16; then divide by 4 and allocate int32_t's
    // to be safely aligned for the refCount
    int32_t words = (int32_t)(((sizeof(int32_t) + capacity * U_SIZEOF_UCHAR + 15) & ~15) >> 2);
    int32_t *array = new int32_t[words];
    if(array != 0) {
      // set initial refCount and point behind the refCount
      *array++ = 1;

      // have fArray point to the first UChar
      fArray = (UChar *)array;
      fCapacity = (int32_t)((words - 1) * (sizeof(int32_t) / U_SIZEOF_UCHAR));
      fFlags = kLongString;
    } else {
      fLength = 0;
      fCapacity = 0;
      fFlags = kIsBogus;
      return FALSE;
    }
  }
  return TRUE;
}

//========================================
// Destructor
//========================================
UnicodeString::~UnicodeString()
{
  releaseArray();
}

//========================================
// Assignment
//========================================
UnicodeString&
UnicodeString::operator= (const UnicodeString& src)
{
  // if assigning to ourselves, do nothing
  if(this == 0 || this == &src) {
    return *this;
  }

  // is the right side bogus?
  if(&src == 0 || src.isBogus()) {
    setToBogus();
    return *this;
  }

  // delete the current contents
  releaseArray();

  // we always copy the length
  fLength = src.fLength;
  if(fLength == 0) {
    // empty string - use the stack buffer
    fArray = fStackBuffer;
    fCapacity = US_STACKBUF_SIZE;
    fFlags = kShortString;
    return *this;
  }

  // fLength>0 and not an "open" src.getBuffer(minCapacity)
  switch(src.fFlags) {
  case kShortString:
    // short string using the stack buffer, do the same
    fArray = fStackBuffer;
    fCapacity = US_STACKBUF_SIZE;
    fFlags = kShortString;
    uprv_memcpy(fStackBuffer, src.fArray, fLength * U_SIZEOF_UCHAR);
    break;
  case kLongString:
    // src uses a refCounted string buffer, use that buffer with refCount
    // src is const, use a cast - we don't really change it
    ((UnicodeString &)src).addRef();
    // fall through to readonly alias copying: copy all fields
  case kReadonlyAlias:
    // src is a readonly alias, do the same
    fArray = src.fArray;
    fCapacity = src.fCapacity;
    fFlags = src.fFlags;
    break;
  case kWritableAlias:
    // src is a writable alias; we make a copy of that instead
    if(allocate(fLength)) {
      uprv_memcpy(fArray, src.fArray, fLength * U_SIZEOF_UCHAR);
      break;
    }
    // if there is not enough memory, then fall through to setting to bogus
  default:
    // if src is bogus, set ourselves to bogus
    // do not call setToBogus() here because fArray and fFlags are not consistent here
    fArray = 0;
    fLength = 0;
    fCapacity = 0;
    fFlags = kIsBogus;
    break;
  }

  return *this;
}

//========================================
// Miscellaneous operations
//========================================
int32_t
UnicodeString::numDisplayCells( UTextOffset start,
                int32_t length,
                UBool asian) const
{
  // pin indices to legal values
  pinIndices(start, length);

  UChar32 c;
  int32_t result = 0;
  UTextOffset limit = start + length;

  while(start < limit) {
    UTF_NEXT_CHAR(fArray, start, limit, c);
    switch(Unicode::getCellWidth(c)) {
    case Unicode::ZERO_WIDTH:
      break;

    case Unicode::HALF_WIDTH:
      result += 1;
      break;

    case Unicode::FULL_WIDTH:
      result += 2;
      break;

    case Unicode::NEUTRAL:
      result += (asian ? 2 : 1);
      break;
    }
  }

  return result;
}

UCharReference
UnicodeString::operator[] (UTextOffset pos)
{
  return UCharReference(this, pos);
}

UnicodeString UnicodeString::unescape() const {
    UnicodeString result;
    for (int32_t i=0; i<length(); ) {
        UChar32 c = charAt(i++);
        if (c == 0x005C /*'\\'*/) {
            c = unescapeAt(i); // advances i
            if (c == (UChar32)0xFFFFFFFF) {
                result.remove(); // return empty string
                break; // invalid escape sequence
            }
        }
        result.append(c);
    }
    return result;
}

// u_unescapeAt() callback to get a UChar from a UnicodeString
U_CDECL_BEGIN
static UChar U_CALLCONV
UnicodeString_charAt(int32_t offset, void *context) {
    return ((UnicodeString*) context)->charAt(offset);
}
U_CDECL_END

UChar32 UnicodeString::unescapeAt(int32_t &offset) const {
    return u_unescapeAt(UnicodeString_charAt, &offset, length(), (void*)this);
}

//========================================
// Read-only implementation
//========================================
int8_t
UnicodeString::doCompare( UTextOffset start,
              int32_t length,
              const UChar *srcChars,
              UTextOffset srcStart,
              int32_t srcLength) const
{
  // compare illegal string values
  if(isBogus()) {
    if(srcChars==0) {
      return 0;
    } else {
      return -1;
    }
  } else if(srcChars==0) {
    return 1;
  }

  // pin indices to legal values
  pinIndices(start, length);

  // get the correct pointer
  const UChar *chars = getArrayStart();

  chars += start;
  srcChars += srcStart;

  UTextOffset minLength;
  int8_t lengthResult;

  // are we comparing different lengths?
  if(length != srcLength) {
    if(length < srcLength) {
      minLength = length;
      lengthResult = -1;
    } else {
      minLength = srcLength;
      lengthResult = 1;
    }
  } else {
    minLength = length;
    lengthResult = 0;
  }

  /*
   * note that uprv_memcmp() returns an int but we return an int8_t;
   * we need to take care not to truncate the result -
   * one way to do this is to right-shift the value to
   * move the sign bit into the lower 8 bits and making sure that this
   * does not become 0 itself
   */

  if(minLength > 0 && chars != srcChars) {
    int32_t result;

#   if U_IS_BIG_ENDIAN 
      // big-endian: byte comparison works
      result = uprv_memcmp(chars, srcChars, minLength * sizeof(UChar));
      if(result != 0) {
        return (int8_t)(result >> 15 | 1);
      }
#   else
      // little-endian: compare UChar units
      do {
        result = ((int32_t)*(chars++) - (int32_t)*(srcChars++));
        if(result != 0) {
          return (int8_t)(result >> 15 | 1);
        }
      } while(--minLength > 0);
#   endif
  }
  return lengthResult;
}

/* String compare in code point order - doCompare() compares in code unit order. */
int8_t
UnicodeString::doCompareCodePointOrder(UTextOffset start,
                                       int32_t length,
                                       const UChar *srcChars,
                                       UTextOffset srcStart,
                                       int32_t srcLength) const
{
  // compare illegal string values
  if(isBogus()) {
    if(srcChars==0) {
      return 0;
    } else {
      return -1;
    }
  } else if(srcChars==0) {
    return 1;
  }

  // pin indices to legal values
  pinIndices(start, length);

  // get the correct pointer
  const UChar *chars = getArrayStart();

  chars += start;
  srcChars += srcStart;

  UTextOffset minLength;
  int8_t lengthResult;

  // are we comparing different lengths?
  if(length != srcLength) {
    if(length < srcLength) {
      minLength = length;
      lengthResult = -1;
    } else {
      minLength = srcLength;
      lengthResult = 1;
    }
  } else {
    minLength = length;
    lengthResult = 0;
  }

  if(minLength > 0 && chars != srcChars) {
    int32_t diff = u_memcmpCodePointOrder(chars, srcChars, minLength);
    if(diff!=0) {
      return (int8_t)(diff >> 15 | 1);
    }
  }
  return lengthResult;
}

int8_t
UnicodeString::doCaseCompare(UTextOffset start,
                             int32_t length,
                             const UChar *srcChars,
                             UTextOffset srcStart,
                             int32_t srcLength,
                             uint32_t options) const
{
  // compare illegal string values
  if(isBogus()) {
    if(srcChars==0) {
      return 0;
    } else {
      return -1;
    }
  } else if(srcChars==0) {
    return 1;
  }

  // pin indices to legal values
  pinIndices(start, length);

  // get the correct pointer
  const UChar *chars = getArrayStart();

  chars += start;
  srcChars += srcStart;

  if(chars != srcChars) {
    int32_t result=u_internalStrcasecmp(chars, length, srcChars, srcLength, options);
    if(result!=0) {
      return (int8_t)(result >> 24 | 1);
    }
  } else if(length != srcLength) {
    return (int8_t)((length - srcLength) >> 24 | 1);
  }
  return 0;
}

int32_t
UnicodeString::getLength() const {
    return length();
}

UChar
UnicodeString::getCharAt(UTextOffset offset) const {
  return charAt(offset);
}

UChar32
UnicodeString::getChar32At(UTextOffset offset) const {
  return char32At(offset);
}

int32_t
UnicodeString::countChar32(UTextOffset start, int32_t length) const {
  pinIndices(start, length);
  // if(isBogus()) then fArray==0 and start==0 - u_countChar32() checks for NULL
  return u_countChar32(fArray+start, length);
}

UTextOffset
UnicodeString::moveIndex32(UTextOffset index, int32_t delta) const {
  // pin index
  if(index<0) {
    index=0;
  } else if(index>fLength) {
    index=fLength;
  }

  if(delta>0) {
    UTF_FWD_N(fArray, index, fLength, delta);
  } else {
    UTF_BACK_N(fArray, 0, index, -delta);
  }

  return index;
}

void
UnicodeString::doExtract(UTextOffset start,
             int32_t length,
             UChar *dst,
             UTextOffset dstStart) const
{
  // pin indices to legal values
  pinIndices(start, length);

  // do not copy anything if we alias dst itself
  if(fArray + start != dst + dstStart) {
    us_arrayCopy(getArrayStart(), start, dst, dstStart, length);
  }
}

int32_t
UnicodeString::extract(UChar *dest, int32_t destCapacity,
                       UErrorCode &errorCode) const {
  if(U_SUCCESS(errorCode)) {
    if(isBogus() || destCapacity<0 || (destCapacity>0 && dest==0)) {
      errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    } else {
      if(fLength>0 && fLength<=destCapacity && fArray!=dest) {
        uprv_memcpy(dest, fArray, fLength*U_SIZEOF_UCHAR);
      }
      return u_terminateUChars(dest, destCapacity, fLength, &errorCode);
    }
  }

  return fLength;
}

UTextOffset 
UnicodeString::indexOf(const UChar *srcChars,
               UTextOffset srcStart,
               int32_t srcLength,
               UTextOffset start,
               int32_t length) const
{
  if(isBogus() || srcChars == 0 || srcStart < 0 || srcLength <= 0) {
    return -1;
  }

  // now we will only work with srcLength-1
  --srcLength;

  // get the indices within bounds
  pinIndices(start, length);

  // set length for the last possible match start position
  // note the --srcLength above
  length -= srcLength;

  if(length <= 0) {
    return -1;
  }

  const UChar *array = getArrayStart();
  UTextOffset limit = start + length;

  // search for the first char, then compare the rest of the string
  // increment srcStart here for that, matching the --srcLength above
  UChar ch = srcChars[srcStart++];

  do {
    if(array[start] == ch && (srcLength == 0 || compare(start + 1, srcLength, srcChars, srcStart, srcLength) == 0)) {
      return start;
    }
  } while(++start < limit);

  return -1;
}

UTextOffset
UnicodeString::doIndexOf(UChar c,
             UTextOffset start,
             int32_t length) const
{
  // pin indices
  pinIndices(start, length);
  if(length == 0) {
    return -1;
  }

  // find the first occurrence of c
  const UChar *begin = getArrayStart() + start;
  const UChar *limit = begin + length;

  do {
    if(*begin == c) {
      return begin - getArrayStart();
    }
  } while(++begin < limit);

  return -1;
}

UTextOffset 
UnicodeString::lastIndexOf(const UChar *srcChars,
               UTextOffset srcStart,
               int32_t srcLength,
               UTextOffset start,
               int32_t length) const
{
  if(isBogus() || srcChars == 0 || srcStart < 0 || srcLength <= 0) {
    return -1;
  }

  // now we will only work with srcLength-1
  --srcLength;

  // get the indices within bounds
  pinIndices(start, length);

  // set length for the last possible match start position
  // note the --srcLength above
  length -= srcLength;

  if(length <= 0) {
    return -1;
  }

  const UChar *array = getArrayStart();
  UTextOffset pos;

  // search for the first char, then compare the rest of the string
  // increment srcStart here for that, matching the --srcLength above
  UChar ch = srcChars[srcStart++];

  pos = start + length;
  do {
    if(array[--pos] == ch && (srcLength == 0 || compare(pos + 1, srcLength, srcChars, srcStart, srcLength) == 0)) {
      return pos;
    }
  } while(pos > start);

  return -1;
}

UTextOffset
UnicodeString::doLastIndexOf(UChar c,
                 UTextOffset start,
                 int32_t length) const
{
  if(isBogus()) {
    return -1;
  }

  // pin indices
  pinIndices(start, length);
  if(length == 0) {
    return -1;
  }

  const UChar *begin = getArrayStart() + start;
  const UChar *limit = begin + length;

  do {
    if(*--limit == c) {
      return limit - getArrayStart();
    }
  } while(limit > begin);

  return -1;
}

UnicodeString& 
UnicodeString::findAndReplace(UTextOffset start,
                  int32_t length,
                  const UnicodeString& oldText,
                  UTextOffset oldStart,
                  int32_t oldLength,
                  const UnicodeString& newText,
                  UTextOffset newStart,
                  int32_t newLength)
{
  if(isBogus() || oldText.isBogus() || newText.isBogus()) {
    return *this;
  }

  pinIndices(start, length);
  oldText.pinIndices(oldStart, oldLength);
  newText.pinIndices(newStart, newLength);

  if(oldLength == 0) {
    return *this;
  }

  while(length > 0 && length >= oldLength) {
    UTextOffset pos = indexOf(oldText, oldStart, oldLength, start, length);
    if(pos < 0) {
      // no more oldText's here: done
      break;
    } else {
      // we found oldText, replace it by newText and go beyond it
      replace(pos, oldLength, newText, newStart, newLength);
      length -= pos + oldLength - start;
      start = pos + newLength;
    }
  }

  return *this;
}


//========================================
// Write implementation
//========================================

void
UnicodeString::setToBogus()
{
  releaseArray();

  fArray = 0;
  fCapacity = fLength = 0;
  fFlags = kIsBogus;
}

// setTo() analogous to the readonly-aliasing constructor with the same signature
UnicodeString &
UnicodeString::setTo(UBool isTerminated,
                     const UChar *text,
                     int32_t textLength)
{
  if(fFlags & kOpenGetBuffer) {
    // do not modify a string that has an "open" getBuffer(minCapacity)
    return *this;
  }

  if(text == 0 || textLength < -1 || textLength == -1 && !isTerminated) {
    setToBogus();
    return *this;
  }

  releaseArray();

  fArray = (UChar *)text;
  if(textLength != -1) {
    fLength = textLength;
  } else {
    // text is terminated, or else it would have failed the above test
    fLength = u_strlen(text);
    fCapacity = fLength + 1;
  }

  fCapacity = isTerminated ? fLength + 1 : fLength;
  fFlags = kReadonlyAlias;
  return *this;
}

// setTo() analogous to the writable-aliasing constructor with the same signature
UnicodeString &
UnicodeString::setTo(UChar *buffer,
                     int32_t buffLength,
                     int32_t buffCapacity) {
  if(fFlags & kOpenGetBuffer) {
    // do not modify a string that has an "open" getBuffer(minCapacity)
    return *this;
  }

  if(buffer == 0 || buffLength < 0 || buffLength > buffCapacity) {
    setToBogus();
    return *this;
  }

  releaseArray();

  fArray = buffer;
  fLength = buffLength;
  fCapacity = buffCapacity;
  fFlags = kWritableAlias;
  return *this;
}

UnicodeString&
UnicodeString::setCharAt(UTextOffset offset,
             UChar c)
{
  if(cloneArrayIfNeeded() && fLength > 0) {
    if(offset < 0) {
      offset = 0;
    } else if(offset >= fLength) {
      offset = fLength - 1;
    }

    fArray[offset] = c;
  }
  return *this;
}

/*
 * Implement argument checking and buffer handling
 * for string case mapping as a common function.
 */
enum {
    TO_LOWER,
    TO_UPPER,
    FOLD_CASE
};

UnicodeString &
UnicodeString::toLower() {
  return caseMap(Locale::getDefault(), 0, TO_LOWER);
}

UnicodeString &
UnicodeString::toLower(const Locale &locale) {
  return caseMap(locale, 0, TO_LOWER);
}

UnicodeString &
UnicodeString::toUpper() {
  return caseMap(Locale::getDefault(), 0, TO_UPPER);
}

UnicodeString &
UnicodeString::toUpper(const Locale &locale) {
  return caseMap(locale, 0, TO_UPPER);
}

UnicodeString &
UnicodeString::foldCase(uint32_t options) {
    return caseMap(Locale::getDefault(), options, FOLD_CASE);
}

// static helper function for string case mapping
// called by u_internalStrToUpper/Lower()
UBool U_CALLCONV
UnicodeString::growBuffer(void *context,
                          UChar **buffer, int32_t *pCapacity, int32_t reqCapacity,
                          int32_t length) {
  UnicodeString *me = (UnicodeString *)context;
  me->fLength = length;
  if(me->cloneArrayIfNeeded(reqCapacity)) {
    *buffer = me->fArray;
    *pCapacity = me->fCapacity;
    return TRUE;
  } else {
    return FALSE;
  }
}

UnicodeString &
UnicodeString::caseMap(const Locale& locale,
                       uint32_t options,
                       int32_t toWhichCase) {
  if(fLength <= 0) {
    // nothing to do
    return *this;
  }

  // We need to allocate a new buffer for the internal string case mapping function.
  // This is very similar to how doReplace() below keeps the old array pointer
  // and deletes the old array itself after it is done.
  // In addition, we are forcing cloneArrayIfNeeded() to always allocate a new array.
  UChar *oldArray = fArray;
  int32_t oldLength = fLength;
  int32_t *bufferToDelete = 0;

  // Make sure that if the string is in fStackBuffer we do not overwrite it!
  int32_t capacity;
  if(fLength <= US_STACKBUF_SIZE) {
    if(fArray == fStackBuffer) {
      capacity = 2 * US_STACKBUF_SIZE; // make sure that cloneArrayIfNeeded() allocates a new buffer
    } else {
      capacity = US_STACKBUF_SIZE;
    }
  } else {
    capacity = fLength + 2;
  }
  if(!cloneArrayIfNeeded(capacity, capacity, FALSE, &bufferToDelete, TRUE)) {
    return *this;
  }

  UErrorCode errorCode = U_ZERO_ERROR;
  if(toWhichCase==TO_LOWER) {
    fLength = u_internalStrToLower(fArray, fCapacity,
                                   oldArray, oldLength,
                                   locale.getName(),
                                   growBuffer, this,
                                   &errorCode);
  } else if(toWhichCase==TO_UPPER) {
    fLength = u_internalStrToUpper(fArray, fCapacity,
                                   oldArray, oldLength,
                                   locale.getName(),
                                   growBuffer, this,
                                   &errorCode);
  } else {
    fLength = u_internalStrFoldCase(fArray, fCapacity,
                                    oldArray, oldLength,
                                    options,
                                    growBuffer, this,
                                    &errorCode);
  }

  delete [] bufferToDelete;
  if(U_FAILURE(errorCode)) {
    setToBogus();
  }
  return *this;
}

UnicodeString&
UnicodeString::doReplace( UTextOffset start,
              int32_t length,
              const UnicodeString& src,
              UTextOffset srcStart,
              int32_t srcLength)
{
  if(!src.isBogus()) {
    // pin the indices to legal values
    src.pinIndices(srcStart, srcLength);

    // get the characters from src
    // and replace the range in ourselves with them
    return doReplace(start, length, src.getArrayStart(), srcStart, srcLength);
  } else {
    // remove the range
    return doReplace(start, length, 0, 0, 0);
  }
}

UnicodeString&
UnicodeString::doReplace(UTextOffset start,
             int32_t length,
             const UChar *srcChars,
             UTextOffset srcStart,
             int32_t srcLength)
{
  // if we're bogus, set us to empty first
  if(isBogus()) {
    fArray = fStackBuffer;
    fLength = 0;
    fCapacity = US_STACKBUF_SIZE;
    fFlags = kShortString;
  }

  if(srcChars == 0) {
    srcStart = srcLength = 0;
  }

  int32_t *bufferToDelete = 0;

  // the following may change fArray but will not copy the current contents;
  // therefore we need to keep the current fArray
  UChar *oldArray = fArray;
  int32_t oldLength = fLength;

  // pin the indices to legal values
  pinIndices(start, length);

  // calculate the size of the string after the replace
  int32_t newSize = oldLength - length + srcLength;

  // clone our array and allocate a bigger array if needed
  if(!cloneArrayIfNeeded(newSize, newSize + (newSize >> 2) + kGrowSize,
                         FALSE, &bufferToDelete)
  ) {
    return *this;
  }

  // now do the replace

  if(fArray != oldArray) {
    // if fArray changed, then we need to copy everything except what will change
    us_arrayCopy(oldArray, 0, fArray, 0, start);
    us_arrayCopy(oldArray, start + length,
                 fArray, start + srcLength,
                 oldLength - (start + length));
  } else if(length != srcLength) {
    // fArray did not change; copy only the portion that isn't changing, leaving a hole
    us_arrayCopy(oldArray, start + length,
                 fArray, start + srcLength,
                 oldLength - (start + length));
  }

  // now fill in the hole with the new string
  us_arrayCopy(srcChars, srcStart, getArrayStart(), start, srcLength);

  fLength = newSize;

  // delayed delete in case srcChars == fArray when we started, and
  // to keep oldArray alive for the above operations
  delete [] bufferToDelete;

  return *this;
}

/**
 * Replaceable API
 */
void
UnicodeString::handleReplaceBetween(UTextOffset start,
                                    UTextOffset limit,
                                    const UnicodeString& text) {
    replaceBetween(start, limit, text);
}

/**
 * Replaceable API
 */
void 
UnicodeString::copy(int32_t start, int32_t limit, int32_t dest) {
    UChar* text = new UChar[limit - start];
    extractBetween(start, limit, text, 0);
    insert(dest, text, 0, limit - start);    
    delete[] text;
}

UnicodeString&
UnicodeString::doReverse(UTextOffset start,
             int32_t length)
{
  if(fLength <= 1 || !cloneArrayIfNeeded()) {
    return *this;
  }

  // pin the indices to legal values
  pinIndices(start, length);

  UChar *left = getArrayStart() + start;
  UChar *right = getArrayStart() + start + length;
  UChar swap;
  UBool hasSupplementary = FALSE;

  while(left < --right) {
    hasSupplementary |= (UBool)UTF_IS_LEAD(swap = *left);
    hasSupplementary |= (UBool)UTF_IS_LEAD(*left++ = *right);
    *right = swap;
  }

  /* if there are supplementary code points in the reversed range, then re-swap their surrogates */
  if(hasSupplementary) {
    UChar swap2;

    left = getArrayStart() + start;
    right = getArrayStart() + start + length - 1; // -1 so that we can look at *(left+1) if left<right
    while(left < right) {
      if(UTF_IS_TRAIL(swap = *left) && UTF_IS_LEAD(swap2 = *(left + 1))) {
        *left++ = swap2;
        *left++ = swap;
      } else {
        ++left;
      }
    }
  }

  return *this;
}

UBool 
UnicodeString::padLeading(int32_t targetLength,
                          UChar padChar)
{
  if(fLength >= targetLength || !cloneArrayIfNeeded(targetLength)) {
    return FALSE;
  } else {
    // move contents up by padding width
    int32_t start = targetLength - fLength;
    us_arrayCopy(fArray, 0, fArray, start, fLength);

    // fill in padding character
    while(--start >= 0) {
      fArray[start] = padChar;
    }
    fLength = targetLength;
    return TRUE;
  }
}

UBool 
UnicodeString::padTrailing(int32_t targetLength,
                           UChar padChar)
{
  if(fLength >= targetLength || !cloneArrayIfNeeded(targetLength)) {
    return FALSE;
  } else {
    // fill in padding character
    int32_t length = targetLength;
    while(--length >= fLength) {
      fArray[length] = padChar;
    }
    fLength = targetLength;
    return TRUE;
  }
}

UnicodeString& 
UnicodeString::trim()
{
  if(isBogus()) {
    return *this;
  }

  UChar32 c;
  UTextOffset i = fLength, length;

  // first cut off trailing white space
  for(;;) {
    length = i;
    if(i <= 0) {
      break;
    }
    UTF_PREV_CHAR(fArray, 0, i, c);
    if(!(c == 0x20 || Unicode::isWhitespace(c))) {
      break;
    }
  }
  if(length < fLength) {
    fLength = length;
  }

  // find leading white space
  UTextOffset start;
  i = 0;
  for(;;) {
    start = i;
    if(i >= length) {
      break;
    }
    UTF_NEXT_CHAR(fArray, i, length, c);
    if(!(c == 0x20 || Unicode::isWhitespace(c))) {
      break;
    }
  }

  // move string forward over leading white space
  if(start > 0) {
    doReplace(0, start, 0, 0, 0);
  }

  return *this;
}

//========================================
// Hashing
//========================================
int32_t
UnicodeString::doHashCode() const
{
    /* Delegate hash computation to uhash.  This makes UnicodeString
     * hashing consistent with UChar* hashing.  */
    int32_t hashCode = uhash_hashUCharsN(getArrayStart(), fLength);
    if (hashCode == kInvalidHashCode) {
        hashCode = kEmptyHashCode;
    }
    return hashCode;
}

//========================================
// Codeset conversion
//========================================
int32_t
UnicodeString::extract(UTextOffset start,
                       int32_t length,
                       char *target,
                       uint32_t dstSize,
                       const char *codepage) const
{
  // if the arguments are illegal, then do nothing
  if(/*dstSize < 0 || */(dstSize > 0 && target == 0)) {
    return 0;
  }

  // pin the indices to legal values
  pinIndices(start, length);

  // create the converter
  UConverter *converter;
  UErrorCode status = U_ZERO_ERROR;

  // just write the NUL if the string length is 0
  if(length == 0) {
    return u_terminateChars(target, dstSize, 0, &status);
  }

  // if the codepage is the default, use our cache
  // if it is an empty string, then use the "invariant character" conversion
  if (codepage == 0) {
    converter = u_getDefaultConverter(&status);
  } else if (*codepage == 0) {
    // use the "invariant characters" conversion
    int32_t destLength;
    // careful: dstSize is unsigned! (0xffffffff means "unlimited")
    if(dstSize >= 0x80000000) {
      destLength = length;
      // make sure that the NUL-termination works (takes int32_t)
      dstSize=0x7fffffff;
    } else if(length <= (int32_t)dstSize) {
      destLength = length;
    } else {
      destLength = (int32_t)dstSize;
    }
    u_UCharsToChars(getArrayStart() + start, target, destLength);
    return u_terminateChars(target, (int32_t)dstSize, length, &status);
  } else {
    converter = ucnv_open(codepage, &status);
  }

  length = doExtract(start, length, target, (int32_t)dstSize, converter, status);

  // close the converter
  if (codepage == 0) {
    u_releaseDefaultConverter(converter);
  } else {
    ucnv_close(converter);
  }

  return length;
}

int32_t
UnicodeString::extract(char *dest, int32_t destCapacity,
                       UConverter *cnv,
                       UErrorCode &errorCode) const {
  if(U_FAILURE(errorCode)) {
    return 0;
  }

  if(isBogus() || destCapacity<0 || (destCapacity>0 && dest==0)) {
    errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    return 0;
  }

  // nothing to do?
  if(fLength<=0) {
    return u_terminateChars(dest, destCapacity, 0, &errorCode);
  }

  // get the converter
  UBool isDefaultConverter;
  if(cnv==0) {
    isDefaultConverter=TRUE;
    cnv=u_getDefaultConverter(&errorCode);
    if(U_FAILURE(errorCode)) {
      return 0;
    }
  } else {
    isDefaultConverter=FALSE;
    ucnv_resetFromUnicode(cnv);
  }

  // convert
  int32_t length=doExtract(0, fLength, dest, destCapacity, cnv, errorCode);

  // release the converter
  if(isDefaultConverter) {
    u_releaseDefaultConverter(cnv);
  }

  return length;
}

int32_t
UnicodeString::doExtract(UTextOffset start, int32_t length,
                         char *dest, int32_t destCapacity,
                         UConverter *cnv,
                         UErrorCode &errorCode) const {
  if(U_FAILURE(errorCode)) {
    if(destCapacity!=0) {
      *dest=0;
    }
    return 0;
  }

  const UChar *src=fArray+start, *srcLimit=src+length;
  char *originalDest=dest;
  const char *destLimit;

  if(destCapacity==0) {
    destLimit=dest=0;
  } else if(destCapacity==-1) {
    // Pin the limit to U_MAX_PTR if the "magic" destCapacity is used.
    destLimit=(char*)U_MAX_PTR(dest);
    // for NUL-termination, translate into highest int32_t
    destCapacity=0x7fffffff;
  } else {
    destLimit=dest+destCapacity;
  }

  // perform the conversion
  ucnv_fromUnicode(cnv, &dest, destLimit, &src, srcLimit, 0, TRUE, &errorCode);
  length=(int32_t)(dest-originalDest);

  // if an overflow occurs, then get the preflighting length
  if(errorCode==U_BUFFER_OVERFLOW_ERROR) {
    char buffer[1024];

    destLimit=buffer+sizeof(buffer);
    do {
      dest=buffer;
      errorCode=U_ZERO_ERROR;
      ucnv_fromUnicode(cnv, &dest, destLimit, &src, srcLimit, 0, TRUE, &errorCode);
      length+=(int32_t)(dest-buffer);
    } while(errorCode==U_BUFFER_OVERFLOW_ERROR);
  }

  return u_terminateChars(originalDest, destCapacity, length, &errorCode);
}

void
UnicodeString::doCodepageCreate(const char *codepageData,
                int32_t dataLength,
                const char *codepage)
{
  // if there's nothing to convert, do nothing
  if(codepageData == 0 || dataLength <= 0) {
    return;
  }

  UErrorCode status = U_ZERO_ERROR;

  // create the converter
  // if the codepage is the default, use our cache
  // if it is an empty string, then use the "invariant character" conversion
  UConverter *converter = (codepage == 0 ?
                             u_getDefaultConverter(&status) :
                             *codepage == 0 ?
                               0 :
                               ucnv_open(codepage, &status));

  // if we failed, set the appropriate flags and return
  if(U_FAILURE(status)) {
    setToBogus();
    return;
  }

  // perform the conversion
  if(converter == 0) {
    // use the "invariant characters" conversion
    if(cloneArrayIfNeeded(dataLength, dataLength, FALSE)) {
      u_charsToUChars(codepageData, getArrayStart(), dataLength);
      fLength = dataLength;
    } else {
      setToBogus();
    }
    return;
  }

  // convert using the real converter
  doCodepageCreate(codepageData, dataLength, converter, status);
  if(U_FAILURE(status)) {
    setToBogus();
  }

  // close the converter
  if(codepage == 0) {
    u_releaseDefaultConverter(converter);
  } else {
    ucnv_close(converter);
  }
}

void
UnicodeString::doCodepageCreate(const char *codepageData,
                                int32_t dataLength,
                                UConverter *converter,
                                UErrorCode &status) {
  if(U_FAILURE(status)) {
    return;
  }

  // set up the conversion parameters
  const char *mySource     = codepageData;
  const char *mySourceEnd  = mySource + dataLength;
  UChar *myTarget;

  // estimate the size needed:
  // 1.25 UChar's per source byte should cover most cases
  int32_t arraySize = dataLength + (dataLength >> 2);

  // we do not care about the current contents
  UBool doCopyArray = FALSE;
  for(;;) {
    if(!cloneArrayIfNeeded(arraySize, arraySize, doCopyArray)) {
      setToBogus();
      break;
    }

    // perform the conversion
    myTarget = fArray + fLength;
    ucnv_toUnicode(converter, &myTarget,  fArray + fCapacity,
           &mySource, mySourceEnd, 0, TRUE, &status);

    // update the conversion parameters
    fLength = myTarget - fArray;

    // allocate more space and copy data, if needed
    if(status == U_BUFFER_OVERFLOW_ERROR) {
      // reset the error code
      status = U_ZERO_ERROR;

      // keep the previous conversion results
      doCopyArray = TRUE;

      // estimate the new size needed, larger than before
      // try 2 UChar's per remaining source byte
      arraySize = fLength + 2 * (mySourceEnd - mySource);
    } else {
      break;
    }
  }
}

//========================================
// External Buffer
//========================================

UChar *
UnicodeString::getBuffer(int32_t minCapacity) {
  if(minCapacity>=-1 && cloneArrayIfNeeded(minCapacity)) {
    fFlags|=kOpenGetBuffer;
    fLength=0;
    return fArray;
  } else {
    return 0;
  }
}

void
UnicodeString::releaseBuffer(int32_t newLength) {
  if(fFlags&kOpenGetBuffer && newLength>=-1) {
    // set the new fLength
    if(newLength==-1) {
      // the new length is the string length, capped by fCapacity
      const UChar *p=fArray, *limit=fArray+fCapacity;
      while(p<limit && *p!=0) {
        ++p;
      }
      fLength=(int32_t)(p-fArray);
    } else if(newLength<=fCapacity) {
      fLength=newLength;
    } else {
      fLength=fCapacity;
    }
    fFlags&=~kOpenGetBuffer;
  }
}

//========================================
// Miscellaneous
//========================================
UBool
UnicodeString::cloneArrayIfNeeded(int32_t newCapacity,
                                  int32_t growCapacity,
                                  UBool doCopyArray,
                                  int32_t **pBufferToDelete,
                                  UBool forceClone) {
  // default parameters need to be static, therefore
  // the defaults are -1 to have convenience defaults
  if(newCapacity == -1) {
    newCapacity = fCapacity;
  }

  // while a getBuffer(minCapacity) is "open",
  // prevent any modifications of the string by returning FALSE here
  if(fFlags & kOpenGetBuffer) {
    return FALSE;
  }

  // if we're bogus, set us to empty first
  if(fFlags & kIsBogus) {
    fArray = fStackBuffer;
    fLength = 0;
    fCapacity = US_STACKBUF_SIZE;
    fFlags = kShortString;
  }

  /*
   * We need to make a copy of the array if
   * the buffer is read-only, or
   * the buffer is refCounted (shared), and refCount>1, or
   * the buffer is too small.
   * Return FALSE if memory could not be allocated.
   */
  if(forceClone ||
     fFlags & kBufferIsReadonly ||
     fFlags & kRefCounted && refCount() > 1 ||
     newCapacity > fCapacity
  ) {
    // save old values
    UChar *array = fArray;
    uint16_t flags = fFlags;

    // check growCapacity for default value and use of the stack buffer
    if(growCapacity == -1) {
      growCapacity = newCapacity;
    } else if(newCapacity <= US_STACKBUF_SIZE && growCapacity > US_STACKBUF_SIZE) {
      growCapacity = US_STACKBUF_SIZE;
    }

    // allocate a new array
    if(allocate(growCapacity) ||
       newCapacity < growCapacity && allocate(newCapacity)
    ) {
      if(doCopyArray) {
        // copy the contents
        // do not copy more than what fits - it may be smaller than before
        if(fCapacity < fLength) {
          fLength = fCapacity;
        }
        us_arrayCopy(array, 0, fArray, 0, fLength);
      } else {
        fLength = 0;
      }

      // release the old array
      if(flags & kRefCounted) {
        // the array is refCounted; decrement and release if 0
        int32_t *pRefCount = ((int32_t *)array - 1);
        if(--*pRefCount == 0) {
          if(pBufferToDelete == 0) {
            delete [] pRefCount;
          } else {
            // the caller requested to delete it himself
            *pBufferToDelete = pRefCount;
          }
        }
      }
    } else {
      // not enough memory for growCapacity and not even for the smaller newCapacity
      // reset the old values for setToBogus() to release the array
      fArray = array;
      fFlags = flags;
      setToBogus();
      return FALSE;
    }
  }
  return TRUE;
}
U_NAMESPACE_END
