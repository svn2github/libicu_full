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
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationruleparser.h"
#include "collationsettings.h"
#include "patternprops.h"
#include "uassert.h"
#include "uvectr32.h"

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
    // TODO: reset parseError
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
        int32_t relation = result & RELATION_MASK;
        if(resetStrength >= PRIMARY) {
            // reset-before rule chain
            if(isFirstRelation) {
                if(relation != resetStrength) {
                    setError("reset-before strength differs from its first relation", errorCode);
                    return;
                }
            } else {
                if(relation < resetStrength) {
                    setError("reset-before strength followed by a stronger relation", errorCode);
                    return;
                }
            }
        }
        int32_t i = ruleIndex + (result >> 8);  // skip over the relation operator
        if((result & 0x10) == 0) {
            parseRelationStrings(i, errorCode);
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
    int32_t relation, resetStrength;
    if(rules->compare(i, BEFORE_LENGTH, BEFORE, 0, BEFORE_LENGTH) == 0 &&
            (j = i + BEFORE_LENGTH) < rules->length() &&
            PatternProps::isWhiteSpace(rules->charAt(j)) &&
            ((j = skipWhiteSpace(j + 1)) + 1) < rules->length() &&
            0x31 <= (c = rules->charAt(j)) && c <= 0x33 &&
            rules->charAt(j + 1) == 0x5d) {
        // &[before n] with n=1 or 2 or 3
        relation = RESET_BEFORE;
        resetStrength = PRIMARY + (c - 0x31);
        i = skipWhiteSpace(j + 2);
    } else {
        relation = resetStrength = RESET_AT;
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
    makeAndInsertToken(relation, errorCode);
    ruleIndex = i;
    return resetStrength;
}

int32_t
CollationRuleParser::parseRelationOperator(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || ruleIndex >= rules->length()) { return NO_RELATION; }
    int32_t relation;
    int32_t i = ruleIndex;
    UChar c = rules->charAt(i++);
    switch(c) {
    case 0x3c:  // '<'
        if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<
            ++i;
            if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<<
                ++i;
                relation = TERTIARY;
            } else {
                relation = SECONDARY;
            }
        } else {
            relation = PRIMARY;
        }
        if(i < rules->length() && rules->charAt(i) == 0x2a) {  // '*'
            ++i;
            relation |= 0x10;
        }
        break;
    case 0x3b:  // ';' same as <<
        relation = SECONDARY;
        break;
    case 0x2c:  // ',' same as <<<
        relation = TERTIARY;
        break;
    case 0x3d:  // '='
        relation = IDENTICAL;
        if(i < rules->length() && rules->charAt(i) == 0x2a) {  // '*'
            ++i;
            relation |= 0x10;
        }
        break;
    default:
        return NO_RELATION;
    }
    return relation | ((i - ruleIndex) << 8);
}

void
CollationRuleParser::parseRelationStrings(int32_t i, UErrorCode &errorCode) {
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
    ruleIndex = i;
}

void
CollationRuleParser::parseStarredCharacters(int32_t relation, int32_t i, UErrorCode &errorCode) {
    resetTailoringStrings();
    i = parseString(i, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    for(int32_t j = 0; j < raw.length() && U_SUCCESS(errorCode);) {
        UChar32 c = raw.char32At(j);
        if(!nfd.isInert(c)) {
            setError("starred-relation string is not all NFD-inert", errorCode);
            return;
        }
        str.setTo(c);
        makeAndInsertToken(relation, errorCode);
        j += U16_LENGTH(c);
    }
    ruleIndex = i;
}

int32_t
CollationRuleParser::parseTailoringString(int32_t i, UErrorCode &errorCode) {
    i = parseString(i, errorCode);
    int32_t nfdLength = nfd.normalize(raw, errorCode).length();
    if(nfdLength > 31) {  // Limited by token string encoding.
        setError("tailoring string too long", errorCode);
    }
    return i;
}

int32_t
CollationRuleParser::parseString(int32_t i, UErrorCode &errorCode) {
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
                // Find the end of the quoted literal text.
                for(;;) {
                    int32_t j = rules->indexOf(0x27, i);
                    if(j < 0) {
                        setError("quoted literal text missing terminating apostrophe", errorCode);
                        return i;
                    }
                    raw.append(*rules, i, j - i);
                    i = j + 1;
                    if(i < rules->length() && rules->charAt(i) == 0x27) {
                        // Double apostrophe inside quoted literal text,
                        // still encodes a single apostrophe.
                        raw.append((UChar)0x27);
                        ++i;
                    } else {
                        break;
                    }
                }
            } else if(c == 0x5c) {  // backslash
                if(i == rules->length()) {
                    setError("backslash escape at the end of the rule string", errorCode);
                    return i;
                }
                c = rules->char32At(i);
                raw.append(c);
                i += U16_LENGTH(c);
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

int32_t
CollationRuleParser::parseSpecialPosition(int32_t i, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    errorCode = U_UNSUPPORTED_ERROR;  // TODO
    // TODO: store special position as contraction U+FFFE + number
    return i;
}

void
CollationRuleParser::parseSetting(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    errorCode = U_UNSUPPORTED_ERROR;  // TODO
}

int32_t
CollationRuleParser::skipComment(int32_t i) const {
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
    U_ASSERT(NO_RELATION < relation && relation <= IDENTICAL);
    // Find the tailoring in a previous rule chain.
    // Merge the new relation with any existing one for prefix+str.
    // If new, then encode the relation and tailoring strings into a token word,
    // and insert the token at tokenIndex.
    int32_t token = relation;
    U_ASSERT(!str.isEmpty());
    UChar32 c;
    if(prefix.isEmpty() && !str.hasMoreChar32Than(0, 0x7fffffff, 1)) {
        c = str.char32At(0);
    } else {
        c = -1;
    }
    if(c >= 0 && expansion.isEmpty()) {
        if(tailoredSet.contains(c)) {
            // TODO: search for existing token
        } else {
            tailoredSet.add(c);
        }
        token |= c << VALUE_SHIFT;
    } else {
        UnicodeString s;
        int32_t lengthsWord = (prefix.length() << 8) | str.length();
        s.append((UChar)lengthsWord).append(prefix).append(str);
        int32_t lengthsSum = prefix.length() + str.length();  // need 6 bits for max 31+31
        token |= lengthsSum << LENGTHS_SUM_SHIFT;
        if(c >= 0 ? tailoredSet.contains(c) : tailoredSet.contains(s)) {
            // TODO: search for existing token
        } else if(c >= 0) {
            tailoredSet.add(c);
        } else {
            tailoredSet.add(s);
        }
        int32_t value = tokenStrings.length();
        if(value > MAX_VALUE) {
            setError("total tailoring strings overflow", errorCode);
            return;
        }
        token |= ~value << VALUE_SHIFT;
        tokenStrings.append(s);
        if(!expansion.isEmpty()) {
            token |= HAS_EXPANSION;
            tokenStrings.append((UChar)expansion.length()).append(expansion);
        }
    }
    if(relation >= PRIMARY) {
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
    if(parseError != NULL) {
        // Note: This relies on the calling code maintaining the ruleIndex
        // at a position that is useful for debugging.
        // For example, at the beginning of a reset or relation etc.
        // TODO
    }
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
