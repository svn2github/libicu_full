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

#include "cstring.h"
#include "mutex.h"
#include "ucln_in.h"
#include "umutex.h"
#include "uhash.h"

static UHashtable* gGenderInfoCache = NULL;
static UMTX gGenderMetaLock = NULL;
static const char* gNeutralStr = "neutral";
static const char* gMailTaintsStr = "maleTaints";
static const char* gMixedNeutralStr = "mixedNeutral";

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


GenderInfo::GenderInfo(int32_t t) : _style(t) {
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
  LocalUResourceBundlePointer rb(
      ures_openDirect(NULL, "genderList", &status));
  if (U_FAILURE(status)) {
    return NULL;
  }
  LocalUResourceBundlePointer locRes(ures_getByKey(rb.getAlias(), "genderList", NULL, &status));
  if (U_FAILURE(status)) {
    return NULL;
  }
  int32_t resLen = 0;
  const char* curLocaleName = locale.getName();
  UErrorCode key_status = U_ZERO_ERROR;
  const UChar* s = ures_getStringByKey(locRes.getAlias(), curLocaleName, &resLen, &key_status);
  if (s == NULL) {
    key_status = U_ZERO_ERROR;
    char parentLocaleName[ULOC_FULLNAME_CAPACITY];
    uprv_strcpy(parentLocaleName, curLocaleName);
    while (s == NULL && uloc_getParent(parentLocaleName, parentLocaleName, ULOC_FULLNAME_CAPACITY, &key_status) > 0) {
      key_status = U_ZERO_ERROR;
      resLen = 0;
      s = ures_getStringByKey(locRes.getAlias(), parentLocaleName, &resLen, &key_status);
      key_status = U_ZERO_ERROR;
    }
  }
  if (s == NULL) {
    return _neutral;
  }
  char type_str[256];
  u_UCharsToChars(s, type_str, resLen + 1);
  if (!uprv_strcmp(type_str, gNeutralStr)) {
    return _neutral;
  }
  if (!uprv_strcmp(type_str, gMixedNeutralStr)) {
    return _mixed;
  }
  if (!uprv_strcmp(type_str, gMailTaintsStr)) {
    return _taints;
  }
  return _neutral;
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
