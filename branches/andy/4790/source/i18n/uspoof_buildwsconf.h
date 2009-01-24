/*
******************************************************************************
*
*   Copyright (C) 2008-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_buildwsconf.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan19
*   created by: Andy Heninger
*
*   Internal classes for compiling whole script confusable data into its binary (runtime) form.
*/

#ifndef __USPOOF_BUILDWSCONF_H__
#define __USPOOF_BUILDWSCONF_H__

#include "uspoof_impl.h"
#include "utrie2.h"

struct BuilderScriptSet: public UMemory {
  public:
    UChar32      codePoint;
    UTrie2      *trie;
    ScriptSet   *sset;
    uint32_t     index;           // scriptSetsVector index, before deleting dups.
    uint32_t     rindex;          // scriptSets index, final (runtime) value.

    BuilderScriptSet();
    ~BuilderScriptSet();
};


class WSConfusableDataBuilder: public UMemory {
  private:
      WSConfusableDataBuilder();
      ~WSConfusableDataBuilder();

      UTrie2    *fAnyCaseTrie;
      UTrie2    *fLowerCaseTrie;
      
    
  public:
    static void buildWSConfusableData(SpoofImpl *spImpl, const char * confusablesWS,
        int32_t confusablesWSLen, UParseError *pe, UErrorCode &status);
};

#endif
