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

U_NAMESPACE_END

#endif  /* USPOOFIM_H */

