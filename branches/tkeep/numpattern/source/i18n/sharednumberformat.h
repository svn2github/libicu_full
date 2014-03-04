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
#include "sharedptr.h"

U_NAMESPACE_BEGIN

class NumberFormat;
class IntFormatter;

class U_I18N_API SharedNumberFormat : public SharedObject {
public:
SharedNumberFormat(NumberFormat *nfToAdopt)
        : ptr(nfToAdopt), intFormatter(NULL) { }
SharedNumberFormat(NumberFormat *nfToAdopt, IntFormatter *ifToAdopt)
        : ptr(nfToAdopt), intFormatter(ifToAdopt) { }
virtual ~SharedNumberFormat();
const NumberFormat *get() const { return ptr; }
const NumberFormat *operator->() const { return ptr; }
const NumberFormat &operator*() const { return *ptr; }
const IntFormatter *getIntFormatter() const { return intFormatter; }
private:
NumberFormat *ptr;
IntFormatter *intFormatter;
SharedNumberFormat(const SharedNumberFormat &);
SharedNumberFormat &operator=(const SharedNumberFormat &);
};

U_NAMESPACE_END

#endif
