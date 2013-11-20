#ifndef __SHARED_PTR_H__
#define __SHARED_PTR_H__

#include "unicode/uobject.h"
#include "umutex.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

// TODO(tkeep): Make resilient to out of memory. In order to do this, the
// u_atomic_int32_t will have to be wrapped in a UMemory.
template<typename T>
class SharedPtr : public UMemory {
public:
    /**
     * Constructor. If there is a memory allocation error creating
     * reference counter then this object will contain NULL. Note that
     * when passing NULL or no argument to constructor, no memory allocation
     * error can happen as NULL pointers are never reference counted.
     */
    explicit SharedPtr(T *adopted=NULL) : ptr(adopted), refPtr(NULL) {
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
                // Cast to UObject to avoid compiler warnings about incomplete
                // type T.
                delete (UObject *) ptr;
                delete refPtr;
            }
        }
    }

    /**
     * adoptInstead adopts a new pointer. On success, returns TRUE.
     * On memory allocation error creating reference counter for adopted
     * pointer, returns FALSE while leaving this object unchanged.
     */
    bool adoptInstead(T *adopted) {
        SharedPtr<T> newValue(adopted);
        if (adopted != NULL && newValue.ptr == NULL) {
            // We couldn't allocate ref counter.
            return FALSE;
        }
        swap(newValue);
        return TRUE;
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

    /**
     * readWrite returns a writable pointer while copying the data first if
     * more than one reference to it is held. On success, returns a non NULL
     * writable pointer with this object referencing the copied data if the
     * data was copied. On memory allocation error or if this object contains
     * NULL it returns NULL leaving this object unchanged.
     */
    T *readWrite() {
        if (refPtr == NULL) {
            return NULL;
        }
        if (umtx_loadAcquire(*refPtr) == 1) {
            return ptr;
        }
        T *result = (T *) ptr->clone();
        if (result == NULL) {
            // Memory allocation error
            return NULL;
        }
        if (!adoptInstead(result)) {
            return NULL;
        }
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
