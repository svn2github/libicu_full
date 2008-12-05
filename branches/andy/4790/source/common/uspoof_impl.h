/*
***************************************************************************
* Copyright (C) 2008, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*
*  uspoofim.h
*
*    Implemenation header for spoof detection
*
*/

#ifndef USPOOFIM_H
#define USPOOFIM_H

#include "unicode/utypes.h"
#include "unicode/uspoof.h"

U_NAMESPACE_BEGIN

// The maximium length of the skeleton replacement string resulting from
//   a single input code point.
static const int32_t MAX_SKELETON_EXPANSION = 20;


class SpoofImpl : public UObject  {
public:
	SpoofImpl(UErrorCode &status);
	SpoofImpl();
	virtual ~SpoofImpl();


	static SpoofImpl *validateThis(USpoofChecker *sc, UErrorCode &status);
	static const SpoofImpl *validateThis(const USpoofChecker *sc, UErrorCode &status);

	/** Get the skeleton transform for a single code point.
	 *  The result is a string with a length between 1 and 18.
	 *  @return   The length in code points of the substition string.
	 */  
	int32_t ToSkeleton(UChar32 inChar, UChar32 *destBuf) const;
	

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

	int32_t   fMagic;             // Internal sanity check.
	int32_t   fChecks;            // Bit vector of checks to perform.
    const UnicodeSet *fAllowedCharsSet; 

};



//
//  Data Wrapper & access functions
class SpoofData {
  public:
    // Map a single code point to a target string, as described section 4 of UTR 39.
    // 
    const UChar32 *GetMapping(UChar srcChar, USpoofChecks mappingTable, int32_t &resultLength);
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


//
//  For the data defined in data file "confusables.txt", we need a mapping from
//      (code point) -> (string, flags)
//  the string will most commonly be only a single char in length.  The longest is 18 chars.

struct SpoofStringLengthsElement {
    uint16_t      fLastString;         // index in string table of last string with this length
    uint16_t      strLength;           // Length of strings
};

   //       StringLengthsTable[n]


//        uint32_t keysTable[n]
//             // bits 0-23    a code point value
//             // bits 24-31   flags
//             //    24:  1 if entry applies to SL table
//             //    25:  1 if entry applies to SA table
//             //    26:  1 if entry applies to ML table
//             //    27:  1 if entry applies to MA table
//             //    28:  1 if there are multiple entries for this code point.
//             // Keytable is sorted in ascending code point order.
//
//        uint16_t stringIndexTable[n]
//             //  parallel table to keyTable, above.
//
//        UChar stringTable[o]
//             //  All replacement chars or strings, all concatenated together.



U_NAMESPACE_END

#endif  /* USPOOFIM_H */

