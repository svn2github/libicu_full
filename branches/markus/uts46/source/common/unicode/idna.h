/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  idna.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010mar05
*   created by: Markus W. Scherer
*/

#ifndef __IDNA_H__
#define __IDNA_H__

/**
 * \file
 * \brief C++ API: New API for IDNA (Internationalizing Domain Names In Applications).
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_IDNA

#include "unicode/uidna.h"
#include "unicode/unistr.h"

enum UIDNAErrors {
    UIDNA_ERROR_DISALLOWED=1,
    UIDNA_ERROR_STD3=2,
    UIDNA_ERROR_PUNYCODE=4,
    UIDNA_ERROR_BIDI=8,
    UIDNA_ERROR_ACE_PREFIX=0x10
    // TODO: More errors see utypes.h U_IDNA_..._ERROR values and processing spec.
};

U_NAMESPACE_BEGIN

/**
 * Abstract base class for IDNA processing.
 * See http://www.unicode.org/reports/tr46/
 * and http://www.ietf.org/rfc/rfc3490.txt
 *
 * This newer API currently only implements UTS #46.
 * The uidna.h C API only implements IDNA2003.
 * @draft ICU 4.6
 */
class U_COMMON_API IDNA : public UObject {
public:
    /**
     * Returns an IDNA instance which implements UTS #46.
     * Returns an unmodifiable instance, owned by the caller.
     * Cache it for multiple operations, and delete it when done.
     *
     * UTS #46 defines Unicode IDNA Compatibility Processing,
     * updated to the latest version of Unicode and compatible with both
     * IDNA2003 and IDNA2008.
     *
     * ToASCII operations use transitional processing, including deviation mappings.
     * ToUnicode operations use non-transitional processing,
     * passing deviation characters through without change.
     *
     * Disallowed characters are mapped to U+FFFD.
     *
     * For available options see the uidna.h header.
     * Operations with the UTS #46 instance do not support the
     * UIDNA_ALLOW_UNASSIGNED option.
     *
     * TODO: Do we need separate toASCIIOptions and toUnicodeOptions?
     *       That is, would users commonly want different options for the
     *       toASCII and toUnicode operations?
     *
     * @param options Bit set to modify the processing and error checking.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return the UTS #46 IDNA instance, if successful
     * @draft ICU 4.6
     */
    static IDNA *
    createUTS46Instance(uint32_t options, UErrorCode &errorCode);

    /**
     * Converts a single domain name label into its ASCII form for DNS lookup.
     * ToASCII can fail if the input label cannot be converted into an ASCII form.
     * In this case, the destination string will be bogus and the errors
     * parameter will be nonzero.
     *
     * The UErrorCode indicates an error only in exceptional cases,
     * such as a U_MEMORY_ALLOCATION_ERROR.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param errors Output bit set of IDNA processing errors.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    labelToASCII(const UnicodeString &label, UnicodeString &dest,
                 uint32_t &errors, UErrorCode &errorCode) const = 0;

    /**
     * Converts a single domain name label into its Unicode form for human-readable display.
     * ToUnicode never fails. If any processing step fails, then the input label
     * is returned, possibly with modifications according to the types of errors,
     * and the errors output value will be nonzero.
     *
     * For available options see the uidna.h header.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param errors Output bit set of IDNA processing errors.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    labelToUnicode(const UnicodeString &label, UnicodeString &dest,
                   uint32_t &errors, UErrorCode &errorCode) const = 0;

    /**
     * Converts a whole domain name into its ASCII form for DNS lookup.
     * ToASCII can fail if the input label cannot be converted into an ASCII form.
     * In this case, the destination string will be bogus and the errors
     * parameter will be nonzero.
     *
     * The UErrorCode indicates an error only in exceptional cases,
     * such as a U_MEMORY_ALLOCATION_ERROR.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param errors Output bit set of IDNA processing errors.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    nameToASCII(const UnicodeString &name, UnicodeString &dest,
                uint32_t &errors, UErrorCode &errorCode) const = 0;

    /**
     * Converts a whole domain name into its Unicode form for human-readable display.
     * ToUnicode never fails. If any processing step fails, then the input domain name
     * is returned, possibly with modifications according to the types of errors,
     * and the errors output value will be nonzero.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param errors Output bit set of IDNA processing errors.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    nameToUnicode(const UnicodeString &name, UnicodeString &dest,
                  uint32_t &errors, UErrorCode &errorCode) const = 0;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     * @returns a UClassID for this class.
     * @draft ICU 4.6
     */
    static UClassID U_EXPORT2 getStaticClassID();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     * @return a UClassID for the actual class.
     * @draft ICU 4.6
     */
    virtual UClassID getDynamicClassID() const = 0;
};

U_NAMESPACE_END

#endif  // UCONFIG_NO_IDNA
#endif  // __IDNA_H__
