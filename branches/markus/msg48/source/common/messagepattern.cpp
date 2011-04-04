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
#include "messageimpl.h"
#include "patternprops.h"

U_NAMESPACE_BEGIN

// Unicode character/code point constants ---------------------------------- ***

enum {
    u_apos=0x27,
    u_plus=0x2B,
    u_minus=0x2D,
    u_dot=0x2E,
    u_A=0x41,
    u_C=0x43,
    u_E=0x45,
    u_H=0x48,
    u_I=0x49,
    u_L=0x4C,
    u_O=0x4F,
    u_P=0x50,
    u_R=0x52,
    u_S=0x53,
    u_T=0x54,
    u_U=0x55,
    u_Z=0x5A,
    u_a=0x61,
    u_c=0x63,
    u_e=0x65,
    u_h=0x68,
    u_i=0x69,
    u_l=0x6C,
    u_o=0x6F,
    u_p=0x70,
    u_r=0x72,
    u_s=0x73,
    u_t=0x74,
    u_u=0x75,
    u_z=0x7A
};

// MessagePatternList ------------------------------------------------------ ***

template<typename T, int32_t stackCapacity>
class MessagePatternList {
public:
    MessagePatternList() {}
    void copyFrom(const MessagePatternList<T, stackCapacity> &other,
                  int32_t length,
                  UErrorCode &errorCode);
    UBool ensureCapacityForOneMore(int32_t oldLength, UErrorCode &errorCode);
    UBool memEquals(const MessagePatternList<T, stackCapacity> &other, int32_t length) const {
        return 0==uprv_memcmp(a.getAlias(), other.a.getAlias(), length*sizeof(T));
    }

    MaybeStackArray<T, stackCapacity> a;
};

template<typename T, int32_t stackCapacity>
void
MessagePatternList<T, stackCapacity>::copyFrom(
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
};

class MessagePatternPartsList : public MessagePatternList<MessagePattern::Part, 32> {
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
    parts=NULL;
    partsLength=0;
    numericValues=NULL;
    numericValuesLength=0;
    if(partsList==NULL) {
        partsList=new MessagePatternPartsList();
        if(partsList==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        parts=partsList->a.getAlias();
    }
    if(other.partsLength>0) {
        partsList->copyFrom(*other.partsList, other.partsLength, errorCode);
        if(U_FAILURE(errorCode)) {
            return;
        }
        parts=partsList->a.getAlias();
        partsLength=other.partsLength;
    }
    if(other.numericValuesLength>0) {
        if(numericValuesList==NULL) {
            numericValuesList=new MessagePatternDoubleList();
            if(numericValuesList==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            numericValues=numericValuesList->a.getAlias();
        }
        numericValuesList->copyFrom(
            *other.numericValuesList, other.numericValuesLength, errorCode);
        if(U_FAILURE(errorCode)) {
            return;
        }
        numericValues=numericValuesList->a.getAlias();
        numericValuesLength=other.numericValuesLength;
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
    if(numericValuesList!=NULL) {
        numericValues=numericValuesList->a.getAlias();
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

int32_t
MessagePattern::skipDouble(int32_t index) {
    int32_t msgLength=msg.length();
    while(index<msgLength) {
        UChar c=msg.charAt(index);
        // U+221E: Allow the infinity symbol, for ChoiceFormat patterns.
        if((c<0x30 && c!=u_plus && c!=u_minus && c!=u_dot) || (c>0x39 && c!=u_e && c!=u_E && c!=0x221e)) {
            break;
        }
        ++index;
    }
    return index;
}

UBool
MessagePattern::isArgTypeChar(UChar32 c) {
    return (u_a<=c && c<=u_z) || (u_A<=c && c<=u_Z);
}

UBool
MessagePattern::isChoice(int32_t index) {
    UChar c;
    return
        ((c=msg.charAt(index++))==u_c || c==u_C) &&
        ((c=msg.charAt(index++))==u_h || c==u_H) &&
        ((c=msg.charAt(index++))==u_o || c==u_O) &&
        ((c=msg.charAt(index++))==u_i || c==u_I) &&
        ((c=msg.charAt(index++))==u_c || c==u_C) &&
        ((c=msg.charAt(index))==u_e || c==u_E);
}

UBool
MessagePattern::isPlural(int32_t index) {
    UChar c;
    return
        ((c=msg.charAt(index++))==u_p || c==u_P) &&
        ((c=msg.charAt(index++))==u_l || c==u_L) &&
        ((c=msg.charAt(index++))==u_u || c==u_U) &&
        ((c=msg.charAt(index++))==u_r || c==u_R) &&
        ((c=msg.charAt(index++))==u_a || c==u_A) &&
        ((c=msg.charAt(index))==u_l || c==u_L);
}

UBool
MessagePattern::isSelect(int32_t index) {
    UChar c;
    return
        ((c=msg.charAt(index++))==u_s || c==u_S) &&
        ((c=msg.charAt(index++))==u_e || c==u_E) &&
        ((c=msg.charAt(index++))==u_l || c==u_L) &&
        ((c=msg.charAt(index++))==u_e || c==u_E) &&
        ((c=msg.charAt(index++))==u_c || c==u_C) &&
        ((c=msg.charAt(index))==u_t || c==u_T);
}

UBool
MessagePattern::inMessageFormatPattern(int32_t nestingLevel) {
    return nestingLevel>0 || partsList->a[0].type==UMSGPAT_PART_TYPE_MSG_START;
}

UBool
MessagePattern::inTopLevelChoiceMessage(int32_t nestingLevel, UMessagePatternArgType parentType) {
    return
        nestingLevel==1 &&
        parentType==UMSGPAT_ARG_TYPE_CHOICE &&
        partsList->a[0].type!=UMSGPAT_PART_TYPE_MSG_START;
}

void
MessagePattern::addPart(UMessagePatternPartType type, int32_t index, int32_t length,
                        int32_t value, UErrorCode &errorCode) {
    if(partsList->ensureCapacityForOneMore(partsLength, errorCode)) {
        Part &part=partsList->a[partsLength++];
        part.type=type;
        part.index=index;
        part.length=(uint16_t)length;
        part.value=(int16_t)value;
        part.limitPartIndex=0;
    }
}

void
MessagePattern::addLimitPart(int32_t start,
                             UMessagePatternPartType type, int32_t index, int32_t length,
                             int32_t value, UErrorCode &errorCode) {
    partsList->a[start].limitPartIndex=partsLength;
    addPart(type, index, length, value, errorCode);
}

void
MessagePattern::addArgDoublePart(double numericValue, int32_t start, int32_t length,
                                 UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }
    int32_t numericIndex=numericValuesLength;
    if(numericValuesList==NULL) {
        numericValuesList=new MessagePatternDoubleList();
        if(numericValuesList==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    } else if(!numericValuesList->ensureCapacityForOneMore(numericValuesLength, errorCode)) {
        return;
    } else {
        if(numericIndex>Part::MAX_VALUE) {
            errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
            return;
        }
    }
    numericValuesList->a[numericValuesLength++]=numericValue;
    addPart(UMSGPAT_PART_TYPE_ARG_DOUBLE, start, length, numericIndex, errorCode);
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

// MessageImpl ------------------------------------------------------------- ***

void
MessageImpl::appendReducedApostrophes(const UnicodeString &s, int32_t start, int32_t limit,
                                      UnicodeString &sb) {
    int32_t doubleApos=-1;
    for(;;) {
        int32_t i=s.indexOf((UChar)u_apos, start);
        if(i<0 || i>=limit) {
            sb.append(s, start, limit-start);
            break;
        }
        if(i==doubleApos) {
            // Double apostrophe at start-1 and start==i, append one.
            sb.append((UChar)u_apos);
            ++start;
            doubleApos=-1;
        } else {
            // Append text between apostrophes and skip this one.
            sb.append(s, start, i-start);
            doubleApos=start=i+1;
        }
    }
}

U_NAMESPACE_END
