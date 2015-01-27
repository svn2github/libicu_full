/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* significantdigitinterval.h
*
* created on: 2015jan6
* created by: Travis Keep
*/

#ifndef __SIGNIFICANTDIGITINTERVAL_H__
#define __SIGNIFICANTDIGITINTERVAL_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

/**
 * An interval of allowed significant digit counts.
 */
class U_I18N_API SignificantDigitInterval : public UMemory {
public:

    /**
     * No limits on significant digits.
     */
    SignificantDigitInterval()
            : fMax(INT32_MAX), fMin(0) { }

    /**
     * Sets maximum significant digits. 0 or negative means no maximum.
     */
    void setMax(int32_t count) {
        fMax = count <= 0 ? INT32_MAX : count;
    }

    /**
     * Get maximum significant digits. INT32_MAX means no maximum.
     */
    int32_t getMax() const {
        return fMax;
    }

    /**
     * Sets minimum significant digits. 0 or negative means no minimum.
     */
    void setMin(int32_t count) {
        fMin = count <= 0 ? 0 : count;
    }

    /**
     * Get maximum significant digits. 0 means no minimum.
     */
    int32_t getMin() const {
        return fMin;
    }

private:
    int32_t fMax;
    int32_t fMin;
};

U_NAMESPACE_END

#endif  // __SIGNIFICANTDIGITINTERVAL_H__
