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
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "charstr.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationruleparser.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "cstring.h"
#include "patternprops.h"
#include "uassert.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

namespace {

static const UChar BEFORE[] = { 0x5b, 0x62, 0x65, 0x66, 0x6f, 0x72, 0x65, 0 };  // "[before"
const int32_t BEFORE_LENGTH = 7;

}  // namespace

CollationRuleParser::Sink::~Sink() {}

void
CollationRuleParser::Sink::suppressContractions(const UnicodeSet &, const char *&, UErrorCode &) {}

void
CollationRuleParser::Sink::optimize(const UnicodeSet &, const char *&, UErrorCode &) {}

CollationRuleParser::Importer::~Importer() {}

CollationRuleParser::CollationRuleParser(const CollationData *base, UErrorCode &errorCode)
        : nfd(*Normalizer2::getNFDInstance(errorCode)),
          nfc(*Normalizer2::getNFCInstance(errorCode)),
          rules(NULL), baseData(base), settings(NULL),
          parseError(NULL), errorReason(NULL),
          sink(NULL), importer(NULL),
          ruleIndex(0) {
}

CollationRuleParser::~CollationRuleParser() {
}

void
CollationRuleParser::parse(const UnicodeString &ruleString,
                           CollationTailoring &outTailoring,
                           UParseError *outParseError,
                           UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    tailoring = &outTailoring;
    settings = &outTailoring.settings;
    parseError = outParseError;
    if(parseError != NULL) {
        parseError->line = 0;
        parseError->offset = -1;
        parseError->preContext[0] = 0;
        parseError->postContext[0] = 0;
    }
    errorReason = NULL;
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
            setParseError("expected a reset or setting or comment", errorCode);
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
        if(result < 0) {
            if(ruleIndex < rules->length() && rules->charAt(ruleIndex) == 0x23) {
                // '#' starts a comment, until the end of the line
                ruleIndex = skipWhiteSpace(skipComment(ruleIndex + 1));
                continue;
            }
            if(isFirstRelation) {
                setParseError("reset not followed by a relation", errorCode);
            }
            return;
        }
        int32_t strength = result & STRENGTH_MASK;
        if(resetStrength < UCOL_IDENTICAL) {
            // reset-before rule chain
            if(isFirstRelation) {
                if(strength != resetStrength) {
                    setParseError("reset-before strength differs from its first relation", errorCode);
                    return;
                }
            } else {
                if(strength < resetStrength) {
                    setParseError("reset-before strength followed by a stronger relation", errorCode);
                    return;
                }
            }
        }
        int32_t i = ruleIndex + (result >> OFFSET_SHIFT);  // skip over the relation operator
        if((result & STARRED_FLAG) == 0) {
            parseRelationStrings(strength, i, errorCode);
        } else {
            parseStarredCharacters(strength, i, errorCode);
        }
        if(U_FAILURE(errorCode)) { return; }
        isFirstRelation = FALSE;
    }
}

int32_t
CollationRuleParser::parseResetAndPosition(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return UCOL_DEFAULT; }
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
        resetStrength = UCOL_PRIMARY + (c - 0x31);
        i = skipWhiteSpace(j + 2);
    } else {
        resetStrength = UCOL_IDENTICAL;
    }
    if(i >= rules->length()) {
        setParseError("reset without position", errorCode);
        return UCOL_DEFAULT;
    }
    UnicodeString str;
    if(rules->charAt(i) == 0x5b) {  // '['
        i = parseSpecialPosition(i, str, errorCode);
    } else {
        i = parseTailoringString(i, str, errorCode);
    }
    sink->addReset(resetStrength, str, errorReason, errorCode);
    if(U_FAILURE(errorCode)) { setErrorContext(); }
    ruleIndex = i;
    return resetStrength;
}

int32_t
CollationRuleParser::parseRelationOperator(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || ruleIndex >= rules->length()) { return UCOL_DEFAULT; }
    int32_t strength;
    int32_t i = ruleIndex;
    UChar c = rules->charAt(i++);
    switch(c) {
    case 0x3c:  // '<'
        if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<
            ++i;
            if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<<
                ++i;
                if(i < rules->length() && rules->charAt(i) == 0x3c) {  // <<<<
                    ++i;
                    strength = UCOL_QUATERNARY;
                } else {
                    strength = UCOL_TERTIARY;
                }
            } else {
                strength = UCOL_SECONDARY;
            }
        } else {
            strength = UCOL_PRIMARY;
        }
        if(i < rules->length() && rules->charAt(i) == 0x2a) {  // '*'
            ++i;
            strength |= STARRED_FLAG;
        }
        break;
    case 0x3b:  // ';' same as <<
        strength = UCOL_SECONDARY;
        break;
    case 0x2c:  // ',' same as <<<
        strength = UCOL_TERTIARY;
        break;
    case 0x3d:  // '='
        strength = UCOL_IDENTICAL;
        if(i < rules->length() && rules->charAt(i) == 0x2a) {  // '*'
            ++i;
            strength |= STARRED_FLAG;
        }
        break;
    default:
        return UCOL_DEFAULT;
    }
    return ((i - ruleIndex) << OFFSET_SHIFT) | strength;
}

void
CollationRuleParser::parseRelationStrings(int32_t strength, int32_t i, UErrorCode &errorCode) {
    // Parse
    //     prefix | str / extension
    // where prefix and extension are optional.
    UnicodeString prefix, str, extension;
    i = parseTailoringString(i, str, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UChar next = (i < rules->length()) ? rules->charAt(i) : 0;
    if(next == 0x7c) {  // '|' separates the context prefix from the string.
        prefix = str;
        i = parseTailoringString(i + 1, str, errorCode);
        if(U_FAILURE(errorCode)) { return; }
        next = (i < rules->length()) ? rules->charAt(i) : 0;
    }
    if(next == 0x2f) {  // '/' separates the string from the extension.
        i = parseTailoringString(i + 1, extension, errorCode);
    }
    if(!prefix.isEmpty()) {
        UChar32 prefix0 = prefix.char32At(0);
        UChar32 c = str.char32At(0);
        if(!nfc.hasBoundaryBefore(prefix0) || !nfc.hasBoundaryBefore(c)) {
            setParseError("in 'prefix|str', prefix and str must each start with an NFC boundary",
                          errorCode);
            return;
        }
    }
    sink->addRelation(strength, prefix, str, extension, errorReason, errorCode);
    if(U_FAILURE(errorCode)) { setErrorContext(); }
    ruleIndex = i;
}

void
CollationRuleParser::parseStarredCharacters(int32_t strength, int32_t i, UErrorCode &errorCode) {
    UnicodeString empty, raw;
    i = parseString(i, TRUE, raw, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UChar32 prev = -1;
    for(int32_t j = 0; j < raw.length() && U_SUCCESS(errorCode);) {
        UChar32 c = raw.char32At(j);
        if(c != 0x2d) {  // '-'
            if(!nfd.isInert(c)) {
                setParseError("starred-relation string is not all NFD-inert", errorCode);
                return;
            }
            sink->addRelation(strength, empty, UnicodeString(c), empty, errorReason, errorCode);
            j += U16_LENGTH(c);
            prev = c;
        } else {
            if(prev < 0) {
                setParseError("range without start in starred-relation string", errorCode);
                return;
            }
            if(++j == raw.length()) {
                setParseError("range without end in starred-relation string", errorCode);
                return;
            }
            c = raw.char32At(j);
            if(!nfd.isInert(c)) {
                setParseError("starred-relation string is not all NFD-inert", errorCode);
                return;
            }
            if(c < prev) {
                setParseError("range start greater than end in starred-relation string", errorCode);
                return;
            }
            j += U16_LENGTH(c);
            // range prev-c
            while(++prev <= c) {
                sink->addRelation(strength, empty, UnicodeString(prev), empty, errorReason, errorCode);
            }
            prev = -1;
        }
    }
    if(U_FAILURE(errorCode)) { setErrorContext(); }
    ruleIndex = i;
}

int32_t
CollationRuleParser::parseTailoringString(int32_t i, UnicodeString &raw, UErrorCode &errorCode) {
    return parseString(i, FALSE, raw, errorCode);
}

int32_t
CollationRuleParser::parseString(int32_t i, UBool allowDash, UnicodeString &raw,
                                 UErrorCode &errorCode) {
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
                        setParseError("quoted literal text missing terminating apostrophe", errorCode);
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
                    setParseError("backslash escape at the end of the rule string", errorCode);
                    return i;
                }
                c = rules->char32At(i);
                raw.append(c);
                i += U16_LENGTH(c);
            } else if(c == 0x2d && allowDash) {  // '-'
                raw.append((UChar)c);
            } else {
                // Any other syntax character terminates a string.
                --i;
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
        setParseError("missing string", errorCode);
        return i;
    }
    for(int32_t j = 0; j < raw.length();) {
        UChar32 c = raw.char32At(j);
        if(U_IS_SURROGATE(c)) {
            setParseError("string contains an unpaired surrogate", errorCode);
            return i;
        }
        if(0xfffd <= c && c <= 0xffff) {
            setParseError("string contains U+FFFD, U+FFFE or U+FFFF", errorCode);
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
    "first regular",
    "last regular",
    "first implicit",
    "last implicit",
    "first trailing",
    "last trailing"
};

}  // namespace

int32_t
CollationRuleParser::parseSpecialPosition(int32_t i, UnicodeString &str, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    UnicodeString raw;
    int32_t j = readWords(i + 1, raw);
    if(j > i && rules->charAt(j) == 0x5d && !raw.isEmpty()) {  // words end with ]
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
    setParseError("not a valid special reset position", errorCode);
    return i;
}

void
CollationRuleParser::parseSetting(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    UnicodeString raw;
    int32_t i = ruleIndex + 1;
    int32_t j = readWords(i, raw);
    if(j <= i || raw.isEmpty()) {
        setParseError("expected a setting/option at '['", errorCode);
    }
    if(rules->charAt(j) == 0x5d) {  // words end with ]
        ++j;
        if(raw.startsWith(UNICODE_STRING_SIMPLE("reorder")) &&
                (raw.length() == 7 || raw.charAt(7) == 0x20)) {
            parseReordering(raw, errorCode);
            ruleIndex = j;
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
        // TODO: maxVariable
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
                    setParseError("[hiraganaQ on] is not supported", errorCode);  // TODO
                }
                // TODO
                ruleIndex = j;
                return;
            }
        } else if(raw == UNICODE_STRING_SIMPLE("import")) {
            CharString lang;
            lang.appendInvariantChars(v, errorCode);
            if(errorCode == U_MEMORY_ALLOCATION_ERROR) { return; }
            // BCP 47 language tag -> ICU locale ID
            char localeID[ULOC_FULLNAME_CAPACITY];
            int32_t parsedLength;
            int32_t length = uloc_forLanguageTag(lang.data(), localeID, ULOC_FULLNAME_CAPACITY,
                                                 &parsedLength, &errorCode);
            if(U_FAILURE(errorCode) ||
                    parsedLength != lang.length() || length >= ULOC_FULLNAME_CAPACITY) {
                errorCode = U_ZERO_ERROR;
                setParseError("expected language tag in [import langTag]", errorCode);
                return;
            }
            // localeID minus all keywords
            char baseID[ULOC_FULLNAME_CAPACITY];
            length = uloc_getBaseName(localeID, baseID, ULOC_FULLNAME_CAPACITY, &errorCode);
            if(U_FAILURE(errorCode) || length >= ULOC_KEYWORDS_CAPACITY) {
                errorCode = U_ZERO_ERROR;
                setParseError("expected language tag in [import langTag]", errorCode);
                return;
            }
            // @collation=type, or length=0 if not specified
            char collationType[ULOC_KEYWORDS_CAPACITY];
            length = uloc_getKeywordValue(localeID, "collation",
                                          collationType, ULOC_KEYWORDS_CAPACITY,
                                          &errorCode);
            if(U_FAILURE(errorCode) || length >= ULOC_KEYWORDS_CAPACITY) {
                errorCode = U_ZERO_ERROR;
                setParseError("expected language tag in [import langTag]", errorCode);
                return;
            }
            if(importer == NULL) {
                setParseError("[import langTag] is not supported", errorCode);
            } else {
                const UnicodeString *importedRules =
                    importer->getRules(baseID,
                                       length > 0 ? collationType : NULL,
                                       errorReason, errorCode);
                if(U_FAILURE(errorCode)) {
                    setErrorContext();
                    return;
                }
                const UnicodeString *outerRules = rules;
                int32_t outerRuleIndex = ruleIndex;
                parse(*importedRules, errorCode);
                if(U_FAILURE(errorCode)) {
                    if(parseError != NULL) {
                        parseError->offset = outerRuleIndex;
                    }
                }
                rules = outerRules;
                ruleIndex = j;
            }
            return;
        }
    } else if(rules->charAt(j) == 0x5b) {  // words end with [
        UnicodeSet set;
        j = parseUnicodeSet(raw, j, set, errorCode);
        if(U_FAILURE(errorCode)) { return; }
        if(raw == UNICODE_STRING_SIMPLE("optimize")) {
            sink->optimize(set, errorReason, errorCode);
            if(U_FAILURE(errorCode)) { setErrorContext(); }
            ruleIndex = j;
            return;
        } else if(raw == UNICODE_STRING_SIMPLE("suppressContractions")) {
            sink->suppressContractions(set, errorReason, errorCode);
            if(U_FAILURE(errorCode)) { setErrorContext(); }
            ruleIndex = j;
            return;
        }
    }
    setParseError("not a valid setting/option", errorCode);
}

void
CollationRuleParser::parseReordering(const UnicodeString &raw, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t i = 7;  // after "reorder"
    if(i == raw.length()) {
        // empty [reorder] with no codes
        uprv_free(const_cast<uint8_t *>(settings->reorderTable));
        settings->reorderTable = NULL;
        return;
    }
    // Count the codes in [reorder aa bb cc], same as the number of collapsed spaces.
    int32_t length = 0;
    for(int32_t j = i; j < raw.length(); ++j) {
        if(raw.charAt(j) == 0x20) { ++length; }
    }
    LocalMemory<int32_t> newReorderCodes((int32_t *)uprv_malloc(length * 4));
    if(newReorderCodes.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    int32_t codeIndex = 0;
    CharString word;
    while(i < raw.length()) {
        ++i;  // skip the word-separating space
        int32_t limit = raw.indexOf((UChar)0x20, i);
        if(limit < 0) { limit = raw.length(); }
        word.clear().appendInvariantChars(raw.tempSubStringBetween(i, limit), errorCode);
        if(U_FAILURE(errorCode)) { return; }
        int32_t code = getReorderCode(word.data());
        if(code < 0) {
            setParseError("unknown script or reorder code", errorCode);
            return;
        }
        newReorderCodes[codeIndex++] = code;
        i = limit;
    }
    U_ASSERT(codeIndex == length);
    if(length == 1 && newReorderCodes[0] == UCOL_REORDER_CODE_DEFAULT) {
        // The root collator does not have a reordering, by definition.
        uprv_free(tailoring->reorderCodes);
        settings->reorderCodes = tailoring->reorderCodes = NULL;
        settings->reorderCodesLength = 0;
        settings->reorderTable = NULL;
        return;
    }
    baseData->makeReorderTable(newReorderCodes.getAlias(), length,
                               tailoring->reorderTable, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    uprv_free(tailoring->reorderCodes);
    settings->reorderCodes = tailoring->reorderCodes = newReorderCodes.orphan();
    settings->reorderCodesLength = length;
    settings->reorderTable = tailoring->reorderTable;
}

static const char *const gSpecialReorderCodes[] = {
    "space", "punct", "symbol", "currency", "digit"
};

int32_t
CollationRuleParser::getReorderCode(const char *word) {
    for(int32_t i = 0; i < LENGTHOF(gSpecialReorderCodes); ++i) {
        if(uprv_stricmp(word, gSpecialReorderCodes[i]) == 0) {
            return UCOL_REORDER_CODE_FIRST + i;
        }
    }
    return u_getPropertyValueEnum(UCHAR_SCRIPT, word);
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
CollationRuleParser::parseUnicodeSet(const UnicodeString &raw, int32_t i, UnicodeSet &set,
                                     UErrorCode &errorCode) {
    // Collect a UnicodeSet pattern between a balanced pair of [brackets].
    int32_t level = 0;
    int32_t j = i;
    for(;;) {
        if(j == raw.length()) {
            setParseError("unbalanced UnicodeSet pattern brackets", errorCode);
            return j;
        }
        UChar c = raw.charAt(j++);
        if(c == 0x5b) {  // '['
            ++level;
        } else if(c == 0x5c) {  // ']'
            if(--level == 0) { break; }
        }
    }
    set.applyPattern(raw.tempSubStringBetween(i, j), errorCode);
    if(U_FAILURE(errorCode)) {
        errorCode = U_ZERO_ERROR;
        setParseError("not a valid UnicodeSet pattern", errorCode);
        return j;
    }
    j = skipWhiteSpace(j);
    if(j == raw.length() || raw.charAt(j) != 0x5d) {
        setParseError("missing option-terminating ']' after UnicodeSet pattern", errorCode);
        return j;
    }
    return ++j;
}

int32_t
CollationRuleParser::readWords(int32_t i, UnicodeString &raw) const {
    static const UChar sp = 0x20;
    raw.remove();
    i = skipWhiteSpace(i);
    for(;;) {
        if(i >= rules->length()) { return 0; }
        UChar c = rules->charAt(i);
        if(isSyntaxChar(c) && c != 0x2d && c != 0x5f) {  // syntax except -_
            if(raw.isEmpty()) { return i; }
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
CollationRuleParser::setParseError(const char *reason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    errorCode = U_PARSE_ERROR;
    errorReason = reason;
    if(parseError != NULL) { setErrorContext(); }
}

void
CollationRuleParser::setErrorContext() {
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
