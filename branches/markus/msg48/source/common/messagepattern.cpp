/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  messagepattern.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011mar14
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/messagepattern.h"
#include "unicode/unistr.h"
#include "cmemory.h"
#include "patternprops.h"

U_NAMESPACE_BEGIN

template<typename T, int32_t stackCapacity>
class MessagePatternList {
public:
    UBool ensureCapacityForOneMore(int32_t oldLength, UErrorCode &errorCode) {
        if(U_FAILURE(errorCode)) {
            return FALSE;
        }
        if(a.getCapacity()>oldLength || a.resize(2*oldLength, oldLength)!=NULL) {
            return TRUE;
        }
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }

    MaybeStackArray<T, stackCapacity> a;
};

class MessagePatternDoubleList : public MessagePatternList<double, 8> {
};

class MessagePatternPartsList : public MessagePatternList<MessagePattern::Part, 32> {
};

MessagePattern::~MessagePattern() {
    delete partsList;
    delete numericValuesList;
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(MessagePattern)

U_NAMESPACE_END
