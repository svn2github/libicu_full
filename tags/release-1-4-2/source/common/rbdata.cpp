/*
 **********************************************************************
 *   Copyright (C) 1998-1999, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
*
* File rbdata.cpp
*
* Modification History:
*
*   Date        Name        Description
*   06/11/99    stephen     Creation. (Moved here from resbund.cpp)
*******************************************************************************
*/

#include "rbdata.h"

UClassID StringList::fgClassID = 0; // Value is irrelevant
UClassID String2dList::fgClassID = 0; // Value is irrelevant
UClassID TaggedList::fgClassID = 0; // Value is irrelevant

//-----------------------------------------------------------------------------

StringList::StringList()
  : fStrings(0), fCount(0) 
{}

StringList::StringList(UnicodeString *adopted, 
		       int32_t count) 
  : fStrings(adopted), fCount(count) 
{}

StringList::~StringList() 
{ delete [] fStrings; }
  
const UnicodeString& 
StringList::operator[](int32_t i) const 
{ return fStrings[i]; }

UClassID 
StringList::getDynamicClassID() const 
{ return getStaticClassID(); }

UClassID 
StringList::getStaticClassID() 
{ return (UClassID)&fgClassID; }

//-----------------------------------------------------------------------------

String2dList::String2dList() 
  : fStrings(0), fRowCount(0), fColCount(0) 
{}
  
String2dList::String2dList(UnicodeString **adopted, 
			   int32_t rowCount, 
			   int32_t colCount) 
  : fStrings(adopted), fRowCount(rowCount), fColCount(colCount) 
{}

String2dList::~String2dList() 
{ 
  for(int32_t i = 0; i < fRowCount; ++i) {
    delete[] fStrings[i]; 
  }
}
  
const UnicodeString& 
String2dList::getString(int32_t rowIndex, 
			int32_t colIndex) 
{ return fStrings[rowIndex][colIndex]; }
  
UClassID 
String2dList::getDynamicClassID() const 
{ return getStaticClassID(); }

UClassID 
String2dList::getStaticClassID() 
{ return (UClassID)&fgClassID; }

//-----------------------------------------------------------------------------

TaggedList::TaggedList()
{
  UErrorCode err = U_ZERO_ERROR;
  fHashtableValues = uhash_open((UHashFunction)uhash_hashUString, &err);
  uhash_setValueDeleter(fHashtableValues, deleteValue);
  
  fHashtableKeys = uhash_open((UHashFunction)uhash_hashUString, &err);
}
  
TaggedList::~TaggedList()
{
  uhash_close(fHashtableValues);
  uhash_close(fHashtableKeys);
}

void 
TaggedList::put(const UnicodeString& tag, 
		const UnicodeString& data)
{
  UErrorCode err = U_ZERO_ERROR;
  
  uhash_putKey(fHashtableValues, 
	       tag.hashCode() & 0x7FFFFFFF,
	       (new UnicodeString(data)),
	       &err);
  
  uhash_putKey(fHashtableKeys,
	       uhash_size(fHashtableValues),
	       (new UnicodeString(tag)),
	       &err);
}

const UnicodeString* 
TaggedList::get(const UnicodeString& tag) const
{
  return (const UnicodeString*)
    uhash_get(fHashtableValues, tag.hashCode() & 0x7FFFFFFF);
}

void
TaggedList::deleteValue(void *value)
{
  delete (UnicodeString*)value;
}

  
UClassID 
TaggedList::getDynamicClassID() const 
{ return getStaticClassID(); }

UClassID 
TaggedList::getStaticClassID() 
{ return (UClassID)&fgClassID; }
