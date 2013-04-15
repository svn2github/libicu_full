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

struct UParseError;

U_NAMESPACE_BEGIN

class Normalizer2;
class UVector32;

struct CollationSettings;

class U_I18N_API CollationRuleParser : public UMemory {
public:
    /** Token bits 31..8 contain the code point or negative string index. */
    static const int32_t VALUE_SHIFT = 8;
    /** Maximum non-negative value (23 bits). */
    static const int32_t MAX_VALUE = 0x7fffff;
    // Token bit 7 is unused.
    /** Token bit 6: The token has an expansion string (e in p|s/e). */
    static const int32_t HAS_EXPANSION = 0x40;
    /** Token bit 5: The token has a contraction string (s has >1 code points in p|s/e). */
    static const int32_t HAS_CONTRACTION = 0x20;
    /** Token bit 4: The token has a prefix string (p in p|s/e). */
    static const int32_t HAS_PREFIX = 0x10;
    /** Token bit 3: Difference relation, not a reset. (Reset compares lower.) */
    static const int32_t DIFF = 8;
    /**
     * Token bits 2..0 contain the relation strength value.
     * "Stronger" relations must compare lower.
     * When DIFF is not set, this is the reset-before strength.
     */
    static const int32_t STRENGTH_MASK = 7;

    /** DIFF & strength together define the relation. */
    static const int32_t RELATION_MASK = DIFF | STRENGTH_MASK;
    /** All HAS_XYZ bits. If none are set, then the token value is the code point. */
    static const int32_t HAS_STRINGS = HAS_EXPANSION | HAS_CONTRACTION | HAS_PREFIX;
    /** The token has a contraction and/or a prefix. */
    static const int32_t HAS_CONTEXT = HAS_CONTRACTION | HAS_PREFIX;

    /** Sentinel value. The token integer should be zero. */
    static const int32_t NO_RELATION = 0;
    static const int32_t PRIMARY = 1;
    static const int32_t SECONDARY = 2;
    static const int32_t TERTIARY = 3;
    static const int32_t QUATERNARY = 4;
    /** Used for reset-at (without DIFF) and identical relation (with DIFF). */
    static const int32_t IDENTICAL = 5;
    // Strength values 6 & 7 are unused.

    /** Special reset positions. */
    enum Position {
        FIRST_TERTIARY_IGNORABLE,
        LAST_TERTIARY_IGNORABLE,
        FIRST_SECONDARY_IGNORABLE,
        LAST_SECONDARY_IGNORABLE,
        FIRST_PRIMARY_IGNORABLE,
        LAST_PRIMARY_IGNORABLE,
        FIRST_VARIABLE,
        LAST_VARIABLE,
        FIRST_IMPLICIT,
        LAST_IMPLICIT,
        FIRST_REGULAR,
        LAST_REGULAR,
        FIRST_TRAILING,
        LAST_TRAILING
    };

    /**
     * First character of contractions that encode special reset positions.
     * U+FFFE cannot be tailored via rule syntax.
     *
     * The second contraction character is POS_BASE + Position.
     */
    static const UChar POS_LEAD = 0xfffe;
    /**
     * Base for the second character of contractions that encode special reset positions.
     * Braille characters U+28xx are printable and normalization-inert.
     * @see POS_LEAD
     */
    static const UChar POS_BASE = 0x2800;

    CollationRuleParser(UErrorCode &errorCode);
    ~CollationRuleParser();

    void parse(const UnicodeString &ruleString,
               CollationSettings &outSettings,
               UParseError *outParseError,
               UErrorCode &errorCode);

    const char *getErrorReason() const { return errorReason; }

    UBool modifiesSettings() const { return TRUE; }  // TODO
    UBool modifiesMappings() const { return TRUE; }  // TODO

    // TODO: Random access API
    int32_t findReset(int32_t start) const { return -1; }
    int32_t findRelation(int32_t strength, int32_t start) const { return 0; }  // up to any stronger relation
    int32_t countRelationMaxStrength(int32_t start) const { return 0; }  // (count << 4) | maxStrength, up to end of rule chain
    int32_t countRelation(int32_t strength, int32_t start) const { return 0; }  // up to any stronger relation

    int32_t getTokenAndSetStrings(int32_t i) { return NO_RELATION; }
    const UnicodeString &getPrefix() const { return prefix; }
    const UnicodeString &getString() const { return str; }
    const UnicodeString &getExpansion() const { return expansion; }

private:
    void parse(const UnicodeString &ruleString, UErrorCode &errorCode);
    void parseRuleChain(UErrorCode &errorCode);
    int32_t parseResetAndPosition(UErrorCode &errorCode);
    int32_t parseRelationOperator(UErrorCode &errorCode);
    void parseRelationStrings(int32_t strength, int32_t i, UErrorCode &errorCode);
    void parseStarredCharacters(int32_t strength, int32_t i, UErrorCode &errorCode);
    int32_t parseTailoringString(int32_t i, UErrorCode &errorCode);
    int32_t parseString(int32_t i, UBool allowDash, UErrorCode &errorCode);

    /**
     * Sets str to a contraction of U+FFFE and (U+2800 + Position).
     * @return rule index after the special reset position
     */
    int32_t parseSpecialPosition(int32_t i, UErrorCode &errorCode);
    void parseSetting(UErrorCode &errorCode);
    void parseReordering(UErrorCode &errorCode);
    static UColAttributeValue getOnOffValue(const UnicodeString &s);

    int32_t readWords(int32_t i);
    int32_t skipComment(int32_t i) const;

    void makeAndInsertToken(int32_t relation, UErrorCode &errorCode);
    void resetTailoringStrings();

    void setError(const char *reason, UErrorCode &errorCode);

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
