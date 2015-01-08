/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitgrouping.h
*
* created on: 2015jan6
* created by: Travis Keep
*/

#ifndef __DIGITGROUPING_H__
#define __DIGITGROUPING_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

/**
 * Handles grouping when formatting a number.
 */
class U_I18N_API DigitGrouping : public UMemory {
public:
    DigitGrouping() : fGrouping(0), fGrouping2(0), fMinGrouping(0) { }

    /**
     * Returns true if a separator is needed after a particular digit.
     * @param digitPos 0 is the one's place; 1 is the 10's place; -1 is the
     *   1/10's place etc.
     */
    UBool isSeparatorAt(int32_t digitsLeftOfDecimal, int32_t digitPos) const;

    /**
     * Returns the total number of separators to be used to format a particular
     * number.
     * @param digitsLeftOfDecimal the total number of digits to the left of
     *   the decimal.
     */
    int32_t getSeparatorCount(int32_t digitsLeftOfDecimal) const;

    /**
     * Returns true if grouping is used FALSE otherwise. When
     * isGroupingUsed() returns FALSE; isSeparatorAt always returns FALSE
     * and separatorCount always returns 0.
     */
    UBool isGroupingUsed() const { return fGrouping > 0; }

public:

    /**
     * Primary grouping size. A value of 0, the default, or a negative
     * number causes isGroupingUsed() to return FALSE.
     */
    int32_t fGrouping;

    /**
     * Secondary grouping size. If > 0, this size is used instead of
     * 'fGrouping' for all but the group just to the left of the decimal
     * point. The default value of 0, or a negative value indicates that
     * there is no secondary grouping size.
     */
    int32_t fGrouping2;

    /**
     * If set (that is > 0), uses no grouping separators if fewer than
     * (fGrouping + fMinGrouping) digits appear left of the decimal place.
     * The default value for this field is 0.
     */
    int32_t fMinGrouping;
private:
    UBool isGroupingEnabled(int32_t digitsLeftOfDecimal) const;
    int32_t getGrouping2() const;
    int32_t getMinGrouping() const;
};

U_NAMESPACE_END

#endif  // __DIGITGROUPING_H__
