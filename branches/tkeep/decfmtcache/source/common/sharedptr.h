#ifndef __SHARED_PTR_H__
#define __SHARED_PTR_H__

#include "unicode/uobject.h"
#include "umutex.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

template<typename T>
class SharedPtr : public UMemory {
public:
    explicit SharedPtr(T *p=NULL) : ptr(p), refPtr(NULL) {
        if (ptr != NULL) {
            refPtr = new u_atomic_int32_t;
            umtx_storeRelease(*refPtr, 1);
        }
    }

    SharedPtr(T *p, u_atomic_int32_t *rp) : ptr(p), refPtr(rp) {
        U_ASSERT(p != NULL && rp != NULL);
        umtx_atomic_inc(refPtr);
    }
        

    SharedPtr(const SharedPtr<T> &other) :
            ptr(other.ptr), refPtr(other.refPtr) {
        if (refPtr != NULL) {
            umtx_atomic_inc(refPtr);
        }
    }

    SharedPtr<T> &operator=(const SharedPtr<T> &other) {
        SharedPtr<T> newValue(other);
        swap(newValue);
        return *this;
    }

    ~SharedPtr() {
        if (refPtr != NULL) {
            if (umtx_atomic_dec(refPtr) == 0) {
                delete ptr;
                delete refPtr;
            }
        }
    }

    void adopt(T *p) {
        SharedPtr<T> newValue(p);
        swap(newValue);
    }

    int32_t count() const {
        if (refPtr == NULL) {
            return 0;
        }
        return umtx_loadAcquire(*refPtr);
    }

    void swap(SharedPtr<T> &other) {
        T *tempPtr = other.ptr;
        u_atomic_int32_t *tempRefPtr = other.refPtr;
        other.ptr = ptr;
        other.refPtr = refPtr;
        ptr = tempPtr;
        refPtr = tempRefPtr;
    }

    const T *readOnly() const {
        return ptr;
    }

    T *readWrite() {
        if (refPtr == NULL) {
            return NULL;
        }
        if (umtx_loadAcquire(*refPtr) == 1) {
            return ptr;
        }
        T *result = (T *) ptr->clone();
        adopt(result);
        return ptr;
    }
private:
    T *ptr;
    u_atomic_int32_t *refPtr;
};

U_NAMESPACE_END

#endif
