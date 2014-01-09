/*
**********************************************************************
* Copyright (c) 2004-2006, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#ifndef __MEASUREUNIT_H__
#define __MEASUREUNIT_H__

#include "unicode/utypes.h"
#include "unicode/unistr.h"

/**
 * \file 
 * \brief C++ API: A unit for measuring a quantity.
 */
 
U_NAMESPACE_BEGIN

/**
 * A unit such as length, mass, volume, currency, etc.  A unit is
 * coupled with a numeric amount to produce a Measure.
 *
 * @author Alan Liu
 * @stable ICU 3.0
 */
class U_I18N_API MeasureUnit: public UObject {
 public:

    /**
     * Copy constructor.
     * @draft ICU 53
     */
    MeasureUnit(const MeasureUnit &other);

    /**
     * Assignment operator.
     * @draft ICU 53.
     */
    MeasureUnit &operator=(const MeasureUnit &other);

    /**
     * Return a polymorphic clone of this object.  The result will
     * have the same class as returned by getDynamicClassID().
     * @stable ICU 3.0
     */
    virtual UObject* clone() const;

    /**
     * Destructor
     * @stable ICU 3.0
     */
    virtual ~MeasureUnit();

    /**
     * Equality operator.  Return true if this object is equal
     * to the given object.
     * @stable ICU 3.0
     */
    virtual UBool operator==(const UObject& other) const;

    /**
     * Get the type.
     * @draft ICU 53
     */
    const UnicodeString &getType() const {
        return fType;
    }

    /**
     * Get the sub type.
     * @draft ICU 53
     */
    const UnicodeString &getSubtype() const {
        return fSubType;
    }

    /**
     * getAvailable gets all of the available units.
     * getAvailable creates a new MeasureUnit object on the heap for each
     * available unit and stores the pointers in dest.
     * If there are too many units to fit into destCapacity then the
     * error code is set to U_BUFFER_OVERFLOW_ERROR.
     *
     * On success, the caller owns all the allocated MeasureUnit objects
     * and must free each one. On failure, getAvailable will free any
     * allocated MeasureUnit objects before returning.
     *
     * @param destCapacity number of const pointers available at dest.
     * @param dest destination buffer.
     * @param errorCode ICU error code.
     * @return number of available units.
     * @draft ICU 53
     */
    static int32_t getAvailable(
            int32_t destCapacity,
            MeasureUnit **dest,
            UErrorCode &errorCode);

    /**
     * getAvailable gets all of the available units for a specific type.
     * getAvailable creates a new MeasureUnit object on the heap for each
     * available unit and stores the pointers in dest.
     * If there are too many units to fit into destCapacity then the
     * error code is set to U_BUFFER_OVERFLOW_ERROR.
     *
     * On success, the caller owns all the allocated MeasureUnit objects
     * and must free each one. On failure, getAvailable will free any
     * allocated MeasureUnit objects before returning.
     *
     * @param type the type
     * @param destCapacity number of const pointers available at dest.
     * @param dest destination buffer.
     * @param errorCode ICU error code.
     * @return number of available units for type.
     * @draft ICU 53
     */
    static int32_t getAvailable(
            const UnicodeString &type,
            int32_t destCapacity,
            MeasureUnit **dest,
            UErrorCode &errorCode);

    /**
     * getAvailableTypes gets all of the available types.
     * getAvailableTypes copies each type to dest.
     * If there are too many types to fit into destCapacity then the
     * error code is set to U_BUFFER_OVERFLOW_ERROR.
     *
     * @param dest destination buffer.
     * @param destCapacity number of const pointers available at dest.
     * @param errorCode ICU error code.
     * @return number of types.
     * @draft ICU 53
     */
    static int32_t getAvailableTypes(
            int32_t destCapacity,
            UnicodeString *dest,
            UErrorCode &errorCode);

    /**
     * CreateGForce returns a MeasureUnit representing GForce.
     * Caller owns returned value.
     * @draft ICU 53.
     */
    static MeasureUnit *createGForce(UErrorCode &status);

    /**
     * CreateGForce returns a MeasureUnit representing year.
     * Caller owns returned value.
     * @draft ICU 53.
     */
    static MeasureUnit *createYear(UErrorCode &status);

    /**
     * For ICU use only.
     * @internal ICU 53
     */
    MeasureUnit(const UnicodeString &type, const UnicodeString &subType);

 protected:
    /**
     * Default constructor.
     * @stable ICU 3.0
     */
    MeasureUnit();

    /**
     * For ICU use only.
     * @internal ICU 53
     */
    MeasureUnit(int32_t typeId, int32_t subTypeId);

    /**
     * For ICU use only.
     * @internal ICU 53
     */
    MeasureUnit(int32_t typeId, const UnicodeString &subType);

private:
    UnicodeString fType;
    UnicodeString fSubType;
    static MeasureUnit *create(int32_t typeId, int32_t subTypeId, UErrorCode &status);
};

U_NAMESPACE_END

#endif // __MEASUREUNIT_H__
