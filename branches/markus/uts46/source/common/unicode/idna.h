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

#include "unicode/bytestream.h"
#include "unicode/stringpiece.h"
#include "unicode/uidna.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class U_COMMON_API IDNAInfo;

/**
 * Abstract base class for IDNA processing.
 * See http://www.unicode.org/reports/tr46/
 * and http://www.ietf.org/rfc/rfc3490.txt
 *
 * This newer API currently only implements UTS #46.
 * The older uidna.h C API only implements IDNA2003.
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
     * The worker functions use transitional processing, including deviation mappings,
     * unless UIDNA_NONTRANSITIONAL_TO_ASCII or UIDNA_NONTRANSITIONAL_TO_UNICODE
     * is used in which case the deviation characters are passed through without change.
     *
     * Disallowed characters are mapped to U+FFFD.
     *
     * For available options see the uidna.h header.
     * Operations with the UTS #46 instance do not support the
     * UIDNA_ALLOW_UNASSIGNED option.
     *
     * By default, the UTS #46 implementation allows all ASCII characters (as valid or mapped).
     * When the UIDNA_USE_STD3_RULES option is used, ASCII characters other than
     * letters, digits, hyphen (LDH) and dot/full stop are disallowed and mapped to U+FFFD.
     *
     * @param options Bit set to modify the processing and error checking.
     *                See option bit set values in uidna.h.
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
     * If any processing step fails, then info.hasErrors() will be TRUE and
     * the result might not be an ASCII string.
     * The label might be modified according to the types of errors.
     * Labels with errors will be left in (or turned into) their Unicode form.
     *
     * The UErrorCode indicates an error only in exceptional cases,
     * such as a U_MEMORY_ALLOCATION_ERROR.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    labelToASCII(const UnicodeString &label, UnicodeString &dest,
                 IDNAInfo &info, UErrorCode &errorCode) const = 0;

    /**
     * Converts a single domain name label into its Unicode form for human-readable display.
     * If any processing step fails, then info.hasErrors() will be TRUE.
     * The domain name might be modified according to the types of errors.
     *
     * The UErrorCode indicates an error only in exceptional cases,
     * such as a U_MEMORY_ALLOCATION_ERROR.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    labelToUnicode(const UnicodeString &label, UnicodeString &dest,
                   IDNAInfo &info, UErrorCode &errorCode) const = 0;

    /**
     * Converts a whole domain name into its ASCII form for DNS lookup.
     * If any processing step fails, then info.hasErrors() will be TRUE and
     * the result might not be an ASCII string.
     * The domain name might be modified according to the types of errors.
     * Labels with errors will be left in (or turned into) their Unicode form.
     *
     * The UErrorCode indicates an error only in exceptional cases,
     * such as a U_MEMORY_ALLOCATION_ERROR.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    nameToASCII(const UnicodeString &name, UnicodeString &dest,
                IDNAInfo &info, UErrorCode &errorCode) const = 0;

    /**
     * Converts a whole domain name into its Unicode form for human-readable display.
     * If any processing step fails, then info.hasErrors() will be TRUE.
     * The domain name might be modified according to the types of errors.
     *
     * The UErrorCode indicates an error only in exceptional cases,
     * such as a U_MEMORY_ALLOCATION_ERROR.
     *
     * @param label Input domain name label
     * @param dest Destination string object
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual UnicodeString &
    nameToUnicode(const UnicodeString &name, UnicodeString &dest,
                  IDNAInfo &info, UErrorCode &errorCode) const = 0;

    // UTF-8 versions of the processing methods ---------------------------- ***

    /**
     * Converts a single domain name label into its ASCII form for DNS lookup.
     * UTF-8 version of labelToASCII(), same behavior.
     *
     * @param label Input domain name label
     * @param dest Destination byte sink
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual void
    labelToASCII_UTF8(const StringPiece &label, ByteSink &dest,
                      IDNAInfo &info, UErrorCode &errorCode) const;

    /**
     * Converts a single domain name label into its Unicode form for human-readable display.
     * UTF-8 version of labelToUnicode(), same behavior.
     *
     * @param label Input domain name label
     * @param dest Destination byte sink
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual void
    labelToUnicodeUTF8(const StringPiece &label, ByteSink &dest,
                       IDNAInfo &info, UErrorCode &errorCode) const;

    /**
     * Converts a whole domain name into its ASCII form for DNS lookup.
     * UTF-8 version of nameToASCII(), same behavior.
     *
     * @param label Input domain name label
     * @param dest Destination byte sink
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual void
    nameToASCII_UTF8(const StringPiece &name, ByteSink &dest,
                     IDNAInfo &info, UErrorCode &errorCode) const;

    /**
     * Converts a whole domain name into its Unicode form for human-readable display.
     * UTF-8 version of nameToUnicode(), same behavior.
     *
     * @param label Input domain name label
     * @param dest Destination byte sink
     * @param info Output container of IDNA processing details.
     * @param errorCode Standard ICU error code. Its input value must
     *                  pass the U_SUCCESS() test, or else the function returns
     *                  immediately. Check for U_FAILURE() on output or use with
     *                  function chaining. (See User Guide for details.)
     * @return dest
     * @draft ICU 4.6
     */
    virtual void
    nameToUnicodeUTF8(const StringPiece &name, ByteSink &dest,
                      IDNAInfo &info, UErrorCode &errorCode) const;

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

class UTS46;

/**
 * Output container for IDNA processing errors.
 * @draft ICU 4.6
 */
class U_COMMON_API IDNAInfo : public UObject {
public:
    /**
     * Constructor for stack allocation.
     * @draft ICU 4.6
     */
    IDNAInfo() : errors(0), labelErrors(0), isTransDiff(FALSE) {}
    /**
     * Were there IDNA processing errors?
     * @return TRUE if there were processing errors
     * @draft ICU 4.6
     */
    UBool hasErrors() const { return errors!=0; }
    /**
     * Returns a bit set indicating IDNA processing errors.
     * See UIDNA_ERROR_... constants.
     * @return bit set of processing errors
     * @draft ICU 4.6
     */
    uint32_t getErrors() const { return errors; }
    /**
     * Returns TRUE if transitional and nontransitional processing produce different results.
     * This is the case when the input label or domain name contains
     * one or more deviation characters outside a Punycode label (see UTS #46).
     * <ul>
     * <li>With nontransitional processing, such characters are
     * copied to the destination string.
     * <li>With transitional processing, such characters are
     * mapped (sharp s/sigma) or removed (joiner/nonjoiner).
     * </ul>
     * @return TRUE if transitional and nontransitional processing produce different results
     * @draft ICU 4.6
     */
    UBool isTransitionalDifferent() const { return isTransDiff; }

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
    virtual UClassID getDynamicClassID() const;

private:
    friend class UTS46;

    IDNAInfo(const IDNAInfo &other);  // no copying
    IDNAInfo &operator=(const IDNAInfo &other);  // no copying

    void reset() {
        errors=labelErrors=0;
        isTransDiff=FALSE;
    }

    uint32_t errors, labelErrors;
    UBool isTransDiff;
};

U_NAMESPACE_END

/*
 * IDNA error bit set values.
 * When a domain name or label fails a processing step or does not meet the
 * validity criteria, then one or more of these error bits are set.
 */
enum {
    /**
     * A non-final domain name label (or the whole domain name) is empty.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_EMPTY_LABEL=1,
    /**
     * A domain name label is longer than 63 bytes.
     * (See STD13/RFC1034 3.1. Name space specifications and terminology.)
     * This is only checked in ToASCII operations, and only if the UIDNA_USE_STD3_RULES is set.
     * @draft ICU 4.6
     */
    UIDNA_ERROR_LABEL_TOO_LONG=2,
    /**
     * A domain name is longer than 255 bytes in its storage form.
     * (See STD13/RFC1034 3.1. Name space specifications and terminology.)
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
     * This can occur in an input string for a single-label function.
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

#endif  // UCONFIG_NO_IDNA
#endif  // __IDNA_H__
