/*
*******************************************************************************
* Copyright (C) 2009-2011, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File PLURFMT.CPP
*
* Modification History:
*
*   Date        Name        Description
*******************************************************************************
*/

#include "unicode/messagepattern.h"
#include "unicode/plurfmt.h"
#include "unicode/plurrule.h"
#include "unicode/utypes.h"
#include "cmemory.h"
#include "plurrule_impl.h"
#include "uassert.h"
#include "uhash.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

#define EQUALS_SIGN       ((UChar)0x003d)

static UChar OFFSET_CHARS[] = {0x006f, 0x0066, 0x0066, 0x0073, 0x0065, 0x0074};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PluralFormat)

#define MAX_KEYWORD_SIZE 30

PluralFormat::PluralFormat(UErrorCode& status) {
    init(NULL, Locale::getDefault(), status);
}

PluralFormat::PluralFormat(const Locale& loc, UErrorCode& status) {
    init(NULL, loc, status);
}

PluralFormat::PluralFormat(const PluralRules& rules, UErrorCode& status) {
    init(&rules, Locale::getDefault(), status);
}

PluralFormat::PluralFormat(const Locale& loc, const PluralRules& rules, UErrorCode& status) {
    init(&rules, loc, status);
}

PluralFormat::PluralFormat(const UnicodeString& pat, UErrorCode& status) {
    init(NULL, Locale::getDefault(), status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const Locale& loc, const UnicodeString& pat, UErrorCode& status) {
    init(NULL, loc, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const PluralRules& rules, const UnicodeString& pat, UErrorCode& status) {
    init(&rules, Locale::getDefault(), status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const Locale& loc, const PluralRules& rules, const UnicodeString& pat, UErrorCode& status) {
    init(&rules, loc, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const PluralFormat& other) : Format(other) {
    zeroAllocs();
    operator=(other);
}


PluralFormat::~PluralFormat() {
    destructHelper();
}

void
PluralFormat::destructHelper() {
    delete pluralRules;   
    delete parsedValues;
    delete numberFormat;
    delete asciiNumberFormat;
    delete [] explicitValues;

    zeroAllocs();
}

void
PluralFormat::zeroAllocs() {
    pluralRules = NULL;
    parsedValues = NULL;
    numberFormat = NULL;
    asciiNumberFormat = NULL;
    explicitValues = NULL;
}

void
PluralFormat::init(const PluralRules* rules, const Locale& curLocale, UErrorCode& status) {
    zeroAllocs();

    if (U_FAILURE(status)) {
        return;
    }

    pattern.remove();
    locale = curLocale;
    replacedNumberFormat = NULL;
    explicitValuesLen = 0;
    offset = 0;

    if ( rules==NULL) {
        pluralRules = PluralRules::forLocale(locale, status);
    } else {
        pluralRules = copyPluralRules(rules, status);
    }
    numberFormat= NumberFormat::createInstance(curLocale, status);

    if (U_FAILURE(status)) {
        destructHelper();
    }
}

void
PluralFormat::applyPattern(const UnicodeString& newPattern, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    this->pattern = newPattern;
    
    if (parsedValues != NULL) {
        parsedValues->removeAll();
    } else {
        parsedValues = new Hashtable(TRUE, status);
        if (U_FAILURE(status)) {
            return;
        }
        parsedValues->setValueDeleter(uhash_deleteUnicodeString);
    }
    explicitValuesLen = 0;
    
    int32_t state = 0;
    int32_t braceCount = 0;
    UnicodeString token;
    UnicodeString currentKeyword;
    double explicitValue = 0;
    bool useExplicit = FALSE;
    bool readSpaceAfterKeyword = FALSE;
    
    for (int32_t i=0; i<pattern.length(); ++i) {
        UChar ch=pattern.charAt(i);
        switch (state) {
        case 0: // Reading keyword.
            if (ch == SPACE) {
                // Skip leading and trailing whitespace.
                if (token.length() > 0) {
                    readSpaceAfterKeyword = TRUE;
            }
                break;
        }
            if (ch == EQUALS_SIGN) {
                if (useExplicit) {
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                useExplicit = TRUE;
                break;
                }
            if (!readSpaceAfterKeyword && ch == COLON) { // Check for offset.
                if (token.length() != 6 ||
                    token.toLower(Locale::getUS()).compare(0, 6, OFFSET_CHARS) != 0) {
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                token.truncate(0);
                state = 2;
                break;
                    }
            if (ch == LEFTBRACE) { // End of keyword definition reached.
                    if (token.length()==0) {
                    status = U_UNEXPECTED_TOKEN;
                        return;
                    }

                currentKeyword = token.toLower(Locale::getUS());
                if (useExplicit) {
                    Formattable result;
                    NumberFormat* fmt = getAsciiNumberFormat(status);
                    if (fmt == NULL) {
                        return;
                    }
                    fmt->parse(currentKeyword, result, status);
                    explicitValue = result.getDouble(status);

                    if (explicitValues != NULL) {
                        for (int32_t i = 0; i < explicitValuesLen; ++i) {
                            if (explicitValues[i].key == explicitValue) {
                                status = U_DUPLICATE_KEYWORD;
                                return;
                }
                        }
                    }
                } else {
                    if (!pluralRules->isKeyword(currentKeyword)) {
                        status = U_UNDEFINED_KEYWORD;
                        return;
                    }

                    if (parsedValues->get(currentKeyword)!= NULL) {
                        status = U_DUPLICATE_KEYWORD;
                        return;
                    }
                }

                token.truncate(0);
                braceCount++;
                state = 1;
                break;
            }
            if (readSpaceAfterKeyword) {
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
            token.append(ch);
            break;

        case 1: // Reading value
            if (ch == SINGLE_QUOTE) {
                // Apostrophe starts and ends quoting of literal text.
                // Skip the quoted text and preserve the apostrophes for
                // subsequent use in MessageFormat.
                int32_t endAposIndex = pattern.indexOf(SINGLE_QUOTE, i + 1);
                if (endAposIndex < 0) {
                    // parsingFailure("Malformed formatting expression. Braces do not match.");
                    status = U_PATTERN_SYNTAX_ERROR;
                    return;
                }
                token.append(pattern, i, endAposIndex + 1 - i);
                i = endAposIndex;
                break;
            } else if (ch == RIGHTBRACE) {
                if (--braceCount == 0) {
                    if (useExplicit) {
                        if ((explicitValuesLen & 0x3) == 0) {
                            ExplicitPair* nv = new ExplicitPair[explicitValuesLen + 4];
                            if (nv == NULL) {
                                status = U_MEMORY_ALLOCATION_ERROR;
                                return;
                    }
                            if (explicitValues != NULL) {
                                for (int32_t i = 0; i < explicitValuesLen; ++i) {
                                    nv[i] = explicitValues[i];
                    }
                                delete[] explicitValues;
                }
                            explicitValues = nv;
                        }
                        explicitValues[explicitValuesLen].key = explicitValue;
                        explicitValues[explicitValuesLen].value = token;
                        explicitValuesLen++;
                        useExplicit = false;
                    } else {
                        UnicodeString *value = new UnicodeString(token);
                        if (value == NULL) {
                            status = U_MEMORY_ALLOCATION_ERROR;
                    return;
                }
                        parsedValues->put(currentKeyword, value, status);
                        if (U_FAILURE(status)) {
                            delete value;
                            return;
                        }
                    }
                    token.truncate(0);
                    readSpaceAfterKeyword = FALSE;
                    state = 0;
                break;
        }

                // else braceCount > 0, we'll append the brace
            } else if (ch == LEFTBRACE) {
                braceCount++;
            }
            token.append(ch);
            break;

        case 2: // Reading value of offset
            if (ch == SPACE) {
                if (token.length() == 0) {
                    break; // ignore leading spaces
    }
                // trigger parse on first whitespace after non-whitespace
                Formattable result;
                NumberFormat* fmt = getAsciiNumberFormat(status);
                if (fmt == NULL) {
        return;
    }
                fmt->parse(token, result, status);
                offset = result.getDouble(status);
                if (U_FAILURE(status)) {
        return;
    }
                token.truncate(0);
                readSpaceAfterKeyword = FALSE;
                state = 0; // back to looking for keywords
                break;
            }
            // Not whitespace, assume it's part of the offset value.
            // If it's '=', too bad, you have to explicitly
            // delimit offset:xxx by a space.  This also means it
            // can't be '{', but this is an error already since there's no
            // keyword.
            token.append(ch);
            break;
        } // switch state
    } // for loop

    if (braceCount != 0) {
        status = U_UNMATCHED_BRACES;
    } else if (!checkSufficientDefinition()) {
        status = U_DEFAULT_KEYWORD_MISSING;
    }
}

UnicodeString&
PluralFormat::format(const Formattable& obj,
                   UnicodeString& appendTo,
                   FieldPosition& pos,
                   UErrorCode& status) const
{
    if (U_FAILURE(status)) return appendTo;
    int32_t number;
    
    switch (obj.getType())
    {
    case Formattable::kDouble:
        return format((int32_t)obj.getDouble(), appendTo, pos, status);
        break;
    case Formattable::kLong:
        number = (int32_t)obj.getLong();
        return format(number, appendTo, pos, status);
        break;
    case Formattable::kInt64:
        return format((int32_t)obj.getInt64(), appendTo, pos, status);
    default:
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }
}

UnicodeString
PluralFormat::format(int32_t number, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return UnicodeString();
    }
    FieldPosition fpos(0);
    UnicodeString result;
    
    return format(number, result, fpos, status);
}

UnicodeString
PluralFormat::format(double number, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return UnicodeString();
    }
    FieldPosition fpos(0);
    UnicodeString result;
    
    return format(number, result, fpos, status);
}


UnicodeString&
PluralFormat::format(int32_t number,
                     UnicodeString& appendTo, 
                     FieldPosition& pos,
                     UErrorCode& status) const {
    return format((double)number, appendTo, pos, status);
}

UnicodeString&
PluralFormat::format(double number,
                     UnicodeString& appendTo, 
                     FieldPosition& pos,
                     UErrorCode& /* status */) const {

    if (parsedValues == NULL) {
        if ( replacedNumberFormat== NULL ) {
            return numberFormat->format(number, appendTo, pos);
        } else {
            return replacedNumberFormat->format(number, appendTo, pos);
        }
    }

    UnicodeString *selectedPattern = NULL;
    if (explicitValuesLen > 0) {
        for (int32_t i = 0; i < explicitValuesLen; ++i) {
            if (number == explicitValues[i].key) {
                selectedPattern = &explicitValues[i].value;
                break;
            }
        }
    }

    // Apply offset only after explicit test.
    number -= offset;

    if (selectedPattern==NULL) {
        UnicodeString selectedRule = pluralRules->select(number);
        selectedPattern = (UnicodeString *)parsedValues->get(selectedRule);
        if (selectedPattern==NULL) {
            selectedPattern = (UnicodeString *)parsedValues->get(pluralRules->getKeywordOther());
        }
    }
    return insertFormattedNumber(number, *selectedPattern, appendTo, pos);
}

UnicodeString&
PluralFormat::toPattern(UnicodeString& appendTo) {
    appendTo+= pattern;
    return appendTo;
}

UBool
PluralFormat::checkSufficientDefinition() {
    // Check that at least the default rule is defined.
    return parsedValues != NULL &&
        parsedValues->get(pluralRules->getKeywordOther()) != NULL;
}

void
PluralFormat::setLocale(const Locale& loc, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
        replacedNumberFormat=NULL;
    destructHelper();
    init(NULL, loc, status);
}

void
PluralFormat::setNumberFormat(const NumberFormat* format, UErrorCode& /*status*/) {
    // TODO: The copy constructor and assignment op of NumberFormat class are protected.
    // create a pointer as the workaround.
    replacedNumberFormat = (NumberFormat *)format;
}

Format*
PluralFormat::clone() const
{
    return new PluralFormat(*this);
}


PluralFormat&
PluralFormat::operator=(const PluralFormat& other) {
    if (this != &other) {
        UErrorCode status = U_ZERO_ERROR;
        destructHelper();

        locale = other.locale;
        pattern = other.pattern;
        replacedNumberFormat = other.replacedNumberFormat;
        explicitValuesLen = other.explicitValuesLen;
        offset = other.offset;

        parsedValues = copyHashtable(other.parsedValues, status);
        pluralRules = copyPluralRules(other.pluralRules, status);
        explicitValues = copyExplicitValues(other.explicitValues, other.explicitValuesLen, status);

        numberFormat = NumberFormat::createInstance(locale, status); // can't clone?

        if (U_FAILURE(status)) {
            destructHelper();
        }
    }

    return *this;
}

UBool
PluralFormat::operator==(const Format& other) const {
    // This protected comparison operator should only be called by subclasses
    // which have confirmed that the other object being compared against is
    // an instance of a sublcass of PluralFormat.  THIS IS IMPORTANT.
    // Format::operator== guarantees that this cast is safe
    PluralFormat* fmt = (PluralFormat*)&other;
    return ((*pluralRules == *(fmt->pluralRules)) && 
            (*numberFormat == *(fmt->numberFormat)));
}

UBool
PluralFormat::operator!=(const Format& other) const {
    return  !operator==(other);
}

void
PluralFormat::parseObject(const UnicodeString& /*source*/,
                        Formattable& /*result*/,
                        ParsePosition& /*pos*/) const
{
    // TODO: not yet supported in icu4j and icu4c
}

UnicodeString&
PluralFormat::insertFormattedNumber(double number,
                                    UnicodeString& message,
                                    UnicodeString& appendTo,
                                    FieldPosition& pos) const {
    int32_t braceStack=0;
    int32_t startIndex=0;

    if (message.length()==0) {
        return appendTo;
    }
    UnicodeString formattedNumber;
    numberFormat->format(number, formattedNumber, pos);
    for(int32_t i=0; i<message.length(); ++i) {
        switch(message.charAt(i)) {
        case SINGLE_QUOTE:
            // Skip/copy quoted literal text.
            i = message.indexOf(SINGLE_QUOTE, i + 1);
            // Unterminated quote should have failed applyPattern().
            U_ASSERT(i >= 0);
            break;
        case LEFTBRACE:
            ++braceStack;
            break;
        case RIGHTBRACE:
            --braceStack;
            break;
        case NUMBER_SIGN:
            if (braceStack==0) {
                appendTo.append(message, startIndex, i - startIndex);
                appendTo += formattedNumber;
                startIndex = i + 1;
            }
            break;
        }
    }
    if ( startIndex < message.length() ) {
        appendTo.append(message, startIndex, message.length()-startIndex);
    }
    return appendTo;
}

NumberFormat *
PluralFormat::getAsciiNumberFormat(UErrorCode& status) {
    if (asciiNumberFormat == NULL) {
        asciiNumberFormat = NumberFormat::createInstance(Locale::getUS(), status);
    }
    return asciiNumberFormat;
}

Hashtable*
PluralFormat::copyHashtable(Hashtable *other, UErrorCode& status) {
    if (other == NULL || U_FAILURE(status)) {
        return NULL;
    }
    Hashtable *result = new Hashtable(TRUE, status);
    if(U_FAILURE(status)){
        return NULL;
    }
    result->setValueDeleter(uhash_deleteUnicodeString);
    int32_t pos = -1;
    const UHashElement* elem = NULL;
    // walk through the hash table and create a deep clone
    while((elem = other->nextElement(pos))!= NULL){
        const UHashTok otherKeyTok = elem->key;
        UnicodeString* otherKey = (UnicodeString*)otherKeyTok.pointer;
        const UHashTok otherKeyToVal = elem->value;
        UnicodeString* otherValue = (UnicodeString*)otherKeyToVal.pointer;
        result->put(*otherKey, new UnicodeString(*otherValue), status);
        if(U_FAILURE(status)){
            delete result;
            return NULL;
        }
    }
    return result;
}

PluralRules*
PluralFormat::copyPluralRules(const PluralRules *rules, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    PluralRules *copy = rules->clone();
    if (copy == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return copy;
}

PluralFormat::ExplicitPair *
PluralFormat::copyExplicitValues(PluralFormat::ExplicitPair *explicitValues, uint32_t len, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    ExplicitPair* nv = new ExplicitPair[(len + 3) & ~0x3];
    if (nv == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    if (explicitValues != NULL) {
        for (int32_t i = 0; i < explicitValuesLen; ++i) {
            nv[i] = explicitValues[i];
        }
    }
    return nv;
}


int32_t PluralFormat::findSubMessage(
    const MessagePattern& pattern, int32_t partIndex,
    const PluralSelector& selector, double number, UErrorCode& ec) {
    if (U_FAILURE(ec)) {
        return 0;
    }
    int32_t count=pattern.countParts();
    double offset;
    const MessagePattern::Part* part=&pattern.getPart(partIndex);
    if (MessagePattern::Part::hasNumericValue(part->getType())) {
        offset=pattern.getNumericValue(*part);
        ++partIndex;
    } else {
        offset=0;
    }
    // The keyword is empty until we need to match against non-explicit, not-"other" value.
    // Then we get the keyword from the selector.
    // (In other words, we never call the selector if we match against an explicit value,
    // or if the only non-explicit keyword is "other".)
    UnicodeString keyword;
    // When we find a match, we set msgStart>0 and also set this boolean to true
    // to avoid matching the keyword again (duplicates are allowed)
    // while we continue to look for an explicit-value match.
    bool haveKeywordMatch=false;
    // msgStart is 0 until we find any appropriate sub-message.
    // We remember the first "other" sub-message if we have not seen any
    // appropriate sub-message before.
    // We remember the first matching-keyword sub-message if we have not seen
    // one of those before.
    // (The parser allows [does not check for] duplicate keywords.
    // We just have to make sure to take the first one.)
    // We avoid matching the keyword twice by also setting haveKeywordMatch=true
    // at the first keyword match.
    // We keep going until we find an explicit-value match or reach the end of the plural style.
    int32_t msgStart=0;
    // Iterate over (ARG_SELECTOR [ARG_INT|ARG_DOUBLE] message) tuples
    // until ARG_LIMIT or end of plural-only pattern.
    do {
        part=&pattern.getPart(partIndex++);
        const UMessagePatternPartType type = part->getType();
        if(type==UMSGPAT_PART_TYPE_ARG_LIMIT) {
            break;
        }
        U_ASSERT (type==UMSGPAT_PART_TYPE_ARG_SELECTOR);
        // part is an ARG_SELECTOR followed by an optional explicit value, and then a message
        if(MessagePattern::Part::hasNumericValue(pattern.getPartType(partIndex))) {
            // explicit value like "=2"
            part=&pattern.getPart(partIndex++);
            if(number==pattern.getNumericValue(*part)) {
                // matches explicit value
                return partIndex;
            }
        } else if(!haveKeywordMatch) {
            // plural keyword like "few" or "other"
            // Compare "other" first and call the selector if this is not "other".
            if(pattern.partSubstringMatches(*part, UNICODE_STRING_SIMPLE("other"))) {
                if(msgStart==0) {
                    msgStart=partIndex;
                    if(keyword.isEmpty() && (0 == keyword.compare(UNICODE_STRING_SIMPLE("other")))) {
                        // This is the first "other" sub-message,
                        // and the selected keyword is also "other".
                        // Do not match "other" again.
                        haveKeywordMatch=true;
                    }
                }
            } else {
                if(keyword.isEmpty()) {
                    keyword=selector.select(number-offset, ec);
                    if(msgStart!=0 && (0 == keyword.compare(UNICODE_STRING_SIMPLE("other")))) {
                        // We have already seen an "other" sub-message.
                        // Do not match "other" again.
                        haveKeywordMatch=true;
                        continue;
                    }
                }
                if(pattern.partSubstringMatches(*part, keyword)) {
                    // keyword matches
                    msgStart=partIndex;
                    // Do not match this keyword again.
                    haveKeywordMatch=true;
                }
            }
        }
        partIndex=pattern.getLimitPartIndex(partIndex);
    } while(++partIndex<count);
    return msgStart;
}


U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
