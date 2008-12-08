/*
***************************************************************************
* Copyright (C) 2008, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*
*  uspoof_impl.h
*
*    Implemenation header for spoof detection
*
*/

#ifndef USPOOFIM_H
#define USPOOFIM_H

#include "unicode/utypes.h"
#include "unicode/uspoof.h"

U_NAMESPACE_BEGIN

// The maximium length (in UTF-16 UChars) of the skeleton replacement string resulting from
//   a single input code point.  This is function of the unicode.org data.
#define USPOOF_MAX_SKELETON_EXPANSION 20

// The default stack buffer size for the NFKD normalization of
//   input strings to be checked.  Longer strings require allocation
//   of a heap buffer.
#define USPOOF_STACK_BUFFER_SIZE 100

class SpoofData;
class SpoofDataHeader;
class SpoofStringLengthsElement;

class SpoofImpl : public UObject  {
public:
	SpoofImpl(UErrorCode &status);
	SpoofImpl();
	virtual ~SpoofImpl();


	static SpoofImpl *validateThis(USpoofChecker *sc, UErrorCode &status);
	static const SpoofImpl *validateThis(const USpoofChecker *sc, UErrorCode &status);

	/** Get the confusable skeleton transform for a single code point.
	 *  The result is a string with a length between 1 and 18.
	 *  @return   The length in UTF-16 code units of the substition string.
	 */  
	int32_t ConfusableLookup(UChar32 inChar, UChar *destBuf) const;
	

    /**
     * Return the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     * <pre>
     * .      Base* polymorphic_pointer = createPolymorphicObject();
     * .      if (polymorphic_pointer->getDynamicClassID() ==
     * .          Derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @stable ICU 2.0
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Return a polymorphic class ID for this object. Different subclasses
     * will return distinct unequal values.
     * @stable ICU 2.0
     */
    virtual UClassID getDynamicClassID(void) const;

	//
	// Data Members
	//

	int32_t           fMagic;             // Internal sanity check.
	int32_t           fChecks;            // Bit vector of checks to perform.

	SpoofData        *fSpoofData;
	
	int32_t           fCheckMask;         // Spoof table selector.  f(Check Type)
	
    const UnicodeSet *fAllowedCharsSet; 

};



//
//  Confusable Mappings Data Structures
//
//    For the confusable data, we are essentially implementing a map,
//       key:    a code point
//       value:  a string.  Most commonly one char in length, but can be more.
//
//    The keys are stored as a sorted array of 32 bit ints.
//             bits 0-23    a code point value
//             bits 24-31   flags
//                24:  1 if entry applies to SL table
//                25:  1 if entry applies to SA table
//                26:  1 if entry applies to ML table
//                27:  1 if entry applies to MA table
//                28:  1 if there are multiple entries for this code point.
//                29-30:  length of value string, in UChars.
//                         values are (1, 2, 3, other)
//        The key table is sorted in ascending code point order.  (not on the
//        32 bit int value, the flag bits do not participate in the sorting.)
//
//        Lookup is done by means of a binary search in the key table.
//
//    The corresponding values are kept in a parallel array of 16 bit ints.
//        If the value string is of length 1, it is literally in the value array.
//        For longer strings, the value array contains an index into the strings table.
//
//    String Table:
//       The strings table contains all of the value strings (those of length two or greater)
//       concatentated together into one long UChar (UTF-16) array.
//
//       The array is arranged by length of the strings - all strings of the same length
//       are stored together.  The sections are ordered by length of the strings -
//       all two char strings first, followed by all of the three Char strings, etc.
//
//       There is no nul character or other mark between adjacent strings.
//
//    String Lengths table
//       The length of strings from 1 to 3 is flagged in the key table.
//       For strings of length 4 or longer, the string length table provides a
//       mapping between an index into the string table and the corresponding length.
//       Strings of these lengths are rare, so lookup time is not an issue.
//

// Flag bits in the Key entries
#define USPOOF_SL_TABLE_FLAG (1<<24)
#define USPOOF_SA_TABLE_FLAG (1<<25)
#define USPOOF_ML_TABLE_FLAG (1<<26)
#define USPOOF_MA_TABLE_FLAG (1<<27)
#define USPOOF_KEY_MULTIPLE_VALULES (1<<28)
#define USPOOF_KEY_LENGTH_FIELD(x) (((x)>>29) & 3)



struct SpoofStringLengthsElement {
    uint16_t      fLastString;         // index in string table of last string with this length
    uint16_t      strLength;           // Length of strings
};


//
//  Spoof Data Wrapper
//
//    A small struct that wraps the raw (usually memory mapped) spoof data.
//    Serves two functions:
//      1.  Convenience.  Contains real pointers to the data, to avoid dealing with
//          the offsets in the raw data.
//      2.  Reference counting.  When a spoof checker is cloned, the raw data is shared
//          and must be retained until all checkers using the data are closed.
//    Nothing in this struct includes state that is specific to any particular
//    USpoofDetector object.
//
class SpoofData {
  public:
    SpoofDataHeader            *fRawData;          // Ptr to the raw memory-mapped data
    
    int32_t                    *fKeys;
    uint16_t                   *fValues;
    SpoofStringLengthsElement   *fStringLengths;
    UChar                      *fStrings;

    };
    

struct SpoofDataHeader {
    int32_t       fMagic;              // (0x8345fdef)
    int32_t       fLength;             // sizeof(SpoofDataHeader)

    // The following four sections refer to data representing the confusable data
    //   from the Unicode.org data from "confusables.txt"

    int32_t       fStringLengths;      // byte offset to String Lengths table
    int32_t       fStringLengthsSize;  // number of entries in lengths table. (2 x 16 bits each)

    int32_t       fKeys;               // byte offset to Keys table
    int32_t       fKeysSize;           // number of entries in keys table  (32 bits each)

    int32_t       fStringIndex;        // byte offset to String Indexes table
    int32_t       fStringIndexSize;    // number of entries in String Indexes table (16 bits each)
                                       //     (number of entries must be same as in Keys table

    int32_t       fStringTable;        // byte offset of String table
    int32_t       fStringTableLen;     // length of string table (in 16 bit UChars)

    int32_t       unused[6];           // Padding, Room for Expansion
 }; 


U_NAMESPACE_END

#endif  /* USPOOFIM_H */

