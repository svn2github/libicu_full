/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         
* others. All Rights Reserved.                                                
*******************************************************************************
*                                                                             
* File SHAREDOBJECTPTR.H                                                             
*******************************************************************************
*/

#ifndef __SHARED_OBJECT_PTR_H__
#define __SHARED_OBJECT_PTR_H__

U_NAMESPACE_BEGIN

/**
 * SharedObjectPtr are shared pointers that support copy-on-write sematics.
 * SharedObjectPtr<T> has the same semantics of SharedPtr<T> except that T must
 * be a subclass of SharedObject. When a SharedObjectPtr adopts a pointer, it
 * increments its reference count by via addRef(). Unlike with SharedPtr,
 * adopting a pointer via constructor or reset method can never fail.
 * Consequently, reset() returns void instead of UBool. SharedObjectPtr<T> has
 * no swap() method. It also has no count() method because SharedObject already
 * has a getRefCount method.
 */
template<typename T>
class SharedObjectPtr {
public:
    /**
     * Constructor.
     */
    explicit SharedObjectPtr(const T *adopted=NULL) : ptr(adopted) {
        if (ptr != NULL) {
            ptr->addRef();
        }
    }

    /**
     * Copy constructor.
     */
    SharedObjectPtr(const SharedObjectPtr<T> &other) : ptr(other.ptr) {
        if (ptr != NULL) {
            ptr->addRef();
        }
    }

    /**
     * assignment operator.
     */
    SharedObjectPtr<T> &operator=(const SharedObjectPtr<T> &other) {
        SharedObject::copyPtr(other.ptr, ptr);
        return *this;
    }

    /**
     * Destructor.
     */
    ~SharedObjectPtr() {
        if (ptr != NULL) {
            ptr->removeRef();
        }
    }

    /**
     * reset adopts a new pointer. reset always succeeds.
     */
    void reset(const T *adopted=NULL) {
        SharedObject::copyPtr(adopted, ptr);
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

    /**
     * readOnly gives const access to this instance's T object. If this
     * instance refers to no object, returns NULL.
     */
    const T *readOnly() const {
        return ptr;
    }

    /**
     * Returns a non const T * performing copy-on-write if necessary.
     */
    T *readWrite() {
        if (ptr == NULL) {
            return NULL;
        }
        return SharedObject::copyOnWrite(ptr);
    }
private:
    const T *ptr;
    // No heap allocation. Use only stack.
    static void * U_EXPORT2 operator new(size_t size);
    static void * U_EXPORT2 operator new[](size_t size);
#if U_HAVE_PLACEMENT_NEW
    static void * U_EXPORT2 operator new(size_t, void *ptr);
#endif
};

U_NAMESPACE_END

#endif
