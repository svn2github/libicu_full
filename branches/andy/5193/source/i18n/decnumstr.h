/*
******************************************************************************
*
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File decnumstr.h
*
* A simple eight bit char string class.  
* Used by decimal number formatting to hold the string form of numbers.
*
* For internal ICU use only.  Not public API.
*
* TODO:  ICU should have a light-weight general purpose (char *) string class
*        available for internal use; this would eliminate the
*        need for this class.
*/

#ifndef DECNUMSTR_H
#define DECNUMSTR_H

#include "unicode/utypes.h"
#include "unicode/stringpiece.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

class DecimalNumberString: public UMemory {
public:
    DecimalNumberString();
    ~DecimalNumberString();

    DecimalNumberString(const StringPiece &);

    DecimalNumberString &append(char);
    DecimalNumberString &append(const StringPiece &);
    char &operator[] (int32_t index);
    const char &operator[] (int32_t index) const;
    int32_t  length() const;
    void     setLength(int32_t length);
    operator StringPiece() const;
  private:
    int32_t  fLength;
    MaybeStackArray<char, 40> fText;
};

U_NAMESPACE_END

#endif  // DECNUMSTR_H

