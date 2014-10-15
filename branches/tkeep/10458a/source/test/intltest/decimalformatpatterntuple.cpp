/*
*******************************************************************************
* Copyright (C) 1997-2014, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "decimalformatpatterntuple.h"
#include "ustrfmt.h"
#include "charstr.h"
#include "cstring.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

DecimalFormatPattern *gNullPtr = NULL;

#define FIELD_OFFSET(fieldName) ((int32_t) (((char *) &gNullPtr->fieldName) - ((char *) gNullPtr)))

static void strToDig(
        const UnicodeString &str, void *digPtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    CharString buffer;
    buffer.appendInvariantChars(str, status);
    static_cast<DigitList *>(digPtr)->set(buffer.data(), status);
}

static void digToStr(
        const void *digPtr, UnicodeString &appendTo) {
    UErrorCode status = U_ZERO_ERROR;
    CharString chrStr;
    static_cast<const DigitList *>(digPtr)->getDecimal(chrStr, status);
    appendTo.append(UnicodeString(chrStr.data()));
}

static void digCopy(const void *src, void *dest) {
    *static_cast<DigitList *>(dest) = *static_cast<const DigitList *>(src);
}

static UBool digEqual(const void *expected, const void *actual) {
    return *static_cast<const DigitList *>(actual)
             == *static_cast<const DigitList *>(expected);
}

static void strToBool(
        const UnicodeString &str, void *boolPtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (str.length() != 1) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if (str[0] == 0x59 || str[0] == 0x79) { // 'Y' 'y'
        *static_cast<UBool *>(boolPtr) = TRUE;
        return;
    }
    if (str[0] == 0x4E || str[0] == 0x6E) { // 'N' 'n'
        *static_cast<UBool *>(boolPtr) = FALSE;
        return;
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
}
 
static void boolToStr(
        const void *boolPtr, UnicodeString &appendTo) {
    if (*static_cast<const UBool *>(boolPtr)) {
        appendTo.append("Y");
    } else {
        appendTo.append("N");
    }
}
 
static void boolCopy(const void *src, void *dest) {
    *static_cast<UBool *>(dest) = *static_cast<const UBool *>(src);
}

static UBool boolEqual(const void *expected, const void *actual) {
    return *static_cast<const UBool *>(actual)
            == *static_cast<const UBool *>(expected);
}

static void identVal(
        const UnicodeString &str, void *strPtr, UErrorCode & /*status*/) {
    *static_cast<UnicodeString *>(strPtr) = str;
}
 
static void identStr(
        const void *strPtr, UnicodeString &appendTo) {
    appendTo.append(*static_cast<const UnicodeString *>(strPtr));
}
 
static void uStrCopy(const void *src, void *dest) {
    *static_cast<UnicodeString *>(dest) =
            *static_cast<const UnicodeString *>(src);
}

static UBool uStrEqual(const void *expected, const void *actual) {
    return *static_cast<const UnicodeString *>(actual) ==
            *static_cast<const UnicodeString *>(expected);
}

static void strToInt(
        const UnicodeString &str, void *intPtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t len = str.length();
    int32_t start = 0;
    UBool neg = FALSE;
    if (len > 0 && str[0] == 0x2D) { // negative
        neg = TRUE;
        start = 1;
    }
    if (start == len) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int32_t value = 0;
    for (int32_t i = start; i < len; ++i) {
        UChar ch = str[i];
        if (ch < 0x30 || ch > 0x39) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        value = value * 10 - 0x30 + (int32_t) ch;
    }
    if (neg) {
        value = -value;
    }
    *static_cast<int32_t *>(intPtr) = value;
}

static void intToStr(
        const void *intPtr, UnicodeString &appendTo) {
    UChar buffer[20];
    int32_t x = *static_cast<const int32_t *>(intPtr);
    UBool neg = FALSE;
    if (x < 0) {
        neg = TRUE;
        x = -x;
    }
    if (neg) {
        appendTo.append(0x2D);
    }
    int32_t len = uprv_itou(buffer, UPRV_LENGTHOF(buffer), (uint32_t) x, 10, 1);
    appendTo.append(buffer, 0, len);
}

static void intCopy(const void *src, void *dest) {
    *static_cast<int32_t *>(dest) = *static_cast<const int32_t *>(src);
}

static UBool intEqual(const void *expected, const void *actual) {
    return *static_cast<const int32_t *>(actual) == *static_cast<const int32_t *>(expected);
}

static void strToEPadPosition(
        const UnicodeString &str, void *ePadPtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    DecimalFormatPattern::EPadPosition *ptr =
            static_cast<DecimalFormatPattern::EPadPosition *>(ePadPtr);
    if (str == "PadBeforePrefix") {
        *ptr = DecimalFormatPattern::kPadBeforePrefix;
    } else if (str == "PadAfterPrefix") {
        *ptr = DecimalFormatPattern::kPadAfterPrefix;
    } else if (str == "PadBeforeSuffix") {
        *ptr = DecimalFormatPattern::kPadBeforeSuffix;
    } else if (str == "PadAfterSuffix") {
        *ptr = DecimalFormatPattern::kPadAfterSuffix;
    } else {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
}

static void ePadPositionToStr(
        const void *ePadPtr, UnicodeString &appendTo) {
    const DecimalFormatPattern::EPadPosition *ptr =
            static_cast<const DecimalFormatPattern::EPadPosition *>(ePadPtr);
    switch (*ptr) {
    case DecimalFormatPattern::kPadBeforePrefix:
        appendTo.append("PadBeforePrefix");
        break;
    case DecimalFormatPattern::kPadAfterPrefix:
        appendTo.append("PadAfterPrefix");
        break;
    case DecimalFormatPattern::kPadBeforeSuffix:
        appendTo.append("PadBeforeSuffix");
        break;
    case DecimalFormatPattern::kPadAfterSuffix:
        appendTo.append("PadAfterSuffix");
        break;
    default:
        break;
    }
}

static void ePadPositionCopy(const void *src, void *dest) {
    *static_cast<DecimalFormatPattern::EPadPosition *>(dest) = *static_cast<const DecimalFormatPattern::EPadPosition *>(src);
}

static UBool ePadPositionEqual(const void *expected, const void *actual) {
    return *static_cast<const DecimalFormatPattern::EPadPosition *>(actual)
       == *static_cast<const DecimalFormatPattern::EPadPosition *>(expected);
}

static void strToUChar32(
        const UnicodeString &str, void *uChar32Ptr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t len = str.length();
    if (len == 0) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    uint32_t value = 0;
    for (int32_t i = 0; i < len; ++i) {
        UChar ch = str[i];
        if (ch >= 0x41 && ch <= 0x46) {
            value = (value << 4) + (int32_t) ch - 0x41 + 10;
        } else if (ch >= 0x61 && ch <= 0x66) {
            value = (value << 4) + (int32_t) ch - 0x61 + 10;
        } else if (ch >= 0x30 && ch <= 0x39) {
            value = (value << 4) - 0x30 + (int32_t) ch;
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }
    *static_cast<UChar32 *>(uChar32Ptr) = (UChar32) value;
}

static void uChar32ToStr(
        const void *uChar32Ptr, UnicodeString &appendTo) {
    UChar buffer[20];
    UChar32 x = *static_cast<const UChar32 *>(uChar32Ptr);
    int32_t len = uprv_itou(buffer, UPRV_LENGTHOF(buffer), (uint32_t) x, 16, 1);
    appendTo.append(buffer, 0, len);
}

static void uChar32Copy(const void *src, void *dest) {
    *static_cast<UChar32 *>(dest) = *static_cast<const UChar32 *>(src);
}

static UBool uChar32Equal(const void *expected, const void *actual) {
    return *static_cast<const UChar32 *>(actual)
       == *static_cast<const UChar32 *>(expected);
}

struct DecimalFormatPatternTupleFieldOps {
    void (*toValue)(const UnicodeString &str, void *valPtr, UErrorCode &);
    void (*toString)(const void *valPtr, UnicodeString &appendTo);
    void (*copier)(const void *src, void *dest);
    UBool (*eq)(const void *expected, const void *actual);
};

const DecimalFormatPatternTupleFieldOps gDigOps = {
        strToDig, digToStr, digCopy, digEqual};
const DecimalFormatPatternTupleFieldOps gBoolOps = {
        strToBool, boolToStr, boolCopy, boolEqual};
const DecimalFormatPatternTupleFieldOps gStrOps = {
        identVal, identStr, uStrCopy, uStrEqual};
const DecimalFormatPatternTupleFieldOps gIntOps = {
        strToInt, intToStr, intCopy, intEqual};
const DecimalFormatPatternTupleFieldOps gEPadOps = {
        strToEPadPosition, ePadPositionToStr, ePadPositionCopy, ePadPositionEqual};
const DecimalFormatPatternTupleFieldOps gUChar32Ops = {
        strToUChar32, uChar32ToStr, uChar32Copy, uChar32Equal};

struct DecimalFormatPatternTupleFieldData {
    const char *name;
    int32_t offset;
    const DecimalFormatPatternTupleFieldOps *ops;
};

const DecimalFormatPatternTupleFieldData gFieldData[kDecimalFormatPatternFieldCount] = {
{"minimumIntegerDigits", FIELD_OFFSET(fMinimumIntegerDigits), &gIntOps},
{"maximumIntegerDigits", FIELD_OFFSET(fMaximumIntegerDigits), &gIntOps},
{"minimumFractionDigits", FIELD_OFFSET(fMinimumFractionDigits), &gIntOps},
{"maximumFractionDigits", FIELD_OFFSET(fMaximumFractionDigits), &gIntOps},
{"useSignificantDigits", FIELD_OFFSET(fUseSignificantDigits), &gBoolOps},
{"minimumSignificantDigits", FIELD_OFFSET(fMinimumSignificantDigits), &gIntOps},
{"maximumSignificantDigits", FIELD_OFFSET(fMaximumSignificantDigits), &gIntOps},
{"useExponentialNotation", FIELD_OFFSET(fUseExponentialNotation), &gBoolOps},
{"minExponentDigits", FIELD_OFFSET(fMinExponentDigits), &gIntOps},
{"exponentSignAlwaysShown", FIELD_OFFSET(fExponentSignAlwaysShown), &gBoolOps},
{"currencySignCount", FIELD_OFFSET(fCurrencySignCount), &gIntOps},
{"groupingUsed", FIELD_OFFSET(fGroupingUsed), &gBoolOps},
{"groupingSize", FIELD_OFFSET(fGroupingSize), &gIntOps},
{"groupingSize2", FIELD_OFFSET(fGroupingSize2), &gIntOps},
{"multiplier", FIELD_OFFSET(fMultiplier), &gIntOps},
{"decimalSeparatorAlwaysShown", FIELD_OFFSET(fDecimalSeparatorAlwaysShown), &gBoolOps},
{"formatWidth", FIELD_OFFSET(fFormatWidth), &gIntOps},
{"roundingIncrementUsed", FIELD_OFFSET(fRoundingIncrementUsed), &gBoolOps},
{"roundingIncrement", FIELD_OFFSET(fRoundingIncrement), &gDigOps},
{"pad", FIELD_OFFSET(fPad), &gUChar32Ops},
{"negPatternsBogus", FIELD_OFFSET(fNegPatternsBogus), &gBoolOps},
{"posPatternsBogus", FIELD_OFFSET(fPosPatternsBogus), &gBoolOps},
{"negPrefixPattern", FIELD_OFFSET(fNegPrefixPattern), &gStrOps},
{"negSuffixPattern", FIELD_OFFSET(fNegSuffixPattern), &gStrOps},
{"posPrefixPattern", FIELD_OFFSET(fPosPrefixPattern), &gStrOps},
{"posSuffixPattern", FIELD_OFFSET(fPosSuffixPattern), &gStrOps},
{"padPosition", FIELD_OFFSET(fPadPosition), &gEPadOps}
};

static const void *getFieldAddress(
        const DecimalFormatPattern &values, int32_t fieldId) {
    return ((const char *) &values) + gFieldData[fieldId].offset;
}

static void *getFieldAddress(
        DecimalFormatPattern &values, int32_t fieldId) {
    return ((char *) &values) + gFieldData[fieldId].offset;
}

EDecimalFormatPatternField DecimalFormatPatternTuple::getFieldByName(
        const UnicodeString &name) {
    CharString buffer;
    UErrorCode status = U_ZERO_ERROR;
    buffer.appendInvariantChars(name, status);
    if (U_FAILURE(status)) {
        return kDecimalFormatPatternFieldCount;
    }
    int32_t result = -1;
    for (int32_t i = 0; i < UPRV_LENGTHOF(gFieldData); ++i) {
        if (uprv_strcmp(gFieldData[i].name, buffer.data()) == 0) {
            result = i;
            break;
        }
    }
    if (result == -1) {
        return kDecimalFormatPatternFieldCount;
    }
    return (EDecimalFormatPatternField) result;
}

UBool DecimalFormatPatternTuple::setField(
        EDecimalFormatPatternField fieldId, 
        const UnicodeString &fieldValue,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (fieldId == kDecimalFormatPatternFieldCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    gFieldData[fieldId].ops->toValue(
            fieldValue, getFieldAddress(fFieldValues, fieldId), status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    fFieldsUsed |= (1 << fieldId);
    return TRUE;
}

UBool DecimalFormatPatternTuple::clearField(
        EDecimalFormatPatternField fieldId, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (fieldId == kDecimalFormatPatternFieldCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    fFieldsUsed &= ~(1 << fieldId);
    return TRUE;
}

void DecimalFormatPatternTuple::merge(
        const DecimalFormatPatternTuple &other, UBool override) {
    for (int32_t i = 0; i < kDecimalFormatPatternFieldCount; ++i) {
        if ((override || !(fFieldsUsed & (1 << i)))
                 && (other.fFieldsUsed & (1 << i))) {
            gFieldData[i].ops->copier(
                    getFieldAddress(other.fFieldValues, i),
                    getFieldAddress(fFieldValues, i));
            fFieldsUsed |= (1 << i);
        }
    }
}

UBool DecimalFormatPatternTuple::verify(
        const DecimalFormatPattern &pattern, UnicodeString &message) const {
    UBool result = TRUE;
    for (int32_t i = 0; i < kDecimalFormatPatternFieldCount; ++i) {
        if (fFieldsUsed & (1 << i)) {
            if (!gFieldData[i].ops->eq(
                    getFieldAddress(fFieldValues, i),
                    getFieldAddress(pattern, i))) {
                result = FALSE;
                message.append(gFieldData[i].name);
                message.append(": Expected: ");
                gFieldData[i].ops->toString(
                        getFieldAddress(fFieldValues, i), message);
                message.append(", got: ");
                gFieldData[i].ops->toString(
                        getFieldAddress(pattern, i), message);
                message.append("; ");
            }
        }
    }
    return result;
}

UnicodeString &DecimalFormatPatternTuple::toString(
        UnicodeString &appendTo) const {
    appendTo.append("{");
    UBool first = TRUE;
    for (int32_t i = 0; i < kDecimalFormatPatternFieldCount; ++i) {
        if (fFieldsUsed & (1 << i)) {
            if (!first) {
                appendTo.append(", ");
            }
            first = FALSE;
            appendTo.append(gFieldData[i].name);
            appendTo.append(": ");
            gFieldData[i].ops->toString(
                    getFieldAddress(fFieldValues, i), appendTo);
        }
    }
    appendTo.append("}");
    return appendTo;
}

UBool DecimalFormatPatternTupleVars::store(
            const UnicodeString &name,
            const DecimalFormatPatternTuple &tuple,
            UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    int32_t idx = find(name);
    if (idx == UPRV_LENGTHOF(names)) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    if (idx == count) {
        names[count++] = name;
    }
    tuples[idx] = tuple;
    return TRUE;
}

UBool DecimalFormatPatternTupleVars::fetch(
            const UnicodeString &name,
            DecimalFormatPatternTuple &tuple) const {
    int32_t idx = find(name);
    if (idx == count) {
        return FALSE;
    }
    tuple = tuples[idx];
    return TRUE;
}

int32_t DecimalFormatPatternTupleVars::find(
        const UnicodeString &name) const {
    int32_t result;
    for (result = 0; result < count; ++result) {
        if (name == names[result]) {
            return result;
        }
    }
    return result;
}

U_NAMESPACE_END

