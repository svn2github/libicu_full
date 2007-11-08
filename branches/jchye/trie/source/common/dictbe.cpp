/**
 **********************************************************************************
 * Copyright (C) 2006,2007, International Business Machines Corporation and others. 
 * All Rights Reserved.                                                        
 **********************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "brkeng.h"
#include "dictbe.h"
#include "unicode/uniset.h"
#include "unicode/chariter.h"
#include "unicode/ubrk.h"
#include "uvector.h"
#include "triedict.h"
#include "uassert.h"
#include "unicode/normlzr.h"

#include <stdio.h>

U_NAMESPACE_BEGIN

/*
 ******************************************************************
 */

/*DictionaryBreakEngine::DictionaryBreakEngine() {
    fTypes = 0;
}*/

DictionaryBreakEngine::DictionaryBreakEngine(uint32_t breakTypes) {
    fTypes = breakTypes;
}

DictionaryBreakEngine::~DictionaryBreakEngine() {
}

UBool
DictionaryBreakEngine::handles(UChar32 c, int32_t breakType) const {
    return (breakType >= 0 && breakType < 32 && (((uint32_t)1 << breakType) & fTypes)
            && fSet.contains(c));
}

int32_t
DictionaryBreakEngine::findBreaks( UText *text,
                                 int32_t startPos,
                                 int32_t endPos,
                                 UBool reverse,
                                 int32_t breakType,
                                 UStack &foundBreaks ) const {
    int32_t result = 0;

    // Find the span of characters included in the set.
    int32_t start = (int32_t)utext_getNativeIndex(text);
    int32_t current;
    int32_t rangeStart;
    int32_t rangeEnd;
    UChar32 c = utext_current32(text);
    if (reverse) {
        UBool   isDict = fSet.contains(c);
        while((current = (int32_t)utext_getNativeIndex(text)) > startPos && isDict) {
            c = utext_previous32(text);
            isDict = fSet.contains(c);
        }
        rangeStart = (current < startPos) ? startPos : current+(isDict ? 0 : 1);
        rangeEnd = start + 1;
    }
    else {
        while((current = (int32_t)utext_getNativeIndex(text)) < endPos && fSet.contains(c)) {
            utext_next32(text);         // TODO:  recast loop for postincrement
            c = utext_current32(text);
        }
        rangeStart = start;
        rangeEnd = current;
    }
    if (breakType >= 0 && breakType < 32 && (((uint32_t)1 << breakType) & fTypes)) {
        result = divideUpDictionaryRange(text, rangeStart, rangeEnd, foundBreaks);
        utext_setNativeIndex(text, current);
    }
    
    return result;
}

void
DictionaryBreakEngine::setCharacters( const UnicodeSet &set ) {
    fSet = set;
    // Compact for caching
    fSet.compact();
}

/*void
DictionaryBreakEngine::setBreakTypes( uint32_t breakTypes ) {
    fTypes = breakTypes;
}*/

/*
 ******************************************************************
 */


// Helper class for improving readability of the Thai word break
// algorithm. The implementation is completely inline.

// List size, limited by the maximum number of words in the dictionary
// that form a nested sequence.
#define POSSIBLE_WORD_LIST_MAX 20

class PossibleWord {
 private:
  // list of word candidate lengths, in increasing length order
  int32_t   lengths[POSSIBLE_WORD_LIST_MAX];
  int       count;      // Count of candidates
  int32_t   prefix;     // The longest match with a dictionary word
  int32_t   offset;     // Offset in the text of these candidates
  int       mark;       // The preferred candidate's offset
  int       current;    // The candidate we're currently looking at

 public:
  PossibleWord();
  ~PossibleWord();
  
  // Fill the list of candidates if needed, select the longest, and return the number found
  int       candidates( UText *text, const TrieWordDictionary *dict, int32_t rangeEnd );
  
  // Select the currently marked candidate, point after it in the text, and invalidate self
  int32_t   acceptMarked( UText *text );
  
  // Back up from the current candidate to the next shorter one; return TRUE if that exists
  // and point the text after it
  UBool     backUp( UText *text );
  
  // Return the longest prefix this candidate location shares with a dictionary word
  int32_t   longestPrefix();
  
  // Mark the current candidate as the one we like
  void      markCurrent();
};

inline
PossibleWord::PossibleWord() {
    offset = -1;
}

inline
PossibleWord::~PossibleWord() {
}

inline int
PossibleWord::candidates( UText *text, const TrieWordDictionary *dict, int32_t rangeEnd ) {
    // TODO: If getIndex is too slow, use offset < 0 and add discardAll()
    int32_t start = (int32_t)utext_getNativeIndex(text);
    if (start != offset) {
        offset = start;
        prefix = dict->matches(text, rangeEnd-start, lengths, count, sizeof(lengths)/sizeof(lengths[0]));
        // Dictionary leaves text after longest prefix, not longest word. Back up.
        if (count <= 0) {
            utext_setNativeIndex(text, start);
        }
    }
    if (count > 0) {
        utext_setNativeIndex(text, start+lengths[count-1]);
    }
    current = count-1;
    mark = current;
    return count;
}

inline int32_t
PossibleWord::acceptMarked( UText *text ) {
    utext_setNativeIndex(text, offset + lengths[mark]);
    return lengths[mark];
}

inline UBool
PossibleWord::backUp( UText *text ) {
    if (current > 0) {
        utext_setNativeIndex(text, offset + lengths[--current]);
        return TRUE;
    }
    return FALSE;
}

inline int32_t
PossibleWord::longestPrefix() {
    return prefix;
}

inline void
PossibleWord::markCurrent() {
    mark = current;
}

// How many words in a row are "good enough"?
#define THAI_LOOKAHEAD 3

// Will not combine a non-word with a preceding dictionary word longer than this
#define THAI_ROOT_COMBINE_THRESHOLD 3

// Will not combine a non-word that shares at least this much prefix with a
// dictionary word, with a preceding word
#define THAI_PREFIX_COMBINE_THRESHOLD 3

// Ellision character
#define THAI_PAIYANNOI 0x0E2F

// Repeat character
#define THAI_MAIYAMOK 0x0E46

// Minimum word size
#define THAI_MIN_WORD 2

// Minimum number of characters for two words
#define THAI_MIN_WORD_SPAN (THAI_MIN_WORD * 2)

ThaiBreakEngine::ThaiBreakEngine(const TrieWordDictionary *adoptDictionary, UErrorCode &status)
    : DictionaryBreakEngine((1<<UBRK_WORD) | (1<<UBRK_LINE)),
      fDictionary(adoptDictionary)
{
    fThaiWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Thai:]&[:LineBreak=SA:]]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fThaiWordSet);
    }
    fMarkSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Thai:]&[:LineBreak=SA:]&[:M:]]"), status);
    fEndWordSet = fThaiWordSet;
    fEndWordSet.remove(0x0E31);             // MAI HAN-AKAT
    fEndWordSet.remove(0x0E40, 0x0E44);     // SARA E through SARA AI MAIMALAI
    fBeginWordSet.add(0x0E01, 0x0E2E);      // KO KAI through HO NOKHUK
    fBeginWordSet.add(0x0E40, 0x0E44);      // SARA E through SARA AI MAIMALAI
    fSuffixSet.add(THAI_PAIYANNOI);
    fSuffixSet.add(THAI_MAIYAMOK);

    // Compact for caching.
    fMarkSet.compact();
    fEndWordSet.compact();
    fBeginWordSet.compact();
    fSuffixSet.compact();
}

ThaiBreakEngine::~ThaiBreakEngine() {
    delete fDictionary;
}

int32_t
ThaiBreakEngine::divideUpDictionaryRange( UText *text,
                                                int32_t rangeStart,
                                                int32_t rangeEnd,
                                                UStack &foundBreaks ) const {
    if ((rangeEnd - rangeStart) < THAI_MIN_WORD_SPAN) {
        return 0;       // Not enough characters for two words
    }

    uint32_t wordsFound = 0;
    int32_t wordLength;
    int32_t current;
    UErrorCode status = U_ZERO_ERROR;
    PossibleWord words[THAI_LOOKAHEAD];
    UChar32 uc;
    
    utext_setNativeIndex(text, rangeStart);
    
    while (U_SUCCESS(status) && (current = (int32_t)utext_getNativeIndex(text)) < rangeEnd) {
        wordLength = 0;

        // Look for candidate words at the current position
        int candidates = words[wordsFound%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
        
        // If we found exactly one, use that
        if (candidates == 1) {
            wordLength = words[wordsFound%THAI_LOOKAHEAD].acceptMarked(text);
            wordsFound += 1;
        }
        
        // If there was more than one, see which one can take us forward the most words
        else if (candidates > 1) {
            // If we're already at the end of the range, we're done
            if ((int32_t)utext_getNativeIndex(text) >= rangeEnd) {
                goto foundBest;
            }
            do {
                int wordsMatched = 1;
                if (words[(wordsFound+1)%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) > 0) {
                    if (wordsMatched < 2) {
                        // Followed by another dictionary word; mark first word as a good candidate
                        words[wordsFound%THAI_LOOKAHEAD].markCurrent();
                        wordsMatched = 2;
                    }
                    
                    // If we're already at the end of the range, we're done
                    if ((int32_t)utext_getNativeIndex(text) >= rangeEnd) {
                        goto foundBest;
                    }
                    
                    // See if any of the possible second words is followed by a third word
                    do {
                        // If we find a third word, stop right away
                        if (words[(wordsFound+2)%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd)) {
                            words[wordsFound%THAI_LOOKAHEAD].markCurrent();
                            goto foundBest;
                        }
                    }
                    while (words[(wordsFound+1)%THAI_LOOKAHEAD].backUp(text));
                }
            }
            while (words[wordsFound%THAI_LOOKAHEAD].backUp(text));
foundBest:
            wordLength = words[wordsFound%THAI_LOOKAHEAD].acceptMarked(text);
            wordsFound += 1;
        }
        
        // We come here after having either found a word or not. We look ahead to the
        // next word. If it's not a dictionary word, we will combine it withe the word we
        // just found (if there is one), but only if the preceding word does not exceed
        // the threshold.
        // The text iterator should now be positioned at the end of the word we found.
        if ((int32_t)utext_getNativeIndex(text) < rangeEnd && wordLength < THAI_ROOT_COMBINE_THRESHOLD) {
            // if it is a dictionary word, do nothing. If it isn't, then if there is
            // no preceding word, or the non-word shares less than the minimum threshold
            // of characters with a dictionary word, then scan to resynchronize
            if (words[wordsFound%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) <= 0
                  && (wordLength == 0
                      || words[wordsFound%THAI_LOOKAHEAD].longestPrefix() < THAI_PREFIX_COMBINE_THRESHOLD)) {
                // Look for a plausible word boundary
                //TODO: This section will need a rework for UText.
                int32_t remaining = rangeEnd - (current+wordLength);
                UChar32 pc = utext_current32(text);
                int32_t chars = 0;
                for (;;) {
                    utext_next32(text);
                    uc = utext_current32(text);
                    // TODO: Here we're counting on the fact that the SA languages are all
                    // in the BMP. This should get fixed with the UText rework.
                    chars += 1;
                    if (--remaining <= 0) {
                        break;
                    }
                    if (fEndWordSet.contains(pc) && fBeginWordSet.contains(uc)) {
                        // Maybe. See if it's in the dictionary.
                        // NOTE: In the original Apple code, checked that the next
                        // two characters after uc were not 0x0E4C THANTHAKHAT before
                        // checking the dictionary. That is just a performance filter,
                        // but it's not clear it's faster than checking the trie.
                        int candidates = words[(wordsFound+1)%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd);
                        utext_setNativeIndex(text, current+wordLength+chars);
                        if (candidates > 0) {
                            break;
                        }
                    }
                    pc = uc;
                }
                
                // Bump the word count if there wasn't already one
                if (wordLength <= 0) {
                    wordsFound += 1;
                }
                
                // Update the length with the passed-over characters
                wordLength += chars;
            }
            else {
                // Back up to where we were for next iteration
                utext_setNativeIndex(text, current+wordLength);
            }
        }
        
        // Never stop before a combining mark.
        int32_t currPos;
        while ((currPos = (int32_t)utext_getNativeIndex(text)) < rangeEnd && fMarkSet.contains(utext_current32(text))) {
            utext_next32(text);
            wordLength += (int32_t)utext_getNativeIndex(text) - currPos;
        }
        
        // Look ahead for possible suffixes if a dictionary word does not follow.
        // We do this in code rather than using a rule so that the heuristic
        // resynch continues to function. For example, one of the suffix characters
        // could be a typo in the middle of a word.
        if ((int32_t)utext_getNativeIndex(text) < rangeEnd && wordLength > 0) {
            if (words[wordsFound%THAI_LOOKAHEAD].candidates(text, fDictionary, rangeEnd) <= 0
                && fSuffixSet.contains(uc = utext_current32(text))) {
                if (uc == THAI_PAIYANNOI) {
                    if (!fSuffixSet.contains(utext_previous32(text))) {
                        // Skip over previous end and PAIYANNOI
                        utext_next32(text);
                        utext_next32(text);
                        wordLength += 1;            // Add PAIYANNOI to word
                        uc = utext_current32(text);     // Fetch next character
                    }
                    else {
                        // Restore prior position
                        utext_next32(text);
                    }
                }
                if (uc == THAI_MAIYAMOK) {
                    if (utext_previous32(text) != THAI_MAIYAMOK) {
                        // Skip over previous end and MAIYAMOK
                        utext_next32(text);
                        utext_next32(text);
                        wordLength += 1;            // Add MAIYAMOK to word
                    }
                    else {
                        // Restore prior position
                        utext_next32(text);
                    }
                }
            }
            else {
                utext_setNativeIndex(text, current+wordLength);
            }
        }
        
        // Did we find a word on this iteration? If so, push it on the break stack
        if (wordLength > 0) {
            foundBreaks.push((current+wordLength), status);
        }
    }
    
    // Don't return a break for the end of the dictionary range if there is one there.
    if (foundBreaks.peeki() >= rangeEnd) {
        (void) foundBreaks.popi();
        wordsFound -= 1;
    }

    return wordsFound;
}

/*
 ******************************************************************
 * CjkBreakEngine
 */
static const uint32_t kuint32max = 0xFFFFFFFF;
CjkBreakEngine::CjkBreakEngine(const TrieWordDictionary *adoptDictionary, LanguageType type, UErrorCode &status)
        : DictionaryBreakEngine(1<<UBRK_WORD), fDictionary(adoptDictionary){
    if (!adoptDictionary->getValued()) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    
    // Korean dictionary only includes Hangul syllables
    fHangulWordSet.applyPattern(UNICODE_STRING_SIMPLE("[\\uac00-\\ud7a3]"), status);
    fHanWordSet.applyPattern(UNICODE_STRING_SIMPLE("[:Han:]"), status);
    fKatakanaWordSet.applyPattern(UNICODE_STRING_SIMPLE("[[:Katakana:]\\u30fc\\uff9e\\uff9f]"), status);
    fHiraganaWordSet.applyPattern(UNICODE_STRING_SIMPLE("[:Hiragana:]"), status);
    
    if (U_SUCCESS(status)) {
        // handle Korean and Japanese/Chinese using different dictionaries
        if (type == kKorean) {
            setCharacters(fHangulWordSet);
        } else { //Chinese and Japanese
            UnicodeSet cjSet;
            cjSet.addAll(fHanWordSet);
            cjSet.addAll(fKatakanaWordSet);
            cjSet.addAll(fHiraganaWordSet);
            setCharacters(cjSet);
        }
    }
}

CjkBreakEngine::~CjkBreakEngine(){
    delete fDictionary;
}

#if 0
int32_t
CjkBreakEngine::findBreaks( UText *text,
                                 int32_t startPos,
                                 int32_t endPos,
                                 UBool reverse,
                                 int32_t breakType,
                                 UStack &foundBreaks ) const {
    /*
    UChar *composedText;
    UErrorCode status = U_ZERO_ERROR;
    utext_extract(text, startPos, endPos, composedText, endPos-startPos, &status);
    Normalizer n(composedText, UNORM_DEFAULT);
    */
//    Normalize::normalize(composedText, COMPOSE_COMPAT, IGNORE_HANGUL, result, &status);
    
    return DictionaryBreakEngine::findBreaks(text, startPos, endPos, reverse, breakType, foundBreaks);
}
#endif
// Halfwidth to Fullwidth table
// All halfwidth characters will be treated as fullwidth characters in
// CjkBreakEngine.
// copied from http://www.unicode.org/charts/PDF/UFF00.pdf
static const uint16_t CJKHalfwidthToFullwidth[] = {
    0xFF00u, /* FF00 */
    0x0021u, /* FF01 */
    0x0022u, /* FF02 */
    0x0023u, /* FF03 */
    0x0024u, /* FF04 */
    0x0025u, /* FF05 */
    0x0026u, /* FF06 */
    0x0027u, /* FF07 */
    0x0028u, /* FF08 */
    0x0029u, /* FF09 */
    0x002Au, /* FF0A */
    0x002Bu, /* FF0B */
    0x002Cu, /* FF0C */
    0x002Du, /* FF0D */
    0x002Eu, /* FF0E */
    0x002Fu, /* FF0F */
    0x0030u, /* FF10 */
    0x0031u, /* FF11 */
    0x0032u, /* FF12 */
    0x0033u, /* FF13 */
    0x0034u, /* FF14 */
    0x0035u, /* FF15 */
    0x0036u, /* FF16 */
    0x0037u, /* FF17 */
    0x0038u, /* FF18 */
    0x0039u, /* FF19 */
    0x003Au, /* FF1A */
    0x003Bu, /* FF1B */
    0x003Cu, /* FF1C */
    0x003Du, /* FF1D */
    0x003Eu, /* FF1E */
    0x003Fu, /* FF1F */
    0x0040u, /* FF20 */
    0x0041u, /* FF21 */
    0x0042u, /* FF22 */
    0x0043u, /* FF23 */
    0x0044u, /* FF24 */
    0x0045u, /* FF25 */
    0x0046u, /* FF26 */
    0x0047u, /* FF27 */
    0x0048u, /* FF28 */
    0x0049u, /* FF29 */
    0x004Au, /* FF2A */
    0x004Bu, /* FF2B */
    0x004Cu, /* FF2C */
    0x004Du, /* FF2D */
    0x004Eu, /* FF2E */
    0x004Fu, /* FF2F */
    0x0050u, /* FF30 */
    0x0051u, /* FF31 */
    0x0052u, /* FF32 */
    0x0053u, /* FF33 */
    0x0054u, /* FF34 */
    0x0055u, /* FF35 */
    0x0056u, /* FF36 */
    0x0057u, /* FF37 */
    0x0058u, /* FF38 */
    0x0059u, /* FF39 */
    0x005Au, /* FF3A */
    0x005Bu, /* FF3B */
    0x005Cu, /* FF3C */
    0x005Du, /* FF3D */
    0x005Eu, /* FF3E */
    0x005Fu, /* FF3F */
    0x0060u, /* FF40 */
    0x0061u, /* FF41 */
    0x0062u, /* FF42 */
    0x0063u, /* FF43 */
    0x0064u, /* FF44 */
    0x0065u, /* FF45 */
    0x0066u, /* FF46 */
    0x0067u, /* FF47 */
    0x0068u, /* FF48 */
    0x0069u, /* FF49 */
    0x006Au, /* FF4A */
    0x006Bu, /* FF4B */
    0x006Cu, /* FF4C */
    0x006Du, /* FF4D */
    0x006Eu, /* FF4E */
    0x006Fu, /* FF4F */
    0x0070u, /* FF50 */
    0x0071u, /* FF51 */
    0x0072u, /* FF52 */
    0x0073u, /* FF53 */
    0x0074u, /* FF54 */
    0x0075u, /* FF55 */
    0x0076u, /* FF56 */
    0x0077u, /* FF57 */
    0x0078u, /* FF58 */
    0x0079u, /* FF59 */
    0x007Au, /* FF5A */
    0x007Bu, /* FF5B */
    0x007Cu, /* FF5C */
    0x007Du, /* FF5D */
    0x007Eu, /* FF5E */
    0x2985u, /* FF5F */
    0x2986u, /* FF60 */
    0x3002u, /* FF61 */
    0x300Cu, /* FF62 */
    0x300Du, /* FF63 */
    0x3001u, /* FF64 */
    0x30FBu, /* FF65 */
    0x30F2u, /* FF66 */
    0x30A1u, /* FF67 */
    0x30A3u, /* FF68 */
    0x30A5u, /* FF69 */
    0x30A7u, /* FF6A */
    0x30A9u, /* FF6B */
    0x30E3u, /* FF6C */
    0x30E5u, /* FF6D */
    0x30E7u, /* FF6E */
    0x30C3u, /* FF6F */
    0x30FCu, /* FF70 */
    0x30A2u, /* FF71 */
    0x30A4u, /* FF72 */
    0x30A6u, /* FF73 */
    0x30A8u, /* FF74 */
    0x30AAu, /* FF75 */
    0x30ABu, /* FF76 */
    0x30ADu, /* FF77 */
    0x30AFu, /* FF78 */
    0x30B1u, /* FF79 */
    0x30B3u, /* FF7A */
    0x30B5u, /* FF7B */
    0x30B7u, /* FF7C */
    0x30B9u, /* FF7D */
    0x30BBu, /* FF7E */
    0x30BDu, /* FF7F */
    0x30BFu, /* FF80 */
    0x30C1u, /* FF81 */
    0x30C4u, /* FF82 */
    0x30C6u, /* FF83 */
    0x30C8u, /* FF84 */
    0x30CAu, /* FF85 */
    0x30CBu, /* FF86 */
    0x30CCu, /* FF87 */
    0x30CDu, /* FF88 */
    0x30CEu, /* FF89 */
    0x30CFu, /* FF8A */
    0x30D2u, /* FF8B */
    0x30D5u, /* FF8C */
    0x30D8u, /* FF8D */
    0x30DBu, /* FF8E */
    0x30DEu, /* FF8F */
    0x30DFu, /* FF90 */
    0x30E0u, /* FF91 */
    0x30E1u, /* FF92 */
    0x30E2u, /* FF93 */
    0x30E4u, /* FF94 */
    0x30E6u, /* FF95 */
    0x30E8u, /* FF96 */
    0x30E9u, /* FF97 */
    0x30EAu, /* FF98 */
    0x30EBu, /* FF99 */
    0x30ECu, /* FF9A */
    0x30EDu, /* FF9B */
    0x30EFu, /* FF9C */
    0x30F3u, /* FF9D */
    0x3099u, /* FF9E */
    0x309Au, /* FF9F */
    0x3164u, /* FFA0 */
    0x3131u, /* FFA1 */
    0x3132u, /* FFA2 */
    0x3133u, /* FFA3 */
    0x3134u, /* FFA4 */
    0x3135u, /* FFA5 */
    0x3136u, /* FFA6 */
    0x3137u, /* FFA7 */
    0x3138u, /* FFA8 */
    0x3139u, /* FFA9 */
    0x313Au, /* FFAA */
    0x313Bu, /* FFAB */
    0x313Cu, /* FFAC */
    0x313Du, /* FFAD */
    0x313Eu, /* FFAE */
    0x313Fu, /* FFAF */
    0x3140u, /* FFB0 */
    0x3141u, /* FFB1 */
    0x3142u, /* FFB2 */
    0x3143u, /* FFB3 */
    0x3144u, /* FFB4 */
    0x3145u, /* FFB5 */
    0x3146u, /* FFB6 */
    0x3147u, /* FFB7 */
    0x3148u, /* FFB8 */
    0x3149u, /* FFB9 */
    0x314Au, /* FFBA */
    0x314Bu, /* FFBB */
    0x314Cu, /* FFBC */
    0x314Du, /* FFBD */
    0x314Eu, /* FFBE */
    0xFFBFu, /* FFBF */
    0xFFC0u, /* FFC0 */
    0xFFC1u, /* FFC1 */
    0x314Fu, /* FFC2 */
    0x3150u, /* FFC3 */
    0x3151u, /* FFC4 */
    0x3152u, /* FFC5 */
    0x3153u, /* FFC6 */
    0x3154u, /* FFC7 */
    0xFFC8u, /* FFC8 */
    0xFFC9u, /* FFC9 */
    0x3155u, /* FFCA */
    0x3156u, /* FFCB */
    0x3157u, /* FFCC */
    0x3158u, /* FFCD */
    0x3159u, /* FFCE */
    0x315Au, /* FFCF */
    0xFFD0u, /* FFD0 */
    0xFFD1u, /* FFD1 */
    0x315Bu, /* FFD2 */
    0x315Cu, /* FFD3 */
    0x315Du, /* FFD4 */
    0x315Eu, /* FFD5 */
    0x315Fu, /* FFD6 */
    0x3160u, /* FFD7 */
    0xFFD8u, /* FFD8 */
    0xFFD9u, /* FFD9 */
    0x3161u, /* FFDA */
    0x3162u, /* FFDB */
    0x3163u, /* FFDC */
    0xFFDDu, /* FFDD */
    0xFFDEu, /* FFDE */
    0xFFDFu, /* FFDF */
    0x00A2u, /* FFE0 */
    0x00A3u, /* FFE1 */
    0x00ACu, /* FFE2 */
    0x00AFu, /* FFE3 */
    0x00A6u, /* FFE4 */
    0x00A5u, /* FFE5 */
    0x20A9u, /* FFE6 */
    0xFFE7u, /* FFE7 */
    0x2502u, /* FFE8 */
    0x2190u, /* FFE9 */
    0x2191u, /* FFEA */
    0x2192u, /* FFEB */
    0x2193u, /* FFEC */
    0x25A0u, /* FFED */
    0x25CBu, /* FFEE */
    0xFFEFu  /* FFEF */
};

// returns a normalized code point
static inline uint16_t NormalizeUnicodeCharacter(uint16_t value) {
  // range FF00-FFEF: Halwidth and Fullwidth Form
  if (value >= 0xFF00 && value <= 0xFFEF) {
    return CJKHalfwidthToFullwidth[value - 0xFF00];
  }
  return value;
}
#if 0
// Returns one of UTF8_OTHERS, or the actual
// unicode value (if UTF8_3BYTE).  Also advances to the next UTF8 char.
//TODO: modify for CjkBreakEngine!
// Precondition: *buf_utf8_ptr < end.
// Postcondition: *buf_utf8_ptr <= end.
inline uint16_t UnicodeFor3ByteUTF8CharAndAdvance(const char **buf_utf8_ptr,
                                         const char *end) {
const char* utf8_str = *buf_utf8_ptr;
int byte = utf8_str[0];
uint16 value = 0u;
// According to UTF-8 Bit Distribution at www.unicode.org, below are the
// byte representations for UTF-8 encoded chars:
// (see http://www.unicode.org/versions/Unicode4.0.0/ch03.pdf, under
//  3.9 Unicode Encoding Forms, UTF-8)
// (see also "man -S7 utf8")
if (value < 0xFF41u) {
    return NormalizeUnicodeCharacter(value);
} else {
    // normalize unicode character e.g., halfwidth to fullwidth
    uint16 normalized_value = NormalizeUnicodeCharacter(value);

    // We must check if the next character is Halfwidth Katakana
    // Voiced Sound Mark (U+FF9E) or Halfwidth Katakana
    // Semi-Voiced Sound Mark (U+FF9F).  If that's the
    // case, we consume the next halfwidth katakana too and return
    // a corresponding single fullwidth katakana.
    //
    // See http://www.unicode.org/charts/PDF/U3040.pdf
    //     (The voiced- and semi-voiced sound marks are in Hiragana table)
    //     http://www.unicode.org/charts/PDF/U30A0.pdf
    //     (Fullwidth katakana table)
    //     http://www.unicode.org/charts/PDF/UFF00.pdf
    //     (Halfwidth katakana table)
    if (*buf_utf8_ptr + 3 <= end) {
        if ((*buf_utf8_ptr)[0] == 0xEF && (*buf_utf8_ptr)[1] == 0xBE) {
            if ((*buf_utf8_ptr)[2] == 0x9E) {
                // Each of the following halfwidths (1st char) can take a voiced
                // sound mark, and the the two katakanas (the first
                // halfwidth and the following voiced sound mark (2nd char))
                // together should be converted into a single fullwidth
                // counterpart.
                if (value == 0xFF73u) {
                    // U
                    normalized_value = 0x30F4;
                    (*buf_utf8_ptr) += 3;
                } else if (value < 0xFF76u) {
                    ;
                } else if (value <= 0xFF84u) {
                    // Ka--To
                    normalized_value++;
                    (*buf_utf8_ptr) += 3;
                } else if (value < 0xFF8Au) {
                    ;
                } else if (value <= 0xFF8Eu) {
                    // Ha--Ho
                    normalized_value++;
                    (*buf_utf8_ptr) += 3;
                } else if (value == 0xFF9Cu) {
                    // Wa
                    normalized_value = 0x30F7;
                    (*buf_utf8_ptr) += 3;
                }
            } else if ((*buf_utf8_ptr)[2] == 0x9F) {
                // Each of the following halfwidths can take a
                // semi-voiced sound mark, and the the two katakanas
                // (the first halfwidth and the following semi-voiced
                // sound mark) together should be converted into a
                // single fullwidth counterpart.
                if (value >= 0xFF8Au && value <= 0xFF8Eu) {
                    // Ha--Ho
                    normalized_value += 2;
                    (*buf_utf8_ptr) += 3;
                }
            }
        }
    }
    return normalized_value;
}
#endif

// The code below is generated by ./katakana_heuristics_codegen.py (TODO: fix)
static const int kMaxKatakanaLength = 8;
static const int kMaxKatakanaGroupLength = 20;
static const uint32_t maxSnlp = 255;

static inline uint32_t getKatakanaCost(int wordLength){
	//TODO: fill array with actual values from dictionary!
	static const uint32_t katakanaCost[kMaxKatakanaLength + 1]
	    = {8192, 1104, 468, 192, 156, 276, 348, 444, 576};
	return (wordLength > kMaxKatakanaLength) ? 8192 : katakanaCost[wordLength];
}


static inline bool isKatakana(uint16_t value) {
  return (value >= 0x30A1u && value <= 0x30FEu && value != 0x30FBu) ||
         (value >= 0xFF66u && value <= 0xFF9fu);
}

/*
 * @param text A UText representing the text
 * @param rangeStart The start of the range of dictionary characters
 * @param rangeEnd The end of the range of dictionary characters
 * @param foundBreaks Output of C array of int32_t break positions, or 0
 * @return The number of breaks found
 */
int32_t CjkBreakEngine::divideUpDictionaryRange( UText *text,
                                         int32_t rangeStart,
                                         int32_t rangeEnd,
                                         UStack &foundBreaks ) const {
    UErrorCode status = U_ZERO_ERROR;
    UChar charString[rangeEnd-rangeStart];
    utext_extract(text, rangeStart, rangeEnd, charString, rangeEnd-rangeStart, &status);
    Normalizer normalizer(charString, rangeEnd-rangeStart, UNORM_NFKC);
    UnicodeString normalizedString;
    UVector charPositions(status);
    charPositions.addElement(0, status);
    while(normalizer.getIndex() < normalizer.endIndex()){
        UChar uc = normalizer.next();
        normalizedString.append(uc);
        charPositions.addElement(normalizer.getIndex(), status);
    }
    UText *normalizedText = NULL;
    normalizedText = utext_openUnicodeString(normalizedText, &normalizedString, &status);
    /*    
    WumSegmenterWorkspace *workspace =
        static_cast<WumSegmenterWorkspace*>(ws);
    if (ws == NULL || workspace == NULL)  // casting failure
        return false;
*/
    //check range for non-kanji Jap chars
    // TODO: is maxWordSize needed? wouldn't it be automatically restricted by word length in dictionary?
    /*
    utext_setNativeIndex(text, rangeStart);
    UBool hasNonKanjiJapanese = FALSE;
    while(utext_getNativeIndex(text) < rangeEnd){
        UChar uc = utext_next32(text);
        if (isKatakana(uc) || fHiraganaWordSet.contains(uc)){
            hasNonKanjiJapanese = TRUE;
            break;
        }
    }
    */
//    const int numChars = rangeEnd - rangeStart;
    const int numChars = charPositions.size() - 1;
    //const int maxWordSize = 5 + hasNonKanjiJapanese? 5 : 0;
    const int maxWordSize = 20;

    // We usually use the fixed size array if the input isn't too long.
    // For rare long strings we'll dynamically allocate space for them.
//    bool not_use_workspace =
//        numChars > AbstractCjkSegmenter::kStaticBufferSize + 1;

    // bestSnlp[i] is the snlp of the best segmentation of the first i characters
    // in the range to be matched.
//    uint32* bestSnlp = workspace->bestSnlp_;
//    if (not_use_workspace) {
        uint32_t *bestSnlp = new uint32_t[numChars + 1];
//    }
        bestSnlp[0] = 0;
    for(int i=1; i<=numChars; i++){
        bestSnlp[i] = kuint32max;
    }

    // prev[i] is the index of the last CJK character in the previous word in 
    // the best segmentation of the first i characters.
//    int* prev = workspace->prev_;
//    if (not_use_workspace) {
        int *prev = new int[numChars + 1];
//    }
    for(int i=0; i<=numChars; i++){
        prev[i] = -1;
    }
    
    // Dynamic programming to find the best segmentation.
    bool is_prev_katakana = false;
    for (int i = 0; i < numChars; ++i) {
        //utext_setNativeIndex(text, rangeStart + i);
        utext_setNativeIndex(normalizedText, i);
        if (bestSnlp[i] == kuint32max)
            continue;
        
        int count;
        // limit maximum word length matched to size of current substring
        int maxSearchLength = (i + maxWordSize < numChars)? maxWordSize: numChars - i; 
        uint16_t values[maxSearchLength];
        int32_t lengths[maxSearchLength];
//        fDictionary->matches(text, maxSearchLength, lengths, count, maxSearchLength, values);
        fDictionary->matches(normalizedText, maxSearchLength, lengths, count, maxSearchLength, values);
        
        // if there are no single character matches found in the dictionary 
        // starting with this charcter, treat character as a 1-character word 
        // with the highest value possible, i.e. the least likely to occur
        if(count == 0 || lengths[0] != 1){ //no single character match
        	values[count] = maxSnlp;
        	lengths[count++] = 1;
        }
        
        for (int j = 0; j < count; j++){
            //U_ASSERT(values[j] >= 0 && values[j] <= maxSnlp);
            uint32_t newSnlp = bestSnlp[i] + values[j];
            if (newSnlp < bestSnlp[lengths[j] + i]) {
                bestSnlp[lengths[j] + i] = newSnlp;
                prev[lengths[j] + i] = i;
            }
        }
        // TODO: handle Japanese katakana!

        // If no word can be found from the dictionary, single utf8 character is
        // segmented as a word. It works well for Chinese, but in Japanese,
        // Katakana word in single character is pretty rare. So we apply
        // the following heuristic to Katakana: any continuous run of Katakana
        // characters is considered a candidate word with a default cost
        // specified in the katakanaCost table according to its length.
        //utext_setNativeIndex(text, rangeStart + i);
        utext_setNativeIndex(normalizedText, i);
//        bool is_katakana = isKatakana(utext_current32(text));
        bool is_katakana = isKatakana(utext_current32(normalizedText));
        if (!is_prev_katakana && is_katakana) {
            int j = i + 1;
            // Find the end of the continuous run of Katakana characters
            while (j < numChars && (j - i) < kMaxKatakanaGroupLength &&
                    isKatakana(utext_current32(normalizedText))) {
//                    isKatakana(utext_current32(text))) {
//                utext_next32(text);
                utext_next32(normalizedText);
                ++j;
            }
            if ((j - i) < kMaxKatakanaGroupLength) { //TODO: what does kMaxKatakanaGroupLength mean?
                uint32_t newSnlp = bestSnlp[i] + getKatakanaCost(j - i);
                if (newSnlp < bestSnlp[j]) {
                    bestSnlp[j] = newSnlp;
                    prev[j] = i;
                }
            }
        }
        is_prev_katakana = is_katakana;
    }

    // Start pushing the optimal offset index into t_boundary (t for tentative).
    // prev[numChars] is guaranteed to be meaningful.
    // We'll first push in the reverse order, i.e.,
    // t_boundary[0] = numChars, and afterwards do a swap.
    int t_boundary[numChars + 1];

    int numBreaks = 0;
    // No segmentation found, set boundary to end of range
    // TODO: find out if this will ever occur: doesn't seem likely!
    //TODO: if normalization is performed in this method, can remove use of this array.
//    if (bestSnlp[numChars] == kuint32max) {
//        t_boundary[numBreaks++] = numChars;
//    } else {
	    for (int i = numChars; i > 0; i = prev[i]){
//          t_boundary[numBreaks++] = i;
	        t_boundary[numBreaks++] = charPositions.elementAti(i);
	        
	    }
	    U_ASSERT(prev[t_boundary[numBreaks-1]] == 0);
//    }
    
    // Reverse offset index in t_boundary.
    // Don't add a break for the start of the dictionary range if there is one
    // there already.
    if (foundBreaks.size() == 0 || foundBreaks.peeki() < rangeStart) {
        t_boundary[numBreaks++] = 0;
    }

    for (int i = numBreaks-1; i >= 0; i--) {
        foundBreaks.push(t_boundary[i] + rangeStart, status);
    }

    // free memory if necessary
//    if (not_use_workspace) {
        delete[] bestSnlp;
        delete[] prev;
//    }
    delete normalizedText;
    return numBreaks;
}

#if 0
/*
 ******************************************************************
 * KoreanBreakEngine
 */

KoreanBreakEngine::KoreanBreakEngine(const TrieWordDictionary *adoptDictionary, UErrorCode &status)
    : CjkBreakEngine(adoptDictionary, status){
    fKoreanWordSet.applyPattern(UNICODE_STRING_SIMPLE("[:Hangul:]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fKoreanWordSet);
    }
}

KoreanBreakEngine::~KoreanBreakEngine(){
}

/*
 ******************************************************************
 * CJBreakEngine
 */

CJBreakEngine::CJBreakEngine(const TrieWordDictionary *adoptDictionary, UErrorCode &status)
    : CjkBreakEngine(adoptDictionary, status){
    fCJWordSet.applyPattern(UNICODE_STRING_SIMPLE("[:Han:]"), status);
    if (U_SUCCESS(status)) {
        setCharacters(fCJWordSet);
    }
}

CJBreakEngine::~CJBreakEngine(){
}
#endif

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
