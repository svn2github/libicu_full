/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedpluralrules.h
*/

#ifndef __SHARED_PLURALRULES_H__
#define __SHARED_PLURALRULES_H__

#include "unicode/utypes.h"
#include "sharedobject.h"
#include "sharedptr.h"

U_NAMESPACE_BEGIN

class PluralRules;

class U_I18N_API SharedPluralRules : public SharedObject {
public:
SharedPtr<PluralRules> ptr;
SharedPluralRules() : ptr(NULL) { }
private:
SharedPluralRules(const SharedPluralRules &);
SharedPluralRules &operator=(const SharedPluralRules &);
};

U_NAMESPACE_END

#endif
