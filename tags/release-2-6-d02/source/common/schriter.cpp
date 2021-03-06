/*
******************************************************************************
* Copyright (C) 1998-2001, International Business Machines Corporation and   *
* others. All Rights Reserved.                                               *
******************************************************************************
*
* File schriter.cpp
*
* Modification History:
*
*   Date        Name        Description
*  05/05/99     stephen     Cleaned up.
******************************************************************************
*/

#include "unicode/chariter.h"
#include "unicode/schriter.h"

U_NAMESPACE_BEGIN

const char StringCharacterIterator::fgClassID = 0;

StringCharacterIterator::StringCharacterIterator()
  : UCharCharacterIterator(),
    text()
{
  // NEVER DEFAULT CONSTRUCT!
}

StringCharacterIterator::StringCharacterIterator(const UnicodeString& textStr)
  : UCharCharacterIterator(textStr.fArray, textStr.length()),
    text(textStr)
{
    // we had set the input parameter's array, now we need to set our copy's array
    UCharCharacterIterator::text = this->text.fArray;
}

StringCharacterIterator::StringCharacterIterator(const UnicodeString& textStr,
                                                 int32_t textPos)
  : UCharCharacterIterator(textStr.fArray, textStr.length(), textPos),
    text(textStr)
{
    // we had set the input parameter's array, now we need to set our copy's array
    UCharCharacterIterator::text = this->text.fArray;
}

StringCharacterIterator::StringCharacterIterator(const UnicodeString& textStr,
                                                 int32_t textBegin,
                                                 int32_t textEnd,
                                                 int32_t textPos)
  : UCharCharacterIterator(textStr.fArray, textStr.length(), textBegin, textEnd, textPos),
    text(textStr)
{
    // we had set the input parameter's array, now we need to set our copy's array
    UCharCharacterIterator::text = this->text.fArray;
}

StringCharacterIterator::StringCharacterIterator(const StringCharacterIterator& that)
  : UCharCharacterIterator(that),
    text(that.text)
{
    // we had set the input parameter's array, now we need to set our copy's array
    UCharCharacterIterator::text = this->text.fArray;
}

StringCharacterIterator::~StringCharacterIterator() {
}

StringCharacterIterator&
StringCharacterIterator::operator=(const StringCharacterIterator& that) {
    UCharCharacterIterator::operator=(that);
    text = that.text;
    // we had set the input parameter's array, now we need to set our copy's array
    UCharCharacterIterator::text = this->text.fArray;
    return *this;
}

UBool
StringCharacterIterator::operator==(const ForwardCharacterIterator& that) const {
    if (this == &that) {
        return TRUE;
    }

    // do not call UCharCharacterIterator::operator==()
    // because that checks for array pointer equality
    // while we compare UnicodeString objects

    if (getDynamicClassID() != that.getDynamicClassID()) {
        return FALSE;
    }

    StringCharacterIterator&    realThat = (StringCharacterIterator&)that;

    return text == realThat.text
        && pos == realThat.pos
        && begin == realThat.begin
        && end == realThat.end;
}

CharacterIterator*
StringCharacterIterator::clone() const {
    return new StringCharacterIterator(*this);
}

void
StringCharacterIterator::setText(const UnicodeString& newText) {
    text = newText;
    UCharCharacterIterator::setText(text.fArray, text.length());
}

void
StringCharacterIterator::getText(UnicodeString& result) {
    result = text;
}
U_NAMESPACE_END
