/*
**********************************************************************
*   Copyright (C) 2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* scriptset.h
*
* created on: 2013 Jan 7
* created by: Andy Heninger
*/

#ifndef __SCRIPTSET_H__
#define __SCRIPTSET_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/uscript.h"

U_NAMESPACE_BEGIN

//-------------------------------------------------------------------------------
//
//  ScriptSet - A bit set representing a set of scripts.
//
//              This class was originally used exclusively with script sets appearing
//              as part of the spoof check whole script confusable binary data. Its
//              use has since become more general, but the continued use to wrap
//              prebuilt binary data does constrain the design.
//
//-------------------------------------------------------------------------------
class U_I18N_API ScriptSet: public UMemory {
  public:
    ScriptSet();
    ~ScriptSet();

    UBool operator == (const ScriptSet &other);
    ScriptSet & operator = (const ScriptSet &other);

    void Union(const ScriptSet &other);
    void Union(UScriptCode script);
    void intersect(const ScriptSet &other);
    void intersect(UScriptCode script);
    void setAll();
    void resetAll();
    int32_t countMembers();

  private:
    uint32_t  bits[6];
};

U_NAMESPACE_END

#endif // __SCRIPTSET_H__
