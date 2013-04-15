/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationruleparser.cpp
*
* created on: 2013apr10
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/normalizer2.h"
#include "unicode/parseerr.h"
#include "unicode/ucol.h"
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationruleparser.h"
#include "collationsettings.h"
#include "patternprops.h"
#include "uassert.h"
#include "uvectr32.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

namespace {

static const UChar BEFORE[] = { 0x5b, 0x62, 0x65, 0x66, 0x6f, 0x72, 0x65, 0 };  // "[before"
const int32_t BEFORE_LENGTH = 7;

}  // namespace

CollationRuleParser::CollationRuleParser(UErrorCode &errorCode)
        : nfd(*Normalizer2::getNFDInstance(errorCode)),
          fcc(*Normalizer2::getInstance(NULL, "nfc", UNORM2_COMPOSE_CONTIGUOUS, errorCode)),
          rules(NULL), settings(NULL),
          parseError(NULL), errorReason(NULL),
          ruleIndex(0),
          tokens(errorCode), tokenIndex(0) {
    tokens.addElement(0, errorCode);  // sentinel
}

CollationRuleParser::~CollationRuleParser() {
}

void
CollationRuleParser::parse(const UnicodeString &ruleString,
                           CollationSettings &outSettings,
                           UParseError *outParseError,
                           UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    settings = &outSettings;
    parseError = outParseError;
    if(parseError != NULL) {
        parseError->line = 0;
        parseError->offset = 0;
        parseError->preContext[0] = 0;
        parseError->postContext[0] = 0;
    }
    errorReason = NULL;
    tokens.removeAllElements();
    tokens.addElement(0, errorCode);  // sentinel
    parse(ruleString, errorCode);
}

void
CollationRuleParser::parse(const UnicodeString &ruleString, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    rules = &ruleString;
    ruleIndex = 0;

    while(ruleIndex < rules->length()) {
        UChar c = rules->charAt(ruleIndex);
        if(PatternProps::isWhiteSpace(c)) {
            ++ruleIndex;
            continue;
        }
        switch(c) {
        case 0x26:  // '&'
            parseRuleChain(errorCode);
            break;
        case 0x5b:  // '['
            parseSetting(errorCode);
            break;
        case 0x23:  // '#' starts a comment, until the end of the line
            ruleIndex = skipComment(ruleIndex + 1);
            break;
        case 0x40:  // '@' is equivalent to [backwards 2]
            settings->setFlag(CollationSettings::BACKWARD_SECONDARY,
                              UCOL_ON, 0, errorCode);
            ++ruleIndex;
            break;
        case 0x21:  // '!' used to turn on Thai/Lao character reversal
            // Accept but ignore. The root collator has contractions
            // that are equivalent to the character reversal, where appropriate.
            ++ruleIndex;
            break;
        default:
            setError("expected a reset or setting or comment", errorCode);
            break;
        }
        if(U_FAILURE(errorCode)) { return; }
    }
}

void
CollationRuleParser::parseRuleChain(UErrorCode &errorCode) {
    int32_t resetStrength = parseResetAndPosition(errorCode);
    UBool isFirstRelation = TRUE;
    for(;;) {
        int32_t result = parseRelationOperator(errorCode);
        if(U_FAILURE(errorCode)) { return; }
        if(result == NO_RELATION) {
            if(ruleIndex < rules->length() && rules->charAt(ruleIndex) == 0x23) {
                // '#' starts a comment, until the end of the line
                ruleIndex = skipWhiteSpace(skipComment(ruleIndex + 1));
                continue;
            }
            if(isFirstRelation) {
                setError("reset not followed by a relation", errorCode);
            }
            return;
        }
        int32_t strength = result & STRENGTH_MASK;
        if(resetStrength < IDENTICAL) {
            // reset-before rule chain
            if(isFirstRelation) {
                if(strength != resetStrength) {
                    setError("reset-before strength differs from its first relation", errorCode);
                    return;
                }
            } else {
                if(strength < resetStrength) {
                    setError("reset-before strength followed by a stronger relation", errorCode);
                    return;
                }
            }
        }
        int32_t i = ruleIndex + (result >> 8);  // skip over the relation operator
        int32_t relation = result & RELATION_MASK;
        if((result & 0x10) == 0) {
            parseRelationStrings(relation, i, errorCode);
        } else {
            parseStarredCharacters(relation, i, errorCode);
        }
        if(U_FAILURE(errorCode)) { return; }
        isFirstRelation = FALSE;
    }
}

int32_t
CollationRuleParser::parseResetAndPosition(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NO_RELATION; }
    int32_t i = skipWhiteSpace(ruleIndex + 1);
    int32_t j;
    UChar c;
    int32_t resetStrength;
    if(rules->compare(i, BEFORE_LENGTH, BEFORE, 0, BEFORE_LENGTH) == 0 &&
            (j = i + BEFORE_LENGTH) < rules->length() &&
            PatternProps::isWhiteSpace(rules->charAt(j)) &&
            ((j = skipWhiteSpace(j + 1)) + 1) < rules->length() &&
            0x31 <= (c = rules->charAt(j)) && c <= 0x33 &&
            rules->charAt(j + 1) == 0x5d) {
        // &[before n] with n=1 or 2 or 3
        resetStrength = PRIMARY + (c - 0x31);
        i = skipWhiteSpace(j + 2);
    } else {
        resetStrength = IDENTICAL;
    }
    if(i >= rules->length()) {
        setError("reset without position", errorCode);
        return NO_RELATION;
    }
    resetTailoringStrings();
    if(rules->charAt(i) == 0x5b) {  // '['
        i = parseSpecialPosition(i, errorCode);
    } else {
        i = parseTailoringString(i, errorCode);
        fcc.normalize(raw, str, errorCode);
    }
    // TODO: find and merge new reset into existing rule chain before makeAndInsertToken()
    // TODO: look back from the old one: if it is in a reset-before chain, then return its before-strength
    makeAndInsertToken(resetStrength, errorCode);
    ruleIndex = i;
    return resetStrength;
}

int32_t
CollationRuleParser::parseRelationOperator(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || ruleIndex >= rules->length()) { return NO_RELATION; }
    int32_t strength;
    int32_t i = ruleIndex;
    UChar c = rules->charAt(i++);
    switch(c) {
    case 0x3c:  // '<'
        if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<
            ++i;
            if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<<
                ++i;
                strength = TERTIARY;
            } else {
                strength = SECONDARY;
            }
        } else {
            strength = PRIMARY;
        }
        if(i < rules->length() && rules->charAt(i) == 0x2a) {  // '*'
            ++i;
            strength |= 0x10;
        }
        break;
    case 0x3b:  // ';' same as <<
        strength = SECONDARY;
        break;
    case 0x2c:  // ',' same as <<<
        strength = TERTIARY;
        break;
    case 0x3d:  // '='
        strength = IDENTICAL;
        if(i < rules->length() && rules->charAt(i) == 0x2a) {  // '*'
            ++i;
            strength |= 0x10;
        }
        break;
    default:
        return NO_RELATION;
    }
    return ((i - ruleIndex) << 8) | DIFF | strength;
}

void
CollationRuleParser::parseRelationStrings(int32_t relation, int32_t i, UErrorCode &errorCode) {
    // Parse
    //     prefix | str / expansion
    // where prefix and expansion are optional.
    resetTailoringStrings();
    i = parseTailoringString(i, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UChar next = (i < rules->length()) ? rules->charAt(i) : 0;
    if(next == 0x7c) {  // '|' separates the context prefix from the string.
        fcc.normalize(raw, prefix, errorCode);
        i = parseTailoringString(i + 1, errorCode);
        if(U_FAILURE(errorCode)) { return; }
        next = (i < rules->length()) ? rules->charAt(i) : 0;
    }
    fcc.normalize(raw, str, errorCode);
    if(next == 0x2f) {  // '/' separates the string from the expansion.
        i = parseTailoringString(i + 1, errorCode);
        fcc.normalize(raw, expansion, errorCode);
    }
    // TODO: if(!prefix.isEmpty()) { check that prefix and str start with hasBoundaryBefore }
    makeAndInsertToken(relation, errorCode);
    ruleIndex = i;
}

void
CollationRuleParser::parseStarredCharacters(int32_t relation, int32_t i, UErrorCode &errorCode) {
    resetTailoringStrings();
    i = parseString(i, TRUE, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UChar32 prev = -1;
    for(int32_t j = 0; j < raw.length() && U_SUCCESS(errorCode);) {
        UChar32 c = raw.char32At(j);
        if(c != 0x2d) {  // '-'
            if(!nfd.isInert(c)) {
                setError("starred-relation string is not all NFD-inert", errorCode);
                return;
            }
            str.setTo(c);
            makeAndInsertToken(relation, errorCode);
            j += U16_LENGTH(c);
            prev = c;
        } else {
            if(prev < 0) {
                setError("range without start in starred-relation string", errorCode);
                return;
            }
            if(++j == raw.length()) {
                setError("range without end in starred-relation string", errorCode);
                return;
            }
            c = raw.char32At(j);
            if(!nfd.isInert(c)) {
                setError("starred-relation string is not all NFD-inert", errorCode);
                return;
            }
            if(c < prev) {
                setError("range start greater than end in starred-relation string", errorCode);
                return;
            }
            j += U16_LENGTH(c);
            // range prev-c
            while(++prev <= c) {
                str.setTo(prev);
                makeAndInsertToken(relation, errorCode);
            }
            prev = -1;
        }
    }
    ruleIndex = i;
}

int32_t
CollationRuleParser::parseTailoringString(int32_t i, UErrorCode &errorCode) {
    i = parseString(i, FALSE, errorCode);
    int32_t nfdLength = nfd.normalize(raw, errorCode).length();
    if(nfdLength > 31) {  // Limited by token string encoding.
        setError("tailoring string too long", errorCode);
    }
    return i;
}

int32_t
CollationRuleParser::parseString(int32_t i, UBool allowDash, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return i; }
    raw.remove();
    for(i = skipWhiteSpace(i); i < rules->length();) {
        UChar32 c = rules->charAt(i++);
        if(isSyntaxChar(c)) {
            if(c == 0x27) {  // apostrophe
                if(i < rules->length() && rules->charAt(i) == 0x27) {
                    // Double apostrophe, encodes a single one.
                    raw.append((UChar)0x27);
                    ++i;
                    continue;
                }
                // Quote literal text until the next single apostrophe.
                for(;;) {
                    if(i == rules->length()) {
                        setError("quoted literal text missing terminating apostrophe", errorCode);
                        return i;
                    }
                    c = rules->charAt(i++);
                    if(c == 0x27) {
                        if(i < rules->length() && rules->charAt(i) == 0x27) {
                            // Double apostrophe inside quoted literal text,
                            // still encodes a single apostrophe.
                            ++i;
                        } else {
                            break;
                        }
                    }
                    raw.append((UChar)c);
                }
            } else if(c == 0x5c) {  // backslash
                if(i == rules->length()) {
                    setError("backslash escape at the end of the rule string", errorCode);
                    return i;
                }
                c = rules->char32At(i);
                raw.append(c);
                i += U16_LENGTH(c);
            } else if(c == 0x2d && allowDash) {  // '-'
                raw.append((UChar)c);
            } else {
                // Any other syntax character terminates a string.
                break;
            }
        } else if(PatternProps::isWhiteSpace(c)) {
            // Unquoted white space terminates a string.
            i = skipWhiteSpace(i);
            break;
        } else {
            raw.append((UChar)c);
        }
    }
    if(raw.isEmpty()) {
        setError("missing string", errorCode);
        return i;
    }
    for(int32_t j = 0; j < raw.length();) {
        UChar32 c = raw.char32At(j);
        if(U_IS_SURROGATE(c)) {
            setError("string contains an unpaired surrogate", errorCode);
            return i;
        }
        if(c == 0xfffe || c == 0xffff) {
            setError("string contains U+FFFE or U+FFFF", errorCode);
            return i;
        }
        j += U16_LENGTH(c);
    }
    return i;
}

namespace {

static const char *const positions[] = {
    "first tertiary ignorable",
    "last tertiary ignorable",
    "first secondary ignorable",
    "last secondary ignorable",
    "first primary ignorable",
    "last primary ignorable",
    "first variable",
    "last variable",
    "first implicit",
    "last implicit",
    "first regular",
    "last regular",
    "first trailing",
    "last trailing"
};

}  // namespace

int32_t
CollationRuleParser::parseSpecialPosition(int32_t i, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    int32_t j = readWords(i);
    if(j > i && rules->charAt(j) == 0x5d) {  // words end with ]
        ++j;
        for(int32_t pos = 0; pos < LENGTHOF(positions); ++pos) {
            if(raw == UnicodeString(positions[pos], -1, US_INV)) {
                str.setTo((UChar)POS_LEAD).append((UChar)(POS_BASE + pos));
                return j;
            }
        }
        if(raw == UNICODE_STRING_SIMPLE("top")) {
            str.setTo((UChar)POS_LEAD).append((UChar)(POS_BASE + LAST_REGULAR));
            return j;
        }
        if(raw == UNICODE_STRING_SIMPLE("variable top")) {
            str.setTo((UChar)POS_LEAD).append((UChar)(POS_BASE + LAST_VARIABLE));
            return j;
        }
    }
    setError("not a valid special reset position", errorCode);
    return i;
}

void
CollationRuleParser::parseSetting(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t i = ruleIndex + 1;
    int32_t j = readWords(i);
    if(j > i) {
        if(rules->charAt(j) == 0x5d) {  // words end with ]
            ++j;
            if(raw.startsWith(UNICODE_STRING_SIMPLE("reorder "))) {
                parseReordering(errorCode);
                return;
            }
            if(raw == UNICODE_STRING_SIMPLE("backwards 2")) {
                settings->setFlag(CollationSettings::BACKWARD_SECONDARY,
                                  UCOL_ON, 0, errorCode);
                ruleIndex = j;
                return;
            }
            UnicodeString v;
            int32_t valueIndex = raw.lastIndexOf((UChar)0x20);
            if(valueIndex >= 0) {
                v.setTo(raw, valueIndex + 1);
                raw.truncate(valueIndex);
            }
            if(raw == UNICODE_STRING_SIMPLE("strength") && v.length() == 1) {
                int32_t value = UCOL_DEFAULT;
                UChar c = v.charAt(0);
                if(0x31 <= c && c <= 0x34) {  // 1..4
                    value = UCOL_PRIMARY + (c - 0x31);
                } else if(c == 0x49) {  // 'I'
                    value = UCOL_IDENTICAL;
                }
                if(value != UCOL_DEFAULT) {
                    settings->setStrength(value, 0, errorCode);
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("alternate")) {
                UColAttributeValue value = UCOL_DEFAULT;
                if(v == UNICODE_STRING_SIMPLE("non-ignorable")) {
                    value = UCOL_NON_IGNORABLE;
                } else if(v == UNICODE_STRING_SIMPLE("shifted")) {
                    value = UCOL_SHIFTED;
                }
                if(value != UCOL_DEFAULT) {
                    settings->setAlternateHandling(value, 0, errorCode);
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("caseFirst")) {
                UColAttributeValue value = UCOL_DEFAULT;
                if(v == UNICODE_STRING_SIMPLE("off")) {
                    value = UCOL_OFF;
                } else if(v == UNICODE_STRING_SIMPLE("lower")) {
                    value = UCOL_LOWER_FIRST;
                } else if(v == UNICODE_STRING_SIMPLE("upper")) {
                    value = UCOL_UPPER_FIRST;
                }
                if(value != UCOL_DEFAULT) {
                    settings->setCaseFirst(value, 0, errorCode);
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("caseLevel")) {
                UColAttributeValue value = getOnOffValue(v);
                if(value != UCOL_DEFAULT) {
                    settings->setFlag(CollationSettings::CASE_LEVEL, value, 0, errorCode);
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("normalization")) {
                UColAttributeValue value = getOnOffValue(v);
                if(value != UCOL_DEFAULT) {
                    settings->setFlag(CollationSettings::CHECK_FCD, value, 0, errorCode);
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("numericOrdering")) {
                UColAttributeValue value = getOnOffValue(v);
                if(value != UCOL_DEFAULT) {
                    settings->setFlag(CollationSettings::NUMERIC, value, 0, errorCode);
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("hiraganaQ")) {
                UColAttributeValue value = getOnOffValue(v);
                if(value != UCOL_DEFAULT) {
                    if(value == UCOL_ON) {
                        setError("[hiraganaQ on] is not supported", errorCode);  // TODO
                    }
                    // TODO
                    ruleIndex = j;
                    return;
                }
            } else if(raw == UNICODE_STRING_SIMPLE("import")) {
                // TODO
            }
        } else if(rules->charAt(j) == 0x5b) {  // words end with [
            // TODO: parseUnicodeSet()
            if(raw == UNICODE_STRING_SIMPLE("optimize")) {
                // TODO
            } else if(raw == UNICODE_STRING_SIMPLE("suppressContractions")) {
                // TODO
            }
        }
    }
    setError("not a valid setting/option", errorCode);
}

void
CollationRuleParser::parseReordering(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    errorCode = U_UNSUPPORTED_ERROR;  // TODO
    int32_t i = 8;  // after "reorder "
}

UColAttributeValue
CollationRuleParser::getOnOffValue(const UnicodeString &s) {
    if(s == UNICODE_STRING_SIMPLE("on")) {
        return UCOL_ON;
    } else if(s == UNICODE_STRING_SIMPLE("off")) {
        return UCOL_OFF;
    } else {
        return UCOL_DEFAULT;
    }
}

int32_t
CollationRuleParser::readWords(int32_t i) {
    static const UChar sp = 0x20;
    raw.remove();
    i = skipWhiteSpace(i);
    for(;;) {
        if(i >= rules->length()) { return 0; }
        UChar c = rules->charAt(i);
        if(isSyntaxChar(c) && c != 0x2d && c != 0x5f) {  // syntax except -_
            if(raw.isEmpty()) { return 0; }
            if(raw.endsWith(&sp, 1)) {  // remove trailing space
                raw.truncate(raw.length() - 1);
            }
            return i;
        }
        if(PatternProps::isWhiteSpace(c)) {
            raw.append(0x20);
            i = skipWhiteSpace(i + 1);
        } else {
            raw.append(c);
            ++i;
        }
    }
}

int32_t
CollationRuleParser::skipComment(int32_t i) const {
    // skip to past the newline
    while(i < rules->length()) {
        UChar c = rules->charAt(i++);
        // LF or FF or CR or NEL or LS or PS
        if(c == 0xa || c == 0xc || c == 0xd || c == 0x85 || c == 0x2028 || c == 0x2029) {
            // Unicode Newline Guidelines: "A readline function should stop at NLF, LS, FF, or PS."
            // NLF (new line function) = CR or LF or CR+LF or NEL.
            // No need to collect all of CR+LF because a following LF will be ignored anyway.
            break;
        }
    }
    return i;
}

void
CollationRuleParser::makeAndInsertToken(int32_t relation, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    U_ASSERT(NO_RELATION < (relation & STRENGTH_MASK) && (relation & STRENGTH_MASK) <= IDENTICAL);
    U_ASSERT((relation & ~RELATION_MASK) == 0);
    U_ASSERT(!str.isEmpty());
    // Find the tailoring in a previous rule chain.
    // Merge the new relation with any existing one for prefix+str.
    // If new, then encode the relation and tailoring strings into a token word,
    // and insert the token at tokenIndex.
    int32_t token = relation;
    if(!prefix.isEmpty()) {
        token |= HAS_PREFIX;
    }
    if(str.hasMoreChar32Than(0, 0x7fffffff, 1)) {
        token |= HAS_CONTRACTION;
    }
    if(!expansion.isEmpty()) {
        token |= HAS_EXPANSION;
    }
    UChar32 c = str.char32At(0);
    if((token & HAS_STRINGS) == 0) {
        if(tailoredSet.contains(c)) {
            // TODO: search for existing token
        } else {
            tailoredSet.add(c);
        }
        token |= c << VALUE_SHIFT;
    } else {
        // The relation maps from str if preceded by the prefix. Look for a duplicate.
        // We ignore the expansion because that adds to the CEs for the relation
        // but is not part of what is being tailored (what is mapped *from*).
        UnicodeString s(prefix);
        s.append(str);
        int32_t lengthsWord = prefix.length() | (str.length() << 5);
        if((token & HAS_CONTEXT) == 0 ? tailoredSet.contains(c) : tailoredSet.contains(s)) {
            // TODO: search for existing token
        } else if((token & HAS_CONTEXT) == 0) {
            tailoredSet.add(c);
        } else {
            tailoredSet.add(s);
        }
        int32_t value = tokenStrings.length();
        if(value > MAX_VALUE) {
            setError("total tailoring strings overflow", errorCode);
            return;
        }
        token |= ~value << VALUE_SHIFT;  // negative
        lengthsWord |= (expansion.length() << 10);
        tokenStrings.append((UChar)lengthsWord).append(s).append(expansion);
    }
    if((relation & DIFF) != 0) {
        // Postpone insertion:
        // Insert the new relation before the next token with a relation at least as strong.
        // Stops before resets and the sentinel.
        for(;;) {
            int32_t t = tokens.elementAti(tokenIndex);
            if((t & RELATION_MASK) <= relation) { break; }
            ++tokenIndex;
        }
    } else {
        // Append a new reset at the end of the token list,
        // before the sentinel, starting a new rule chain.
        tokenIndex = tokens.size() - 1;
    }
    tokens.insertElementAt(token, tokenIndex++, errorCode);
}

void
CollationRuleParser::resetTailoringStrings() {
    prefix.remove();
    str.remove();
    expansion.remove();
}

void
CollationRuleParser::setError(const char *reason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    errorCode = U_PARSE_ERROR;
    errorReason = reason;
    if(parseError == NULL) { return; }

    // Note: This relies on the calling code maintaining the ruleIndex
    // at a position that is useful for debugging.
    // For example, at the beginning of a reset or relation etc.
    parseError->offset = ruleIndex;
    parseError->line = 0;  // We are not counting line numbers.

    // before ruleIndex
    int32_t start = ruleIndex - (U_PARSE_CONTEXT_LEN - 1);
    if(start < 0) {
        start = 0;
    } else if(start > 0 && U16_IS_TRAIL(rules->charAt(start))) {
        ++start;
    }
    int32_t length = ruleIndex - start;
    rules->extract(start, length, parseError->preContext);
    parseError->preContext[length] = 0;

    // starting from ruleIndex
    length = rules->length() - ruleIndex;
    if(length >= U_PARSE_CONTEXT_LEN) {
        length = U_PARSE_CONTEXT_LEN - 1;
        if(U16_IS_LEAD(rules->charAt(ruleIndex + length - 1))) {
            --length;
        }
    }
    rules->extract(ruleIndex, length, parseError->postContext);
    parseError->postContext[length] = 0;
}

UBool
CollationRuleParser::isSyntaxChar(UChar32 c) {
    return 0x21 <= c && c <= 0x7e &&
            (c <= 0x2f || (0x3a <= c && c <= 0x40) ||
            (0x5b <= c && c <= 0x60) || (0x7b <= c));
}

int32_t
CollationRuleParser::skipWhiteSpace(int32_t i) const {
    while(i < rules->length() && PatternProps::isWhiteSpace(rules->charAt(i))) {
        ++i;
    }
    return i;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
