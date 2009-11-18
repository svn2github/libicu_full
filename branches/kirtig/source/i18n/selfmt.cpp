/*
*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
* Copyright (C) 2009, Yahoo! Inc.
*******************************************************************************
*
* File SELFMT.CPP
*
* Modification History:
*
*   Date        Name        Description
*   11/11/09    kirtig      Finished first cut of implementation.
*******************************************************************************
*/


#include "unicode/utypes.h"
#include "unicode/selfmt.h"
#include "selfmtimpl.h"
#include "unicode/utypes.h"


#include "unicode/ustring.h"
#include "unicode/ucnv_err.h"
#include "unicode/uchar.h"
#include "unicode/umsg.h"
#include "unicode/rbnf.h"
#include "cmemory.h"
#include "util.h"
#include "uassert.h"
#include "ustrfmt.h"
#include "uvector.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

U_CDECL_BEGIN

static void U_CALLCONV
deleteHashStrings(void *obj) {
    delete (UnicodeString *)obj;
}

U_CDECL_END

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SelectFormat)

#define MAX_KEYWORD_SIZE 30
static const UChar SELECT_KEYWORD_OTHER[]={LOW_O,LOW_T,LOW_H,LOW_E,LOW_R,0};

SelectFormat::SelectFormat(UErrorCode& status) {
   if (U_FAILURE(status)) {
      return;
   } 
   init(status);
}

SelectFormat::SelectFormat(const UnicodeString& pat, UErrorCode& status) {
   if (U_FAILURE(status)) {
      return;
   } 
   init(status);
   applyPattern(pat, status);
}

SelectFormat::SelectFormat(const SelectFormat& other) : Format(other) {
   UErrorCode status = U_ZERO_ERROR;
   pattern = other.pattern;
   copyHashtable(other.parsedValuesHash, status);
}

SelectFormat::~SelectFormat() {
    delete parsedValuesHash;
}

void
SelectFormat::init(UErrorCode& status) {
    parsedValuesHash=NULL;
    pattern.remove();
    status = U_ZERO_ERROR;
}

void
SelectFormat::applyPattern(const UnicodeString& newPattern, UErrorCode& status) {
    if (U_FAILURE(status)) {
      return;
    } 
    this->parsedValuesHash=NULL;
    this->pattern = newPattern;
    UnicodeString token;
    UnicodeString temp = "";
    int32_t braceCount=0;
    fmtToken type;
    UBool spaceIncluded=FALSE;
    
    if (parsedValuesHash==NULL) {
        parsedValuesHash = new Hashtable(TRUE, status);
        if (U_FAILURE(status)) {
            return;
        }
        parsedValuesHash->setValueDeleter(deleteHashStrings);
    }
    
    UBool getKeyword=TRUE;
    UnicodeString hashKeyword;
    UnicodeString *hashPattern;
    
    for (int32_t i=0; i<pattern.length(); ++i) {
        UChar ch=pattern.charAt(i);

        if ( !inRange(ch, type) ) {
            if (getKeyword) {
                status = U_ILLEGAL_CHARACTER;
                return;
            }
            else {
                token += ch;
                continue;
            }
        }
        switch (type) {
            case tSpace:
                if (token.length()==0) {
                    continue;
                }
                if (getKeyword) {
                    // space after keyword
                    spaceIncluded = TRUE;
                }
                else {
                    token += ch;
                }
                break;
            case tLeftBrace:
                if ( getKeyword ) {
                    if (parsedValuesHash->get(token)!= NULL) {
                        status = U_DUPLICATE_KEYWORD;
                        return; 
                    }
                    if (token.length()==0) {
                        status = U_PATTERN_SYNTAX_ERROR;
                        return;
                    }
                    
                    hashKeyword = token;
                    getKeyword = FALSE;
                    token.remove();
                    token = UnicodeString();
                }
                else  {
                    if (braceCount==0) {
                        status = U_PATTERN_SYNTAX_ERROR;
                        return;
                    }
                    else {
                        token += ch;
                    }
                }
                braceCount++;
                spaceIncluded = FALSE;
                break;
            case tRightBrace:
                if ( getKeyword ) {
                    status = U_PATTERN_SYNTAX_ERROR;
                    return;
                }
                else  {
                    hashPattern = new UnicodeString(token);
                    parsedValuesHash->put(hashKeyword, hashPattern, status);
                    braceCount--;
                    if ( braceCount==0 ) {
                        getKeyword=TRUE;
                        hashKeyword.remove();
                        hashPattern=NULL;
                        token.remove();
                        token = UnicodeString(); 
                    }
                    else {
                        token += ch;
                    }
                }
                spaceIncluded = FALSE;
                break;
            case tLetter:
                if (spaceIncluded) {
                    status = U_PATTERN_SYNTAX_ERROR;
                    return;
                }
            default:
                token+=ch;
                break;
        }
    }
    if ( checkSufficientDefinition() ) {
        return;
    }
    else {
        status = U_DEFAULT_KEYWORD_MISSING;
        return;
    }
}

UnicodeString&
SelectFormat::format(const Formattable& obj,
                   UnicodeString& appendTo,
                   FieldPosition& pos,
                   UErrorCode& status) const
{
    if (U_FAILURE(status)) return appendTo;
    
    switch (obj.getType())
    {
    case Formattable::kString:
        return format((UnicodeString)obj.getString(), appendTo, pos, status);
    default:
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }
}

UnicodeString&
SelectFormat::format(UnicodeString sInput,
                     UnicodeString& appendTo, 
                     FieldPosition& pos,
                     UErrorCode& success) const {

    if (U_FAILURE(success)) return appendTo;
    if (parsedValuesHash==NULL) {
        success = U_INVALID_FORMAT_ERROR;
        return appendTo;
    }

    UnicodeString *selectedPattern = (UnicodeString *)parsedValuesHash->get(sInput);
    if (selectedPattern==NULL) {
        selectedPattern = (UnicodeString *)parsedValuesHash->get(SELECT_KEYWORD_OTHER);
    }
    appendTo = insertFormattedSelect(*selectedPattern, appendTo);
    
    return appendTo;
}

UnicodeString&
SelectFormat::toPattern(UnicodeString& appendTo) {
    appendTo+= pattern;
    return appendTo;
}

UBool
SelectFormat::inRange(UChar ch, fmtToken& type) {
    if ((ch>=CAP_A) && (ch<=CAP_Z)) {
        // we assume all characters are in lower case already.
        return FALSE;
    }
    if ((ch>=LOW_A) && (ch<=LOW_Z)) {
        type = tLetter;
        return TRUE;
    }
    switch (ch) {
        case LEFTBRACE: 
            type = tLeftBrace;
            return TRUE;
        case SPACE:
            type = tSpace;
            return TRUE;
        case RIGHTBRACE:
            type = tRightBrace;
            return TRUE;
        case HYPHEN:
            type = tLetter;
            return TRUE;
        default :
            type = none;
            return FALSE;
    }
}

UBool
SelectFormat::checkSufficientDefinition() {
    // Check that at least the default rule is defined.
    if (parsedValuesHash==NULL)  return FALSE;
    if (parsedValuesHash->get(SELECT_KEYWORD_OTHER) == NULL) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

Format* SelectFormat::clone() const
{
    return new SelectFormat(*this);
}

SelectFormat&
SelectFormat::operator=(const SelectFormat& other) {
    if (this != &other) {
        UErrorCode status = U_ZERO_ERROR;
        delete parsedValuesHash;
        pattern = other.pattern;
        copyHashtable(other.parsedValuesHash, status);
    }
    return *this;
}

UBool
SelectFormat::operator==(const Format& other) const {
    // This protected comparison operator should only be called by subclasses
    // which have confirmed that the other object being compared against is
    // an instance of a sublcass of SelectFormat.  THIS IS IMPORTANT.
    // Format::operator== guarantees that this cast is safe
    SelectFormat* fmt = (SelectFormat*)&other;
    Hashtable* hashOther = fmt->parsedValuesHash;
    if( parsedValuesHash == NULL && hashOther==NULL)
        return TRUE;
    if( parsedValuesHash == NULL || hashOther==NULL)
        return FALSE;
    if( hashOther->count() != parsedValuesHash->count() ){
        return FALSE;
    }

    const UHashElement* elem = NULL;
    int32_t pos = -1;
    while((elem=hashOther->nextElement(pos))!=NULL) {
        const UHashTok otherKeyTok = elem->key;
        UnicodeString* otherKey = (UnicodeString*)otherKeyTok.pointer;
        const UHashTok otherKeyToVal = elem->value;
        UnicodeString* otherValue = (UnicodeString*)otherKeyToVal.pointer; 

        UnicodeString* thisElemValue = (UnicodeString*)parsedValuesHash->get(*otherKey);
        if( thisElemValue ==NULL ){
            return FALSE;
        }
        if( *thisElemValue != *otherValue){
            return FALSE;
        }
        
    }
    pos=-1;
    while((elem=parsedValuesHash->nextElement(pos))!=NULL) {
        const UHashTok thisKeyTok = elem->key;
        UnicodeString* thisKey = (UnicodeString*)thisKeyTok.pointer;
        const UHashTok thisKeyToVal = elem->value;
        UnicodeString* thisValue = (UnicodeString*)thisKeyToVal.pointer;

        UnicodeString* otherElemValue = (UnicodeString*)hashOther->get(*thisKey);
        if( otherElemValue ==NULL ){
            return FALSE;
        }
        if( *otherElemValue != *thisValue){
            return FALSE;
        }

    }
    return TRUE;
}

UBool
SelectFormat::operator!=(const Format& other) const {
    return  !operator==(other);
}

void
SelectFormat::parseObject(const UnicodeString& /*source*/,
                        Formattable& /*result*/,
                        ParsePosition& /*pos*/) const
{
    // TODO: not yet supported in icu4j and icu4c
}

UnicodeString
SelectFormat::insertFormattedSelect( UnicodeString& message,
                                    UnicodeString& appendTo) const {
    UnicodeString result;
    int32_t braceStack=0;
    int32_t startIndex=0;
    
    if (message.length()==0) {
        return result;
    }
    for(int32_t i=0; i<message.length(); ++i) {
        switch(message.charAt(i)) {
        case LEFTBRACE:
            ++braceStack;
            break;
        case RIGHTBRACE:
            --braceStack;
            break;
        }
    }
    if ( startIndex < message.length() ) {
        result += UnicodeString(message, startIndex, message.length()-startIndex);
    }
    appendTo = result;
    return result;
}

void
SelectFormat::copyHashtable(Hashtable *other, UErrorCode& status) {
    if (other == NULL) {
        parsedValuesHash = NULL;
        return;
    }
    parsedValuesHash = new Hashtable(TRUE, status);
    if(U_FAILURE(status)){
        return;
    }
    parsedValuesHash->setValueDeleter(deleteHashStrings);
    int32_t pos = -1;
    const UHashElement* elem = NULL;
    // walk through the hash table and create a deep clone
    while((elem = other->nextElement(pos))!= NULL){
        const UHashTok otherKeyTok = elem->key;
        UnicodeString* otherKey = (UnicodeString*)otherKeyTok.pointer;
        const UHashTok otherKeyToVal = elem->value;
        UnicodeString* otherValue = (UnicodeString*)otherKeyToVal.pointer;
        parsedValuesHash->put(*otherKey, new UnicodeString(*otherValue), status);
        if(U_FAILURE(status)){
            return;
        }
    }
}


U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
