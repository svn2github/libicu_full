/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1996                                                 *
*   (C) Copyright International Business Machines Corporation,  1996-1998                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*/
//=============================================================================
//
// File coll.cpp
//
// 
//
// Created by: Helena Shih
//
// Modification History:
//
//  Date        Name        Description
//  2/5/97      aliu        Modified createDefault to load collation data from
//                          binary files when possible.  Added related methods
//                          createCollationFromFile, chopLocale, createPathName.
//  2/11/97     aliu        Added methods addToCache, findInCache, which implement
//                          a Collation cache.  Modified createDefault to look in
//                          cache first, and also to store newly created Collation
//                          objects in the cache.  Modified to not use gLocPath.
//  2/12/97     aliu        Modified to create objects from RuleBasedCollator cache.
//                          Moved cache out of Collation class.
//  2/13/97     aliu        Moved several methods out of this class and into
//                          RuleBasedCollator, with modifications.  Modified
//                          createDefault() to call new RuleBasedCollator(Locale&)
//                          constructor.  General clean up and documentation.
//  2/20/97     helena      Added clone, operator==, operator!=, operator=, and copy
//                          constructor.
// 05/06/97     helena      Added memory allocation error detection.
// 05/08/97     helena      Added createInstance().
//  6/20/97     helena      Java class name change.
// 04/23/99     stephen     Removed EDecompositionMode, merged with 
//                          Normalizer::EMode
//=============================================================================

#include "colcache.h"

#include "coll.h"

#include "tblcoll.h"
#include "sortkey.h"
#include "resbund.h"
#include "mutex.h"
#include "normlzr.h"

//-----------------------------------------------------------------------------

Collator::Collator()
  : strength(Collator::TERTIARY), decmp(Normalizer::DECOMP)
{
}

Collator::Collator(ECollationStrength collationStrength,
           Normalizer::EMode decompositionMode)
  : strength(collationStrength), decmp(decompositionMode)
{
}

Collator::~Collator()
{
}

Collator::Collator(const    Collator&   other)
  : strength(other.strength), decmp(other.decmp)
{
}

const Collator&
Collator::operator=(const   Collator&   other)
{
  if (this != &other) {
    strength = other.strength;
    decmp = other.decmp;
  }
  return *this;
}

Collator*
Collator::createInstance(UErrorCode& success) {
  if (FAILURE(success))
    return 0;
  return createInstance(Locale::getDefault(), success);
}

Collator*
Collator::createInstance(const Locale&  desiredLocale, 
                         UErrorCode&     status)
{
  if (FAILURE(status)) return 0;

  /**
   * A bit of explanation is required here.  Although in the current implementation
   * Collator::createInstance() is just turning around and calling RuleBasedCollator(Locale&),
   * this will not necessarily always be the case.  For example, suppose we modify
   * this code to handle a non-table-based Collator, such as that for Thai.  In this
   * case, createInstance() will have to be modified to somehow determine this fact
   * (perhaps a field in the resource bundle).  Then it can construct the non-table-based
   * Collator in some other way, when it sees that it needs to.
   *
   * The specific caution is this:  RuleBasedCollator(Locale&) will ALWAYS return a
   * valid collation object, if the system if functioning properly.  The reason
   * is that it will fall back, use the default locale, and even use the built-in
   * default collation rules.  THEREFORE, createInstance() should in general ONLY CALL
   * RuleBasedCollator(Locale&) IF IT KNOWS IN ADVANCE that the given locale's collation
   * is properly implemented as a RuleBasedCollator.
   *
   * Currently, we don't do this...we always return a RuleBasedCollator, whether it
   * is strictly correct to do so or not, without checking, because we currently
   * have no way of checking.
   */
  RuleBasedCollator* collation = new RuleBasedCollator(desiredLocale, status);
  if (FAILURE(status))
    {
      delete collation;
      collation = 0;
    }
  return collation;
}

bool_t
Collator::equals(const UnicodeString& source, 
         const UnicodeString& target) const
{
  return (compare(source, target) == Collator::EQUAL);
}
bool_t
Collator::greaterOrEqual(const UnicodeString& source, 
             const UnicodeString& target) const
{
  return (compare(source, target) != Collator::LESS);
}
bool_t
Collator::greater(const UnicodeString& source, 
          const UnicodeString& target) const
{
  return (compare(source, target) == Collator::GREATER);
}

Collator::ECollationStrength 
Collator::getStrength() const
{
  return strength;
}

void 
Collator::setStrength(Collator::ECollationStrength newStrength)
{
  strength = newStrength;
}

Normalizer::EMode
Collator::getDecomposition() const
{
  return decmp;
}
void 
Collator::setDecomposition(Normalizer::EMode decompositionMode)
{
  decmp = decompositionMode;
}


const Locale*
Collator::getAvailableLocales(int32_t& count) 
{
  return Locale::getAvailableLocales(count);
}

UnicodeString&
Collator::getDisplayName(   const   Locale&     objectLocale,
                            const   Locale&     displayLocale,
                UnicodeString& name)
{
  return objectLocale.getDisplayName(displayLocale, name);
}

UnicodeString& 
Collator::getDisplayName(   const   Locale&     objectLocale,
                UnicodeString& name)
{   
  return objectLocale.getDisplayName(Locale::getDefault(), name);
}

//eof
