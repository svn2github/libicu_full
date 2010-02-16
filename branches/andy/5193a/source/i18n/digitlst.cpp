/*
**********************************************************************
*   Copyright (C) 1997-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File DIGITLST.CPP
*
* Modification History:
*
*   Date        Name        Description
*   03/21/97    clhuang     Converted from java.
*   03/21/97    clhuang     Implemented with new APIs.
*   03/27/97    helena      Updated to pass the simple test after code review.
*   03/31/97    aliu        Moved isLONG_MIN to here, and fixed it.
*   04/15/97    aliu        Changed MAX_COUNT to DBL_DIG.  Changed Digit to char.
*                           Reworked representation by replacing fDecimalAt
*                           with fExponent.
*   04/16/97    aliu        Rewrote set() and getDouble() to use sprintf/atof
*                           to do digit conversion.
*   09/09/97    aliu        Modified for exponential notation support.
*   08/02/98    stephen     Added nearest/even rounding
*                            Fixed bug in fitsIntoLong
******************************************************************************
*/

#include "digitlst.h"

#if !UCONFIG_NO_FORMATTING
#include "unicode/putil.h"
#include "cmemory.h"
#include "cstring.h"
#include "putilimp.h"
#include "uassert.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

// ***************************************************************************
// class DigitList
//    A wrapper onto decNumber.
//    Used to be standalone.
// ***************************************************************************

/**
 * This is the zero digit.  The base for the digits returned by getDigit()
 */
#define kZero '0'

static char gDecimal = 0;

/* Only for 32 bit numbers. Ignore the negative sign. */
static const char LONG_MIN_REP[] = "2147483648";
static const char I64_MIN_REP[] = "9223372036854775808";


U_NAMESPACE_BEGIN

// -------------------------------------
// default constructor

DigitList::DigitList()
{
    uprv_decContextDefault(&fContext, DEC_INIT_BASE);
    fContext.traps  = 0;
    uprv_decContextSetRounding(&fContext, DEC_ROUND_HALF_EVEN);
    fContext.digits = fStorage.getCapacity() - sizeof(decNumber);

    fDecNumber = (decNumber *)(fStorage.getAlias());
    uprv_decNumberZero(fDecNumber);

    fDouble = 0.0;
    fHaveDouble = TRUE;
}

// -------------------------------------

DigitList::~DigitList()
{
}

// -------------------------------------
// copy constructor

DigitList::DigitList(const DigitList &other)
{
    fDecNumber = NULL;
    *this = other;
}


// -------------------------------------
// assignment operator

DigitList&
DigitList::operator=(const DigitList& other)
{
    if (this != &other)
    {
        uprv_memcpy(&fContext, &other.fContext, sizeof(decContext));

        fStorage.resize(other.fStorage.getCapacity());
        fDecNumber = (decNumber *)fStorage.getAlias();
        uprv_decNumberCopy(fDecNumber, other.fDecNumber);

        fDouble = other.fDouble;
        fHaveDouble = other.fHaveDouble;
    }
    return *this;
}

// -------------------------------------
//    operator ==  (does not exactly match the old DigitList function)

UBool
DigitList::operator==(const DigitList& that) const
{
    if (this == &that) {
        return TRUE;
    }
    decNumber n;  // Has space for only a none digit value.
    decContext c;
    uprv_decContextDefault(&c, DEC_INIT_BASE);
    c.digits = 1;
    c.traps = 0;

    uprv_decNumberCompare(&n, this->fDecNumber, that.fDecNumber, &c);
    UBool result = decNumberIsZero(&n);
    return result;
}

// -------------------------------------
//      comparison function.   Returns 
//         Not Comparable :  -2
//                      < :  -1
//                     == :   0
//                      > :  +1
int32_t DigitList::compare(const DigitList &other) {
    decNumber   result;
    int32_t     savedDigits = fContext.digits;
    fContext.digits = 1;
    uprv_decNumberCompare(&result, this->fDecNumber, other.fDecNumber, &fContext);
    fContext.digits = savedDigits;
    if (decNumberIsZero(&result)) {
        return 0;
    } else if (decNumberIsSpecial(&result)) {
        return -2;
    } else if (result.bits & DECNEG) {
        return -1;
    } else {
        return 1;
    }
}


// -------------------------------------
//  Reduce - remove trailing zero digits.
void
DigitList::reduce() {
    uprv_decNumberReduce(fDecNumber, fDecNumber, &fContext);
}


// -------------------------------------
// Resets the digit list; sets all the digits to zero.

void
DigitList::clear()
{
    uprv_decNumberZero(fDecNumber);
    uprv_decContextSetRounding(&fContext, DEC_ROUND_HALF_EVEN);
    fDouble = 0.0;
    fHaveDouble = TRUE;
}


/**
 * Formats a int64_t number into a base 10 string representation, and NULL terminates it.
 * @param number The number to format
 * @param outputStr The string to output to.  Must be at least MAX_DIGITS+2 in length (21),
 *                  to hold the longest int64_t value.
 * @return the number of digits written, not including the sign.
 */
static int32_t
formatBase10(int64_t number, char *outputStr) {
    // The number is output backwards, starting with the LSD.
    // Fill the buffer from the far end.  After the number is complete,
    // slide the string contents to the front.

    const int32_t MAX_IDX = MAX_DIGITS+2;
    int32_t destIdx = MAX_IDX;
    outputStr[--destIdx] = 0; 

    int64_t  n = number;
    if (number < 0) {   // Negative numbers are slightly larger than a postive
        outputStr[--destIdx] = (char)(-(n % 10) + kZero);
        n /= -10;
    }
    do { 
        outputStr[--destIdx] = (char)(n % 10 + kZero);
        n /= 10;
    } while (n > 0);
    
    if (number < 0) {
        outputStr[--destIdx] = '-';
    }

    // Slide the number to the start of the output str
    U_ASSERT(destIdx >= 0);
    int32_t length = MAX_IDX - destIdx;
    uprv_memmove(outputStr, outputStr+MAX_IDX-length, length);

    return length;
}


// -------------------------------------

void 
DigitList::setRoundingMode(DecimalFormat::ERoundingMode m) {
    enum rounding r;

    switch (m) {
      case  DecimalFormat::kRoundCeiling:  r = DEC_ROUND_CEILING;   break;
      case  DecimalFormat::kRoundFloor:    r = DEC_ROUND_FLOOR;     break;
      case  DecimalFormat::kRoundDown:     r = DEC_ROUND_DOWN;      break;
      case  DecimalFormat::kRoundUp:       r = DEC_ROUND_UP;        break;
      case  DecimalFormat::kRoundHalfEven: r = DEC_ROUND_HALF_EVEN; break;
      case  DecimalFormat::kRoundHalfDown: r = DEC_ROUND_HALF_DOWN; break;
      case  DecimalFormat::kRoundHalfUp:   r = DEC_ROUND_HALF_UP;   break;
      default:
         // TODO: how to report the problem?
         // Leave existing mode unchanged.
         r = uprv_decContextGetRounding(&fContext);
    }
    uprv_decContextSetRounding(&fContext, r);
  
}

// -------------------------------------
//  Set to zero, but preserve sign 
void  
DigitList::setToZero() {
    uint8_t signbit = fDecNumber->bits & DECNEG;
    uprv_decNumberZero(fDecNumber);
    fDecNumber->bits |= signbit; 
    fDouble = 0.0;
    fHaveDouble = TRUE;
    if (signbit != 0) {
       fDouble /= -1;
    }
}

// -------------------------------------

void  
DigitList::setPositive(UBool s) {
    if (s) {
        fDecNumber->bits &= ~DECNEG; 
    } else {
        fDecNumber->bits |= DECNEG;
    }
    fHaveDouble = FALSE;
}
// -------------------------------------

void     
DigitList::setDecimalAt(int32_t d) {
    U_ASSERT((fDecNumber->bits & DECSPECIAL) == 0);  // Not Infinity or NaN
    U_ASSERT(d-1>-999999999);
    U_ASSERT(d-1< 999999999);
    int32_t adjustedDigits = fDecNumber->digits;
    if (decNumberIsZero(fDecNumber)) {
        // Account for difference in how zero is represented between DigitList & decNumber.
        adjustedDigits = 0;
    }
    fDecNumber->exponent = d - adjustedDigits;
    fHaveDouble = FALSE;
}

int32_t  
DigitList::getDecimalAt() {
    U_ASSERT((fDecNumber->bits & DECSPECIAL) == 0);  // Not Infinity or NaN
    if (decNumberIsZero(fDecNumber) || (fDecNumber->bits & DECSPECIAL != 0)) {
        return fDecNumber->exponent;  // Exponent should be zero for these cases.
    }
    return fDecNumber->exponent + fDecNumber->digits;
}

void     
DigitList::setCount(int32_t c)  {
    U_ASSERT(c <= fContext.digits);
    if (c == 0) {
        // For a value of zero, DigitList sets all fields to zero, while
        // decNumber keeps one digit (with that digit being a zero)
        c = 1;
        fDecNumber->lsu[0] = 0;
    }
    fDecNumber->digits = c;
    fHaveDouble = FALSE;
}

int32_t  
DigitList::getCount() {
    if (decNumberIsZero(fDecNumber) && fDecNumber->exponent==0) {
       // The extra test for exponent==0 is needed because parsing sometimes appends
       // zero digits.  It's bogus, decimalFormatter parsing needs to be cleaned up.
       return 0;
    } else {
       return fDecNumber->digits;
    }
}
    
void     
DigitList::setDigit(int32_t i, char v) {
    int32_t count = fDecNumber->digits;
    U_ASSERT(i<count);
    U_ASSERT(v>='0' && v<='9');
    v &= 0x0f;
    fDecNumber->lsu[count-i-1] = v;
    fHaveDouble = FALSE;
}

char     
DigitList::getDigit(int32_t i) {
    int32_t count = fDecNumber->digits;
    U_ASSERT(i<count);
    return fDecNumber->lsu[count-i-1] + '0';
}

// -------------------------------------
// Appends the digit to the digit list if it's not out of scope.
// Ignores the digit, otherwise.
// 
// This function is horribly inefficient to implement with decNumber because
// the digits are stored least significant first, which requires moving all
// existing digits down one to make space for the new one to be appended.
//
void
DigitList::append(char digit)
{
    U_ASSERT(digit>='0' && digit<='9');
    // Ignore digits which exceed the precision we can represent
    //    And don't fix for larger precision.  Fix callers instead.
    if (decNumberIsZero(fDecNumber)) {
        // Zero needs to be special cased because of the difference in the way
        // that the old DigitList and decNumber represent it.
        // digit cout was zero for digitList, is one for decNumber
        fDecNumber->lsu[0] = digit & 0x0f;
        fDecNumber->digits = 1;
        fDecNumber->exponent--;     // To match the old digit list implementation.
    } else {
        int32_t nDigits = fDecNumber->digits;
        if (nDigits < fContext.digits) {
            int i;
            for (i=nDigits; i>0; i--) {
                fDecNumber->lsu[i] = fDecNumber->lsu[i-1];
            }
            fDecNumber->lsu[0] = digit & 0x0f;
            fDecNumber->digits++;
            // DigitList emulation - appending doesn't change the magnitude of existing
            //                       digits.  With decNumber's decimal being after the
            //                       least signficant digit, we need to adjust the exponent.
            fDecNumber->exponent--;
        }
    }
    fHaveDouble = FALSE;
}

// -------------------------------------

/**
 * Currently, getDouble() depends on atof() to do its conversion.
 *
 * WARNING!!
 * This is an extremely costly function. ~1/2 of the conversion time
 * can be linked to this function.
 */
double
DigitList::getDouble() /*const*/
{
    if (fHaveDouble) {
        return fDouble;
    }

    if (gDecimal == 0) {
        char rep[MAX_DIGITS];
        // For machines that decide to change the decimal on you,
        // and try to be too smart with localization.
        // This normally should be just a '.'.
        sprintf(rep, "%+1.1f", 1.0);
        gDecimal = rep[2];
    }

    if (decNumberIsZero(fDecNumber)) {
        fDouble = 0.0;
        if (decNumberIsNegative(fDecNumber)) {
            fDouble /= -1;
        }
    }
    else {
        MaybeStackArray<char, MAX_DBL_DIGITS+18> s;
           // Note:  14 is a  magic constant from the decNumber library documentation,
           //        the max number of extra characters beyond the number of digits 
           //        needed to represent the number in string form.  Add a few more
           //        for the additional digits we retain.

        // Round down to appx. double precision, if the number is longer than that.
        // Copy the number first, so that we don't trash the original.
        reduce();    // Removes any trailing zeros, so that digit count is good.
        if (getCount() > MAX_DBL_DIGITS) {
            DigitList numToConvert(*this);
            numToConvert.round(MAX_DBL_DIGITS+3);
            uprv_decNumberToString(numToConvert.fDecNumber, s);
            // TODO:  how many extra digits should be included for an accurate conversion?
        } else {
            uprv_decNumberToString(this->fDecNumber, s);
        }
        
        if (gDecimal != '.') {
            char *decimalPt = strchr(s, '.');
            if (decimalPt != NULL) {
                *decimalPt = gDecimal;
            }
        }
        char *end = NULL;
        fDouble = uprv_strtod(s, &end);
    }
    fHaveDouble = TRUE;
    return fDouble;
}

// -------------------------------------

/**
 *  convert this number to an int32_t.   Round if there is a fractional part.
 *  Return zero if the number cannot be represented.
 */
int32_t DigitList::getLong() /*const*/
{
    if (fDecNumber->digits + fDecNumber->exponent > 10) {
        // Overflow, absolute value too big.
        return 0;
    }
    // TODO:  copy the number; do not trash the original.
    if (fDecNumber->exponent != 0) {
        // Force to an integer, with zero exponent, rounding if necessary.
        DigitList zero;
        uprv_decNumberQuantize(fDecNumber, fDecNumber, zero.fDecNumber, &fContext);
    }

    int32_t n = uprv_decNumberToInt32(fDecNumber, &fContext);
    return n;
}


/**
 *  convert this number to an int64_t.   Round if there is a fractional part.
 *  Return zero if the number cannot be represented.
 */
int64_t DigitList::getInt64() /*const*/ {
    // Round if non-integer.   (Truncate or round?)
    // Return 0 if out of range.
    // Range of in64_t is -9223372036854775808 to 9223372036854775807  (19 digits)
    //
    if (fDecNumber->digits + fDecNumber->exponent > 19) {
        // Overflow, absolute value too big.
        return 0;
    }
    if (fDecNumber->exponent != 0) {
        // Force to an integer, with zero exponent, rounding if necessary.
        DigitList zero;
        uprv_decNumberQuantize(fDecNumber, fDecNumber, zero.fDecNumber, &fContext);
    }

    uint64_t value = 0;
    int32_t numDigits =fDecNumber->digits;
    for (int i = numDigits-1; i>=0 ; --i) {
        int v = fDecNumber->lsu[i];
        value = value * (uint64_t)10 + (uint64_t)v;
    }
    if (decNumberIsNegative(fDecNumber)) {
        value = ~value;
        value += 1;
    }
    int64_t svalue = (int64_t)value;

    // Check overflow.  It's convenient that the MSD is 9 only on overflow, the amount of
    //                  overflow can't wrap too far.  The test will also fail -0, but
    //                  that does no harm; the right answer is 0.
    if (numDigits == 19) {
        if (( decNumberIsNegative(fDecNumber) && svalue>0) ||
            (!decNumberIsNegative(fDecNumber) && svalue<0)) {
            value = 0;
        }
    }
        
    return svalue;
}


/**
 *  Return a string form of this number.
 *     Format is as defined by the decNumber library, for interchange of
 *     decimal numbers.
 */
void DigitList::getDecimal(DecimalNumberString &str, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    
    // A decimal number in string form can, worst case, be 14 characters longer
    //  than the number of digits.  So says the decNumber library doc.
    int32_t maxLength = fDecNumber->digits + 15;
    str.setLength(maxLength, status);
    if (U_FAILURE(status)) {
        return;    // Memory allocation error on growing the string.
    }
    uprv_decNumberToString(this->fDecNumber, &str[0]);
    int32_t len = uprv_strlen(&str[0]);
    U_ASSERT(len <= maxLength);
    str.setLength(len, status);
}

/**
 * Return true if this is an integer value that can be held
 * by an int32_t type.
 */
UBool
DigitList::fitsIntoLong(UBool ignoreNegativeZero) /*const*/
{
    if (decNumberIsSpecial(this->fDecNumber)) {
        // NaN or Infinity.  Does not fit in int32.
        return FALSE;
    }
    uprv_decNumberTrim(this->fDecNumber);
    if (fDecNumber->exponent < 0) {
        // Number contains fraction digits.
        return FALSE;
    }
    if (decNumberIsZero(this->fDecNumber) && !ignoreNegativeZero &&
        (fDecNumber->bits & DECNEG) != 0) {
        // Negative Zero, not ingored.  Cannot represent as a long.
        return FALSE;
    }
    if (fDecNumber->digits + fDecNumber->exponent < 10) {
        // The number is 9 or fewer digits.
        // The max and min int32 are 10 digts, so this number fits.
        // This is the common case.
        return TRUE;
    }

    // TODO:  Need to cache these constants; construction is relatively costly.
    //        But not of huge consequence; they're only needed for 10 digit ints.
    UErrorCode status = U_ZERO_ERROR;
    DigitList min32; min32.set("-2147483648", status);
    if (this->compare(min32) < 0) {
        return FALSE;
    }
    DigitList max32; max32.set("2147483647", status);
    if (this->compare(max32) > 0) {
        return FALSE;
    }
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return true;
}



/**
 * Return true if the number represented by this object can fit into
 * a long.
 */
UBool
DigitList::fitsIntoInt64(UBool ignoreNegativeZero) /*const*/
{
    if (decNumberIsSpecial(this->fDecNumber)) {
        // NaN or Infinity.  Does not fit in int32.
        return FALSE;
    }
    uprv_decNumberTrim(this->fDecNumber);
    if (fDecNumber->exponent < 0) {
        // Number contains fraction digits.
        return FALSE;
    }
    if (decNumberIsZero(this->fDecNumber) && !ignoreNegativeZero &&
        (fDecNumber->bits & DECNEG) != 0) {
        // Negative Zero, not ingored.  Cannot represent as a long.
        return FALSE;
    }
    if (fDecNumber->digits + fDecNumber->exponent < 19) {
        // The number is 18 or fewer digits.
        // The max and min int64 are 19 digts, so this number fits.
        // This is the common case.
        return TRUE;
    }

    // TODO:  Need to cache these constants; construction is relatively costly.
    //        But not of huge consequence; they're only needed for 19 digit ints.
    UErrorCode status = U_ZERO_ERROR;
    DigitList min64; min64.set("-9223372036854775808", status);
    if (this->compare(min64) < 0) {
        return FALSE;
    }
    DigitList max64; max64.set("9223372036854775807", status);
    if (this->compare(max64) > 0) {
        return FALSE;
    }
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return true;
}


// -------------------------------------

void
DigitList::set(int32_t source, int32_t maximumDigits)
{
    set((int64_t)source, maximumDigits);

    if (maximumDigits == 0) {
        fDouble = source;
        fHaveDouble = TRUE;
    }
}

// -------------------------------------
/**
 * @param maximumDigits The maximum digits to be generated.  If zero,
 * there is no maximum -- generate all digits.
 */
void
DigitList::set(int64_t source, int32_t maximumDigits)
{
    char str[MAX_DIGITS+2];   // Leave room for sign and trailing nul.
    formatBase10(source, str);

    if (maximumDigits == 0) {
        maximumDigits = MAX_DIGITS;
        // MAX_DIGITS is 19, big enough for a complete int64_t
    }

    int32_t savedDigits = fContext.digits;
    fContext.digits = maximumDigits;
    uprv_decNumberFromString(fDecNumber, str, &fContext);
    fContext.digits = savedDigits;
    fHaveDouble = FALSE;
}


// -------------------------------------
/**
 * Set the DigitList from a decimal number string.
 * TODO:  sort out terminated vs non-terminated strings.
 *        decNumber wants terminated, is not easily changed.
 *        StringPiece doesn't know about terminated.
 */
void
DigitList::set(StringPiece source, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    // Figure out a max number of digits to use during the conversion, and
    // resize the number up if necessary.
    int32_t numDigits = source.length();
    if (numDigits > fContext.digits) {
        fContext.digits = numDigits;
        char *t = fStorage.resize(sizeof(decNumber) + numDigits, fStorage.getCapacity());
        if (t == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        fDecNumber = (decNumber *)fStorage.getAlias();
    }
        
    uprv_decNumberFromString(fDecNumber, source.data(), &fContext);
    // TODO:  check for conversion errors, set status.
    fHaveDouble = FALSE;
}   

/**
 * Set the digit list to a representation of the given double value.
 * This method supports both fixed-point and exponential notation.
 * @param source Value to be converted.
 * @param maximumDigits The most fractional or total digits which should
 * be converted.  If total digits, and the value is zero, then
 * there is no maximum -- generate all digits.
 * @param fixedPoint If true, then maximumDigits is the maximum
 * fractional digits to be converted.  If false, total digits.
 */
void
DigitList::set(double source, int32_t maximumDigits, UBool fixedPoint)
{
    // for now, simple implementation; later, do proper IEEE stuff
    char rep[MAX_DIGITS + 8]; // Extra space for '+', '.', e+NNN, and '\0' (actually +8 is enough)

    // fIsPositive = !uprv_isNegative(source);    // Allow +0 and -0

    // Generate a representation of the form /[+-][0-9]+e[+-][0-9]+/
    sprintf(rep, "%+1.*e", MAX_DBL_DIGITS - 1, source);

    // Create a decNumber from the string.
    uprv_decNumberFromString(fDecNumber, rep, &fContext);
    uprv_decNumberTrim(fDecNumber);

    fDouble = source;
    fHaveDouble = TRUE;

    if (fixedPoint && (fDecNumber->exponent < -maximumDigits)) {
        // Fixed point rounding needed.
        decNumber  t;     // Temporary 1 digit decimal number w/ right no. of fraction digits.
        uprv_decNumberZero(&t);
        t.exponent = -maximumDigits;
        t.lsu[0] = 1;
        uprv_decNumberQuantize(fDecNumber, fDecNumber, &t, &fContext);   // Do the rounding.
        uprv_decNumberTrim(fDecNumber);
        fHaveDouble = FALSE;
    }

    if (!fixedPoint && maximumDigits > 0 && maximumDigits < fDecNumber->digits) {
        round(maximumDigits);
        fHaveDouble = FALSE;
    }
}

// -------------------------------------

/**
 * Round the representation to the given number of digits.
 * @param maximumDigits The maximum number of digits to be shown.
 * Upon return, count will be less than or equal to maximumDigits.
 */
void
DigitList::round(int32_t maximumDigits)
{
    int32_t savedDigits  = fContext.digits;
    fContext.digits = maximumDigits;
    uprv_decNumberPlus(fDecNumber, fDecNumber, &fContext);
    fContext.digits = savedDigits;
    uprv_decNumberTrim(fDecNumber);
    fHaveDouble = FALSE;
}


// -------------------------------------

void
DigitList::toIntegralValue() {
    uprv_decNumberToIntegralValue(fDecNumber, fDecNumber, &fContext);
}


// -------------------------------------
UBool
DigitList::isZero() const
{
    return decNumberIsZero(fDecNumber);
}

U_NAMESPACE_END
#endif // #if !UCONFIG_NO_FORMATTING

//eof
