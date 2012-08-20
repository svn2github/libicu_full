/*
*******************************************************************************
* Copyright (C) 2010-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* utf16collationiterator.h
*
* created on: 2010oct27
* created by: Markus W. Scherer
*/

#ifndef __UTF16COLLATIONITERATOR_H__
#define __UTF16COLLATIONITERATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "normalizer2impl.h"

U_NAMESPACE_BEGIN

/**
 * UTF-16 collation element and character iterator.
 * Handles normalized UTF-16 text inline, with length or NUL-terminated.
 * Unnormalized text is handled by a subclass.
 */
class U_I18N_API UTF16CollationIterator : public CollationIterator {
public:
    UTF16CollationIterator(const CollationData *d,
                           const UChar *s, const UChar *lim)
            // Optimization: Skip initialization of fields that are not used
            // until they are set together with other state changes.
            : CollationIterator(d),
              start(s), pos(s), limit(lim) {}

    virtual ~UTF16CollationIterator();

    virtual void resetToStart();

    void setText(const UChar *s, const UChar *lim) {
        reset();
        start = pos = s;
        limit = lim;
    }

    // TODO: setText(start, pos, limit)  ?

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UChar handleGetTrailSurrogate();

    virtual UBool foundNULTerminator();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual const void *saveLimitAndSetAfter(UChar32 c);

    virtual void restoreLimit(const void *savedLimit);

    // UTF-16 string pointers.
    // limit can be NULL for NUL-terminated strings.
    // This class assumes that whole code points are stored within [start..limit[.
    // That is, a trail surrogate at start or a lead surrogate at limit-1
    // will be assumed to be a surrogate code point rather than attempting to pair it
    // with a surrogate retrieved from the subclass.
    const UChar *start, *pos, *limit;
    // TODO: getter for limit, so that caller can find out length of NUL-terminated text?
};

/**
 * Checks the input text for FCD, passes already-FCD segments into the base iterator,
 * and normalizes other segments on the fly.
 */
class U_I18N_API FCDUTF16CollationIterator : public UTF16CollationIterator {
public:
    FCDUTF16CollationIterator(const CollationData *data,
                              const UChar *s, const UChar *lim,
                              UErrorCode &errorCode);

    virtual void resetToStart();

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual const void *saveLimitAndSetAfter(UChar32 c);

    virtual void restoreLimit(const void *savedLimit);

private:
    /**
     * Makes the next text segment available, if there is one.
     * To be called when inFastPath || pos == limit.
     * @return TRUE if there is more text and pos != limit
     */
    UBool nextSegment(UErrorCode &errorCode);

    /**
     * Makes the previous text segment available, if there is one.
     * To be called when inFastPath || pos == start.
     * @return TRUE if there is more text and pos != start
     */
    UBool previousSegment(UErrorCode &errorCode);

    /**
     * Tibetan composite vowel signs (U+0F73, U+0F75, U+0F81)
     * must be decomposed before reaching the core collation code,
     * or else some sequences including them, even ones passing the FCD check,
     * do not yield canonically equivalent results.
     *
     * They have distinct lccc/tccc combinations: 129/130 or 129/132.
     *
     * @param fcd16 the FCD value (lccc/tccc combination) of a code point
     * @return TRUE if fcd16 is from U+0F73, U+0F75 or U+0F81
     */
    static inline UBool isFCD16OfTibetanCompositeVowel(uint16_t fcd16) {
        return fcd16 == 0x8182 || fcd16 == 0x8184;
    }

    UBool normalize(UErrorCode &errorCode);

    // Text pointers: The input text is [rawStart, rawLimit[
    // where rawLimit can be NULL for NUL-terminated text.
    //
    // Fast path:
    //
    // Fetching characters is combined with a very quick boundary check.
    // No tracking of an FCD segment. limit == rawLimit.
    //
    // Slow path:
    //
    // segmentStart and segmentLimit point into the text and indicate
    // the start and exclusive end of the text segment currently being processed.
    // They are at FCD boundaries.
    // Either the current text segment already passes the FCD check
    // and segmentStart==start<=pos<=limit==segmentLimit,
    // or the current segment had to be normalized so that
    // [segmentStart, segmentLimit[ turned into the normalized string,
    // corresponding to normalized.getBuffer()==start<=pos<=limit==start+normalized.length().
    const UChar *rawStart;
    const UChar *segmentStart;
    const UChar *segmentLimit;
    // rawLimit==NULL for a NUL-terminated string.
    const UChar *rawLimit;
    // Normally zero.
    // Between calls to saveLimitAndSetAfter() and restoreLimit(),
    // it tracks the positive number of normalized UChars
    // between the start pointer and the temporary iteration limit.
    int32_t lengthBeforeLimit;

    const Normalizer2Impl &nfcImpl;
    UnicodeString normalized;
    UBool inFastPath;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __UTF16COLLATIONITERATOR_H__
