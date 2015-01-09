/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef FORMATTEDVALUE_H
#define FORMATTEDVALUE_H

#if !UCONFIG_NO_FORMATTING

#include "unicode/utypes.h"

U_NAMESPACE_BEGIN

/**
 * A formatted value. 
 * Padding code and prefix suffix formatting code uses this as a placeholder.
 * These instances hide the details of formatting such as fixed decimal or
 * scientific notation as well as the exact representation of the value.
 * All that is exposed is the minimum for the padding code and prefix/suffix
 * formatting code to do its job.
 */
class FormattedValue : public UObject {
 public:
  virtual ~FormattedValue();
  /**
   * Appends this formatted value to appendTo and returns appendTo.
   */
  virtual UnicodeString &append(UnicodeString &appendTo) const = 0;

  /**
   * Returns true iff this value is negative.
   */
  virtual UBool isNegative() const = 0;

  /**
   * Returns the plural form of this value using rules.
   */
  virtual UnicodeString select(const PluralRules &rules) const = 0;

  /**
   * Returns the number of code points in this formatted string.
   * Needed for padding.
   */
  virtual int32_t countChar32() const;
};

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */

#endif /* FPHDLIMP_H */
