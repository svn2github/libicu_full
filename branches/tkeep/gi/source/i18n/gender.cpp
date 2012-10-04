/*
*******************************************************************************
* Copyright (C) 2008-2012, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
*
* File GENDER.CPP
*
* Modification History:*
*   Date        Name        Description
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/gender.h"
#include "unicode/ugender.h"
#include "unicode/ures.h"

#include "mutex.h"
#include "ucln_in.h"
#include "umutex.h"
#include "uhash.h"

static UHashtable* gGenderInfoCache = NULL;
static UMTX gGenderMetaLock = NULL;

enum GenderStyle {
  NEUTRAL,
  MIXED_NEUTRAL,
  MALE_TAINTS
};

U_CDECL_BEGIN

static UBool U_CALLCONV gender_cleanup(void) {
  umtx_destroy(&gGenderMetaLock);
  if (gGenderInfoCache != NULL) {
    uhash_close(gGenderInfoCache);
    gGenderInfoCache = NULL;
  }
  return TRUE;
}

U_CDECL_END

U_NAMESPACE_BEGIN

GenderInfo* GenderInfo::_neutral = new GenderInfo(NEUTRAL);
GenderInfo* GenderInfo::_mixed = new GenderInfo(MIXED_NEUTRAL);
GenderInfo* GenderInfo::_taints = new GenderInfo(MALE_TAINTS);


GenderInfo::GenderInfo(int32_t style) : _style(style) {
}

GenderInfo::~GenderInfo() {
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(GenderInfo)

const GenderInfo* GenderInfo::getInstance(const Locale& locale, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }

  // Make sure our cache exists.
  bool needed;
  UMTX_CHECK(&gGenderMetaLock, (gGenderInfoCache == NULL), needed);
  if (needed) {
    {
      Mutex lock(&gGenderMetaLock);
      if (gGenderInfoCache == NULL) {
        gGenderInfoCache = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
        if (U_SUCCESS(status)) {
          ucln_i18n_registerCleanup(UCLN_I18N_GENDERINFO, gender_cleanup);
        }
      }
    }
    if (U_FAILURE(status)) {
      return NULL;
    }
  }

  GenderInfo* result = NULL;
  const char* key = locale.getName();
  {
    Mutex lock(&gGenderMetaLock);
    result = (GenderInfo*) uhash_get(gGenderInfoCache, key);
  }
  if (result) {
    return result;
  }

  // On cache miss, try to create GenderInfo from CLDR data
  result = loadInstance(locale, status);

  // Try to put our GenderInfo object in cache. If there is a race condition,
  // favor the GenderInfo object that is already in the cache.
  {
    Mutex lock(&gGenderMetaLock);
    GenderInfo* temp = (GenderInfo*) uhash_get(gGenderInfoCache, key);
    if (temp) {
      result = temp;
    } else {
      uhash_put(gGenderInfoCache, (void*) key, result, &status);
      if (U_FAILURE(status)) {
        return NULL;
      }
    }
  }
  return result;
}

GenderInfo* GenderInfo::loadInstance(const Locale& locale, UErrorCode& status) {
  /*
  ures_openDirect(NULL, "genderList", &status);
  if (U_FAILURE(status)) {
    return NULL;
  }
  */
  if (locale == Locale::getUS()) {
    return _neutral;
  } else if (locale == Locale::getFrench()) {
    return _taints;
  } else {
    return _mixed;
  }
}


UGender GenderInfo::getListGender(const UGender* genders, int32_t length, UErrorCode& status) const {
  if (U_FAILURE(status)) {
    return UGENDER_OTHER;
  }
  if (length == 0 || _style == NEUTRAL) {
    return UGENDER_OTHER;
  }
  if (length == 1) {
    return genders[0];
  }
  bool has_female = false;
  bool has_male = false;
  switch (_style) {
    case MIXED_NEUTRAL:
      for (int32_t i = 0; i < length; ++i) {
        switch (genders[i]) {
          case UGENDER_OTHER:
            return UGENDER_OTHER;
            break;
          case UGENDER_FEMALE:
            if (has_male) {
              return UGENDER_OTHER;
            }
            has_female = true;
            break;
          case UGENDER_MALE:
            if (has_female) {
              return UGENDER_OTHER;
            }
            has_male = true;
            break;
          default:
            break;
        }
      }
      return has_male ? UGENDER_MALE : UGENDER_FEMALE;
      break;
    case MALE_TAINTS:
      for (int32_t i = 0; i < length; ++i) {
        if (genders[i] != UGENDER_FEMALE) {
          return UGENDER_MALE;
        }
      }
      return UGENDER_FEMALE;
      break;
    default:
      return UGENDER_OTHER;
      break;
  }
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
