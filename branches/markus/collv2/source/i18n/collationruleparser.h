/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationruleparser.h
*
* created on: 2013apr10
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONRULEPARSER_H__
#define __COLLATIONRULEPARSER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "uvectr32.h"

U_NAMESPACE_BEGIN

class UnicodeString;
class UVector32;

struct CollationSettings;
struct UParseError;

class U_I18N_API CollationRuleParser : public UMemory {
public:
    /**
     * Token bits 2..0 contain the relation strength value.
     * "Stronger" relations must have lower numeric values.
     */
    enum Relation {
        /** Sentinel value. The token integer should be zero. */
        NO_RELATION,
        /** Reset-before; its before-strength is determined by the following relation. */
        RESET_BEFORE,
        RESET_AT,
        PRIMARY,
        SECONDARY,
        TERTIARY,
        QUATERNARY,
        IDENTICAL
    };
    /** Token bit 3 indicates whether the token string is followed by an expansion string. */
    static const int32_t HAS_EXPANSION = 8;

    CollationRuleParser(UErrorCode &errorCode);
    ~CollationRuleParser();

    void parse(const UnicodeString &ruleString,
               CollationSettings &outSettings,
               UParseError *outParseError,
               UErrorCode &errorCode);
private:
    void parse(const UnicodeString &ruleString, UErrorCode &errorCode);
    void parseRuleChain(UErrorCode &errorCode);
    void parseSetting(UErrorCode &errorCode);
    void skipComment();

    void setError(const char *reason);

    int32_t skipWhiteSpace(int32_t i) const;

    const UnicodeString *rules;
    CollationSettings *settings;
    UParseError *parseError;
    const char *errorReason;

    int32_t ruleIndex;

    UVector32 tokens;
    int32_t tokenIndex;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONRULEPARSER_H__
