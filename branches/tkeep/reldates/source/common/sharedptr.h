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

    SharedPtr(const SharedPtr<T> &other) :
            ptr(other.ptr), refPtr(other.refPtr) {
        if (refPtr != NULL) {
            umtx_atomic_inc(refPtr);
        }
    }

    template<typename U>
    SharedPtr(const SharedPtr<U> &other) :
            ptr((T *) other.ptr), refPtr(other.refPtr) {
        if (refPtr != NULL) {
            umtx_atomic_inc(refPtr);
        }
    }

    SharedPtr<T> &operator=(const SharedPtr<T> &other) {
        if (ptr != other.ptr) {
            SharedPtr<T> newValue(other);
            swap(newValue);
        }
        return *this;
    }

    template<typename U>
    SharedPtr<T> &operator=(const SharedPtr<U> &other) {
        if (ptr != other.ptr) {
            SharedPtr<T> newValue(other);
            swap(newValue);
        }
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

    void adoptInstead(T *p) {
        SharedPtr<T> newValue(p);
        swap(newValue);
    }

    void clear() {
        adoptInstead(NULL);
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

    const T *operator->() const {
        return ptr;
    }

    const T &operator*() const {
        return *ptr;
    }

    bool operator==(const T *other) const {
        return ptr == other;
    }

    bool operator!=(const T *other) const {
        return ptr != other;
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
        adoptInstead(result);
        return ptr;
    }
private:
    T *ptr;
    u_atomic_int32_t *refPtr;
    // No heap allocation. Use only stack.
    static void * U_EXPORT2 operator new(size_t size);
    static void * U_EXPORT2 operator new[](size_t size);
#if U_HAVE_PLACEMENT_NEW
    static void * U_EXPORT2 operator new(size_t, void *ptr);
#endif
    template<typename U> friend class SharedPtr;
};

U_NAMESPACE_END

#endif
