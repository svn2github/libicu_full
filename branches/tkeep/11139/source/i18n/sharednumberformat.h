/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharednumberformat.h
*/

#ifndef __SHARED_NUMBERFORMAT_H__
#define __SHARED_NUMBERFORMAT_H__

#include "unicode/utypes.h"
#include "sharedobject.h"

U_NAMESPACE_BEGIN

class NumberFormat;

class U_I18N_API SharedNumberFormat : public SharedObject {
public:
    SharedNumberFormat(NumberFormat *nfToAdopt) : ptr(nfToAdopt) { }
    virtual ~SharedNumberFormat();
    const NumberFormat *get() const { return ptr; }

    /**
     * If multiple references are held, returns a clone of the internal
     * number format and removes this reference. If this is the only reference,
     * it transfers ownership of the internal number format to the caller and
     * deletes this shared object.
     */
    NumberFormat *orphanOrCloneAndRemoveRef() const;
    const NumberFormat *operator->() const { return ptr; }
    const NumberFormat &operator*() const { return *ptr; }
private:
    NumberFormat *ptr;
    SharedNumberFormat(const SharedNumberFormat &);
    SharedNumberFormat &operator=(const SharedNumberFormat &);
};

U_NAMESPACE_END

#endif
