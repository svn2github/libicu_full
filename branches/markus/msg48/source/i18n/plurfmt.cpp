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
#include "messageimpl.h"
#include "plurrule_impl.h"
#include "uassert.h"
#include "uhash.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

#define EQUALS_SIGN       ((UChar)0x003d)

static UChar OFFSET_CHARS[] = {0x006f, 0x0066, 0x0066, 0x0073, 0x0065, 0x0074};

static const UChar OTHER_STRING[] = {
    0x6F, 0x74, 0x68, 0x65, 0x72, 0  // "other"
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PluralFormat)

#define MAX_KEYWORD_SIZE 30

PluralFormat::PluralFormat(UErrorCode& status) : msgPattern(status) {
    init(NULL, Locale::getDefault(), status);
}

PluralFormat::PluralFormat(const Locale& loc, UErrorCode& status) : msgPattern(status) {
    init(NULL, loc, status);
}

PluralFormat::PluralFormat(const PluralRules& rules, UErrorCode& status) : msgPattern(status) {
    init(&rules, Locale::getDefault(), status);
}

PluralFormat::PluralFormat(const Locale& loc,
                           const PluralRules& rules,
                           UErrorCode& status) : msgPattern(status) {
    init(&rules, loc, status);
}

PluralFormat::PluralFormat(const UnicodeString& pat,
                           UErrorCode& status) : msgPattern(status) {
    init(NULL, Locale::getDefault(), status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const Locale& loc,
                           const UnicodeString& pat,
                           UErrorCode& status) : msgPattern(status) {
    init(NULL, loc, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const PluralRules& rules,
                           const UnicodeString& pat,
                           UErrorCode& status) : msgPattern(status) {
    init(&rules, Locale::getDefault(), status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const Locale& loc,
                           const PluralRules& rules,
                           const UnicodeString& pat,
                           UErrorCode& status) : msgPattern(status) {
    init(&rules, loc, status);
    applyPattern(pat, status);
}

PluralFormat::PluralFormat(const PluralFormat& other) : Format(other), msgPattern(other.msgPattern) {
    zeroAllocs();
    operator=(other);
}


PluralFormat::~PluralFormat() {
    destructHelper();
}

void
PluralFormat::destructHelper() {
    delete numberFormat;
    delete asciiNumberFormat;

    zeroAllocs();
}

void
PluralFormat::zeroAllocs() {
    numberFormat = NULL;
    asciiNumberFormat = NULL;
}

void
PluralFormat::init(const PluralRules* rules, const Locale& curLocale, UErrorCode& status) {
    zeroAllocs();

    if (U_FAILURE(status)) {
        return;
    }

    pattern.remove();
    msgPattern.clear();
    locale = curLocale;
    replacedNumberFormat = NULL;
    offset = 0;

    if (rules==NULL) {
        pluralRulesWrapper.pluralRules = PluralRules::forLocale(locale, status);
    } else {
        pluralRulesWrapper.pluralRules = copyPluralRules(rules, status);
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
    pattern = newPattern;
    msgPattern.parsePluralStyle(pattern, NULL, status);
    if (U_FAILURE(status)) {
        pattern.remove();
        msgPattern.clear();
        return;
    }
    offset = msgPattern.getPluralOffset(0);
}

UnicodeString&
PluralFormat::format(const Formattable& obj,
                   UnicodeString& appendTo,
                   FieldPosition& pos,
                   UErrorCode& status) const
{
    if (U_FAILURE(status)) return appendTo;

    int32_t number;

    if (obj.isNumeric()) {
        return format(obj.getDouble(), appendTo, pos, status);
    } else {
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
                     UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    if (msgPattern.countParts() == 0) {
        if ( replacedNumberFormat== NULL ) {
            return numberFormat->format(number, appendTo, pos);
        } else {
            return replacedNumberFormat->format(number, appendTo, pos);
        }
    }
    // Get the appropriate sub-message.
    int32_t partIndex = findSubMessage(msgPattern, 0, pluralRulesWrapper, number, status);
    // Replace syntactic # signs in the top level of this sub-message
    // (not in nested arguments) with the formatted number-offset.
    number -= offset;
    int32_t prevIndex = msgPattern.getPart(partIndex).getLimit();
    for (;;) {
        const MessagePattern::Part& part = msgPattern.getPart(++partIndex);
        const UMessagePatternPartType type = part.getType();
        int32_t index = part.getIndex();
        if (type == UMSGPAT_PART_TYPE_MSG_LIMIT) {
            return appendTo.append(pattern, prevIndex, index - prevIndex);
        } else if ((type == UMSGPAT_PART_TYPE_REPLACE_NUMBER) ||
            (type == UMSGPAT_PART_TYPE_SKIP_SYNTAX && MessageImpl::jdkAposMode(msgPattern))) {
            appendTo.append(pattern, prevIndex, index - prevIndex);
            if (type == UMSGPAT_PART_TYPE_REPLACE_NUMBER) {
                numberFormat->format(number, appendTo);
            }
            prevIndex = part.getLimit();
        } else if (type == UMSGPAT_PART_TYPE_ARG_START) {
            appendTo.append(pattern, prevIndex, index - prevIndex);
            prevIndex = index;
            partIndex = msgPattern.getLimitPartIndex(partIndex);
            index = msgPattern.getPart(partIndex).getLimit();
            MessageImpl::appendReducedApostrophes(pattern, prevIndex, index, appendTo);
            prevIndex = index;
        }
    }
}

UnicodeString&
PluralFormat::toPattern(UnicodeString& appendTo) {
    appendTo+= pattern;
    return appendTo;
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
        offset = other.offset;
        msgPattern = other.msgPattern;

        pluralRulesWrapper.pluralRules = copyPluralRules(other.pluralRulesWrapper.pluralRules, status);

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
    return ((*pluralRulesWrapper.pluralRules == *(fmt->pluralRulesWrapper.pluralRules)) && 
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

int32_t PluralFormat::findSubMessage(const MessagePattern& pattern, int32_t partIndex,
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
    UnicodeString other(FALSE, OTHER_STRING, 5);
    // When we find a match, we set msgStart>0 and also set this boolean to true
    // to avoid matching the keyword again (duplicates are allowed)
    // while we continue to look for an explicit-value match.
    UBool haveKeywordMatch=FALSE;
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
            if(pattern.partSubstringMatches(*part, other)) {
                if(msgStart==0) {
                    msgStart=partIndex;
                    if(0 == keyword.compare(other)) {
                        // This is the first "other" sub-message,
                        // and the selected keyword is also "other".
                        // Do not match "other" again.
                        haveKeywordMatch=TRUE;
                    }
                }
            } else {
                if(keyword.isEmpty()) {
                    keyword=selector.select(number-offset, ec);
                    if(msgStart!=0 && (0 == keyword.compare(other))) {
                        // We have already seen an "other" sub-message.
                        // Do not match "other" again.
                        haveKeywordMatch=TRUE;
                        continue;
                    }
                }
                if(pattern.partSubstringMatches(*part, keyword)) {
                    // keyword matches
                    msgStart=partIndex;
                    // Do not match this keyword again.
                    haveKeywordMatch=TRUE;
                }
            }
        }
        partIndex=pattern.getLimitPartIndex(partIndex);
    } while(++partIndex<count);
    return msgStart;
}

PluralFormat::PluralSelectorAdapter::~PluralSelectorAdapter() {
    delete pluralRules;
}

UnicodeString PluralFormat::PluralSelectorAdapter::select(double number,
                                                          UErrorCode& /*ec*/) const {
    return pluralRules->select(number);
}

void PluralFormat::PluralSelectorAdapter::reset() {
    delete pluralRules;
    pluralRules = NULL;
}


U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
