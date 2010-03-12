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

/*
 * IDNA option bit set values.
 */
enum {
    /**
    * IDNA option to check for whether the input conforms to the BiDi rules.
    * @draft ICU 4.6
    */
    UIDNA_CHECK_BIDI=4,
    /**
    * IDNA option to check for whether the input conforms to the CONTEXTJ rules.
    * @draft ICU 4.6
    */
    UIDNA_CHECK_CONTEXTJ=8
};

/*
 * IDNA error bit set values.
 * When a domain name or label fails a processing step or does not meet the
 * validity criteria, then one or more of these error bits are set.
 */
enum {
    /**
     * A non-final domain name label is empty.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_EMPTY_LABEL=1,
    /**
     * A domain name label is longer than 63 bytes. (See STD3/RFC1123 2. GENERAL ISSUES.)
     * This is only checked in ToASCII operations, and only if the UIDNA_USE_STD3_RULES is set.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_LABEL_TOO_LONG=2,
    /**
     * A domain name is longer than 255 bytes. (See STD3/RFC1123 2. GENERAL ISSUES.)
     * This is only checked in ToASCII operations, and only if the UIDNA_USE_STD3_RULES is set.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_DOMAIN_NAME_TOO_LONG=4,
    /**
     * A label starts with a hyphen-minus ('-').
     * @draft ICU 4.6
     */
    UIDNA_ERROR_LEADING_HYPHEN=8,
    /**
     * A label ends with a hyphen-minus ('-').
     * @draft ICU 4.6
     */
    UIDNA_ERROR_TRAILING_HYPHEN=0x10,
    /**
     * A label contains hyphen-minus ('-') in the third and fourth positions.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_HYPHEN_3_4=0x20,
    /**
     * A label starts with a combining mark.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_LEADING_COMBINING_MARK=0x40,
    /**
     * A label or domain name contains disallowed characters.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_DISALLOWED=0x80,
    /**
     * A label starts with "xn--" but does not contain valid Punycode.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_PUNYCODE=0x100,
    /**
     * A label contains a dot=full stop.
     * This can occur in an ACE label, and in an input string for a single-label function.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_LABEL_HAS_DOT=0x200,
    /**
     * An ACE label is not valid.
     * It might contain characters that are not allowed in ACE labels,
     * or it might not be normalized, or both.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_INVALID_ACE_LABEL=0x400,
    /**
     * A label does not meet the IDNA BiDi requirements (for right-to-left characters).
     * @draft ICU 4.6
     */
    UIDNA_ERROR_BIDI=0x800,
    /**
     * A label does not meet the IDNA CONTEXTJ requirements.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_CONTEXTJ=0x1000
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
     * Note that UTS #46 disallows all ASCII characters other than
     * letters, digits, hyphen (LDH) and dot/full stop, regardless of
     * whether the UIDNA_USE_STD3_RULES is set.
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
