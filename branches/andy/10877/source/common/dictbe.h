/**
 *******************************************************************************
 * Copyright (C) 2006-2014, International Business Machines Corporation   *
 * and others. All Rights Reserved.                                            *
 *******************************************************************************
 */

#ifndef DICTBE_H
#define DICTBE_H

#include "unicode/utypes.h"
#include "unicode/uniset.h"
#include "unicode/utext.h"

#include "brkeng.h"

U_NAMESPACE_BEGIN

class DictionaryMatcher;
class Normalizer2;
class UVector32;

/*******************************************************************
 * DictionaryBreakEngine
 */

/**
 * <p>DictionaryBreakEngine is a kind of LanguageBreakEngine that uses a
 * dictionary to determine language-specific breaks.</p>
 *
 * <p>After it is constructed a DictionaryBreakEngine may be shared between
 * threads without synchronization.</p>
 */
class DictionaryBreakEngine : public LanguageBreakEngine {
 private:
    /**
     * The set of characters handled by this engine
     * @internal
     */

  UnicodeSet    fSet;

    /**
     * The set of break types handled by this engine
     * @internal
     */

  uint32_t      fTypes;

  /**
   * <p>Default constructor.</p>
   *
   */
  DictionaryBreakEngine();

 public:

  /**
   * <p>Constructor setting the break types handled.</p>
   *
   * @param breakTypes A bitmap of types handled by the engine.
   */
  DictionaryBreakEngine( uint32_t breakTypes );

  /**
   * <p>Virtual destructor.</p>
   */
  virtual ~DictionaryBreakEngine();

  /**
   * <p>Indicate whether this engine handles a particular character for
   * a particular kind of break.</p>
   *
   * @param c A character which begins a run that the engine might handle
   * @param breakType The type of text break which the caller wants to determine
   * @return TRUE if this engine handles the particular character and break
   * type.
   */
  virtual UBool handles( UChar32 c, int32_t breakType ) const;

  /**
   * <p>Find any breaks within a run in the supplied text.</p>
   *
   * @param text A UText representing the text. Breaking begins at the
   *             iterators current position.
   * The iterator is left at the end of the run of characters which the engine is capable of handling 
   * that starts from the first (or last) character in the range.
   * @param endPos The end of the run within the supplied text.
   * @param breakType The type of break desired, or -1.
   * @param foundBreaks A vector to receive the break positions.
   */
  virtual void findBreaks( UText *text,
                           int32_t endPos,
                           int32_t breakType,
                           UVector32 &foundBreaks,
                           UErrorCode &status) const;

  /**
   * @return The status tag for word breaks found using this break engine.
   */
  virtual int32_t getTagValue(int32_t breakType) const;

 protected:

 /**
  * <p>Set the character set handled by this engine.</p>
  *
  * @param set A UnicodeSet of the set of characters handled by the engine
  */
  virtual void setCharacters( const UnicodeSet &set );

 /**
  * <p>Set the break types handled by this engine.</p>
  *
  * @param breakTypes A bitmap of types handled by the engine.
  */
//  virtual void setBreakTypes( uint32_t breakTypes );

 /**
  * <p>Divide up a range of known dictionary characters handled by this break engine.</p>
  *
  * @param text A UText representing the text
  * @param rangeStart The start of the range of dictionary characters
  * @param rangeEnd The end of the range of dictionary characters
  * @param foundBreaks A vector to which the identified breaks are appended.
  */
  virtual void divideUpDictionaryRange( UText *text,
                                           int32_t rangeStart,
                                           int32_t rangeEnd,
                                           int32_t breakType,
                                           UVector32 &foundBreaks,
                                           UErrorCode &status) const = 0;

};

/*******************************************************************
 * ThaiBreakEngine
 */

/**
 * <p>ThaiBreakEngine is a kind of DictionaryBreakEngine that uses a
 * dictionary and heuristics to determine Thai-specific breaks.</p>
 *
 * <p>After it is constructed a ThaiBreakEngine may be shared between
 * threads without synchronization.</p>
 */
class ThaiBreakEngine : public DictionaryBreakEngine {
 private:
    /**
     * The set of characters handled by this engine
     * @internal
     */

  UnicodeSet                fThaiWordSet;
  UnicodeSet                fEndWordSet;
  UnicodeSet                fBeginWordSet;
  UnicodeSet                fSuffixSet;
  UnicodeSet                fMarkSet;
  DictionaryMatcher  *fDictionary;

 public:

  /**
   * <p>Default constructor.</p>
   *
   * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the
   * engine is deleted.
   */
  ThaiBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status);

  /**
   * <p>Virtual destructor.</p>
   */
  virtual ~ThaiBreakEngine();

 protected:
 /**
  * <p>Divide up a range of known dictionary characters handled by this break engine.</p>
  *
  * @param text A UText representing the text
  * @param rangeStart The start of the range of dictionary characters
  * @param rangeEnd The end of the range of dictionary characters
  * @param foundBreaks Vector to which the word boundaries are appended.
  *                    Includes boundaries at the start and end of the range being split.
  * @internal
  */
  virtual void divideUpDictionaryRange( UText *text,
                                        int32_t rangeStart,
                                        int32_t rangeEnd,
                                        int32_t breakType,
                                        UVector32 &foundBreaks,
                                        UErrorCode &status) const;

};

/*******************************************************************
 * LaoBreakEngine
 */

/**
 * <p>LaoBreakEngine is a kind of DictionaryBreakEngine that uses a
 * dictionary and heuristics to determine Lao-specific breaks.</p>
 *
 * <p>After it is constructed a LaoBreakEngine may be shared between
 * threads without synchronization.</p>
 */
class LaoBreakEngine : public DictionaryBreakEngine {
 private:
    /**
     * The set of characters handled by this engine
     * @internal
     */

  UnicodeSet                fLaoWordSet;
  UnicodeSet                fEndWordSet;
  UnicodeSet                fBeginWordSet;
  UnicodeSet                fMarkSet;
  DictionaryMatcher  *fDictionary;

 public:

  /**
   * <p>Default constructor.</p>
   *
   * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the
   * engine is deleted.
   */
  LaoBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status);

  /**
   * <p>Virtual destructor.</p>
   */
  virtual ~LaoBreakEngine();

 protected:
 /**
  * <p>Divide up a range of known dictionary characters handled by this break engine.</p>
  *
  * @param text A UText representing the text
  * @param rangeStart The start of the range of dictionary characters
  * @param rangeEnd The end of the range of dictionary characters
  * @param foundBreaks Vector to which the word boundaries are appended.
  *                    Includes boundaries at the start and end of the range being split.
  */
  virtual void divideUpDictionaryRange( UText *text,
                                        int32_t rangeStart,
                                        int32_t rangeEnd,
                                        int32_t breakType,
                                        UVector32 &foundBreaks,
                                        UErrorCode &status) const;

};

/******************************************************************* 
 * BurmeseBreakEngine 
 */ 
 
/** 
 * <p>BurmeseBreakEngine is a kind of DictionaryBreakEngine that uses a 
 * DictionaryMatcher and heuristics to determine Burmese-specific breaks.</p> 
 * 
 * <p>After it is constructed a BurmeseBreakEngine may be shared between 
 * threads without synchronization.</p> 
 */ 
class BurmeseBreakEngine : public DictionaryBreakEngine { 
 private: 
    /** 
     * The set of characters handled by this engine 
     * @internal 
     */ 
 
  UnicodeSet                fBurmeseWordSet; 
  UnicodeSet                fEndWordSet; 
  UnicodeSet                fBeginWordSet; 
  UnicodeSet                fMarkSet; 
  DictionaryMatcher  *fDictionary; 
 
 public: 
 
  /** 
   * <p>Default constructor.</p> 
   * 
   * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the 
   * engine is deleted. 
   */ 
  BurmeseBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status); 
 
  /** 
   * <p>Virtual destructor.</p> 
   */ 
  virtual ~BurmeseBreakEngine(); 
 
 protected: 
 /** 
  * <p>Divide up a range of known dictionary characters.</p> 
  * 
  * @param text A UText representing the text 
  * @param rangeStart The start of the range of dictionary characters 
  * @param rangeEnd The end of the range of dictionary characters 
  * @param foundBreaks Output of C array of int32_t break positions, or 0 
  * @return The number of breaks found 
  */ 
  virtual void divideUpDictionaryRange( UText *text,
                                        int32_t rangeStart,
                                        int32_t rangeEnd,
                                        int32_t breakType,
                                        UVector32 &foundBreaks,
                                        UErrorCode &status) const;
 
}; 
 
/******************************************************************* 
 * KhmerBreakEngine 
 */ 
 
/** 
 * <p>KhmerBreakEngine is a kind of DictionaryBreakEngine that uses a 
 * DictionaryMatcher and heuristics to determine Khmer-specific breaks.</p> 
 * 
 * <p>After it is constructed a KhmerBreakEngine may be shared between 
 * threads without synchronization.</p> 
 */ 
class KhmerBreakEngine : public DictionaryBreakEngine { 
 private: 
    /** 
     * The set of characters handled by this engine 
     * @internal 
     */ 
 
  UnicodeSet                fKhmerWordSet; 
  UnicodeSet                fEndWordSet; 
  UnicodeSet                fBeginWordSet; 
  UnicodeSet                fMarkSet; 
  DictionaryMatcher  *fDictionary; 
 
 public: 
 
  /** 
   * <p>Default constructor.</p> 
   * 
   * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the 
   * engine is deleted. 
   */ 
  KhmerBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status); 
 
  /** 
   * <p>Virtual destructor.</p> 
   */ 
  virtual ~KhmerBreakEngine(); 
 
 protected: 
 /** 
  * <p>Divide up a range of known dictionary characters.</p> 
  * 
  * @param text A UText representing the text 
  * @param rangeStart The start of the range of dictionary characters 
  * @param rangeEnd The end of the range of dictionary characters 
  * @param foundBreaks Vector to which the word boundaries are appended.
  *                    Includes boundaries at the start and end of the range being split.
  */ 
  virtual void divideUpDictionaryRange( UText *text, 
                                        int32_t rangeStart, 
                                        int32_t rangeEnd, 
                                        int32_t breakType,
                                        UVector32 &foundBreaks,
                                        UErrorCode &status) const; 
 
}; 
 
/*******************************************************************
 * FrequencyBreakEngine
 */

/**
 * <p>FrequencyBreakEngine is a kind of DictionaryBreakEngine that uses a
 * dictionary with costs associated with each word and
 * Viterbi decoding to determine breaks.</p>
 */
class FrequencyBreakEngine : public DictionaryBreakEngine {
 protected:
  DictionaryMatcher        *fDictionary;
  const Normalizer2        *nfkcNorm2;

 public:

    /**
     * <p>Default constructor.</p>
     *
     * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the
     * engine is deleted. The DictionaryMatcher must contain costs for each word
     * in order for the dictionary to work properly.
     */
  FrequencyBreakEngine(DictionaryMatcher *adoptDictionary, uint32_t breakTypes, UErrorCode &status);

    /**
     * <p>Virtual destructor.</p>
     */
  virtual ~FrequencyBreakEngine();

 protected:
    /**
     * <p>Divide up a range of known dictionary characters handled by this break engine.</p>
     *
     * @param text A UText representing the text
     * @param rangeStart The start of the range of dictionary characters
     * @param rangeEnd The end of the range of dictionary characters
     * @param foundBreaks Vector to which the word boundaries are appended.
     *                    Includes boundaries at the start and end of the range being split.
     */
  virtual void    divideUpDictionaryRange( UText *text,
          int32_t rangeStart,
          int32_t rangeEnd,
          int32_t breakType,
          UVector32 &foundBreaks,
          UErrorCode &status) const;

  virtual void findBoundaries(UnicodeString *inString,
		  uint32_t numCodePts,
		  UVector32 *bestSnlp,
		  UVector32 *prev,
		  UErrorCode &status) const;

  /**
    * Filter function for boundaries produced by the dictionary.
    * Function is called for each candidate boundary.
    *  Return TRUE if the boundary is OK, FALSE if it should be suppressed.
    */
  virtual UBool acceptBoundary(int32_t boundary, const UnicodeString *text, int32_t breakType) const;
};

/*******************************************************************
 * ThaiFrequencyBreakEngine
 */

/**
 * <p>ThaiFrequencyBreakEngine is a kind of FrequencyBreakEngine that uses a
 * weighted dictionary to determine Thai-specific breaks.</p>
 */
class ThaiFrequencyBreakEngine : public FrequencyBreakEngine {
 private:
    /**
     * The set of characters handled by this engine
     * @internal
     */
  UnicodeSet                fThaiWordSet;

 public:

  /**
   * <p>Default constructor.</p>
   *
   * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the
   * engine is deleted.
   */
  ThaiFrequencyBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status);

  /**
   * <p>Virtual destructor.</p>
   */
  virtual ~ThaiFrequencyBreakEngine();
};

#if !UCONFIG_NO_NORMALIZATION

/*******************************************************************
 * CjkBreakEngine
 */

/**
 * <p>CjkBreakEngine is a kind of DictionaryBreakEngine that uses a
 * dictionary with costs associated with each word and
 * Viterbi decoding to determine CJK-specific breaks.</p>
 */
class CjBreakEngine : public FrequencyBreakEngine {
 protected:
    /**
     * The set of characters handled by this engine
     * @internal
     */
  UnicodeSet                fHanWordSet;
  UnicodeSet                fKatakanaWordSet;
  UnicodeSet                fHiraganaWordSet;

 public:

    /**
     * <p>Default constructor.</p>
     *
     * @param adoptDictionary A DictionaryMatcher to adopt. Deleted when the
     * engine is deleted. The DictionaryMatcher must contain costs for each word
     * in order for the dictionary to work properly.
     */
  CjBreakEngine(DictionaryMatcher *adoptDictionary, UErrorCode &status);

    /**
     * <p>Virtual destructor.</p>
     */
  virtual ~CjBreakEngine();

  /**
   * @return The status tag for word breaks found using this break engine.
   */
  virtual int32_t getTagValue(int32_t breakType) const;

 protected:
  virtual void findBoundaries(UnicodeString *inString,
		  uint32_t numCodePts,
		  UVector32 *bestSnlp,
		  UVector32 *prev,
		  UErrorCode &status) const;
};
#endif

U_NAMESPACE_END

    /* DICTBE_H */
#endif
