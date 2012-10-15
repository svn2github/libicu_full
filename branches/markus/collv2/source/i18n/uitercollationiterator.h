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

/**
 * Incrementally checks the input text for FCD and normalizes where necessary.
 */
class U_I18N_API FCDUIterCollationIterator : public UIterCollationIterator {
public:
    FCDUIterCollationIterator(const CollationData *data, UCharIterator &ui);

    virtual void resetToStart();

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

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
    // Direction of incremental FCD check. See comments before rawStart.
    int8_t checkDir;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __UITERCOLLATIONITERATOR_H__
