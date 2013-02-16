/*
******************************************************************************
*
*   Copyright (C) 1997-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File UVECTOR_BASE.H
*
*  Contains VectorBase template.
*
* @author       Travis Keep
*
* Modification History:
*
*   Date        Name        Description
******************************************************************************
*/

#ifndef UVECTOR_BASE_H
#define UVECTOR_BASE_H

#include "cmemory.h"

U_NAMESPACE_BEGIN

template<typename T>
class UVectorBase {
public:
    /**
     * Empty vector with initial capacity of 16 elements.
     */
    UVectorBase() : stack(), len(0) {}

    UVectorBase(int32_t initialCapacity, UErrorCode &status) {
        if (U_FAILURE(status)) {
            return;
        }
        if (stack.resize(initialCapacity) == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }

    ~UVectorBase() { }

    void addElement(T elem, UErrorCode &status) {
        if (U_FAILURE(status)) {
            return;
        }
        if (len == stack.getCapacity()) {
            if (stack.resize(2 * len, len) == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        }
        stack[len++] = elem;
    }

    T elementAti(int32_t index) const {
        if (index >= len || index < 0) {
            return 0;
        }
        return stack[len];
    }

    void setElementAt(T elem, int32_t index) {
        if (index >= len || index < 0) {
            return;
        }
        stack[index] = elem;
    }
private:
    MaybeStackArray<T, 16> stack;
    int32_t len
}

U_NAMESPACE_END

#endif  /* UVECTOR_BASE_H */
