/*
***************************************************************************
* Copyright (C) 2008, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*   file name:  uspoof.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2008Feb13
*   created by: Andy Heninger
*
*   Unicode Spoof Detection
*/

/**
 * \file
 * \brief C API: Unicode Spoof Detection
 *
 * <p>C API for Unicode Security and Spoofing Detection</p>
 */
#ifndef USPOOF_H
#define USPOOF_H

#include "unicode/utypes.h"
#include "unicode/uset.h"

#ifdef XP_CPLUSPLUS
#include "unicode/unistr.h"
#endif


struct USpoofChecker;
typedef struct USpoofChecker USpoofChecker;

/*
 * Enum the kinds of checks that USpoofChecker can perform.
 * These enum values are used both to select the set of checks that
 * will be performed, and to report results from the check function.
 */
typedef enum USpoofChecks {
    USPOOF_SINGLE_SCRIPT_CONFUSABLE =  1,
    USPOOF_MIXED_SCRIPT_CONFUSABLE  =  2,
    USPOOF_WHOLE_SCRIPT_CONFUSABLE  =  4,
    USPOOF_SECURE_ID                =  8,
    USPOOF_MIXED_SCRIPT             = 16,
    USPOOF_SCRIPT_LIMIT             = 32
    };
    
    
/*
 *  Create a Unicode Spoof Checker, configured to perform a 
 *  default set of checks.
 *
 *  @param status  the error code, set if an errors occured during the
 *                 creation of the spoof checker.
 *  @return        the newly created Spoof Checker
 *  @draft  ICU 4.0
 */
U_DRAFT USpoofChecker * U_EXPORT2
uspoof_open(UErrorCode *status);


/*
 * Specify the set of checks that will be performed by this Spoof Checker.
 *
 */
U_DRAFT void U_EXPORT2
uspoof_setChecks(USpoofChecker *sc, int32_t checks, UErrorCode *status);

U_DRAFT int32_t U_EXPORT2
uspoof_getChecks(const USpoofChecker sc);

/*
 * Limit the acceptable characters to those of the scripts for the
 *    specifed set of languages, plus characters from the "common" and
 *    "inherited" script categories which 
 *    Any previously set script limit
 *    is replaced by the new settings.
 *
 * Supplying an empty list of locales removes all restrictions;
 * characters from any script will be allowed.
 *
 * The USPOOF_SCRIPT_LIMIT test is automatically enabled for this
 * USoofChecker by this function.
 *
 * @param sc           The USpoofChecker 
 * @param localesList  A list list of locales, from which the language
 *                     and associated script are extracted.  The list
 *                     is an eight bit string containing only invariant
 *                     characters.  The locale names are separated by
 *                     spaces.
 * @status       The error code, set if this function encountes a problem.
 */
U_DRAFT void U_EXPORT2
uspoof_limitScriptsTo(USpoofChecker *sc, char *localesList, UErrorCode *status);


/*
 * Limit the acceptable characters to those specified by a Unicode Set.
 *   Any previously set script limit or character limit is
 *    is replaced by the new settings.
 *
 * The USPOOF_SCRIPT_LIMIT test is automatically enabled for this
 * USoofChecker by this function, and any check errors are reported as
 * as to be this kind, although the limitations need not correspond to
 * scripts.
 *
 * @param sc     The USpoofChecker 
 * @param chars  A Unicode Set containing the complete list of
 *               charcters that are permitted.  Ownership of the set
 *               remains with the caller.  The incoming set is cloned by
 *               this function, so there are no restrictions on modifying
 *               or deleting the USet after calling this function.
 * @status       The error code, set if this function encountes a problem.
 */
U_DRAFT void U_EXPORT2
uspoof_limitChars(USpoofChecker *sc, const USet *chars, UErrorCode *status);


/*
 * Check the specified string for possible security issues.
 * The text will typically be an indentifier of some sort.
 * The set of checks to be performed can be specified by uspoof_setChecks().
 * 
 * @param sc      The USpoofChecker 
 * @param text    The string to be checked for possible security issues,
 *                UTF-16 format.
 * @param length  the length of the string to be checked, expressed in
 *                16 bit UTF-16 code units, or -1 if the string is 
 *                zero terminated.
 * @param status  The error code, set if an error occured while attempting to
 *                perform the check.
 *                Spoofing or security issues detected with the input string are
 *                not reported here, but through the function's return value.
 * @return        An integer value with bits set for any potential security
 *                or spoofing issues detected.  The bits are defined by
 *                enum USpoofChecks.  Zero is returned if no issues
 *                are found with the input string.
 * @draft ICU 4.0
 */
U_DRAFT int32_t U_EXPORT2
uspoof_check(const USpoofChecker *sc, const UChar *text, int32_t length, UErrorCode *status);


/*
 * Check the specified string for possible security issues.
 * The text will typically be an indentifier of some sort.
 * The set of checks to be performed can be specified by uspoof_setChecks().
 * 
 * @param sc      The USpoofChecker 
 * @param text    A UTF-8 string to be checked for possible security issues.
 * @param length  the length of the string to be checked, or -1 if the string is 
 *                zero terminated.
 * @param status  The error code, set if an error occured while attempting to
 *                perform the check.
 *                Spoofing or security issues detected with the input string are
 *                not reported here, but through the function's return value.
 * @return        An integer value with bits set for any potential security
 *                or spoofing issues detected.  The bits are defined by
 *                enum USpoofChecks.  Zero is returned if no issues
 *                are found with the input string.
 * @draft ICU 4.0
 */
U_DRAFT int32_t U_EXPORT2
uspoof_checkUTF8(const USpoofChecker *sc, const char *text, int32_t length, UErrorCode *status);


#ifdef XP_CPLUSPLUS
/*
 * Check the specified string for possible security issues.
 * The text will typically be an indentifier of some sort.
 * The set of checks to be performed can be specified by uspoof_setChecks().
 * 
 * @param sc      The USpoofChecker 
 * @param text    A UnicodeString to be checked for possible security issues.
 * @param status  The error code, set if an error occured while attempting to
 *                perform the check.
 *                Spoofing or security issues detected with the input string are
 *                not reported here, but through the function's return value.
 * @return        An integer value with bits set for any potential security
 *                or spoofing issues detected.  The bits are defined by
 *                enum USpoofChecks.  Zero is returned if no issues
 *                are found with the input string.
 * @draft ICU 4.0
 */
U_DRAFT int32_t U_EXPORT2
uspoof_checkUString(const USpoofChecker *sc,
                    const U_NAMESPACE_QUALIFIER UnicodeString &text, 
                    int32_t length, UErrorCode *status);

#endif


/*
 * Check the whether the two specified strings are visually confusable.
 * The types of confusability to be tested - single script, mixed script,
 * or whole script - are determined by the check options set into the
 * USpoofChecker.
 * 
 * @param sc      The USpoofChecker 
 * @param s1      The first of the two strings to be compared for 
 *                confusability.  The strings are in UTF-16 format.
 * @param length1 the length of the first string, expressed in
 *                16 bit UTF-16 code units, or -1 if the string is 
 *                zero terminated.
 * @param s1      The second of the two strings to be compared for 
 *                confusability.  The strings are in UTF-16 format.
 * @param length1 the length of the second string, expressed in
 *                16 bit UTF-16 code units, or -1 if the string is 
 *                zero terminated.
 * @param status  The error code, set if an error occured while attempting to
 *                perform the check.
 *                Confusability of the strings is not reported here,
 *                but through the function's return value.
 * @return        An integer value with bit(s) set corresponding to
 *                the type of confusability found, as defined by
 *                enum USpoofChecks.  Zero is returned if the strings
 *                are not confusable.
 * @draft ICU 4.0
 */
U_DRAFT int32_t U_EXPORT2
uspoof_areConfusable(const USpoofChecker *sc,
                     const UChar *s1, int32_t length1,
                     const UChar *s2, int32_t length2,
                     UErrorCode *status);



U_DRAFT int32_t U_EXPORT2
uspoof_areConfusableUTF8(const USpoofChecker *sc,
                         const char *s1, int32_t length1,
                         const char *s2, int32_t length2,
                         UErrorCode *status);




#ifdef XP_CPLUSPLUS
U_DRAFT int32_t U_EXPORT2
uspoof_areConfusableUString(const USpoofChecker *sc,
                            const U_NAMESPACE_QUALIFIER UnicodeString &text,
                            const U_NAMESPACE_QUALIFIER UnicodeString &text
                            UErrorCode *status);
#endif



// True if string is limited to characters from the specified srcipt or 
//  the script associtated with the specified locale
limitedToScript(UScriptCode script, UErrorCode *status);
limitedToScriptForLocale(const char *locale, UErrorCode *status);


if (USpoof_hasWholeScriptConfusableUTF8(str, status)) ...

if (USpoof_isWholeScriptConfusableUTF8(str1, str2, status)) {...


#endif
