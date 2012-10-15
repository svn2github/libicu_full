/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* uitercollationiterator.h
*
* created on: 2012sep23 (from utf16collationiterator.h)
* created by: Markus W. Scherer
*/

#ifndef __UITERCOLLATIONITERATOR_H__
#define __UITERCOLLATIONITERATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uiter.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "normalizer2impl.h"

U_NAMESPACE_BEGIN

/**
 * UCharIterator-based collation element and character iterator.
 * Handles normalized text inline, with length or NUL-terminated.
 * Unnormalized text is handled by a subclass.
 */
class U_I18N_API UIterCollationIterator : public CollationIterator {
public:
    UIterCollationIterator(const CollationData *d, UCharIterator &ui)
            : CollationIterator(d), iter(ui) {}

    virtual void resetToStart();

    /* TODO: void setText(const UChar *s, const UChar *lim) {
        reset();
        start = pos = s;
        limit = lim;
    }*/

    // TODO: setText(start, pos, limit)  ?

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UChar handleGetTrailSurrogate();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    UCharIterator &iter;
};

// TODO: The following class only inherits the "iter" field from its parent. Keep this class hierarchy?
/**
 * Incrementally checks the input text for FCD and normalizes where necessary.
 */
class U_I18N_API FCDUIterCollationIterator : public UIterCollationIterator {
public:
    FCDUIterCollationIterator(const CollationData *data, UCharIterator &ui, int32_t startIndex);

    virtual void resetToStart();  // TODO: Do we really need *CollationIterator::resetToStart()?

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UChar handleGetTrailSurrogate();

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

    UBool normalize(const UnicodeString &s, UErrorCode &errorCode);

    enum State {
        /**
         * The input text [start..(iter index)[ passes the FCD check.
         * Moving forward checks incrementally.
         * pos & limit are undefined.
         */
        ITER_CHECK_FWD,
        /**
         * The input text [(iter index)..limit[ passes the FCD check.
         * Moving backward checks incrementally.
         * start & pos are undefined.
         */
        ITER_CHECK_BWD,
        /**
         * The input text [start..limit[ passes the FCD check.
         * pos tracks the current text index.
         */
        ITER_IN_FCD_SEGMENT,
        /**
         * The input text [start..limit[ failed the FCD check and was normalized.
         * pos tracks the current index in the normalized string.
         * The text iterator is at the limit index.
         */
        IN_NORM_ITER_AT_LIMIT,
        /**
         * The input text [start..limit[ failed the FCD check and was normalized.
         * pos tracks the current index in the normalized string.
         * The text iterator is at the start index.
         */
        IN_NORM_ITER_AT_START
    };

    State state;

    int32_t start;
    int32_t pos;
    int32_t limit;

    const Normalizer2Impl &nfcImpl;
    UnicodeString normalized;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __UITERCOLLATIONITERATOR_H__
