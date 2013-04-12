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

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

class Normalizer2;
class UVector32;

struct CollationSettings;
struct UParseError;

class U_I18N_API CollationRuleParser : public UMemory {
public:
    /** "Stronger" relations must have lower numeric values. */
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

    CollationRuleParser(UErrorCode &errorCode);
    ~CollationRuleParser();

    void parse(const UnicodeString &ruleString,
               CollationSettings &outSettings,
               UParseError *outParseError,
               UErrorCode &errorCode);

    UBool modifiesSettings() const { return TRUE; }  // TODO
    UBool modifiesMappings() const { return TRUE; }  // TODO

private:
    void parse(const UnicodeString &ruleString, UErrorCode &errorCode);
    void parseRuleChain(UErrorCode &errorCode);
    int32_t parseResetAndPosition(UErrorCode &errorCode);
    int32_t parseRelationOperator(UErrorCode &errorCode);
    void parseRelationStrings(int32_t i, UErrorCode &errorCode);
    void parseStarredCharacters(int32_t relation, int32_t i, UErrorCode &errorCode);
    int32_t parseTailoringString(int32_t i, UErrorCode &errorCode);
    int32_t parseString(int32_t i, UErrorCode &errorCode);

    int32_t parseSpecialPosition(int32_t i, UErrorCode &errorCode);
    void parseSetting(UErrorCode &errorCode);

    int32_t skipComment(int32_t i) const;

    void makeAndInsertToken(int32_t relation, UErrorCode &errorCode);
    void resetTailoringStrings();

    void setError(const char *reason, UErrorCode &errorCode);

    /** Token bits 31..10 contain the code point or negative string index. */
    static const int32_t VALUE_SHIFT = 10;
    /** Maximum non-negative value (21 bits). */
    static const int32_t MAX_VALUE = 0x1fffff;
    /** Token bit 9 is set if the token string is followed by an expansion string. */
    static const int32_t HAS_EXPANSION = 0x200;
    /** Token bits 8..3 contain the sum of the lengths of prefix & contraction (max 31+31). */
    static const int32_t LENGTHS_SUM_SHIFT = 3;
    /** Mask after shifting. */
    static const int32_t LENGTHS_SUM_MASK = 0x3f;
    /** Token bits 2..0 contain the relation strength value. */
    static const int32_t RELATION_MASK = 7;

    /**
     * ASCII [:P:] and [:S:]:
     * [\u0021-\u002F \u003A-\u0040 \u005B-\u0060 \u007B-\u007E]
     */
    static UBool isSyntaxChar(UChar32 c);
    int32_t skipWhiteSpace(int32_t i) const;

    const Normalizer2 &nfd, &fcc;

    const UnicodeString *rules;
    CollationSettings *settings;
    UParseError *parseError;
    const char *errorReason;

    int32_t ruleIndex;

    UnicodeString raw;
    // Tailoring strings are normalized to FCC:
    // We need a canonical form so that we can find duplicates,
    // and we want to tailor only strings that pass the FCD test.
    // FCD itself is not a unique form.
    // FCC also preserves most composites which helps with storing
    // tokenized rules in a compact form.
    UnicodeString prefix, str, expansion;

    UVector32 tokens;
    int32_t tokenIndex;

    UnicodeString tokenStrings;
    UnicodeSet tailoredSet;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONRULEPARSER_H__
