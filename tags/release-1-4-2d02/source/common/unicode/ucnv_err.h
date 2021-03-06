/*
**********************************************************************
*   Copyright (C) 1999, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
 *
 *
 *   ucnv_err.h:
 *   defines error behaviour functions called by T_UConverter_{from,to}Unicode
 *
 *   These Functions, although public, should NEVER be called directly, they should be used as parameters to
 *   the T_UConverter_setMissing{Char,Unicode}Action API, to set the behaviour of a converter
 *   when it encounters ILLEGAL/UNMAPPED/INVALID sequences.
 *
 *   usage example:
 *
 *        ...
 *        UErrorCode err = U_ZERO_ERROR;
 *        UConverter* myConverter = T_UConverter_create("ibm-949", &err);
 *
 *        if (U_SUCCESS(err))
 *        {
 *       T_UConverter_setMissingUnicodeAction(myConverter, (MissingUnicodeAction)UCNV_FROM_U_CALLBACK_STOP, &err);
 *       T_UConverter_setMissingCharAction(myConverter, (MissingCharAction)UCNV_TO_U_CALLBACK_SUBSTITUTE, &err);
 *        }
 *        ...
 *
 *   The code above tells "myConverter" to stop when it encounters a ILLEGAL/TRUNCATED/INVALID sequences when it is used to
 *   convert from Unicode -> Codepage.
 *   and to substitute with a codepage specific substitutions sequence when converting from Codepage -> Unicode
 */


#ifndef UCNV_ERR_H
#define UCNV_ERR_H

#include "unicode/ucnv.h"
#include "unicode/utypes.h"


/**
 * Functor STOPS at the ILLEGAL_SEQUENCE 
 * @stable
 */
U_CAPI void U_EXPORT2 UCNV_FROM_U_CALLBACK_STOP (UConverter * _this,
				     char **target,
				     const char *targetLimit,
				     const UChar ** source,
				     const UChar * sourceLimit,
				     int32_t* offsets,
				     bool_t flush,
				     UErrorCode * err);


/**
 * Functor STOPS at the ILLEGAL_SEQUENCE 
 * @stable
 */
U_CAPI void U_EXPORT2 UCNV_TO_U_CALLBACK_STOP (UConverter * _this,
				  UChar ** target,
				  const UChar * targetLimit,
				  const char **source,
				  const char *sourceLimit,
				  int32_t* offsets,
				  bool_t flush,
				  UErrorCode * err);




/**
 * Functor SKIPs the ILLEGAL_SEQUENCE 
 * @stable
 */
U_CAPI void U_EXPORT2 UCNV_FROM_U_CALLBACK_SKIP (UConverter * _this,
				     char **target,
				     const char *targetLimit,
				     const UChar ** source,
				     const UChar * sourceLimit,
				     int32_t* offsets,
				     bool_t flush,
				     UErrorCode * err);

/**
 * Functor Substitute the ILLEGAL SEQUENCE with the current substitution string assiciated with _this,
 * in the event target buffer is too small, it will store the extra info in the UConverter, and err
 * will be set to U_INDEX_OUTOFBOUNDS_ERROR. The next time T_UConverter_fromUnicode is called, it will
 * store the left over data in target, before transcoding the "source Stream"
 * @stable
 */

U_CAPI void U_EXPORT2 UCNV_FROM_U_CALLBACK_SUBSTITUTE (UConverter * _this,
					   char **target,
					   const char *targetLimit,
					   const UChar ** source,
					   const UChar * sourceLimit,
					   int32_t* offsets,
					   bool_t flush,
					   UErrorCode * err);

/**
 * Functor Substitute the ILLEGAL SEQUENCE with a sequence escaped codepoints corresponding to the ILLEGAL
 * SEQUENCE (format  %UXXXX, e.g. "%uFFFE%u00AC%uC8FE"). In the Event the Converter doesn't support the
 * characters {u,%}[A-F][0-9], it will substitute  the illegal sequence with the substitution characters
 * (it will behave like the above functor).
 * in the event target buffer is too small, it will store the extra info in the UConverter, and err
 * will be set to U_INDEX_OUTOFBOUNDS_ERROR. The next time T_UConverter_fromUnicode is called, it will
 * store the left over data in target, before transcoding the "source Stream"
 * @stable
 */

U_CAPI void U_EXPORT2 UCNV_FROM_U_CALLBACK_ESCAPE (UConverter * _this,
						    char **target,
						    const char *targetLimit,
						    const UChar ** source,
						  const UChar * sourceLimit,
						    int32_t* offsets,
						    bool_t flush,
						    UErrorCode * err);


/**
 * Functor SKIPs the ILLEGAL_SEQUENCE 
 * @stable
 */
U_CAPI void U_EXPORT2 UCNV_TO_U_CALLBACK_SKIP (UConverter * _this,
				  UChar ** target,
				  const UChar * targetLimit,
				  const char **source,
				  const char *sourceLimit,
				  int32_t* offsets,
				  bool_t flush,
				  UErrorCode * err);


/**
 * Functor Substitute the ILLEGAL SEQUENCE with the current substitution string assiciated with _this,
 * in the event target buffer is too small, it will store the extra info in the UConverter, and err
 * will be set to U_INDEX_OUTOFBOUNDS_ERROR. The next time T_UConverter_fromUnicode is called, it will
 * store the left over data in target, before transcoding the "source Stream"
 * @stable
 */
U_CAPI void U_EXPORT2 UCNV_TO_U_CALLBACK_SUBSTITUTE (UConverter * _this,
					UChar ** target,
					const UChar * targetLimit,
					const char **source,
					const char *sourceLimit,
					int32_t* offsets,
					bool_t flush,
					UErrorCode * err);

/**
 * Functor Substitute the ILLEGAL SEQUENCE with a sequence escaped codepoints corresponding to the
 * ILLEGAL SEQUENCE (format  %XNN, e.g. "%XFF%X0A%XC8%X03").
 * in the event target buffer is too small, it will store the extra info in the UConverter, and err
 * will be set to U_INDEX_OUTOFBOUNDS_ERROR. The next time T_UConverter_fromUnicode is called, it will
 * store the left over data in target, before transcoding the "source Stream"
 * @stable
 */

U_CAPI void U_EXPORT2 UCNV_TO_U_CALLBACK_ESCAPE (UConverter * _this,
						 UChar ** target,
						 const UChar * targetLimit,
						 const char **source,
						 const char *sourceLimit,
						 int32_t* offsets,
						 bool_t flush,
						 UErrorCode * err);


#endif/*UCNV_ERR_H*/ 
