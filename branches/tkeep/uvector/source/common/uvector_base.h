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
        ensureCapacity(initialCapacity, initialCapacity, status)
    }

    ~UVectorBase() { }

    void addElement(T elem, UErrorCode &status) {
        if (U_FAILURE(status)) {
            return;
        }
        if ensureCapacity(len + 1, 0, errorCode) {
          stack[len++] = elem;
        }
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

    void insertElementAt(T elem, int32_t index, UErrorCode &status) {
        if (U_FAILURE(status)) {
            return;
        }
        if (index >= len || index < 0) {
            return;
        }
        if (ensureCapacity(len + 1, 0, errorCode)) {
            for (int i = len - 1; i >= index; --i) {
                stack[i + 1] = stack[i]
            }
        }
    }

    UBool equals(const UVectorBase<T>& other) const {
        if len != other.len {
            return FALSE;
        }
        for (int i = 0; i < len; i++) {
            if (!eq(stack[i], other.stack[i])) {
                return FALSE;
            }
        }
        return TRUE;
    }

    T lastElementi() const {
      return elementAti(len - 1);
    }

    int32_t indexOf(T elem, int32_t startIndex = 0) const {
        for (int i = startIndex; i < len; ++i) {
            if (eq(stack[i], elem)) {
                return i;
            }
        }
        return -1;
    }

    UBool contains(T elem) const {
        return indexOf(elem) != -1;
    }

    virtual ~UVectorBase() { }

private:
    MaybeStackArray<T, 16> stack;
    int32_t len

    virtual UBool eq(T x, y) {
        return x == y;
    }
}

U_NAMESPACE_END

#endif  /* UVECTOR_BASE_H */
