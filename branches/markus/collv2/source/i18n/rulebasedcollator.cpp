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
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/ucol.h"
#include "cmemory.h"
#include "collation.h"
#include "collationcompare.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "rulebasedcollator.h"
#include "uassert.h"
#include "utf16collationiterator.h"

U_NAMESPACE_BEGIN

// TODO: Add UTRACE_... calls back in.

RuleBasedCollator2::~RuleBasedCollator2() {
    if(ownedData != NULL) {
        delete ownedData->reorderTable;
        delete ownedData;
    }
}

Collator *
RuleBasedCollator2::clone() const {
    return NULL;  // TODO
}

Collator *
RuleBasedCollator2::safeClone() {
    // TODO: safeClone() should be const, see http://bugs.icu-project.org/trac/ticket/9346
    return NULL;  // TODO
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedCollator2)

UBool
RuleBasedCollator2::operator==(const Collator& other) const {
    return FALSE;  // TODO
}

UBool
RuleBasedCollator2::operator!=(const Collator& other) const {
    return FALSE;  // TODO
}

int32_t
RuleBasedCollator2::hashCode() const {
    return 0;  // TODO
}

const Locale
RuleBasedCollator2::getLocale(ULocDataLocaleType type, UErrorCode& status) const {
    return Locale::getDefault();
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

UColAttributeValue
RuleBasedCollator2::getAttribute(UColAttribute attr, UErrorCode &errorCode) {
    return UCOL_DEFAULT;  // TODO
}

void
RuleBasedCollator2::setAttribute(UColAttribute attr, UColAttributeValue value,
                                 UErrorCode &errorCode) {
    // TODO
}

Collator::ECollationStrength
RuleBasedCollator2::getStrength() const {
    UErrorCode errorCode = U_ZERO_ERROR;
    // TODO: getAttribute() should be const, see http://bugs.icu-project.org/trac/ticket/9346
    return (ECollationStrength)const_cast<RuleBasedCollator2 *>(this)->getAttribute(UCOL_STRENGTH, errorCode);
}

void
RuleBasedCollator2::setStrength(ECollationStrength strength) {
    UErrorCode errorCode = U_ZERO_ERROR;
    setAttribute(UCOL_STRENGTH, (UColAttributeValue)strength, errorCode);
}

uint32_t
RuleBasedCollator2::getVariableTop(UErrorCode &errorCode) const {
    return 0;  // TODO
}

uint32_t
RuleBasedCollator2::setVariableTop(const UChar *varTop, int32_t len, UErrorCode &errorCode) {
    return 0;  // TODO
}

uint32_t
RuleBasedCollator2::setVariableTop(const UnicodeString varTop, UErrorCode &errorCode) {
    return 0;  // TODO
}

void
RuleBasedCollator2::setVariableTop(const uint32_t varTop, UErrorCode &errorCode) {
    // TODO
}

int32_t
RuleBasedCollator2::getReorderCodes(int32_t *dest,
                                    int32_t destCapacity,
                                    UErrorCode &errorCode) const {
    return 0;  // TODO
}

void
RuleBasedCollator2::setReorderCodes(const int32_t *reorderCodes,
                                    int32_t reorderCodesLength,
                                    UErrorCode &errorCode) {
    // TODO
}

/* TODO int32_t getEquivalentReorderCodes(int32_t reorderCode,
                                int32_t* dest,
                                int32_t destCapacity,
                                UErrorCode& status) */


UCollationResult
RuleBasedCollator2::compare(const UnicodeString &left, const UnicodeString &right,
                            UErrorCode &errorCode) const {
    return compare(left.getBuffer(), left.length(),
                   right.getBuffer(), right.length(), FALSE, errorCode);
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
    return compare(left.getBuffer(), leftLength,
                   right.getBuffer(), rightLength, FALSE, errorCode);
}

UCollationResult
RuleBasedCollator2::compare(const UChar *left, int32_t leftLength,
                            const UChar *right, int32_t rightLength,
                            UErrorCode &errorCode) const {
    return compare(left, leftLength, right, rightLength, TRUE, errorCode);
}

UCollationResult
RuleBasedCollator2::compare(const UChar *left, int32_t leftLength,
                            const UChar *right, int32_t rightLength,
                            UBool maybeNUL,
                            UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return UCOL_EQUAL; }
    if((left == NULL && (leftLength != 0)) || (right == NULL && (rightLength != 0))) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return UCOL_EQUAL;
    }

    const UChar *leftLimit;
    const UChar *rightLimit;
    if(maybeNUL) {
        leftLimit = (leftLength >= 0) ? left + leftLength : NULL;
        rightLimit = (rightLength >= 0) ? right + rightLength : NULL;
    } else {
        U_ASSERT(leftLength >= 0);
        U_ASSERT(rightLength >= 0);
        leftLimit = left + leftLength;
        rightLimit = right + rightLength;
    }
    if(left == right && leftLength == rightLength) {
        return UCOL_EQUAL;
    }

    // TODO: Add identical-prefix test.
    int32_t strength = data->getStrength();
    UCollationResult result;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        UTF16CollationIterator leftIter(data, left, leftLimit);
        UTF16CollationIterator rightIter(data, right, rightLimit);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    } else {
        FCDUTF16CollationIterator leftIter(data, left, leftLimit, errorCode);
        FCDUTF16CollationIterator rightIter(data, right, rightLimit, errorCode);
        result = CollationCompare::compareUpToQuaternary(leftIter, rightIter, errorCode);
    }
    if(result != UCOL_EQUAL || strength < UCOL_IDENTICAL || U_FAILURE(errorCode)) {
        return result;
    }

    // TODO: If NUL-terminated, get actual limits from iterators?

    uint32_t options = U_COMPARE_CODE_POINT_ORDER;
    if((data->options & CollationData::CHECK_FCD) == 0) {
        options |= UNORM_INPUT_IS_FCD;
    }
    int32_t intResult = unorm_compare(left, leftLength, right, rightLength, options, &errorCode);
    return (intResult == 0) ? UCOL_EQUAL : (intResult < 0) ? UCOL_LESS : UCOL_GREATER;
}

CollationKey &
RuleBasedCollator2::getCollationKey(const UnicodeString &source,
                                    CollationKey &key,
                                    UErrorCode &errorCode) const {
    return key;  // TODO
}

CollationKey &
RuleBasedCollator2::getCollationKey(const UChar *source, int32_t sourceLength,
                                    CollationKey& key,
                                    UErrorCode &errorCode) const {
    return key;  // TODO
}

int32_t
RuleBasedCollator2::getSortKey(const UnicodeString &source, uint8_t *result,
                               int32_t resultLength) const {
    return 0;  // TODO
}

int32_t
RuleBasedCollator2::getSortKey(const UChar *source, int32_t sourceLength,
                               uint8_t *result, int32_t resultLength) const {
    return 0;  // TODO
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
