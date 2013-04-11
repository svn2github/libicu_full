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

#include "unicode/parseerr.h"
#include "unicode/unistr.h"
#include "collation.h"
#include "collationruleparser.h"
#include "collationsettings.h"
#include "patternprops.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

CollationRuleParser::CollationRuleParser(UErrorCode &errorCode)
        : rules(NULL), settings(NULL),
          parseError(NULL), errorReason(NULL),
          ruleIndex(0),
          tokens(errorCode), tokenIndex(0) {
    if(U_FAILURE(errorCode)) { return; }
    tokens.addElement(0, errorCode);  // sentinel
}

CollationRuleParser::~CollationRuleParser() {
}

void
CollationRuleParser::parse(const UnicodeString &ruleString,
                           CollationSettings &outSettings,
                           UParseError *outParseError,
                           UErrorCode &errorCode) {
    settings = &outSettings;
    parseError = outParseError;
    errorReason = NULL;
    tokens.removeAllElements();
    tokens.addElement(0, errorCode);  // sentinel
    parse(ruleString, errorCode);
}

void
CollationRuleParser::parse(const UnicodeString &ruleString, UErrorCode &errorCode) {
    rules = &ruleString;
    ruleIndex = 0;

    while(ruleIndex < rules->length() && U_SUCCESS(errorCode)) {
        UChar c = rules->charAt(ruleIndex);
        if(PatternProps::isWhiteSpace(c)) { continue; }
        switch(c) {
        case 0x26:  // '&'
            parseRuleChain(errorCode);
            break;
        case 0x5b:  // '['
            parseSetting(errorCode);
            break;
        case 0x23:  // '#' starts a comment, until the end of the line
            skipComment();
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
            setError("expected a reset or setting or comment");
            errorCode = U_PARSE_ERROR;
            break;
        }
    }
}

void
CollationRuleParser::parseRuleChain(UErrorCode &errorCode) {
    // TODO
}

void
CollationRuleParser::parseSetting(UErrorCode &errorCode) {
    // TODO
}

void
CollationRuleParser::skipComment() {
    int32_t i;
    for(i = ruleIndex + 1; i < rules->length(); ++i) {
        UChar c = rules->charAt(i);
        // LF or FF or CR or NEL or LS or PS
        if(c == 0xa || c == 0xc || c == 0xd || c == 0x85 || c == 0x2028 || c == 0x2029) {
            // Unicode Newline Guidelines: "A readline function should stop at NLF, LS, FF, or PS."
            // NLF (new line function) = CR or LF or CR+LF or NEL.
            // No need to collect all of CR+LF because a following LF will be ignored anyway.
            ++i;
            break;
        }
    }
    ruleIndex = i;
}

void
CollationRuleParser::setError(const char *reason) {
    errorReason = reason;
    if(parseError != NULL) {
        // TODO
    }
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
