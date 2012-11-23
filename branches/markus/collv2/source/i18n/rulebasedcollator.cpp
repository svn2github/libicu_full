/*
*******************************************************************************
* Copyright (C) 1996-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* rulebasedcollator.cpp
*
* created on: 2012feb14 with new and old collation code
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/sortkey.h"
#include "unicode/ucol.h"
#include "unicode/uiter.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/utf8.h"
#include "bocsu.h"
#include "cmemory.h"
#include "collation.h"
#include "collationcompare.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "collationkeys.h"
#include "cstring.h"
#include "rulebasedcollator.h"
#include "uassert.h"
#include "uitercollationiterator.h"
#include "utf16collationiterator.h"
#include "utf8collationiterator.h"

U_NAMESPACE_BEGIN

namespace {

class FixedSortKeyByteSink : public SortKeyByteSink2 {
public:
    FixedSortKeyByteSink(char *dest, int32_t destCapacity)
            : SortKeyByteSink2(dest, destCapacity) {}
    virtual ~FixedSortKeyByteSink();

private:
    virtual void AppendBeyondCapacity(const char *bytes, int32_t n, int32_t length);
    virtual UBool Resize(int32_t appendCapacity, int32_t length);
};

FixedSortKeyByteSink::~FixedSortKeyByteSink() {}

void
FixedSortKeyByteSink::AppendBeyondCapacity(const char *bytes, int32_t /*n*/, int32_t length) {
    // buffer_ != NULL && bytes != NULL && n > 0 && appended_ > capacity_
    // Fill the buffer completely.
    int32_t available = capacity_ - length;
    if (available > 0) {
        uprv_memcpy(buffer_ + length, bytes, available);
    }
}

UBool
FixedSortKeyByteSink::Resize(int32_t /*appendCapacity*/, int32_t /*length*/) {
    return FALSE;
}

}  // namespace

// Not in an anonymous namespace, so that it can be a friend of CollationKey.
class CollationKeyByteSink2 : public SortKeyByteSink2 {
public:
    CollationKeyByteSink2(CollationKey &key)
            : SortKeyByteSink2(reinterpret_cast<char *>(key.getBytes()), key.getCapacity()),
              key_(key) {}
    virtual ~CollationKeyByteSink2();

private:
    virtual void AppendBeyondCapacity(const char *bytes, int32_t n, int32_t length);
    virtual UBool Resize(int32_t appendCapacity, int32_t length);

    CollationKey &key_;
};

CollationKeyByteSink2::~CollationKeyByteSink2() {}

void
CollationKeyByteSink2::AppendBeyondCapacity(const char *bytes, int32_t n, int32_t length) {
    // buffer_ != NULL && bytes != NULL && n > 0 && appended_ > capacity_
    if (Resize(n, length)) {
        uprv_memcpy(buffer_ + length, bytes, n);
    }
}

UBool
CollationKeyByteSink2::Resize(int32_t appendCapacity, int32_t length) {
    if (buffer_ == NULL) {
        return FALSE;  // allocation failed before already
    }
    int32_t newCapacity = 2 * capacity_;
    int32_t altCapacity = length + 2 * appendCapacity;
    if (newCapacity < altCapacity) {
        newCapacity = altCapacity;
    }
    if (newCapacity < 200) {
        newCapacity = 200;
    }
    uint8_t *newBuffer = key_.reallocate(newCapacity, length);
    if (newBuffer == NULL) {
        SetNotOk();
        return FALSE;
    }
    buffer_ = reinterpret_cast<char *>(newBuffer);
    capacity_ = newCapacity;
    return TRUE;
}

// TODO: Add UTRACE_... calls back in.

RuleBasedCollator2::~RuleBasedCollator2() {
    delete ownedData;
    uprv_free(ownedReorderTable);
    uprv_free(ownedReorderCodes);
}

Collator *
RuleBasedCollator2::clone() const {
    // TODO: new RuleBasedCollator2(), copyFrom(*this, UErrorCode), delete & return NULL if U_FAILURE
    return NULL;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedCollator2)

UBool
RuleBasedCollator2::operator==(const Collator& other) const {
    return FALSE;  // TODO
}

int32_t
RuleBasedCollator2::hashCode() const {
    return 0;  // TODO
}

Locale
RuleBasedCollator2::getLocale(ULocDataLocaleType type, UErrorCode& status) const {
    return Locale::getDefault();  // TODO
}

// TODO: const UnicodeString& getRules(void) const;
// TODO: void getRules(UColRuleOption delta, UnicodeString &buffer);

// TODO: uint8_t *cloneRuleData(int32_t &length, UErrorCode &status);

// TODO: int32_t cloneBinary(uint8_t *buffer, int32_t capacity, UErrorCode &status);

void
RuleBasedCollator2::getVersion(UVersionInfo info) const {
    // TODO
}

// TODO: int32_t getMaxExpansion(int32_t order) const;

UnicodeSet *
RuleBasedCollator2::getTailoredSet(UErrorCode &status) const {
    return NULL;  // TODO
}

UBool
RuleBasedCollator2::ensureOwnedData(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    if(ownedData != NULL) { return TRUE; }
    ownedData = new CollationData(*defaultData);
    if(ownedData == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    data = ownedData;
    return TRUE;
}

UColAttributeValue
RuleBasedCollator2::getAttribute(UColAttribute attr, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return UCOL_DEFAULT; }
    int32_t option;
    switch(attr) {
    case UCOL_FRENCH_COLLATION:
        option = CollationData::BACKWARD_SECONDARY;
        break;
    case UCOL_ALTERNATE_HANDLING:
        return data->getAlternateHandling();
    case UCOL_CASE_FIRST:
        return data->getCaseFirst();
    case UCOL_CASE_LEVEL:
        option = CollationData::CASE_LEVEL;
        break;
    case UCOL_NORMALIZATION_MODE:
        option = CollationData::CHECK_FCD;
        break;
    case UCOL_STRENGTH:
        return (UColAttributeValue)data->getStrength();
    case UCOL_HIRAGANA_QUATERNARY_MODE:
        // Deprecated attribute, unsettable.
        return UCOL_OFF;
    case UCOL_NUMERIC_COLLATION:
        option = CollationData::CODAN;
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return UCOL_DEFAULT;
    }
    return ((data->options & option) == 0) ? UCOL_OFF : UCOL_ON;
}

void
RuleBasedCollator2::setAttribute(UColAttribute attr, UColAttributeValue value,
                                 UErrorCode &errorCode) {
    UColAttributeValue oldValue = getAttribute(attr, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    if(value == oldValue) {
        setAttributeExplicitly(attr);
        return;
    }
    if(ownedData == NULL) {
        if(value == UCOL_DEFAULT) {
            setAttributeDefault(attr);
            return;
        }
        if(!ensureOwnedData(errorCode)) { return; }
    }

    switch(attr) {
    case UCOL_FRENCH_COLLATION:
        ownedData->setFlag(CollationData::BACKWARD_SECONDARY, value, defaultData->options, errorCode);
        break;
    case UCOL_ALTERNATE_HANDLING:
        ownedData->setAlternateHandling(value, defaultData->options, errorCode);
        break;
    case UCOL_CASE_FIRST:
        ownedData->setCaseFirst(value, defaultData->options, errorCode);
        break;
    case UCOL_CASE_LEVEL:
        ownedData->setFlag(CollationData::CASE_LEVEL, value, defaultData->options, errorCode);
        break;
    case UCOL_NORMALIZATION_MODE:
        ownedData->setFlag(CollationData::CHECK_FCD, value, defaultData->options, errorCode);
        break;
    case UCOL_STRENGTH:
        ownedData->setStrength(value, defaultData->options, errorCode);
        break;
    case UCOL_HIRAGANA_QUATERNARY_MODE:
        // Deprecated attribute. Check for valid values but do not change anything.
        if(value != UCOL_OFF && value != UCOL_ON && value != UCOL_DEFAULT) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        }
        break;
    case UCOL_NUMERIC_COLLATION:
        ownedData->setFlag(CollationData::CODAN, value, defaultData->options, errorCode);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
    if(U_FAILURE(errorCode)) { return; }
    if(value == UCOL_DEFAULT) {
        setAttributeDefault(attr);
    } else {
        setAttributeExplicitly(attr);
    }
}

uint32_t
RuleBasedCollator2::getVariableTop(UErrorCode &errorCode) const {
    return data->variableTop;
}

uint32_t
RuleBasedCollator2::setVariableTop(const UChar *varTop, int32_t len, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    if(varTop == NULL && len !=0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if(len < 0) { len = u_strlen(varTop); }
    if(len == 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    int64_t ce1, ce2;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF16CollationIterator ci(data, varTop, varTop + len);
        ce1 = ci.nextCE(errorCode);
        ce2 = ci.nextCE(errorCode);
    } else {
        FCDUTF16CollationIterator ci(data, varTop, varTop + len);
        ce1 = ci.nextCE(errorCode);
        ce2 = ci.nextCE(errorCode);
    }
    if(ce1 == Collation::NO_CE || ce2 != Collation::NO_CE) {
        errorCode = U_CE_NOT_FOUND_ERROR;
        return 0;
    }
    setVariableTop((uint32_t)(ce1 >> 32), errorCode);
    return data->variableTop;
}

uint32_t
RuleBasedCollator2::setVariableTop(const UnicodeString &varTop, UErrorCode &errorCode) {
    return setVariableTop(varTop.getBuffer(), varTop.length(), errorCode);
}

void
RuleBasedCollator2::setVariableTop(uint32_t varTop, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || varTop == data->variableTop) { return; }
    if(!ensureOwnedData(errorCode)) { return; }
    ownedData->variableTop = varTop;
    if(varTop == defaultData->variableTop) {
        setAttributeDefault(ATTR_VARIABLE_TOP);
    } else {
        setAttributeExplicitly(ATTR_VARIABLE_TOP);
    }
}

// TODO: hack for prototyping, remove
static const CollationBaseData *gBaseData = NULL;

U_CAPI void U_EXPORT2 ucol_setCollationBaseData(const CollationBaseData *base) {
    gBaseData = base;
}

static const CollationBaseData *ucol_getCollationBaseData(UErrorCode & /*errorCode*/) { return gBaseData; }  // TODO

int32_t
RuleBasedCollator2::getReorderCodes(int32_t *dest, int32_t capacity,
                                    UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    if(capacity < 0 || (dest == NULL && capacity > 0)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    int32_t length = data->reorderCodesLength;
    if(length == 0) { return 0; }
    if(length > capacity) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return length;
    }
    uprv_memcpy(dest, data->reorderCodes, length * 4);
    return length;
}

void
RuleBasedCollator2::setReorderCodes(const int32_t *reorderCodes, int32_t length,
                                    UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(length < 0 || (reorderCodes == NULL && length > 0)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(length == 1 && reorderCodes[0] == UCOL_REORDER_CODE_DEFAULT) {
        if(ownedData != NULL) {
            ownedData->reorderTable = defaultData->reorderTable;
            ownedData->reorderCodes = defaultData->reorderCodes;
            ownedData->reorderCodesLength = defaultData->reorderCodesLength;
        }
        return;
    }
    if(!ensureOwnedData(errorCode)) { return; }
    if(ownedReorderTable == NULL) {
        ownedReorderTable = (uint8_t *)uprv_malloc(256);
        if(ownedReorderTable == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }
    const CollationBaseData *baseData = ucol_getCollationBaseData(errorCode);
    baseData->makeReorderTable(reorderCodes, length, ownedReorderTable, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    if(length > ownedReorderCodesCapacity) {
        int32_t newCapacity = length + 20;
        int32_t *newReorderCodes = (int32_t *)uprv_malloc(newCapacity * 4);
        if(newReorderCodes == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        uprv_free(ownedReorderCodes);
        ownedReorderCodes = newReorderCodes;
        ownedReorderCodesCapacity = newCapacity;
    }
    uprv_memcpy(ownedReorderCodes, reorderCodes, length * 4);
    ownedData->reorderTable = ownedReorderTable;
    ownedData->reorderCodes = ownedReorderCodes;
    ownedData->reorderCodesLength = length;
}

int32_t
RuleBasedCollator2::getEquivalentReorderCodes(int32_t reorderCode,
                                              int32_t *dest, int32_t capacity,
                                              UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    if(capacity < 0 || (dest == NULL && capacity > 0)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    const CollationBaseData *baseData = ucol_getCollationBaseData(errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    return baseData->getEquivalentScripts(reorderCode, dest, capacity, errorCode);
}

UCollationResult
RuleBasedCollator2::compare(const UnicodeString &left, const UnicodeString &right,
                            UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }
    return doCompare(left.getBuffer(), left.length(),
                     right.getBuffer(), right.length(), errorCode);
}

UCollationResult
RuleBasedCollator2::compare(const UnicodeString &left, const UnicodeString &right,
                            int32_t length, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode) || length == 0) { return UCOL_EQUAL; }
    if(length < 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return UCOL_EQUAL;
    }
    int32_t leftLength = left.length();
    int32_t rightLength = right.length();
    if(leftLength > length) { leftLength = length; }
    if(rightLength > length) { rightLength = length; }
    return doCompare(left.getBuffer(), leftLength,
                     right.getBuffer(), rightLength, errorCode);
}

UCollationResult
RuleBasedCollator2::compare(const UChar *left, int32_t leftLength,
                            const UChar *right, int32_t rightLength,
                            UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }
    if((left == NULL && leftLength != 0) || (right == NULL && rightLength != 0)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return UCOL_EQUAL;
    }
    // Make sure both or neither strings have a known length.
    // We do not optimize for mixed length/termination.
    if(leftLength >= 0) {
        if(rightLength < 0) { rightLength = u_strlen(right); }
    } else {
        if(rightLength >= 0) { leftLength = u_strlen(left); }
    }
    return doCompare(left, leftLength, right, rightLength, errorCode);
}

UCollationResult
RuleBasedCollator2::compareUTF8(const StringPiece &left, const StringPiece &right,
                                UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }
    const uint8_t *leftBytes = reinterpret_cast<const uint8_t *>(left.data());
    const uint8_t *rightBytes = reinterpret_cast<const uint8_t *>(right.data());
    if((leftBytes == NULL && !left.empty()) || (rightBytes == NULL && !right.empty())) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return UCOL_EQUAL;
    }
    return doCompare(leftBytes, left.length(), rightBytes, right.length(), errorCode);
}

UCollationResult
RuleBasedCollator2::compareUTF8(const char *left, int32_t leftLength,
                                const char *right, int32_t rightLength,
                                UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }
    if((left == NULL && leftLength != 0) || (right == NULL && rightLength != 0)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return UCOL_EQUAL;
    }
    // Make sure both or neither strings have a known length.
    // We do not optimize for mixed length/termination.
    if(leftLength >= 0) {
        if(rightLength < 0) { rightLength = uprv_strlen(right); }
    } else {
        if(rightLength >= 0) { leftLength = uprv_strlen(left); }
    }
    return doCompare(reinterpret_cast<const uint8_t *>(left), leftLength,
                     reinterpret_cast<const uint8_t *>(right), rightLength, errorCode);
}

namespace {

/**
 * Abstract iterator for identical-level string comparisons.
 * Returns FCD code points and handles temporary switching to NFD.
 */
class NFDIterator {
public:
    NFDIterator() : index(-1), length(0) {}
    virtual ~NFDIterator() {}
    /**
     * Returns the next code point from the internal normalization buffer,
     * or else the next text code point.
     * Returns -1 at the end of the text.
     */
    UChar32 nextCodePoint() {
        if(index >= 0) {
            if(index == length) {
                index = -1;
            } else {
                UChar32 c;
                U16_NEXT_UNSAFE(decomp, index, c);
                return c;
            }
        }
        return nextRawCodePoint();
    }
    /**
     * @param nfcImpl
     * @param c the last code point returned by nextCodePoint() or nextDecomposedCodePoint()
     * @return the first code point in c's decomposition,
     *         or c itself if it was decomposed already or if it does not decompose
     */
    UChar32 nextDecomposedCodePoint(const Normalizer2Impl &nfcImpl, UChar32 c) {
        if(index >= 0) { return c; }
        decomp = nfcImpl.getDecomposition(c, buffer, length);
        if(decomp == NULL) { return c; }
        index = 0;
        U16_NEXT_UNSAFE(decomp, index, c);
        return c;
    }
protected:
    /**
     * Returns the next text code point in FCD order.
     * Returns -1 at the end of the text.
     */
    virtual UChar32 nextRawCodePoint() = 0;
private:
    const UChar *decomp;
    UChar buffer[4];
    int32_t index;
    int32_t length;
};

class UTF16NFDIterator : public NFDIterator {
public:
    UTF16NFDIterator(const UChar *text, const UChar *textLimit) : s(text), limit(textLimit) {}
protected:
    virtual UChar32 nextRawCodePoint() {
        if(s == limit) { return U_SENTINEL; }
        UChar32 c = *s++;
        if(limit == NULL && c == 0) {
            s = NULL;
            return U_SENTINEL;
        }
        UChar trail;
        if(U16_IS_LEAD(c) && s != limit && U16_IS_TRAIL(trail = *s)) {
            ++s;
            c = U16_GET_SUPPLEMENTARY(c, trail);
        }
        return c;
    }

    const UChar *s;
    const UChar *limit;
};

class FCDUTF16NFDIterator : public UTF16NFDIterator {
public:
    FCDUTF16NFDIterator(const Normalizer2Impl &nfcImpl, const UChar *text, const UChar *textLimit)
            : UTF16NFDIterator(NULL, NULL) {
        UErrorCode errorCode = U_ZERO_ERROR;
        const UChar *spanLimit = nfcImpl.makeFCD(text, textLimit, NULL, errorCode);
        if(U_FAILURE(errorCode)) { return; }
        if(spanLimit == textLimit || (textLimit == NULL && *spanLimit == 0)) {
            s = text;
            limit = spanLimit;
        } else {
            str.setTo(text, (int32_t)(spanLimit - text));
            {
                ReorderingBuffer buffer(nfcImpl, str);
                if(buffer.init(str.length(), errorCode)) {
                    nfcImpl.makeFCD(spanLimit, textLimit, &buffer, errorCode);
                }
            }
            if(U_SUCCESS(errorCode)) {
                s = str.getBuffer();
                limit = s + str.length();
            }
        }
    }
private:
    UnicodeString str;
};

class UTF8NFDIterator : public NFDIterator {
public:
    UTF8NFDIterator(const uint8_t *text, int32_t textLength)
        : s(text), pos(0), length(textLength) {}
protected:
    virtual UChar32 nextRawCodePoint() {
        if(pos == length || (s[pos] == 0 && length < 0)) { return U_SENTINEL; }
        UChar32 c;
        U8_NEXT_OR_FFFD(s, pos, length, c);
        return c;
    }

    const uint8_t *s;
    int32_t pos;
    int32_t length;
};

class FCDUTF8NFDIterator : public NFDIterator {
public:
    FCDUTF8NFDIterator(const CollationData *data, const uint8_t *text, int32_t textLength)
            : u8ci(data, text, textLength) {}
protected:
    virtual UChar32 nextRawCodePoint() {
        UErrorCode errorCode = U_ZERO_ERROR;
        return u8ci.nextCodePoint(errorCode);
    }
private:
    FCDUTF8CollationIterator u8ci;
};

class UIterNFDIterator : public NFDIterator {
public:
    UIterNFDIterator(UCharIterator &it) : iter(it) {}
protected:
    virtual UChar32 nextRawCodePoint() {
        return uiter_next32(&iter);
    }
private:
    UCharIterator &iter;
};

class FCDUIterNFDIterator : public NFDIterator {
public:
    FCDUIterNFDIterator(const CollationData *data, UCharIterator &it, int32_t startIndex)
            : uici(data, it, startIndex) {}
protected:
    virtual UChar32 nextRawCodePoint() {
        UErrorCode errorCode = U_ZERO_ERROR;
        return uici.nextCodePoint(errorCode);
    }
private:
    FCDUIterCollationIterator uici;
};

UCollationResult compareNFDIter(const Normalizer2Impl &nfcImpl,
                                NFDIterator &left, NFDIterator &right) {
    for(;;) {
        // Fetch the next FCD code point from each string.
        UChar32 leftCp = left.nextCodePoint();
        UChar32 rightCp = right.nextCodePoint();
        if(leftCp == rightCp) {
            if(leftCp < 0) { break; }
            continue;
        }
        // If they are different, then decompose each and compare again.
        if(leftCp < 0) {
            leftCp = -2;  // end of string
        } else if(leftCp == 0xfffe) {
            leftCp = -1;  // U+FFFE: merge separator
        } else {
            leftCp = left.nextDecomposedCodePoint(nfcImpl, leftCp);
        }
        if(rightCp < 0) {
            rightCp = -2;  // end of string
        } else if(rightCp == 0xfffe) {
            rightCp = -1;  // U+FFFE: merge separator
        } else {
            rightCp = right.nextDecomposedCodePoint(nfcImpl, rightCp);
        }
        if(leftCp < rightCp) { return UCOL_LESS; }
        if(leftCp > rightCp) { return UCOL_GREATER; }
    }
    return UCOL_EQUAL;
}

}  // namespace

UCollationResult
RuleBasedCollator2::doCompare(const UChar *left, int32_t leftLength,
                              const UChar *right, int32_t rightLength,
                              UErrorCode &errorCode) const {
    // U_FAILURE(errorCode) checked by caller.
    if(left == right && leftLength == rightLength) {
        return UCOL_EQUAL;
    }

    // Identical-prefix test.
    const UChar *leftLimit;
    const UChar *rightLimit;
    int32_t equalPrefixLength = 0;
    if(leftLength < 0) {
        leftLimit = NULL;
        rightLimit = NULL;
        UChar c;
        while((c = left[equalPrefixLength]) == right[equalPrefixLength]) {
            if(c == 0) { return UCOL_EQUAL; }
            ++equalPrefixLength;
        }
    } else {
        leftLimit = left + leftLength;
        rightLimit = right + rightLength;
        for(;;) {
            if(equalPrefixLength == leftLength) {
                if(equalPrefixLength == rightLength) { return UCOL_EQUAL; }
                break;
            } else if(equalPrefixLength == rightLength ||
                      left[equalPrefixLength] != right[equalPrefixLength]) {
                break;
            }
            ++equalPrefixLength;
        }
    }

    if(equalPrefixLength > 0) {
        if((equalPrefixLength != leftLength && data->isUnsafeBackward(left[equalPrefixLength])) ||
                (equalPrefixLength != rightLength && data->isUnsafeBackward(right[equalPrefixLength]))) {
            // Identical prefix: Back up to the start of a contraction or reordering sequence.
            while(--equalPrefixLength > 0 && data->isUnsafeBackward(left[equalPrefixLength])) {}
        } else if((data->options & CollationData::BACKWARD_SECONDARY) != 0) {
            // With a backward level, a longer string can compare less-than a prefix of it.
            // TODO: add a unit test for this case
        } else if(equalPrefixLength == leftLength) {
            return UCOL_LESS;
        } else if(equalPrefixLength == rightLength) {
            return UCOL_GREATER;
        }
        left += equalPrefixLength;
        right += equalPrefixLength;
    }

    UCollationResult result;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF16CollationIterator leftIter(data, left, leftLimit);
        UTF16CollationIterator rightIter(data, right, rightLimit);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    } else {
        FCDUTF16CollationIterator leftIter(data, left, leftLimit);
        FCDUTF16CollationIterator rightIter(data, right, rightLimit);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    }
    if(result != UCOL_EQUAL || data->getStrength() < UCOL_IDENTICAL || U_FAILURE(errorCode)) {
        return result;
    }

    // TODO: If NUL-terminated, get actual limits from iterators?

    // Compare identical level.
    const Normalizer2Impl &nfcImpl = data->nfcImpl;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF16NFDIterator leftIter(left, leftLimit);
        UTF16NFDIterator rightIter(right, rightLimit);
        result = compareNFDIter(nfcImpl, leftIter, rightIter);
    } else {
        FCDUTF16NFDIterator leftIter(nfcImpl, left, leftLimit);
        FCDUTF16NFDIterator rightIter(nfcImpl, right, rightLimit);
        result = compareNFDIter(nfcImpl, leftIter, rightIter);
    }
    return result;
}

UCollationResult
RuleBasedCollator2::doCompare(const uint8_t *left, int32_t leftLength,
                              const uint8_t *right, int32_t rightLength,
                              UErrorCode &errorCode) const {
    // U_FAILURE(errorCode) checked by caller.
    if(left == right && leftLength == rightLength) {
        return UCOL_EQUAL;
    }

    // Identical-prefix test.
    int32_t equalPrefixLength = 0;
    if(leftLength < 0) {
        uint8_t c;
        while((c = left[equalPrefixLength]) == right[equalPrefixLength]) {
            if(c == 0) { return UCOL_EQUAL; }
            ++equalPrefixLength;
        }
    } else {
        for(;;) {
            if(equalPrefixLength == leftLength) {
                if(equalPrefixLength == rightLength) { return UCOL_EQUAL; }
                break;
            } else if(equalPrefixLength == rightLength ||
                      left[equalPrefixLength] != right[equalPrefixLength]) {
                break;
            }
            ++equalPrefixLength;
        }
    }
    // Back up to the start of a partially-equal code point.
    if(equalPrefixLength > 0 &&
            ((equalPrefixLength != leftLength && U8_IS_TRAIL(left[equalPrefixLength])) ||
            (equalPrefixLength != rightLength && U8_IS_TRAIL(right[equalPrefixLength])))) {
        while(--equalPrefixLength > 0 && U8_IS_TRAIL(left[equalPrefixLength])) {}
    }

    if(equalPrefixLength > 0) {
        UBool unsafe = FALSE;
        if(equalPrefixLength != leftLength) {
            int32_t i = equalPrefixLength;
            UChar32 c;
            U8_NEXT_OR_FFFD(left, i, leftLength, c);
            unsafe = data->isUnsafeBackward(c);
        }
        if(!unsafe && equalPrefixLength != rightLength) {
            int32_t i = equalPrefixLength;
            UChar32 c;
            U8_NEXT_OR_FFFD(left, i, leftLength, c);
            unsafe = data->isUnsafeBackward(c);
        }
        if(unsafe) {
            // Identical prefix: Back up to the start of a contraction or reordering sequence.
            UChar32 c;
            do {
                U8_PREV_OR_FFFD(left, 0, equalPrefixLength, c);
            } while(equalPrefixLength > 0 && data->isUnsafeBackward(c));
        } else if((data->options & CollationData::BACKWARD_SECONDARY) != 0) {
            // With a backward level, a longer string can compare less-than a prefix of it.
        } else if(equalPrefixLength == leftLength) {
            return UCOL_LESS;
        } else if(equalPrefixLength == rightLength) {
            return UCOL_GREATER;
        }
        left += equalPrefixLength;
        right += equalPrefixLength;
        if(leftLength > 0) {
            leftLength -= equalPrefixLength;
            rightLength -= equalPrefixLength;
        }
    }

    UCollationResult result;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF8CollationIterator leftIter(data, left, leftLength);
        UTF8CollationIterator rightIter(data, right, rightLength);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    } else {
        FCDUTF8CollationIterator leftIter(data, left, leftLength);
        FCDUTF8CollationIterator rightIter(data, right, rightLength);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    }
    if(result != UCOL_EQUAL || data->getStrength() < UCOL_IDENTICAL || U_FAILURE(errorCode)) {
        return result;
    }

    // TODO: If NUL-terminated, get actual limits from iterators?

    // Compare identical level.
    const Normalizer2Impl &nfcImpl = data->nfcImpl;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF8NFDIterator leftIter(left, leftLength);
        UTF8NFDIterator rightIter(right, rightLength);
        result = compareNFDIter(nfcImpl, leftIter, rightIter);
    } else {
        FCDUTF8NFDIterator leftIter(data, left, leftLength);
        FCDUTF8NFDIterator rightIter(data, right, rightLength);
        result = compareNFDIter(nfcImpl, leftIter, rightIter);
    }
    return result;
}

UCollationResult
RuleBasedCollator2::compare(UCharIterator &left, UCharIterator &right,
                            UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode) || &left == &right) { return UCOL_EQUAL; }

    // Identical-prefix test.
    int32_t equalPrefixLength = 0;
    {
        UChar32 leftUnit;
        UChar32 rightUnit;
        while((leftUnit = left.next(&left)) == (rightUnit = right.next(&right))) {
            if(leftUnit < 0) { return UCOL_EQUAL; }
            ++equalPrefixLength;
        }

        // Back out the code units that differed, for the real collation comparison.
        if(leftUnit >= 0) { left.previous(&left); }
        if(rightUnit >= 0) { right.previous(&right); }

        if(equalPrefixLength > 0) {
            if((leftUnit >= 0 && data->isUnsafeBackward(leftUnit)) ||
                    (rightUnit >= 0 && data->isUnsafeBackward(rightUnit))) {
                // Identical prefix: Back up to the start of a contraction or reordering sequence.
                do {
                    --equalPrefixLength;
                    leftUnit = left.previous(&left);
                    right.previous(&right);
                } while(equalPrefixLength > 0 && data->isUnsafeBackward(leftUnit));
            } else if((data->options & CollationData::BACKWARD_SECONDARY) != 0) {
                // With a backward level, a longer string can compare less-than a prefix of it.
            } else if(leftUnit < 0) {
                return UCOL_LESS;
            } else if(rightUnit < 0) {
                return UCOL_GREATER;
            }
        }
    }

    UCollationResult result;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UIterCollationIterator leftIter(data, left);
        UIterCollationIterator rightIter(data, right);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    } else {
        FCDUIterCollationIterator leftIter(data, left, equalPrefixLength);
        FCDUIterCollationIterator rightIter(data, right, equalPrefixLength);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    }
    if(result != UCOL_EQUAL || data->getStrength() < UCOL_IDENTICAL || U_FAILURE(errorCode)) {
        return result;
    }

    // Compare identical level.
    left.move(&left, equalPrefixLength, UITER_ZERO);
    right.move(&right, equalPrefixLength, UITER_ZERO);
    const Normalizer2Impl &nfcImpl = data->nfcImpl;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UIterNFDIterator leftIter(left);
        UIterNFDIterator rightIter(right);
        result = compareNFDIter(nfcImpl, leftIter, rightIter);
    } else {
        FCDUIterNFDIterator leftIter(data, left, equalPrefixLength);
        FCDUIterNFDIterator rightIter(data, right, equalPrefixLength);
        result = compareNFDIter(nfcImpl, leftIter, rightIter);
    }
    return result;
}

CollationKey &
RuleBasedCollator2::getCollationKey(const UnicodeString &s, CollationKey &key,
                                    UErrorCode &errorCode) const {
    return getCollationKey(s.getBuffer(), s.length(), key, errorCode);
}

CollationKey &
RuleBasedCollator2::getCollationKey(const UChar *s, int32_t length, CollationKey& key,
                                    UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return key; }
    if(s == NULL && length != 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return key;
    }
    CollationKeyByteSink2 sink(key);
    writeSortKey(s, length, sink, errorCode);
    if(U_FAILURE(errorCode)) {
        key.setToBogus();
    } else if(key.isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    } else {
        key.setLength(sink.NumberOfBytesAppended());
    }
    return key;
}

int32_t
RuleBasedCollator2::getSortKey(const UnicodeString &s,
                               uint8_t *dest, int32_t capacity) const {
    return getSortKey(s.getBuffer(), s.length(), dest, capacity);
}

int32_t
RuleBasedCollator2::getSortKey(const UChar *s, int32_t length,
                               uint8_t *dest, int32_t capacity) const {
    if((s == NULL && length != 0) || capacity < 0 || (dest == NULL && capacity > 0)) {
        return 0;
    }
    uint8_t noDest[1] = { 0 };
    if(dest == NULL) {
        // Distinguish pure preflighting from an allocation error.
        dest = noDest;
        capacity = 0;
    }
    FixedSortKeyByteSink sink(reinterpret_cast<char *>(dest), capacity);
    UErrorCode errorCode = U_ZERO_ERROR;
    writeSortKey(s, length, sink, errorCode);
    return U_SUCCESS(errorCode) ? sink.NumberOfBytesAppended() : 0;
}

void
RuleBasedCollator2::writeSortKey(const UChar *s, int32_t length,
                                 SortKeyByteSink2 &sink, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return; }
    const UChar *limit = (length >= 0) ? s + length : NULL;
    CollationKeys::LevelCallback callback;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF16CollationIterator iter(data, s, limit);
        CollationKeys::writeSortKeyUpToQuaternary(iter, sink, Collation::PRIMARY_LEVEL,
                                                  callback, errorCode);
    } else {
        FCDUTF16CollationIterator iter(data, s, limit);
        CollationKeys::writeSortKeyUpToQuaternary(iter, sink, Collation::PRIMARY_LEVEL,
                                                  callback, errorCode);
    }
    if(data->getStrength() == UCOL_IDENTICAL) {
        writeIdenticalLevel(s, limit, sink, errorCode);
    }
    static const char terminator = 0;  // TERMINATOR_BYTE
    sink.Append(&terminator, 1);
}

void
RuleBasedCollator2::writeIdenticalLevel(const UChar *s, const UChar *limit,
                                        SortKeyByteSink2 &sink, UErrorCode &errorCode) const {
    // NFD quick check
    const UChar *nfdQCYesLimit = data->nfcImpl.decompose(s, limit, NULL, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    sink.Append(Collation::LEVEL_SEPARATOR_BYTE);
    UChar32 prev = 0;
    if(nfdQCYesLimit != s) {
        prev = u_writeIdenticalLevelRun(prev, s, (int32_t)(nfdQCYesLimit - s), sink);
    }
    // Is there non-NFD text?
    int32_t destLengthEstimate;
    if(limit != NULL) {
        if(nfdQCYesLimit == limit) { return; }
        destLengthEstimate = (int32_t)(limit - nfdQCYesLimit);
    } else {
        // s is NUL-terminated
        if(*nfdQCYesLimit == 0) { return; }
        destLengthEstimate = -1;
    }
    UnicodeString nfd;
    data->nfcImpl.decompose(nfdQCYesLimit, limit, nfd, -1, errorCode);
    u_writeIdenticalLevelRun(prev, nfd.getBuffer(), nfd.length(), sink);
}

namespace {

/**
 * nextSortKeyPart() calls CollationKeys::writeSortKeyUpToQuaternary()
 * with an instance of this callback class.
 * When another level is about to be written, the callback
 * records the level and the number of bytes that will be written until
 * the sink (which is actually a FixedSortKeyByteSink) fills up.
 *
 * When nextSortKeyPart() is called again, it restarts with the last level
 * and ignores as many bytes as were written previously for that level.
 */
class PartLevelCallback : public CollationKeys::LevelCallback {
public:
    PartLevelCallback(const SortKeyByteSink2 &s)
            : sink(s), level(Collation::PRIMARY_LEVEL) {
        levelCapacity = sink.GetRemainingCapacity();
    }
    virtual ~PartLevelCallback() {}
    virtual UBool needToWrite(Collation::Level l) {
        if(!sink.Overflowed()) {
            // Remember a level that will be at least partially written.
            level = l;
            levelCapacity = sink.GetRemainingCapacity();
            return TRUE;
        } else {
            return FALSE;
        }
    }
    Collation::Level getLevel() const { return level; }
    int32_t getLevelCapacity() const { return levelCapacity; }

private:
    const SortKeyByteSink2 &sink;
    Collation::Level level;
    int32_t levelCapacity;
};

}  // namespace

int32_t
RuleBasedCollator2::nextSortKeyPart(UCharIterator *iter, uint32_t state[2],
                                    uint8_t *dest, int32_t count, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    if(iter == NULL || state == NULL || count < 0 || (count > 0 && dest == NULL)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if(count == 0) { return 0; }

    // We currently assume that iter is at its start.

    FixedSortKeyByteSink sink(reinterpret_cast<char *>(dest), count);
    sink.IgnoreBytes((int32_t)state[1]);

    Collation::Level level = (Collation::Level)state[0];
    if(level <= Collation::QUATERNARY_LEVEL) {
        PartLevelCallback callback(sink);
        if((data->options & CollationData::CHECK_FCD) == 0) {
            UIterCollationIterator ci(data, *iter);
            CollationKeys::writeSortKeyUpToQuaternary(ci, sink, level, callback, errorCode);
        } else {
            FCDUIterCollationIterator ci(data, *iter, 0);
            CollationKeys::writeSortKeyUpToQuaternary(ci, sink, level, callback, errorCode);
        }
        if(U_FAILURE(errorCode)) { return 0; }
        if(sink.NumberOfBytesAppended() > count) {
            state[0] = (uint32_t)callback.getLevel();
            state[1] = (uint32_t)callback.getLevelCapacity();
            return count;
        }
        // All of the normal levels are done.
        if(data->getStrength() == UCOL_IDENTICAL) {
            level = Collation::IDENTICAL_LEVEL;
            iter->move(iter, 0, UITER_START);
        }
        // else fall through to setting ZERO_LEVEL
    }

    if(level == Collation::IDENTICAL_LEVEL) {
        // TODO: Remove u_writeIdenticalLevelRunTwoChars() since this v2 implementation
        // does not need it. If it remains, fix its U+FFFE handling.
        int32_t levelCapacity = sink.GetRemainingCapacity();
        UnicodeString s;
        for(;;) {
            UChar32 c = iter->next(iter);
            if(c < 0) { break; }
            s.append((UChar)c);
        }
        const UChar *sArray = s.getBuffer();
        writeIdenticalLevel(sArray, sArray + s.length(), sink, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        if(sink.NumberOfBytesAppended() > count) {
            state[0] = (uint32_t)level;
            state[1] = (uint32_t)levelCapacity;
            return count;
        }
    }

    // ZERO_LEVEL: Fill the remainder of dest with 00 bytes.
    state[0] = (uint32_t)Collation::ZERO_LEVEL;
    state[1] = 0;
    int32_t length = sink.NumberOfBytesAppended();
    int32_t i = length;
    while(i < count) { dest[i++] = 0; }
    return length;
}

CollationElementIterator *
RuleBasedCollator2::createCollationElementIterator(const UnicodeString& source) const {
    return NULL;  // TODO
}

CollationElementIterator *
RuleBasedCollator2::createCollationElementIterator(const CharacterIterator& source) const {
    return NULL;  // TODO
}

// TODO: If we generate a Latin-1 sort key table like in v1,
// then make sure to exclude contractions that contain non-Latin-1 characters.
// (For example, decomposed versions of Latin-1 composites.)
// (Unless there is special, clever contraction-matching code that can consume them.)

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
