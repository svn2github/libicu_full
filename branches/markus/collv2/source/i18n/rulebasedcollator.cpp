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
#include "unicode/uiter.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/ucol.h"
#include "bocsu.h"
#include "cmemory.h"
#include "collation.h"
#include "collationcompare.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "collationkeys.h"
#include "rulebasedcollator.h"
#include "uassert.h"
#include "uitercollationiterator.h"
#include "utf16collationiterator.h"

U_NAMESPACE_BEGIN

// TODO: CollationKey should have
// - internal buffer of some 32 bytes (make sizeof(CollationKey)=64)
// - ensureCapacity() does not at all work as its name suggests
//   - should return UBool and not changing anything if capacity suffices
//   - should not memset(old capacity) to 0
//   - should copy fCount bytes
//   - should not set fCount=new capacity
class CollationKeyByteSink : public ByteSink {
public:
    CollationKeyByteSink(CollationKey &k) : key(k) { key.reset(); }
    virtual ~CollationKeyByteSink();
    virtual void Append(const char *bytes, int32_t n);
    virtual char* GetAppendBuffer(int32_t min_capacity,
                                  int32_t desired_capacity_hint,
                                  char *scratch, int32_t scratch_capacity,
                                  int32_t *result_capacity);
private:
    CollationKey &key;
    CollationKeyByteSink();  ///< default constructor not implemented 
    CollationKeyByteSink(const CollationKeyByteSink &);  ///< copy constructor not implemented
    CollationKeyByteSink &operator=(const CollationKeyByteSink &);  ///< assignment operator not implemented
};

CollationKeyByteSink::~CollationKeyByteSink() {}

void
CollationKeyByteSink::Append(const char *bytes, int32_t n) {
    if(n <= 0 || key.fBogus) {
        return;
    }
    int32_t newLength = key.fCount + n;
    if(newLength > key.fCapacity) {
        int32_t newCapacity = 2 * key.fCapacity;
        if(newCapacity < 32) {
            newCapacity = 32;
        } else if(newCapacity < 200) {
            newCapacity = 200;
        }
        if(newCapacity < newLength) {
            newCapacity = key.fCount + 2 * n;
        }
        uint8_t *newBytes = static_cast<uint8_t *>(uprv_malloc(newCapacity));
        if(newBytes == NULL) {
            key.setToBogus();
            return;
        }
        if(key.fCount != 0) {
            uprv_memcpy(newBytes, key.fBytes, key.fCount);
        }
        uprv_free(key.fBytes);
        key.fBytes = newBytes;
        key.fCapacity = newCapacity;
    }
    if(n > 0 && bytes != (reinterpret_cast<char *>(key.fBytes) + key.fCount)) {
        uprv_memcpy(key.fBytes + key.fCount, bytes, n);
    }
    key.fCount = newLength;
}

char *
CollationKeyByteSink::GetAppendBuffer(int32_t min_capacity,
                                      int32_t /*desired_capacity_hint*/,
                                      char *scratch,
                                      int32_t scratch_capacity,
                                      int32_t *result_capacity) {
    if(min_capacity < 1 || scratch_capacity < min_capacity) {
        *result_capacity = 0;
        return NULL;
    }
    int32_t available = key.fCapacity - key.fCount;
    if(available >= min_capacity) {
        *result_capacity = available;
        return reinterpret_cast<char *>(key.fBytes) + key.fCount;
    } else {
        *result_capacity = scratch_capacity;
        return scratch;
    }
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
                U16_NEXT_UNSAFE(buffer, index, c);
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
        const UChar *s = nfcImpl.getDecomposition(c, buffer, length);
        if(s == NULL) { return c; }
        index = 0;
        U16_NEXT_UNSAFE(buffer, index, c);
        return c;
    }
protected:
    /**
     * Returns the next text code point in FCD order.
     * Returns -1 at the end of the text.
     */
    virtual UChar32 nextRawCodePoint() = 0;
private:
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
    FCDUIterNFDIterator(const CollationData *data, UCharIterator &it) : uici(data, it) {}
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
        FCDUIterCollationIterator leftIter(data, left);
        FCDUIterCollationIterator rightIter(data, right);
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
        FCDUIterNFDIterator leftIter(data, left);
        FCDUIterNFDIterator rightIter(data, right);
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
    CollationKeyByteSink sink(key);
    writeSortKey(s, length, sink, errorCode);
    if(U_SUCCESS(errorCode)) {
        if(key.isBogus()) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
        }
    } else {
        key.setToBogus();
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
    CheckedArrayByteSink sink(reinterpret_cast<char *>(dest), capacity);
    UErrorCode errorCode = U_ZERO_ERROR;
    writeSortKey(s, length, sink, errorCode);
    return U_SUCCESS(errorCode) ? sink.NumberOfBytesAppended() : 0;
}

void
RuleBasedCollator2::writeSortKey(const UChar *s, int32_t length,
                                 ByteSink &sink, UErrorCode &errorCode) const {
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
        const Normalizer2 *norm2 = Normalizer2Factory::getNFDInstance(errorCode);
        if(U_FAILURE(errorCode)) { return; }
        UnicodeString source((UBool)(length < 0), s, length);
        int32_t qcYesLength = norm2->spanQuickCheckYes(source, errorCode);
        static const char separator = 1;  // LEVEL_SEPARATOR_BYTE
        sink.Append(&separator, 1);
        UChar32 prev = 0;
        if(qcYesLength > 0) {
            prev = u_writeIdenticalLevelRun(prev, s, qcYesLength, sink);
        }
        if(qcYesLength < source.length()) {
            UnicodeString rest(FALSE, s + qcYesLength, source.length() - qcYesLength);
            UnicodeString nfd;
            norm2->normalize(rest, nfd, errorCode);
            u_writeIdenticalLevelRun(prev, nfd.getBuffer(), nfd.length(), sink);
        }
    }
    static const char terminator = 0;  // TERMINATOR_BYTE
    sink.Append(&terminator, 1);
}

CollationElementIterator *
RuleBasedCollator2::createCollationElementIterator(const UnicodeString& source) const {
    return NULL;  // TODO
}

CollationElementIterator *
RuleBasedCollator2::createCollationElementIterator(const CharacterIterator& source) const {
    return NULL;  // TODO
}

// TODO: For sort key generation, an iterator like QuaternaryIterator might be useful.
// Public fields p, s, t, q for the result weights. UBool next().
// Should help share code between normal & partial sortkey generation.

// TODO: If we generate a Latin-1 sort key table like in v1,
// then make sure to exclude contractions that contain non-Latin-1 characters.
// (For example, decomposed versions of Latin-1 composites.)
// (Unless there is special, clever contraction-matching code that can consume them.)

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
