/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  messagepattern.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011mar14
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/messagepattern.h"
#include "unicode/unistr.h"
#include "cmemory.h"
#include "patternprops.h"

U_NAMESPACE_BEGIN

// MessagePatternList ------------------------------------------------------ ***

template<typename T, int32_t stackCapacity>
class MessagePatternList {
public:
    MessagePatternList() {}
    MessagePatternList(const MessagePatternList<T, stackCapacity> &other,
                       int32_t length,
                       UErrorCode &errorCode);
    UBool ensureCapacityForOneMore(int32_t oldLength, UErrorCode &errorCode);

    MaybeStackArray<T, stackCapacity> a;
};

template<typename T, int32_t stackCapacity>
MessagePatternList<T, stackCapacity>::MessagePatternList(
        const MessagePatternList<T, stackCapacity> &other,
        int32_t length,
        UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && length>0) {
        if(length>a.getCapacity() && NULL==a.resize(length)) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        uprv_memcpy(a.getAlias(), other.a.getAlias(), length*sizeof(T));
    }
}

template<typename T, int32_t stackCapacity>
UBool
MessagePatternList<T, stackCapacity>::ensureCapacityForOneMore(int32_t oldLength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
    if(a.getCapacity()>oldLength || a.resize(2*oldLength, oldLength)!=NULL) {
        return TRUE;
    }
    errorCode=U_MEMORY_ALLOCATION_ERROR;
    return FALSE;
}

// MessagePatternList specializations -------------------------------------- ***

class MessagePatternDoubleList : public MessagePatternList<double, 8> {
public:
    MessagePatternDoubleList() {}
    MessagePatternDoubleList(const MessagePatternDoubleList &other,
                             int32_t length, UErrorCode &errorCode)
        : MessagePatternList<double, 8>(other, length, errorCode) {}
};

class MessagePatternPartsList : public MessagePatternList<MessagePattern::Part, 32> {
public:
    MessagePatternPartsList() {}
    MessagePatternPartsList(const MessagePatternPartsList &other,
                            int32_t length, UErrorCode &errorCode)
        : MessagePatternList<MessagePattern::Part, 32>(other, length, errorCode) {}

    UBool memEquals(const MessagePatternPartsList &other, int32_t length) const {
        return 0==uprv_memcmp(a.getAlias(), other.a.getAlias(),
                              length*sizeof(MessagePattern::Part));
    }
};

// MessagePattern constructors etc. ---------------------------------------- ***

MessagePattern::MessagePattern(UErrorCode &errorCode)
        : aposMode(UCONFIG_MSGPAT_DEFAULT_APOSTROPHE_MODE),
          partsList(NULL), parts(NULL), partsLength(0),
          numericValuesList(NULL), numericValues(NULL), numericValuesLength(0),
          hasArgNames(FALSE), hasArgNumbers(FALSE), needsAutoQuoting(FALSE) {
    init(errorCode);
}

MessagePattern::MessagePattern(UMessagePatternApostropheMode mode, UErrorCode &errorCode)
        : aposMode(mode),
          partsList(NULL), parts(NULL), partsLength(0),
          numericValuesList(NULL), numericValues(NULL), numericValuesLength(0),
          hasArgNames(FALSE), hasArgNumbers(FALSE), needsAutoQuoting(FALSE) {
    init(errorCode);
}

MessagePattern::MessagePattern(const UnicodeString &pattern, UParseError *parseError, UErrorCode &errorCode)
        : aposMode(UCONFIG_MSGPAT_DEFAULT_APOSTROPHE_MODE),
          partsList(NULL), parts(NULL), partsLength(0),
          numericValuesList(NULL), numericValues(NULL), numericValuesLength(0),
          hasArgNames(FALSE), hasArgNumbers(FALSE), needsAutoQuoting(FALSE) {
    if(init(errorCode)) {
        parse(pattern, parseError, errorCode);
    }
}

UBool
MessagePattern::init(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
    partsList=new MessagePatternPartsList();
    if(partsList==NULL) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    parts=partsList->a.getAlias();
    return TRUE;
}

MessagePattern::MessagePattern(const MessagePattern &other)
        : aposMode(other.aposMode), msg(other.msg),
          partsList(NULL), parts(NULL), partsLength(0),
          numericValuesList(NULL), numericValues(NULL), numericValuesLength(0),
          hasArgNames(other.hasArgNames), hasArgNumbers(other.hasArgNumbers),
          needsAutoQuoting(other.needsAutoQuoting) {
    UErrorCode errorCode=U_ZERO_ERROR;
    copyStorage(other, errorCode);
}

MessagePattern &
MessagePattern::operator=(const MessagePattern &other) {
    aposMode=other.aposMode;
    msg=other.msg;
    delete partsList;
    partsList=NULL;
    parts=NULL;
    partsLength=0;
    delete numericValuesList;
    numericValuesList=NULL;
    numericValues=NULL;
    numericValuesLength=0;
    hasArgNames=other.hasArgNames;
    hasArgNumbers=other.hasArgNumbers;
    needsAutoQuoting=other.needsAutoQuoting;
    UErrorCode errorCode=U_ZERO_ERROR;
    copyStorage(other, errorCode);
    return *this;
}

void
MessagePattern::copyStorage(const MessagePattern &other, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }
    if(other.partsLength>0) {
        partsList=new MessagePatternPartsList(*other.partsList, other.partsLength, errorCode);
        if(partsList==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if(U_FAILURE(errorCode)) {
            delete partsList;
            partsList=NULL;
            return;
        }
        parts=partsList->a.getAlias();
    }
    if(other.numericValuesLength>0) {
        numericValuesList=new MessagePatternDoubleList(
            *other.numericValuesList, other.numericValuesLength, errorCode);
        if(numericValuesList==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if(U_FAILURE(errorCode)) {
            delete numericValuesList;
            numericValuesList=NULL;
            return;
        }
        numericValues=numericValuesList->a.getAlias();
    }
}

MessagePattern::~MessagePattern() {
    delete partsList;
    delete numericValuesList;
}

// MessagePattern API ------------------------------------------------------ ***

MessagePattern &
MessagePattern::parse(const UnicodeString &pattern, UParseError *parseError, UErrorCode &errorCode) {
    preParse(pattern, parseError, errorCode);
    parseMessage(0, 0, 0, UMSGPAT_ARG_TYPE_NONE, parseError, errorCode);
    postParse();
    return *this;
}

MessagePattern &
MessagePattern::parseChoiceStyle(const UnicodeString &pattern,
                                 UParseError *parseError, UErrorCode &errorCode) {
    preParse(pattern, parseError, errorCode);
    parseChoiceStyle(0, 0, parseError, errorCode);
    postParse();
    return *this;
}

MessagePattern &
MessagePattern::parsePluralStyle(const UnicodeString &pattern,
                                 UParseError *parseError, UErrorCode &errorCode) {
    preParse(pattern, parseError, errorCode);
    parsePluralOrSelectStyle(UMSGPAT_ARG_TYPE_PLURAL, 0, 0, parseError, errorCode);
    postParse();
    return *this;
}

MessagePattern &
MessagePattern::parseSelectStyle(const UnicodeString &pattern,
                                 UParseError *parseError, UErrorCode &errorCode) {
    preParse(pattern, parseError, errorCode);
    parsePluralOrSelectStyle(UMSGPAT_ARG_TYPE_SELECT, 0, 0, parseError, errorCode);
    postParse();
    return *this;
}

void
MessagePattern::clear() {
    // Mostly the same as preParse().
    msg.remove();
    hasArgNames=hasArgNumbers=FALSE;
    needsAutoQuoting=FALSE;
    partsLength=0;
    numericValuesLength=0;
}

UBool
MessagePattern::operator==(const MessagePattern &other) const {
    if(this==&other) {
        return TRUE;
    }
    return
        aposMode==other.aposMode &&
        msg==other.msg &&
        // parts.equals(o.parts)
        partsLength==other.partsLength &&
        (partsLength==0 || partsList->memEquals(*other.partsList, partsLength));
    // No need to compare numericValues if msg and parts are the same.
}

int32_t
MessagePattern::hashCode() const {
    int32_t hash=(aposMode*37+msg.hashCode())*37+partsLength;
    for(int32_t i=0; i<partsLength; ++i) {
        hash=hash*37+parts[i].hashCode();
    }
    return hash;
}

int32_t
MessagePattern::validateArgumentName(const UnicodeString &name) {
    if(!PatternProps::isIdentifier(name.getBuffer(), name.length())) {
        return UMSGPAT_ARG_NAME_NOT_VALID;
    }
    return parseArgNumber(name, 0, name.length());
}

UnicodeString
MessagePattern::autoQuoteApostropheDeep() const {
    if(!needsAutoQuoting) {
        return msg;
    }
    UnicodeString modified(msg);
    // Iterate backward so that the insertion indexes do not change.
    int32_t count=countParts();
    for(int32_t i=count; i>0;) {
        const Part &part=getPart(--i);
        if(part.getType()==UMSGPAT_PART_TYPE_INSERT_CHAR) {
            modified.insert(part.index, part.value);
        }
    }
    return modified;
}

double
MessagePattern::getNumericValue(const Part &part) const {
    UMessagePatternPartType type=part.type;
    if(type==UMSGPAT_PART_TYPE_ARG_INT) {
        return part.value;
    } else if(type==UMSGPAT_PART_TYPE_ARG_DOUBLE) {
        return numericValues[part.value];
    } else {
        return UMSGPAT_NO_NUMERIC_VALUE;
    }
}

/**
  * Returns the "offset:" value of a PluralFormat argument, or 0 if none is specified.
  * @param pluralStart the index of the first PluralFormat argument style part. (0..countParts()-1)
  * @return the "offset:" value.
  * @draft ICU 4.8
  */
double
MessagePattern::getPluralOffset(int32_t pluralStart) const {
    const Part &part=getPart(pluralStart);
    if(Part::hasNumericValue(part.type)) {
        return getNumericValue(part);
    } else {
        return 0;
    }
}

// MessagePattern::Part ---------------------------------------------------- ***

UBool
MessagePattern::Part::operator==(const Part &other) const {
    if(this==&other) {
        return TRUE;
    }
    return
        type==other.type &&
        index==other.index &&
        length==other.length &&
        value==other.value &&
        limitPartIndex==other.limitPartIndex;
}

// MessagePattern parser --------------------------------------------------- ***

void
MessagePattern::preParse(const UnicodeString &pattern, UParseError *parseError, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }
    if(parseError!=NULL) {
        parseError->line=0;
        parseError->offset=0;
        parseError->preContext[0]=0;
        parseError->postContext[0]=0;
    }
    msg=pattern;
    hasArgNames=hasArgNumbers=FALSE;
    needsAutoQuoting=FALSE;
    partsLength=0;
    numericValuesLength=0;
}

void
MessagePattern::postParse() {
    if(partsList!=NULL) {
        parts=partsList->a.getAlias();
    }
}

int32_t
MessagePattern::skipWhiteSpace(int32_t index) {
    const UChar *s=msg.getBuffer();
    int32_t msgLength=msg.length();
    const UChar *t=PatternProps::skipWhiteSpace(s+index, msgLength-index);
    return (int32_t)(t-s);
}

int32_t
MessagePattern::skipIdentifier(int32_t index) {
    const UChar *s=msg.getBuffer();
    int32_t msgLength=msg.length();
    const UChar *t=PatternProps::skipIdentifier(s+index, msgLength-index);
    return (int32_t)(t-s);
}

void
MessagePattern::setParseError(UParseError *parseError, int32_t index) {
    if(parseError==NULL) {
        return;
    }
    parseError->offset=index;

    // Set preContext to some of msg before index.
    // Avoid splitting a surrogate pair.
    int32_t length=index;
    if(length>=U_PARSE_CONTEXT_LEN) {
        length=U_PARSE_CONTEXT_LEN-1;
        if(length>0 && U16_IS_TRAIL(msg[index-length])) {
            --length;
        }
    }
    msg.extract(index-length, length, parseError->preContext);
    parseError->preContext[length]=0;

    // Set postContext to some of msg starting at index.
    length=msg.length()-index;
    if(length>=U_PARSE_CONTEXT_LEN) {
        length=U_PARSE_CONTEXT_LEN-1;
        if(length>0 && U16_IS_LEAD(msg[index+length-1])) {
            --length;
        }
    }
    msg.extract(index, length, parseError->postContext);
    parseError->postContext[length]=0;
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(MessagePattern)

U_NAMESPACE_END
