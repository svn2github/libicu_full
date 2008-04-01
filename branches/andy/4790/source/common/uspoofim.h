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


class SpoofImpl : public UObject  {
public:
	SpoofImpl(UErrorCode &status);
	SpoofImpl();
	virtual ~SpoofImpl();


	static SpoofImpl *validateThis(USpoofChecker *sc, UErrorCode &status);
	static const SpoofImpl *validateThis(const USpoofChecker *sc, UErrorCode &status);

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
// Data Structures for Confusable Mappings
//

// There are four tables, as described in UAX39, 
//    Single-Script, Lowercase
//    Single-Script, Any-Case
//    Mixed-Script,  Lowercase
//    Mixed-Script,  Any-Case
//
// The purpose of each is to map a single input character
//   to a replacement string, often also a single char.
// Each table has three parts,
//   BMP table
//   Supplemental Table
//   Replacement string table

//
//  BMP confusable mapping table.
//      If the mapping is to a single BMP character,
//        it will be in the mappedChar field.
//      If the mapping is to a string, or to a
//        supplementary char, the mapped char will have
//        an index in the surrogate range 0xd800-0xdfff,
//        and (mappedChar - 0xd800) will be an index into
//        the string table.
struct BMPConfusableTableRow {
   UChar  srcChar;
   UChar  mappedChar;
};

// 
//  The BMP ConfusableTable is an array of rows,
//    sorted by the srcChar.
//
//BMPConfusableTableRow  *BMPConfusableTable; 


//
//  Supplemental Table.  Each entry is a uint32, split as follows:
//      bits 0-19   The supplemental code point value - 0x10000
//                  (BMP values are not in this table)
//      bits 20-31  Index into string table, which will
//                  contain the mapping for this char.
//   The table is sorted by source character value
//         (bits 0-19)
//
//uint32_t  *SupplementalConfusableTable;

//
//   String 

U_NAMESPACE_END

#endif  /* USPOOFIM_H */

