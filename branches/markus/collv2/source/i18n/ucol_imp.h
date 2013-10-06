/*
*******************************************************************************
*
*   Copyright (C) 1998-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* Private implementation header for C collation
*   file name:  ucol_imp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000dec11
*   created by: Vladimir Weinstein
*
* Modification history
* Date        Name      Comments
* 02/16/2001  synwee    Added UCOL_GETPREVCE for the use in ucoleitr
* 02/27/2001  synwee    Added getMaxExpansion data structure in UCollator
* 03/02/2001  synwee    Added UCOL_IMPLICIT_CE
* 03/12/2001  synwee    Added pointer start to collIterate.
*/

#ifndef UCOL_IMP_H
#define UCOL_IMP_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

/**
 * Convenience string denoting the Collation data tree
 */
#define U_ICUDATA_COLL U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll"

/* TODO: can we move some of the following into test header files? */

/* mask off anything but primary order */
#define UCOL_PRIMARYORDERMASK 0xffff0000
/* mask off anything but secondary order */
#define UCOL_SECONDARYORDERMASK 0x0000ff00
/* mask off anything but tertiary order */
#define UCOL_TERTIARYORDERMASK 0x000000ff
/* primary order shift */
#define UCOL_PRIMARYORDERSHIFT 16
/* secondary order shift */
#define UCOL_SECONDARYORDERSHIFT 8

#define UCOL_IGNORABLE 0

/* get weights from a CE */
#define UCOL_PRIMARYORDER(order) (((order) >> 16) & 0xffff)
#define UCOL_SECONDARYORDER(order) (((order) & UCOL_SECONDARYORDERMASK)>> UCOL_SECONDARYORDERSHIFT)
#define UCOL_TERTIARYORDER(order) ((order) & UCOL_TERTIARYORDERMASK)

#define UCOL_UPPER_CASE 0x80
#define UCOL_MIXED_CASE 0x40
#define UCOL_LOWER_CASE 0x00

#define UCOL_CONTINUATION_MARKER 0xC0
#define UCOL_REMOVE_CONTINUATION 0xFFFFFF3F

#define isContinuation(CE) (((CE) & UCOL_CONTINUATION_MARKER) == UCOL_CONTINUATION_MARKER)

/* TODO: move the following to a new collationloader.h header (rename this one??), or remove other definitions? */
#ifdef __cplusplus

U_NAMESPACE_BEGIN

struct CollationTailoring;

class Locale;
class UnicodeString;

class CollationLoader {
public:
    static void appendRootRules(UnicodeString &s);
    static UnicodeString *loadRules(const char *localeID, const char *collationType,
                                    UErrorCode &errorCode);
    static const CollationTailoring *loadTailoring(const Locale &locale, Locale &validLocale,
                                                   UErrorCode &errorCode);

private:
    CollationLoader();  // not implemented, all methods are static
    static void loadRootRules(UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  /* __cplusplus */

#endif /* #if !UCONFIG_NO_COLLATION */

#endif
