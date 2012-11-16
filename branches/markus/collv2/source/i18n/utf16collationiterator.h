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
            : CollationIterator(d),
              start(s), pos(s), limit(lim) {}

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

    // UTF-16 string pointers.
    // limit can be NULL for NUL-terminated strings.
    const UChar *start, *pos, *limit;
    // TODO: getter for limit, so that caller can find out length of NUL-terminated text?
};

/**
 * Incrementally checks the input text for FCD and normalizes where necessary.
 */
class U_I18N_API FCDUTF16CollationIterator : public UTF16CollationIterator {
public:
    FCDUTF16CollationIterator(const CollationData *data,
                              const UChar *s, const UChar *lim)
            : UTF16CollationIterator(data, s, lim),
              rawStart(s), segmentStart(s), segmentLimit(NULL), rawLimit(lim),
              nfcImpl(data->nfcImpl),
              checkDir(1) {}

    virtual void resetToStart();

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UBool foundNULTerminator();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

private:
    /**
     * Switches to forward checking if possible.
     * To be called when checkDir < 0 || (checkDir == 0 && pos == limit).
     * Returns with checkDir > 0 || (checkDir == 0 && pos != limit).
     */
    void switchToForward();

    /**
     * Extend the FCD text segment forward or normalize around pos.
     * To be called when checkDir > 0 && pos != limit.
     * @return TRUE if success, checkDir == 0 and pos != limit
     */
    UBool nextSegment(UErrorCode &errorCode);

    /**
     * Switches to backward checking.
     * To be called when checkDir > 0 || (checkDir == 0 && pos == start).
     * Returns with checkDir < 0 || (checkDir == 0 && pos != start).
     */
    void switchToBackward();

    /**
     * Extend the FCD text segment backward or normalize around pos.
     * To be called when checkDir < 0 && pos != start.
     * @return TRUE if success, checkDir == 0 and pos != start
     */
    UBool previousSegment(UErrorCode &errorCode);

    UBool normalize(const UChar *from, const UChar *to, UErrorCode &errorCode);

    // Text pointers: The input text is [rawStart, rawLimit[
    // where rawLimit can be NULL for NUL-terminated text.
    //
    // checkDir > 0:
    //
    // The input text [segmentStart..pos[ passes the FCD check.
    // Moving forward checks incrementally.
    // segmentLimit is undefined. limit == rawLimit.
    //
    // checkDir < 0:
    // The input text [pos..segmentLimit[ passes the FCD check.
    // Moving backward checks incrementally.
    // segmentStart is undefined, start == rawStart.
    //
    // checkDir == 0:
    //
    // The input text [segmentStart..segmentLimit[ is being processed.
    // These pointers are at FCD boundaries.
    // Either this text segment already passes the FCD check
    // and segmentStart==start<=pos<=limit==segmentLimit,
    // or the current segment had to be normalized so that
    // [segmentStart..segmentLimit[ turned into the normalized string,
    // corresponding to normalized.getBuffer()==start<=pos<=limit==start+normalized.length().
    const UChar *rawStart;
    const UChar *segmentStart;
    const UChar *segmentLimit;
    // rawLimit==NULL for a NUL-terminated string.
    const UChar *rawLimit;

    const Normalizer2Impl &nfcImpl;
    UnicodeString normalized;
    // Direction of incremental FCD check. See comments before rawStart.
    int8_t checkDir;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __UTF16COLLATIONITERATOR_H__
