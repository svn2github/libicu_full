/*
***************************************************************************
*   Copyright (C) 1999-2014 International Business Machines Corporation
*   and others. All rights reserved.
***************************************************************************
*/
//
//  file:  rbbi.c    Contains the implementation of the rule based break iterator
//                   runtime engine and the API implementation for
//                   class RuleBasedBreakIterator
//

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/rbbi.h"
#include "unicode/schriter.h"
#include "unicode/uchriter.h"
#include "unicode/udata.h"
#include "unicode/uclean.h"
#include "rbbidata.h"
#include "rbbirb.h"
#include "cmemory.h"
#include "cstring.h"
#include "umutex.h"
#include "ucln_cmn.h"
#include "brkeng.h"

#include "uassert.h"
#include "uvectr32.h"

// if U_LOCAL_SERVICE_HOOK is defined, then localsvc.cpp is expected to be included.
#if U_LOCAL_SERVICE_HOOK
#include "localsvc.h"
#endif

#ifdef RBBI_DEBUG
static UBool fTrace = FALSE;
#endif

U_NAMESPACE_BEGIN

// The state number of the starting state
#define START_STATE 1

// The state-transition value indicating "stop"
#define STOP_STATE  0


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedBreakIterator)


//=======================================================================
// constructors
//=======================================================================

/**
 * Constructs a RuleBasedBreakIterator that uses the already-created
 * tables object that is passed in as a parameter.
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(RBBIDataHeader* data, UErrorCode &status)
{
    init(status);
    fData = new RBBIDataWrapper(data, status); // status checked in constructor
    if (U_FAILURE(status)) {
        return;
    }
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

/**
 * Same as above but does not adopt memory
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(const RBBIDataHeader* data, enum EDontAdopt, UErrorCode &status)
{
    init(status);
    fData = new RBBIDataWrapper(data, RBBIDataWrapper::kDontAdopt, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}


//
//  Construct from precompiled binary rules (tables).  This constructor is public API,
//  taking the rules as a (const uint8_t *) to match the type produced by getBinaryRules().
//
RuleBasedBreakIterator::RuleBasedBreakIterator(const uint8_t *compiledRules,
                       uint32_t       ruleLength,
                       UErrorCode     &status) {
    init(status);
    if (U_FAILURE(status)) {
        return;
    }
    if (compiledRules == NULL || ruleLength < sizeof(RBBIDataHeader)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    const RBBIDataHeader *data = (const RBBIDataHeader *)compiledRules;
    if (data->fLength > ruleLength) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    fData = new RBBIDataWrapper(data, RBBIDataWrapper::kDontAdopt, status); 
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}    


//-------------------------------------------------------------------------------
//
//   Constructor   from a UDataMemory handle to precompiled break rules
//                 stored in an ICU data file.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(UDataMemory* udm, UErrorCode &status)
{
    init(status);
    fData = new RBBIDataWrapper(udm, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}



//-------------------------------------------------------------------------------
//
//   Constructor       from a set of rules supplied as a string.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator( const UnicodeString  &rules,
                                                UParseError          &parseError,
                                                UErrorCode           &status)
{
    init(status);
    if (U_FAILURE(status)) {return;}
    RuleBasedBreakIterator *bi = (RuleBasedBreakIterator *)
        RBBIRuleBuilder::createRuleBasedBreakIterator(rules, &parseError, status);
    // Note:  This is a bit awkward.  The RBBI ruleBuilder has a factory method that
    //        creates and returns a complete RBBI.  From here, in a constructor, we
    //        can't just return the object created by the builder factory, hence
    //        the assignment of the factory created object to "this".
    if (U_SUCCESS(status)) {
        *this = *bi;
        delete bi;
    }
}


//-------------------------------------------------------------------------------
//
// Default Constructor.      Create an empty shell that can be set up later.
//                           Used when creating a RuleBasedBreakIterator from a set
//                           of rules.
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator() {
    UErrorCode status = U_ZERO_ERROR;
    init(status);
}


//-------------------------------------------------------------------------------
//
//   Copy constructor.  Will produce a break iterator with the same behavior,
//                      and which iterates over the same text, as the one passed in.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(const RuleBasedBreakIterator& other)
: BreakIterator(other)
{
    UErrorCode status = U_ZERO_ERROR;
    this->init(status);
    *this = other;
}


/**
 * Destructor
 */
RuleBasedBreakIterator::~RuleBasedBreakIterator() {
    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        // fCharIter was adopted from the outside.
        delete fCharIter;
    }
    fCharIter = NULL;
    delete fSCharIter;
    fSCharIter = NULL;
    delete fDCharIter;
    fDCharIter = NULL;
    
    utext_close(fText);

    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    delete fBreakCache;
    fBreakCache = NULL;

    delete fLanguageBreakEngines;
    fLanguageBreakEngines = NULL;

    delete fUnhandledBreakEngine;
    fUnhandledBreakEngine = NULL;
}

/**
 * Assignment operator.  Sets this iterator to have the same behavior,
 * and iterate over the same text, as the one passed in.
 */
RuleBasedBreakIterator&
RuleBasedBreakIterator::operator=(const RuleBasedBreakIterator& that) {
    if (this == &that) {
        return *this;
    }

    fBreakType = that.fBreakType;
    delete fLanguageBreakEngines;
    fLanguageBreakEngines = NULL;   // Just rebuild for now
    // TODO: clone fLanguageBreakEngines from "that"


    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    if (that.fData != NULL) {
        fData = that.fData->addReference();
    }

    UErrorCode status = U_ZERO_ERROR;
    setText(that.fText, status);

    return *this;
}



//-----------------------------------------------------------------------------
//
//    init()      Shared initialization routine.   Used by all the constructors.
//                Initializes all fields, leaving the object in a consistent state.
//
//-----------------------------------------------------------------------------
void RuleBasedBreakIterator::init(UErrorCode &status) {
    fText                 = NULL;
    fCharIter             = NULL;
    fSCharIter            = NULL;
    fDCharIter            = NULL;
    fData                 = NULL;
    fLastRuleStatusIndex  = 0;  // < 0 if rule status is invalid.
    fBreakType            = UBRK_WORD;  // Defaulting BreakType to word gives reasonable
                                        //   dictionary behavior for Break Iterators that are
                                        //   built from rules.  Even better would be the ability to
                                        //   declare the type in the rules.

    fBreakCache            = NULL;
    fLanguageBreakEngines  = NULL;
    fUnhandledBreakEngine  = NULL;

    if (U_FAILURE(status)) {
        return;
    }
    fText                    = utext_openUChars(NULL, NULL, 0, &status);
    fBreakCache              = new BreakCache(this, status);

    if (fText == NULL || fBreakCache == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }

#ifdef RBBI_DEBUG
    static UBool debugInitDone = FALSE;
    if (debugInitDone == FALSE) {
        char *debugEnv = getenv("U_RBBIDEBUG");
        if (debugEnv && uprv_strstr(debugEnv, "trace")) {
            fTrace = TRUE;
        }
        debugInitDone = TRUE;
    }
#endif
}



//-----------------------------------------------------------------------------
//
//    clone - Returns a newly-constructed RuleBasedBreakIterator with the same
//            behavior, and iterating over the same text, as this one.
//            Virtual function: does the right thing with subclasses.
//
//-----------------------------------------------------------------------------
BreakIterator*
RuleBasedBreakIterator::clone(void) const {
    return new RuleBasedBreakIterator(*this);
}

/**
 * Equality operator.  Returns TRUE if both BreakIterators are of the
 * same class, have the same behavior, and iterate over the same text.
 */
UBool
RuleBasedBreakIterator::operator==(const BreakIterator& that) const {
    if (typeid(*this) != typeid(that)) {
        return FALSE;
    }

    const RuleBasedBreakIterator& that2 = (const RuleBasedBreakIterator&) that;

    if (!utext_equals(fText, that2.fText)) {
        // The two break iterators are operating on different text,
        //   or have a different interation position.
        return FALSE;
    };

    // TODO:  need a check for when in a dictionary region at different offsets.

    if (that2.fData == fData ||
        (fData != NULL && that2.fData != NULL && *that2.fData == *fData)) {
            // The two break iterators are using the same rules.
            return TRUE;
        }
    return FALSE;
}

/**
 * Compute a hash code for this BreakIterator
 * @return A hash code
 */
int32_t
RuleBasedBreakIterator::hashCode(void) const {
    int32_t   hash = 0;
    if (fData != NULL) {
        hash = fData->hashCode();
    }
    return hash;
}


void RuleBasedBreakIterator::setText(UText *ut, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fBreakCache->reset();
    fText = utext_clone(fText, ut, FALSE, TRUE, &status);

    // Set up a dummy CharacterIterator to be returned if anyone
    //   calls getText().  With input from UText, there is no reasonable
    //   way to return a characterIterator over the actual input text.
    //   Return one over an empty string instead - this is the closest
    //   we can come to signaling a failure.
    //   (GetText() is obsolete, this failure is sort of OK)
    if (fDCharIter == NULL) {
        static const UChar c = 0;
        fDCharIter = new UCharCharacterIterator(&c, 0);
        if (fDCharIter == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }

    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        // existing fCharIter was adopted from the outside.  Delete it now.
        delete fCharIter;
    }
    fCharIter = fDCharIter;

    this->first();
}


UText *RuleBasedBreakIterator::getUText(UText *fillIn, UErrorCode &status) const {
    UText *result = utext_clone(fillIn, fText, FALSE, TRUE, &status);  
    return result;
}



/**
 * Returns the description used to create this iterator
 */
const UnicodeString&
RuleBasedBreakIterator::getRules() const {
    if (fData != NULL) {
        return fData->getRuleSourceString();
    } else {
        static const UnicodeString *s;
        if (s == NULL) {
            // TODO:  something more elegant here.
            //        perhaps API should return the string by value.
            //        Note:  thread unsafe init & leak are semi-ok, better than
            //               what was before.  Sould be cleaned up, though.
            s = new UnicodeString;
        }
        return *s;
    }
}

//=======================================================================
// BreakIterator overrides
//=======================================================================

/**
 * Return a CharacterIterator over the text being analyzed.  
 */
CharacterIterator&
RuleBasedBreakIterator::getText() const {
    return *fCharIter;
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::adoptText(CharacterIterator* newText) {
    // If we are holding a CharacterIterator adopted from a 
    //   previous call to this function, delete it now.
    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        delete fCharIter;
    }

    fCharIter = newText;
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->reset();
    if (newText==NULL || newText->startIndex() != 0) {   
        // startIndex !=0 wants to be an error, but there's no way to report it.
        // Make the iterator text be an empty string.
        fText = utext_openUChars(fText, NULL, 0, &status);
    } else {
        fText = utext_openCharacterIterator(fText, newText, &status);
    }
    this->first();
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::setText(const UnicodeString& newText) {
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->reset();
    fText = utext_openConstUnicodeString(fText, &newText, &status);

    // Set up a character iterator on the string.  
    //   Needed in case someone calls getText().
    //  Can not, unfortunately, do this lazily on the (probably never)
    //  call to getText(), because getText is const.
    if (fSCharIter == NULL) {
        fSCharIter = new StringCharacterIterator(newText);
    } else {
        fSCharIter->setText(newText);
    }

    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        // old fCharIter was adopted from the outside.  Delete it.
        delete fCharIter;
    }
    fCharIter = fSCharIter;

    this->first();
}


/**
 *  Provide a new UText for the input text.  Must reference text with contents identical
 *  to the original.
 *  Intended for use with text data originating in Java (garbage collected) environments
 *  where the data may be moved in memory at arbitrary times.
 */
RuleBasedBreakIterator &RuleBasedBreakIterator::refreshInputText(UText *input, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (input == NULL) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    int64_t pos = utext_getNativeIndex(fText);
    //  Shallow read-only clone of the new UText into the existing input UText
    fText = utext_clone(fText, input, FALSE, TRUE, &status);
    if (U_FAILURE(status)) {
        return *this;
    }
    utext_setNativeIndex(fText, pos);
    if (utext_getNativeIndex(fText) != pos) {
        // Sanity check.  The new input utext is supposed to have the exact same
        // contents as the old.  If we can't set to the same position, it doesn't.
        // The contents underlying the old utext might be invalid at this point,
        // so it's not safe to check directly.
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return *this;
}


/**
 * Sets the current iteration position to the beginning of the text, position zero.
 * @return The new iterator position, which is zero.
 */
int32_t RuleBasedBreakIterator::first(void) {
    fLastRuleStatusIndex  = 0;
    fLastTagValue = -1;
    utext_setNativeIndex(fText, 0);
    return 0;
}

/**
 * Sets the current iteration position to the end of the text.
 * @return The text's past-the-end offset.
 */
int32_t RuleBasedBreakIterator::last(void) {
    fLastTagValue = -1;
    if (fText == NULL) {
        fLastRuleStatusIndex  = 0;
        return BreakIterator::DONE;
    }

    // TODO: check cache, it may extend to the end.

    fLastRuleStatusIndex  = -1;
    int32_t pos = (int32_t)utext_nativeLength(fText);
    utext_setNativeIndex(fText, pos);
    return pos;
}

/**
 * Advances the iterator either forward or backward the specified number of steps.
 * Negative values move backward, and positive values move forward.  This is
 * equivalent to repeatedly calling next() or previous().
 * @param n The number of steps to move.  The sign indicates the direction
 * (negative is backwards, and positive is forwards).
 * @return The character offset of the boundary position n boundaries away from
 * the current one.
 */
int32_t RuleBasedBreakIterator::next(int32_t n) {
    int32_t result = current();
    while (n > 0) {
        result = next();
        --n;
    }
    while (n < 0) {
        result = previous();
        ++n;
    }
    return result;
}

/**
 * Advances the iterator to the next boundary position.
 * @return The position of the first boundary after this one.
 */
int32_t RuleBasedBreakIterator::next(void) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t startPos = current();
    int32_t result = fBreakCache->following(startPos);
    if (result != BreakIterator::DONE) {
        fLastTagValue = fBreakCache->getCurrentRuleStatus();
        utext_setNativeIndex(fText, result);
        return result;
    }

    UBool useDictionary = FALSE;
    result = handleNext(fData->fForwardTable, &useDictionary);
    if (useDictionary) { // if only dict words, or contains mix of dict and non-dict words.
        int32_t tagValue = 0;
        if (fLastRuleStatusIndex >= 0) {
            // TODO(jchye): copied from getRuleStatus(). Refactor?
            int32_t  idx = fLastRuleStatusIndex + fData->fRuleStatusTable[fLastRuleStatusIndex];
            tagValue = fData->fRuleStatusTable[idx];
        }
        fBreakCache->populate(startPos, result, fLastTagValue, tagValue, status);
        result = fBreakCache->following(startPos);
        fLastTagValue = tagValue;
    } else {
        fLastTagValue = -1;
    }
    if (result != BreakIterator::DONE) {
        utext_setNativeIndex(fText, result);
    }
    return result;
}


/**
 * Advances the iterator backwards, to the last boundary preceding this one.
 * @return The position of the last boundary position preceding this one.
 */
int32_t RuleBasedBreakIterator::previous(void) {
    int32_t startPos = current();

    // if we're already sitting at the beginning of the text, return DONE
    if (fText == NULL || startPos == 0) {
        fLastRuleStatusIndex  = 0;
        return BreakIterator::DONE;
    }

    // If the iteration is in a cached dictionary range, return a boundary from that.
    int32_t result = fBreakCache->preceding(startPos);
    if (result != BreakIterator::DONE) {
        fLastTagValue = fBreakCache->getCurrentRuleStatus();
        utext_setNativeIndex(fText, result);
        return result;
    }

    // If we have exact reverse rules, use them.
    UErrorCode status = U_ZERO_ERROR;
    if (fData->fSafeRevTable != NULL || fData->fSafeFwdTable != NULL) {
        UBool useDictionary = FALSE;
        result = handlePrevious(fData->fReverseTable, &useDictionary);
        if (useDictionary) {
            fBreakCache->populate(result, startPos, fLastTagValue, -1, status);
            result = fBreakCache->preceding(startPos);
        }
        utext_setNativeIndex(fText, result);
        return result;
    }

    // Old rule syntax - reverse rules will move to a safe point at or before
    // the desired boundary.

    fLastTagValue = -1;
    result = handlePrevious(fData->fReverseTable);
    if (result == UBRK_DONE) {
        result = 0;
        utext_setNativeIndex(fText, 0);
    }

    // From the safe point, iterate forward until we pass our
    // starting point.  The last break position before the starting
    // point is our return value

    int32_t ruleStatusIndex = -1;
    for (;;) {
        int32_t nextResult = next();
        if (nextResult == BreakIterator::DONE || nextResult >= startPos) {
            break;
        }
        result = nextResult;
        ruleStatusIndex = fLastRuleStatusIndex;
    }

    // Note edge case when result == BreakIterator::DONE, which is -1,
    //     utext_setNativeIndex() will clip the -1 back to zero, which is what is needed.
    utext_setNativeIndex(fText, result);
    fLastRuleStatusIndex  = ruleStatusIndex;       // for use by getRuleStatus()
    return result;
}


/**
 * Sets the iterator to refer to the first boundary position following
 * the specified position.
 * @offset The position from which to begin searching for a break position.
 * @return The position of the first break after the current position.
 */
int32_t RuleBasedBreakIterator::following(int32_t offset) {
    // if the offset passed in is already past the end of the text,
    // just return DONE; if it's before the beginning, return the
    // text's starting offset
    if (fText == NULL || offset >= utext_nativeLength(fText)) {
        last();
        return next();
    }
    else if (offset < 0) {
        return first();
    }

    // if we have cached break positions and offset is in the range
    // covered by them, use them.
    int32_t result = fBreakCache->following(offset);
    if (result != BreakIterator::DONE) {
        utext_setNativeIndex(fText, result);
        return result;
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8.
    utext_setNativeIndex(fText, offset);
    offset = utext_getNativeIndex(fText);

    // Set our internal iteration position (temporarily)
    // to the position passed in.  If this is the _beginning_ position,
    // then we can just use next() to get our return value

    if (fData->fSafeRevTable != NULL) {
        // new rule syntax
        utext_setNativeIndex(fText, offset);
        // move forward one codepoint to prepare for moving back to a
        // safe point.
        // this handles offset being between a supplementary character
        // TODO: is this still needed, with move to code point boundary handled above?
        (void)UTEXT_NEXT32(fText);
        // handlePrevious will move most of the time to < 1 boundary away
        handlePrevious(fData->fSafeRevTable);
        result = next();
        while (result != BreakIterator::DONE && result <= offset) {
            result = next();
        }
        return result;
    }
    if (fData->fSafeFwdTable != NULL) {
        // backup plan if forward safe table is not available
        utext_setNativeIndex(fText, offset);
        (void)UTEXT_PREVIOUS32(fText);
        // handle next will give result >= offset
        handleNext(fData->fSafeFwdTable);
        // previous will give result 0 or 1 boundary away from offset,
        // most of the time
        // we have to
        int32_t oldresult = previous();
        while (oldresult > offset) {
            result = previous();
            if (result <= offset) {
                return oldresult;
            }
            oldresult = result;
        }
        result = next();
        if (result <= offset) {
            return next();
        }
        return result;
    }
    // otherwise, we have to sync up first.  Use handlePrevious() to back
    // up to a known break position before the specified position (if
    // we can determine that the specified position is a break position,
    // we don't back up at all).  This may or may not be the last break
    // position at or before our starting position.  Advance forward
    // from here until we've passed the starting position.  The position
    // we stop on will be the first break position after the specified one.
    // old rule syntax

    utext_setNativeIndex(fText, offset);
    if (offset==0 || 
        (offset==1  && utext_getNativeIndex(fText)==0)) {
        return next();
    }
    // TODO (andy): revisit this. Old style rules.
    result = previous();

    while (result != BreakIterator::DONE && result <= offset) {
        result = next();
    }

    return result;
}

/**
 * Sets the iterator to refer to the last boundary position before the
 * specified position.
 * @offset The position to begin searching for a break from.
 * @return The position of the last boundary before the starting position.
 */
int32_t RuleBasedBreakIterator::preceding(int32_t offset) {
    // if the offset passed in is already past the end of the text,
    // just return DONE; if it's before the beginning, return the
    // text's starting offset
    if (fText == NULL || offset > utext_nativeLength(fText)) {
        return last();
    }
    else if (offset < 0) {
        return first();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8.
    utext_setNativeIndex(fText, offset);
    offset = utext_getNativeIndex(fText);

    // if we have cached break positions and offset is in the range
    // covered by them, use them

    int32_t result = fBreakCache->preceding(offset);
    if (result != BreakIterator::DONE) {
        utext_setNativeIndex(fText, result);
        return result;
    }


    // if we start by updating the current iteration position to the
    // position specified by the caller, we can just use previous()
    // to carry out this operation

    if (fData->fSafeFwdTable != NULL) {
        // new rule syntax
        utext_setNativeIndex(fText, offset);
        int32_t newOffset = (int32_t)UTEXT_GETNATIVEINDEX(fText);
        if (newOffset != offset) {
            // Will come here if specified offset was not a code point boundary
            // For breakitereator::preceding only, these non-code-point indices need to be moved
            //   up to refer to the following codepoint.
            // TODO (andy): revisit this.
            (void)utext_next32(fText);
            offset = (int32_t)utext_getNativeIndex(fText);
        }

        (void)utext_previous32(fText);
        handleNext(fData->fSafeFwdTable);
        result = (int32_t)utext_getNativeIndex(fText);
        while (result >= offset) {
            result = previous();
        }
        return result;
    }
    if (fData->fSafeRevTable != NULL) {
        // backup plan if forward safe table is not available
        //  TODO:  check whether this path can be discarded
        //         It's probably OK to say that rules must supply both safe tables
        //            if they use safe tables at all.  We have certainly never described
        //            to anyone how to work with just one safe table.
        utext_setNativeIndex(fText, offset);
        (void)utext_next32(fText);
        
        // handle previous will give result <= offset
        handlePrevious(fData->fSafeRevTable);

        // next will give result 0 or 1 boundary away from offset,
        // most of the time
        // we have to
        int32_t oldresult = next();
        while (oldresult < offset) {
            result = next();
            if (result >= offset) {
                return oldresult;
            }
            oldresult = result;
        }
        result = previous();
        if (result >= offset) {
            return previous();
        }
        return result;
    }

    // old rule syntax
    utext_setNativeIndex(fText, offset);
    // TODO (andy): This shouldn't work. Investigate.
    return previous();
}

/**
 * Returns true if the specfied position is a boundary position.  As a side
 * effect, leaves the iterator pointing to the first boundary position at
 * or after "offset".
 * @param offset the offset to check.
 * @return True if "offset" is a boundary position.
 */
UBool RuleBasedBreakIterator::isBoundary(int32_t offset) {
    // the beginning index of the iterator is always a boundary position by definition
    if (offset == 0) {
        first();       // For side effects on current position, tag values.
        return TRUE;
    }

    if (offset == (int32_t)utext_nativeLength(fText)) {
        last();       // For side effects on current position, tag values.
        return TRUE;
    }

    // out-of-range indexes are never boundary positions
    if (offset < 0) {
        first();       // For side effects on current position, tag values.
        return FALSE;
    }

    if (offset > utext_nativeLength(fText)) {
        last();        // For side effects on current position, tag values.
        return FALSE;
    }

    // otherwise, we can use following() on the position before the specified
    // one and return true if the position we get back is the one the user
    // specified
    utext_previous32From(fText, offset);
    int32_t backOne = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    int32_t follow = following(backOne);
    UBool result = (follow == offset);
    return result;
}

/**
 * Returns the current iteration position.
 * @return The current iteration position.
 */
int32_t RuleBasedBreakIterator::current(void) const {
    int32_t  pos = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    return pos;
}
 
//=======================================================================
// implementation
//=======================================================================

//
// RBBIRunMode  -  the state machine runs an extra iteration at the beginning and end
//                 of user text.  A variable with this enum type keeps track of where we
//                 are.  The state machine only fetches user input while in the RUN mode.
//
enum RBBIRunMode {
    RBBI_START,     // state machine processing is before first char of input
    RBBI_RUN,       // state machine processing is in the user text
    RBBI_END        // state machine processing is after end of user text.
};


//-----------------------------------------------------------------------------------
//
//  handleNext(stateTable)
//     This method is the actual implementation of the rbbi next() method. 
//     This method initializes the state machine to state 1
//     and advances through the text character by character until we reach the end
//     of the text or the state machine transitions to state 0.  We update our return
//     value every time the state machine passes through an accepting state.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handleNext(const RBBIStateTable *statetable, UBool *dictionaryCharsSeen) {
    int32_t             state;
    uint16_t            category        = 0;
    RBBIRunMode         mode;
    
    RBBIStateTableRow  *row;
    UChar32             c;
    int32_t             lookaheadStatus = 0;
    int32_t             lookaheadTagIdx = 0;
    int32_t             result          = 0;
    int32_t             initialPosition = 0;
    int32_t             lookaheadResult = 0;
    UBool               lookAheadHardBreak = (statetable->fFlags & RBBI_LOOKAHEAD_HARD_BREAK) != 0;
    const char         *tableData       = statetable->fTableData;
    uint32_t            tableRowLen     = statetable->fRowLen;
    int32_t             firstDictionaryPosition = INT32_MAX;

    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPuts("Handle Next   pos   char  state category");
        }
    #endif

    if (dictionaryCharsSeen != NULL) {
        *dictionaryCharsSeen = FALSE;
    }

    // No matter what, handleNext always correctly sets the break tag value.
    fLastRuleStatusIndex = 0;

    // if we're already at the end of the text, return DONE.
    initialPosition = (int32_t)UTEXT_GETNATIVEINDEX(fText); 
    result          = initialPosition;
    c               = UTEXT_NEXT32(fText);
    if (fData == NULL || c==U_SENTINEL) {
        return BreakIterator::DONE;
    }

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RBBIStateTableRow *)
            //(statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);
            
    
    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the end of the text or transition to state 0
    //
    for (;;) {
        if (c == U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END) {
                // We have already run the loop one last time with the 
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                if (lookaheadResult > result) {
                    // We ran off the end of the string with a pending look-ahead match.
                    // Treat this as if the look-ahead condition had been met, and return
                    //  the match at the / position from the look-ahead rule.
                    result               = lookaheadResult;
                    fLastRuleStatusIndex = lookaheadTagIdx;
                    lookaheadStatus = 0;
                } 
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
            // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
            //        not the size of the character going in, which is a UChar32.
            //
            UTRIE_GET16(&fData->fTrie, c, category);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iterators (subclasses).
            //    Chars that need to be handled by a dictionary have a flag bit set
            //    in their category values.
            //
            if ((category & 0x4000) != 0)  {
                // Stop matching if non-dictionary characters were previously
                // matched.
                if (firstDictionaryPosition == INT32_MAX) {
                  firstDictionaryPosition = utext_getNativeIndex(fText);
                }
                //  And off the dictionary flag bit.
                category &= ~0x4000;
            }
        }

       #ifdef RBBI_DEBUG
            if (fTrace) {
                RBBIDebugPrintf("             %4ld   ", utext_getNativeIndex(fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //

        // Note: fNextState is defined as uint16_t[2], but we are casting
        // a generated RBBI table to RBBIStateTableRow and some tables
        // actually have more than 2 categories.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RBBIStateTableRow *)
            // (statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);


        if (row->fAccepting == -1) {
            // Match found, common case.
            if (mode != RBBI_START) {
                result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            }
            fLastRuleStatusIndex = row->fTagIdx;   // Remember the break status (tag) values.
        }

        if (row->fLookAhead != 0) {
            if (lookaheadStatus != 0
                && row->fAccepting == lookaheadStatus) {
                // Lookahead match is completed.  
                result               = lookaheadResult;
                fLastRuleStatusIndex = lookaheadTagIdx;
                lookaheadStatus      = 0;
                // TODO:  make a standalone hard break in a rule work.
                if (lookAheadHardBreak) {
                    UTEXT_SETNATIVEINDEX(fText, result);
                    *dictionaryCharsSeen = (firstDictionaryPosition < INT32_MAX);
                    return result;
                }
                // Look-ahead completed, but other rules may match further.  Continue on
                //  TODO:  junk this feature?  I don't think it's used anywhwere.
                goto continueOn;
            }

            int32_t  r = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            lookaheadResult = r;
            lookaheadStatus = row->fLookAhead;
            lookaheadTagIdx = row->fTagIdx;
            goto continueOn;
        }


        if (row->fAccepting != 0) {
            // Because this is an accepting state, any in-progress look-ahead match
            //   is no longer relavant.  Clear out the pending lookahead status.
            lookaheadStatus = 0;           // clear out any pending look-ahead match.
        }

continueOn:
        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }
        
        // Advance to the next character.  
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            c = UTEXT_NEXT32(fText);
        } else {
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }


    }

    // The state machine is done.  Check whether it found a match...

    if (dictionaryCharsSeen != NULL && firstDictionaryPosition <= result) {
        *dictionaryCharsSeen = TRUE;
    }

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        UTEXT_SETNATIVEINDEX(fText, initialPosition);
        UTEXT_NEXT32(fText);
        result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    }

    // Leave the iterator at our result position.
    UTEXT_SETNATIVEINDEX(fText, result);
    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}



//-----------------------------------------------------------------------------------
//
//  handlePrevious()
//
//      Iterate backwards, according to the logic of the reverse rules.
//      This version handles the exact style backwards rules.
//
//      The logic of this function is very similar to handleNext(), above.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handlePrevious(const RBBIStateTable *statetable, UBool *dictionaryCharsSeen) {
    int32_t             state;
    uint16_t            category        = 0;
    RBBIRunMode         mode;
    RBBIStateTableRow  *row;
    UChar32             c;
    int32_t             lookaheadStatus = 0;
    int32_t             result          = 0;
    int32_t             initialPosition = 0;
    int32_t             lookaheadResult = 0;
    UBool               lookAheadHardBreak = (statetable->fFlags & RBBI_LOOKAHEAD_HARD_BREAK) != 0;
    int32_t             firstDictionaryPosition = -1;

    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPuts("Handle Previous   pos   char  state category");
        }
    #endif

    if (dictionaryCharsSeen != NULL) {
        *dictionaryCharsSeen = FALSE;
    }

    // handlePrevious() never gets the rule status.
    // Flag the status as invalid; if the user ever asks for status, we will need
    // to back up, then re-find the break position using handleNext(), which does
    // get the status value.
    fLastRuleStatusIndex = -1;

    // if we're already at the start of the text, return DONE.
    if (fText == NULL || fData == NULL || UTEXT_GETNATIVEINDEX(fText)==0) {
        return BreakIterator::DONE;
    }

    //  Set up the starting char.
    initialPosition = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    result          = initialPosition;
    c               = UTEXT_PREVIOUS32(fText);

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));
    category = 3;
    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the start of the text or transition to state 0
    //
    for (;;) {
        if (c == U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END) {
                // We have already run the loop one last time with the 
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                if (lookaheadResult < result) {
                    // We ran off the end of the string with a pending look-ahead match.
                    // Treat this as if the look-ahead condition had been met, and return
                    //  the match at the / position from the look-ahead rule.
                    result               = lookaheadResult;
                    lookaheadStatus = 0;
                } else if (result == initialPosition) {
                    // Ran off start, no match found.
                    // move one index one (towards the start, since we are doing a previous())
                    UTEXT_SETNATIVEINDEX(fText, initialPosition);
                    (void)UTEXT_PREVIOUS32(fText);   // TODO:  shouldn't be necessary.  We're already at beginning.  Check.
                }
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
            // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
            //        not the size of the character going in, which is a UChar32.
            //
            UTRIE_GET16(&fData->fTrie, c, category);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iterators (subclasses).
            //    Chars that need to be handled by a dictionary have a flag bit set
            //    in their category values.
            //
            if ((category & 0x4000) != 0)  {
                if (firstDictionaryPosition == -1) {
                    firstDictionaryPosition = utext_getNativeIndex(fText);
                }
                //  And off the dictionary flag bit.
                category &= ~0x4000;
            }
        }

        #ifdef RBBI_DEBUG
            if (fTrace) {
                RBBIDebugPrintf("             %4d   ", (int32_t)utext_getNativeIndex(fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //

        // Note: fNextState is defined as uint16_t[2], but we are casting
        // a generated RBBI table to RBBIStateTableRow and some tables
        // actually have more than 2 categories.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));

        if (row->fAccepting == -1) {
            // Match found, common case.
            result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
        }

        if (row->fLookAhead != 0) {
            if (lookaheadStatus != 0
                && row->fAccepting == lookaheadStatus) {
                // Lookahead match is completed.  
                result               = lookaheadResult;
                lookaheadStatus      = 0;
                // TODO:  make a standalone hard break in a rule work.
                if (lookAheadHardBreak) {
                    UTEXT_SETNATIVEINDEX(fText, result);
                    return result;
                }
                // Look-ahead completed, but other rules may match further.  Continue on
                //  TODO:  junk this feature?  I don't think it's used anywhwere.
                goto continueOn;
            }

            int32_t  r = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            lookaheadResult = r;
            lookaheadStatus = row->fLookAhead;
            goto continueOn;
        }


        if (row->fAccepting != 0) {
            // Because this is an accepting state, any in-progress look-ahead match
            //   is no longer relavant.  Clear out the pending lookahead status.
            lookaheadStatus = 0;    
        }

continueOn:
        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }

        // Move (backwards) to the next character to process.  
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            c = UTEXT_PREVIOUS32(fText);
        } else {            
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }
    }

    // The state machine is done.  Check whether it found a match...

    if (dictionaryCharsSeen != NULL && firstDictionaryPosition > result) {
        *dictionaryCharsSeen = TRUE;
    }

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        UTEXT_SETNATIVEINDEX(fText, initialPosition);
        UTEXT_PREVIOUS32(fText);
        result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    }
    // Leave the iterator at our result position.
    UTEXT_SETNATIVEINDEX(fText, result);
    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}


//-------------------------------------------------------------------------------
//
//   getRuleStatus()   Return the break rule tag associated with the current
//                     iterator position.  If the iterator arrived at its current
//                     position by iterating forwards, the value will have been
//                     cached by the handleNext() function.
//
//                     If no cached status value is available, the status is
//                     found by doing a previous() followed by a next(), which
//                     leaves the iterator where it started, and computes the
//                     status while doing the next().
//
//-------------------------------------------------------------------------------
void RuleBasedBreakIterator::makeRuleStatusValid() {
    if (fLastRuleStatusIndex < 0) {
        //  No cached status is available.
        if (fText == NULL || current() == 0) {
            //  At start of text, or there is no text.  Status is always zero.
            fLastRuleStatusIndex = 0;
        } else {
            //  Not at start of text.  Find status the tedious way.
            int32_t pa = current();
            previous();
            int32_t pb = next();
            (void)pa; (void)pb;  // Suppress compiler warning re unused variables.
            U_ASSERT(pa == pb);
        }
    }
    U_ASSERT(fLastRuleStatusIndex >= 0  &&  fLastRuleStatusIndex < fData->fStatusMaxIdx ||
             fLastTagValue >= 0);
}


int32_t  RuleBasedBreakIterator::getRuleStatus() const {
    if (fLastTagValue < 0) {
      RuleBasedBreakIterator *nonConstThis  = (RuleBasedBreakIterator *)this;
      nonConstThis->makeRuleStatusValid();
    }

    if (fLastTagValue >= 0) {
      return fLastTagValue;
    }

    // fLastRuleStatusIndex indexes to the start of the appropriate status record
    //                                                 (the number of status values.)
    //   This function returns the last (largest) of the array of status values.
    int32_t  idx = fLastRuleStatusIndex + fData->fRuleStatusTable[fLastRuleStatusIndex];
    return fData->fRuleStatusTable[idx];
}




int32_t RuleBasedBreakIterator::getRuleStatusVec(
             int32_t *fillInVec, int32_t capacity, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return 0;
    }

    if (fLastTagValue < 0) {
      RuleBasedBreakIterator *nonConstThis  = (RuleBasedBreakIterator *)this;
      nonConstThis->makeRuleStatusValid();
    }

    if (fLastTagValue >= 0) {
      fillInVec[0] = fLastTagValue;
      return 1;
    }

    int32_t  numVals = fData->fRuleStatusTable[fLastRuleStatusIndex];
    int32_t  numValsToCopy = numVals;
    if (numVals > capacity) {
        status = U_BUFFER_OVERFLOW_ERROR;
        numValsToCopy = capacity;
    }
    int i;
    for (i=0; i<numValsToCopy; i++) {
        fillInVec[i] = fData->fRuleStatusTable[fLastRuleStatusIndex + i + 1];
    }
    return numVals;
}



//-------------------------------------------------------------------------------
//
//   getBinaryRules        Access to the compiled form of the rules,
//                         for use by build system tools that save the data
//                         for standard iterator types.
//
//-------------------------------------------------------------------------------
const uint8_t  *RuleBasedBreakIterator::getBinaryRules(uint32_t &length) {
    const uint8_t  *retPtr = NULL;
    length = 0;

    if (fData != NULL) {
        retPtr = (const uint8_t *)fData->fHeader;
        length = fData->fHeader->fLength;
    }
    return retPtr;
}


BreakIterator *  RuleBasedBreakIterator::createBufferClone(void * /*stackBuffer*/,
                                   int32_t &bufferSize,
                                   UErrorCode &status)
{
    if (U_FAILURE(status)){
        return NULL;
    }

    if (bufferSize == 0) {
        bufferSize = 1;  // preflighting for deprecated functionality
        return NULL;
    }

    BreakIterator *clonedBI = clone();
    if (clonedBI == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } else {
        status = U_SAFECLONE_ALLOCATED_WARNING;
    }
    return (RuleBasedBreakIterator *)clonedBI;
}


//-------------------------------------------------------------------------------
//
//  isDictionaryChar      Return true if the category lookup for this char
//                        indicates that it is in the set of dictionary lookup
//                        chars.
//
//                        This function is intended for use by dictionary based
//                        break iterators.
//
//-------------------------------------------------------------------------------
UBool RuleBasedBreakIterator::isDictionaryChar(UChar32 c) {
    if (fData == NULL) {
        return FALSE;
    }
    uint16_t category;
    UTRIE_GET16(&fData->fTrie, c, category);
    return (category & 0x4000) != 0;
}


U_NAMESPACE_END


static icu::UStack *gLanguageBreakFactories = NULL;
static icu::UInitOnce gLanguageBreakFactoriesInitOnce = U_INITONCE_INITIALIZER;

/**
 * Release all static memory held by breakiterator.  
 */
U_CDECL_BEGIN
static UBool U_CALLCONV breakiterator_cleanup_dict(void) {
    if (gLanguageBreakFactories) {
        delete gLanguageBreakFactories;
        gLanguageBreakFactories = NULL;
    }
    gLanguageBreakFactoriesInitOnce.reset();
    return TRUE;
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV _deleteFactory(void *obj) {
    delete (icu::LanguageBreakFactory *) obj;
}
U_CDECL_END
U_NAMESPACE_BEGIN

static void U_CALLCONV initLanguageFactories() {
    UErrorCode status = U_ZERO_ERROR;
    U_ASSERT(gLanguageBreakFactories == NULL);
    gLanguageBreakFactories = new UStack(_deleteFactory, NULL, status);
    if (gLanguageBreakFactories != NULL && U_SUCCESS(status)) {
        ICULanguageBreakFactory *builtIn = new ICULanguageBreakFactory(status);
        gLanguageBreakFactories->push(builtIn, status);
#ifdef U_LOCAL_SERVICE_HOOK
        LanguageBreakFactory *extra = (LanguageBreakFactory *)uprv_svc_hook("languageBreakFactory", &status);
        if (extra != NULL) {
            gLanguageBreakFactories->push(extra, status);
        }
#endif
    }
    ucln_common_registerCleanup(UCLN_COMMON_BREAKITERATOR_DICT, breakiterator_cleanup_dict);
}


static const LanguageBreakEngine*
getLanguageBreakEngineFromFactory(UChar32 c, int32_t breakType)
{
    umtx_initOnce(gLanguageBreakFactoriesInitOnce, &initLanguageFactories);
    if (gLanguageBreakFactories == NULL) {
        return NULL;
    }
    
    int32_t i = gLanguageBreakFactories->size();
    const LanguageBreakEngine *lbe = NULL;
    while (--i >= 0) {
        LanguageBreakFactory *factory = (LanguageBreakFactory *)(gLanguageBreakFactories->elementAt(i));
        lbe = factory->getEngineFor(c, breakType);
        if (lbe != NULL) {
            break;
        }
    }
    return lbe;
}


//-------------------------------------------------------------------------------
//
//  getLanguageBreakEngine  Find an appropriate LanguageBreakEngine for the
//                          the character c.
//
//-------------------------------------------------------------------------------
const LanguageBreakEngine *
RuleBasedBreakIterator::getLanguageBreakEngine(UChar32 c) {
    const LanguageBreakEngine *lbe = NULL;
    UErrorCode status = U_ZERO_ERROR;
    
    if (fLanguageBreakEngines == NULL) {
        fLanguageBreakEngines = new UStack(status);
        if (fLanguageBreakEngines == NULL || U_FAILURE(status)) {
            delete fLanguageBreakEngines;
            fLanguageBreakEngines = 0;
            return NULL;
        }
    }
    
    int32_t i = fLanguageBreakEngines->size();
    while (--i >= 0) {
        lbe = (const LanguageBreakEngine *)(fLanguageBreakEngines->elementAt(i));
        if (lbe->handles(c, fBreakType)) {
            return lbe;
        }
    }
    
    // No existing dictionary took the character. See if a factory wants to
    // give us a new LanguageBreakEngine for this character.
    lbe = getLanguageBreakEngineFromFactory(c, fBreakType);
    
    // If we got one, use it and push it on our stack.
    if (lbe != NULL) {
        fLanguageBreakEngines->push((void *)lbe, status);
        // Even if we can't remember it, we can keep looking it up, so
        // return it even if the push fails.
        return lbe;
    }
    
    // No engine is forthcoming for this character. Add it to the
    // reject set. Create the reject break engine if needed.
    if (fUnhandledBreakEngine == NULL) {
        fUnhandledBreakEngine = new UnhandledEngine(status);
        if (U_SUCCESS(status) && fUnhandledBreakEngine == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        // Put it last so that scripts for which we have an engine get tried
        // first.
        fLanguageBreakEngines->insertElementAt(fUnhandledBreakEngine, 0, status);
        // If we can't insert it, or creation failed, get rid of it
        if (U_FAILURE(status)) {
            delete fUnhandledBreakEngine;
            fUnhandledBreakEngine = 0;
            return NULL;
        }
    }
    
    // Tell the reject engine about the character; at its discretion, it may
    // add more than just the one character.
    fUnhandledBreakEngine->handleCharacter(c, fBreakType);
        
    return fUnhandledBreakEngine;
}



int32_t RuleBasedBreakIterator::getBreakType() const {
    return fBreakType;
}

void RuleBasedBreakIterator::setBreakType(int32_t type) {
    fBreakType = type;
}


/*
 *  Break Cache Implementation
 */

RuleBasedBreakIterator::BreakCache::BreakCache(RuleBasedBreakIterator *This, UErrorCode &status) :
        fThis(This), fBreaks(NULL), fRawBreaks(NULL), fTagValues(NULL), fStatusIndex(0), fLastIndex(0) {
    fBreaks = new UVector32(status);
    fRawBreaks = new UVector32(status);
    fTagValues = new UVector32(status);
    if (U_FAILURE(status)) {
        return;
    }
    if (fBreaks == NULL|| fRawBreaks == NULL || fTagValues == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    reset();
}


RuleBasedBreakIterator::BreakCache::~BreakCache() {
    delete fBreaks;
    delete fRawBreaks;
    delete fTagValues;
}

void RuleBasedBreakIterator::BreakCache::reset() {
    fBreaks->removeAllElements();
    fTagValues->removeAllElements();
    fStatusIndex = 0;
    fLastIndex = 0;
}


int32_t RuleBasedBreakIterator::BreakCache::following(int32_t position) {
    if (fBreaks->size() == 0 ||
            position < fBreaks->elementAti(0) ||
            fBreaks->lastElementi() <= position) {
        return UBRK_DONE;
    }

    if (position == fBreaks->elementAti(0)) {
        fLastIndex = 1;
        return fBreaks->elementAti(fLastIndex);
    }

    if (position == fBreaks->elementAti(fLastIndex)) {
        ++fLastIndex;
        U_ASSERT(fLastIndex < fBreaks->size());
        return fBreaks->elementAti(fLastIndex);
    }

    for (fLastIndex = 1; fBreaks->elementAti(fLastIndex) < position; ++fLastIndex) {
        U_ASSERT(fLastIndex < fBreaks->size());
    };
    return fBreaks->elementAti(fLastIndex);
}


int32_t RuleBasedBreakIterator::BreakCache::preceding(int32_t position) {
    if (fBreaks->size() == 0 ||
            position <= fBreaks->elementAti(0) ||
            fBreaks->lastElementi() < position) {
        return UBRK_DONE;
    }

    if (position == fBreaks->elementAti(fLastIndex)) {
        --fLastIndex;
        U_ASSERT(fLastIndex >= 0);
        return fBreaks->elementAti(fLastIndex);
    }

    if (position == fBreaks->lastElementi()) {
        fLastIndex = fBreaks->size() - 2;
        U_ASSERT(fLastIndex >= 0);
        return fBreaks->elementAti(fLastIndex);
    }

    for (fLastIndex = fBreaks->size() - 2; fBreaks->elementAti(fLastIndex) >= position; --fLastIndex) {
        U_ASSERT(fLastIndex >= 0);
    }
    return fBreaks->elementAti(fLastIndex);
}

int32_t RuleBasedBreakIterator::BreakCache::getCurrentRuleStatus(void) {
  return fTagValues->elementAti(fLastIndex);
}

class DictTextRange {
  public:
    RuleBasedBreakIterator *fBreakIterator;
    int fRangeStart;    // Index of 1st char in a range, result of iteration of ranges.
    int fRangeLimit;    // Limit of range.
    int fDictStart;     // Index of first dictionary char in range.
    int fDictLimit;     // Dictionary limit: index following last dictionary char in range.
    int fFinalLimit;
    bool fUseDictionary;// True if the current range is a dictionary range.
    int fNonDictTagValue;
    const LanguageBreakEngine *fLBE;


    DictTextRange(RuleBasedBreakIterator *bi, int32_t start, int32_t limit, int32_t tagValue)
            : fBreakIterator(bi), fRangeStart(0), fRangeLimit(start), fFinalLimit(limit), fUseDictionary(false), fNonDictTagValue(tagValue) {};
    ~DictTextRange() {};

    UBool next();
};

UBool DictTextRange::next() {
    if (fRangeLimit >= fFinalLimit) {
        return FALSE;
    }
    fRangeStart = fRangeLimit;

    if (fRangeStart >= fFinalLimit) {
        fLBE = NULL;
        return FALSE;
    }

    // Advance to the first word-like dictionary character. Normally the first char.

    fDictStart = -1;
    int32_t category = 0;
    UChar32 c;
    UChar32 prevC;
    utext_setNativeIndex(fBreakIterator->fText, fRangeStart);
    int32_t index;
    while ((index = utext_getNativeIndex(fBreakIterator->fText)) < fFinalLimit) {
        prevC = c;
        c = utext_next32(fBreakIterator->fText);
        if (c == U_SENTINEL) {
            break;
        }
        UTRIE_GET16(&fBreakIterator->fData->fTrie, c, category);
        if ((category & 0x4000) != 0 && u_isalpha(c)) {
          // problem: breaks after parenthesis in line break -> problem for thai!
            fDictStart = index;
            break;
        }
    }

    if (fDictStart == -1) {
      // TODO(jchye): faulty because last range may not be handled correctly!
      // should split out next() and hasNext().
        fLBE = NULL;
        fRangeLimit = fFinalLimit;
        return TRUE;  // was false
    }

    // If there was a non-dictionary word range before the dictionary range, back up
    // and return the non-dictionary range first.
    if (fDictStart > fRangeStart && !u_ispunct(prevC)) {
      // TODO: fDictStart and fDictLimit should be removed since next() now
      // only returns a dict range or a non-dict range. Use fRangeStart and
      // fRangeLimit instead.
      // TODO 2: this breaks line-breaking! e.g. (thai-text will break after (
      // NOTE: if prevC was an extend char isbase == false. how to get around?
      // only ignore punctuation?
      fLBE = NULL;
      fDictStart = fRangeStart;
      c = utext_previous32(fBreakIterator->fText);
      fDictLimit = fRangeLimit = utext_getNativeIndex(fBreakIterator->fText);
      return TRUE;
    }

    fDictLimit = utext_getNativeIndex(fBreakIterator->fText);
    fUseDictionary = (category & 0x4000) != 0;
    if (!fUseDictionary) {
        // This range contains non-dictionary alphabetic chars.
        // The end of the range is either an occurrence of a dictionary character
        // or the overall end of the text identified by the rules.
        fLBE = NULL;
        while (utext_getNativeIndex(fBreakIterator->fText) < fFinalLimit) {
            c = utext_next32(fBreakIterator->fText);
            if (c == U_SENTINEL) {
                U_ASSERT(FALSE);  // Should not happen.
                break;
            }
            UTRIE_GET16(&fBreakIterator->fData->fTrie, c, category);
            if ((category & 0x4000) == 0) {
                fDictLimit = utext_getNativeIndex(fBreakIterator->fText);
            } else {
                // Hit any base dictionary character; this terminates this non-dictionary range.
                // Back up to last char in this range.
                c = utext_previous32(fBreakIterator->fText);
                break;
            }
        }
    } else {
        // This range will contain all contiguous chars belonging to 
        //    the same break engine plus any non-dictionary, non-alphabetic characters.
        //  TODO: probably need to close dictionary sets over normalization forms.
        fLBE = fBreakIterator->getLanguageBreakEngine(c);

        while (utext_getNativeIndex(fBreakIterator->fText) < fFinalLimit) {
            c = utext_next32(fBreakIterator->fText);
            if (c == U_SENTINEL) {
                U_ASSERT(FALSE);  // Should not happen.
                break;
            }
            UTRIE_GET16(&fBreakIterator->fData->fTrie, c, category);
            // TODO: add || u_isextend(c) or equiv function?
            //if ((category & 0x4000) != 0/* || u_isbase(c)*/) {   // It's a word-like character
              // may not be able to just remove dictionary char check. (breaks line segmentation)
                if (fLBE->handles(c, fBreakIterator->fBreakType)) {
                    // Character belongs to language break engine for this range.
                    fDictLimit = utext_getNativeIndex(fBreakIterator->fText);
                } else if (u_isbase(c)) { //fBreakIterator->fBreakType == UBRK_WORD
                  // TODO: should thai chars be separated from numbers?
                    // Character doesn't belong to this range.
                    // It's either a dictionary char belonging to a different language break engine,
                    // or a non-dictionary alphabetic.
                    // Back up one; this character will go with the next range.
                    c = utext_previous32(fBreakIterator->fText);
                    break;
                }
                /*
            } else if (u_isbase(c)) {  // Base non-dictionary character.
              // problem: isbase includes modifier letters like ff9e
                // Hit any non-dictionary character; this terminates this non-dictionary range.
                // Back up to last char in this range.
                c = utext_previous32(fBreakIterator->fText);
                break;
            }
            */
        }
    }

    fRangeLimit = utext_getNativeIndex(fBreakIterator->fText);
    U_ASSERT(fRangeLimit <= fFinalLimit);
    return TRUE;
}
    
        
// Note: a region of text identified by break rules may contain both dictionary and non-dictionary chars.
//       There must be no break between leading non-dictionary chars and the first dictionary word.
//       There must be no break between trailing non-dicitionary chars and the last dictionary word.
//       These are things like opening and closing punctuation in line break.
//       Non-dictionary chars in the interior of a region are handed to the language (dictionary)
//       breaker, to do with as it will.

void RuleBasedBreakIterator::BreakCache::populate(int32_t start, int32_t limit, int32_t previousTagValue, int32_t defaultTagValue, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fBreaks->removeAllElements();
    fBreaks->addElement(start, status);
    fTagValues->removeAllElements();
    fTagValues->addElement(previousTagValue, status);

    DictTextRange  range(fThis, start, limit, defaultTagValue);
    while (range.next()) {
        // Convention: ranges do not add a break at their start.
        //             They do add a break at their end.
        //             End of one range will always be the start of the next.
        addContainedWords(range, status);
    }

    U_ASSERT(fBreaks->lastElementi() == limit);
}

void RuleBasedBreakIterator::BreakCache::addContainedWords(const DictTextRange &range, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    // Fill fRawBreaks with the word boundaries from the break engine 
    // for the dictionary characters within this range.

    fRawBreaks->removeAllElements();
    utext_setNativeIndex(fThis->fText, range.fDictStart);
    uint32_t tagValue = -1;
    if (range.fLBE != NULL) {
        range.fLBE->findBreaks(fThis->fText, range.fDictLimit, fThis->fBreakType, *fRawBreaks, status);
        if (range.fUseDictionary) {
          tagValue = range.fLBE->getTagValue(fThis->fBreakType);
        } else {
          tagValue = range.fNonDictTagValue;
        }
    } else {
        // Range containing non-dictionary characters only.
        // Has boundaries at its start and end only.
      // TODO: use dict or range limits?
        fRawBreaks->addElement(range.fDictStart, status);
        fRawBreaks->addElement(range.fDictLimit, status);
        tagValue = range.fNonDictTagValue;
    }

    // Break engine should always show a break at the start and end
    // of the region it processed.
    U_ASSERT(fRawBreaks->size() > 0);
    U_ASSERT(fRawBreaks->elementAti(0) == range.fDictStart);
    U_ASSERT(fRawBreaks->lastElementi() == range.fDictLimit);
        
    // Append the boundaries for this range to fBreaks.
    // Within a single range there should be no duplicate boundaries.
    // Omit the first boundary; it is added by the outer range loop in BreakCache::populate().
    // Omit the last, adding fRangeLimit instead. This has the effect of concatenating any
    //   trailing non-dictionary characters to the last word; happens with line break where,
    //   for example, closing punctuation attaches to a word.
    // TODO(jchye): this poses a problem if the dictionary word is followed by
    // non-dict alphabetic characters: should be split up but they're put together!
    // can't just add a break because of possible joiners before dictionary segment.

    U_ASSERT(fRawBreaks->elementAti(0) == range.fDictStart);
    for (int32_t i=1; i<fRawBreaks->size()-1; ++i) {
        int32_t boundary = fRawBreaks->elementAti(i);
        U_ASSERT(boundary > fBreaks->lastElementi());
        if (fBreaks->size() == 0 || boundary > fBreaks->lastElementi()) {
            add(boundary, tagValue, status);
        }
    }
    U_ASSERT(fBreaks->lastElementi() < range.fRangeLimit);
    // Add the default tag value if there are non-dictionary chars at
    // the end because the segment's tag value may be changed.
    if (fRawBreaks->lastElementi() < range.fRangeLimit) {
      tagValue = range.fNonDictTagValue;
    }
    add(range.fRangeLimit, tagValue, status);
}

void RuleBasedBreakIterator::BreakCache::add(const int32_t index, const int32_t tagValue, UErrorCode &status) {
    fBreaks->addElement(index, status);
    fTagValues->addElement(tagValue, status);
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
