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
    enum State{ startState , keywordState , pastKeywordState , phraseState};

    //Initialization
    UnicodeString keyword = UnicodeString();
    UnicodeString phrase = UnicodeString();
    UnicodeString* ptrPhrase ;
    int32_t braceCount=0;

    if (parsedValuesHash==NULL) {
        parsedValuesHash = new Hashtable(TRUE, status);
        parsedValuesHash = new Hashtable(TRUE, status);
        if (U_FAILURE(status)) {
            return;
        }
        parsedValuesHash->setValueDeleter(deleteHashStrings);
    }

    //Process the state machine
    static State state = startState;
    for (int32_t i=0; i<pattern.length(); ++i) {
        UChar ch=pattern.charAt(i);
        //fmtToken type;
        characterClass type;
        //Get the charcter and chek for validity
        if ( !classifyCharacters(ch, type) ) {
            if( state == keywordState ){
                status = U_ILLEGAL_CHARACTER;
                return;
            }else {
                phrase += ch;
                continue;
            }
        }
        //Process the character
        switch (state) {
            //At the start of pattern
            case startState:
                switch (type) {
                    case tSpace:
                        break;
                    case tStartKeyword:
                        state = keywordState;
                        keyword += ch;
                        break;
                    //If anything else is encountered,it's a syntax error
                    case tContinueKeyword:
                    case tLeftBrace:
                    case tRightBrace:
                    case tOther:
                    default:
                        status = U_PATTERN_SYNTAX_ERROR;
                        return;
                }//end of switch(type)
                break;
            //Handle the keyword state
            case keywordState:
                switch (type) {
                    case tSpace:
                        if( keyword.length() > 0)
                            state = pastKeywordState;
                        break;
                    case tStartKeyword:
                    case tContinueKeyword:
                        keyword += ch;
                        break;
                    case tLeftBrace:
                        state = phraseState;
                        break;
                    //If anything else is encountered,it's a syntax error
                    case tRightBrace:
                    case tOther:
                    default:
                        status = U_PATTERN_SYNTAX_ERROR;
                        return;
                }//end of switch(type)
                break;
            //Handle the pastkeyword state
            case pastKeywordState:
                switch (type) {
                    case tSpace:
                        break;
                    case tLeftBrace:
                        state = phraseState;
                        break;
                    //If anything else is encountered,it's a syntax error
                    case tStartKeyword:
                    case tContinueKeyword:
                    case tRightBrace:
                    case tOther:
                    default:
                        status = U_PATTERN_SYNTAX_ERROR;
                        return;
                }//end of switch(type)
                break;
            //Handle the phrase state
            case phraseState:
                switch (type) {
                    case tLeftBrace:
                        braceCount++;
                        phrase += ch;
                        break;
                    case tRightBrace:
                        if(braceCount == 0){
                            //Check validity of keyword
                            if (parsedValuesHash->get(keyword)!= NULL) {
                                status = U_DUPLICATE_KEYWORD;
                                return; 
                            }
                            if (keyword.length()==0) {
                                status = U_PATTERN_SYNTAX_ERROR;
                                return;
                            }
                            //Store the keyword, phrase pair in hashTable
                            ptrPhrase = new UnicodeString(phrase);
                            parsedValuesHash->put( keyword, ptrPhrase, status);
                            //Reinitialize
                            keyword.remove();
                            keyword = UnicodeString();
                            phrase.remove();
                            phrase = UnicodeString();
                            ptrPhrase = NULL;
                            braceCount=0;
                            state=startState;
                        }
                        if(braceCount > 0){
                            braceCount-- ;
                            phrase += ch;
                        }
                        break;
                    case tSpace:
                    case tStartKeyword:
                    case tContinueKeyword:
                    case tOther:
                    default:
                        phrase += ch;
                }//end of switch(type)
                break;
            //Handle the  default case of switch
            default:
                break;
        }//end of switch(state)
    }
    //Check if "other" keyword is present 
    if ( !checkSufficientDefinition() ) {
        status = U_DEFAULT_KEYWORD_MISSING;
    }
    return;
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
/*
    if ( startIndex < message.length() ) {
        result += UnicodeString(message, startIndex, message.length()-startIndex);
    }
*/
    
    return appendTo;
}

UnicodeString&
SelectFormat::toPattern(UnicodeString& appendTo) {
    appendTo+= pattern;
    return appendTo;
}

UBool
SelectFormat::classifyCharacters(UChar ch, characterClass& type) {
    if ((ch>=CAP_A) && (ch<=CAP_Z)) {
        // we assume all characters are in lower case already.
        return FALSE;
    }
    if ((ch>=LOW_A) && (ch<=LOW_Z)) {
        type = tStartKeyword;
        return TRUE;
    }
    if ((ch>=U_ZERO) && (ch<=U_NINE)) {
        type = tContinueKeyword;
        return TRUE;
    }
    switch (ch) {
        case LEFTBRACE: 
            type = tLeftBrace;
            return TRUE;
        case RIGHTBRACE:
            type = tRightBrace;
            return TRUE;
        case SPACE:
            type = tSpace;
            return TRUE;
        case HYPHEN:
        case LOWLINE:
            type = tContinueKeyword;
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

/*
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
*/

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
