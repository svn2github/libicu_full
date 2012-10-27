/*
*******************************************************************************
* Copyright (C) 2012, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatabuilder.cpp
*
* created on: 2012apr01
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/localpointer.h"
#include "unicode/ucharstriebuilder.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

// TODO: Move to utrie2.h.
// TODO: Used here?
U_DEFINE_LOCAL_OPEN_POINTER(LocalUTrie2Pointer, UTrie2, utrie2_close);

/**
 * Build-time context and CE32 for a code point.
 * If a code point has contextual mappings, then the default (no-context) mapping
 * and all conditional mappings are stored in a singly-linked list
 * of ConditionalCE32, sorted by context strings.
 *
 * Context strings sort by prefix length, then by prefix, then by contraction suffix.
 * Context strings must be unique and in ascending order.
 */
struct ConditionalCE32 : public UMemory {
    ConditionalCE32(const UnicodeString &ct, uint32_t ce) : context(ct), ce32(ce), next(-1) {}

    /**
     * Empty string for the first entry for any code point, with its default CE32.
     *
     * Otherwise one unit with the length of the prefix string,
     * then the prefix string, then the contraction suffix.
     */
    UnicodeString context;
    /**
     * CE32 for the code point and its context.
     * Can be special (e.g., for an expansion) but not contextual (prefix or contraction tag).
     */
    uint32_t ce32;
    /**
     * Index of the next ConditionalCE32.
     * Negative for the end of the list.
     */
    int32_t next;
};

U_CDECL_BEGIN

U_CAPI void U_CALLCONV
uprv_deleteConditionalCE32(void *obj) {
    delete static_cast<ConditionalCE32 *>(obj);
}

U_CDECL_END

CollationDataBuilder::CollationDataBuilder(UErrorCode &errorCode)
        : nfcImpl(*Normalizer2Factory::getNFCImpl(errorCode)),
          base(NULL), trie(NULL),
          ce32s(errorCode), ce64s(errorCode), conditionalCE32s(errorCode) {
    // Reserve the first CE32 for U+0000.
    ce32s.addElement(0, errorCode);
    conditionalCE32s.setDeleter(uprv_deleteConditionalCE32);
}

CollationDataBuilder::~CollationDataBuilder() {
    utrie2_close(trie);
}

void
CollationDataBuilder::initTailoring(const CollationData *b, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    if(b == NULL) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    base = b;

    // For a tailoring, the default is to fall back to the base.
    trie = utrie2_open(Collation::MIN_SPECIAL_CE32, 0, &errorCode);

    unsafeBackwardSet = *b->unsafeBackwardSet;

    if(U_FAILURE(errorCode)) { return; }
    // TODO
}

UBool
CollationDataBuilder::maybeSetPrimaryRange(UChar32 start, UChar32 end,
                                           uint32_t primary, int32_t step,
                                           UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    U_ASSERT(start <= end);
    // TODO: Do we need to check what values are currently set for start..end?
    // An offset range is worth it only if we can achieve an overlap between
    // adjacent UTrie2 blocks of 32 code points each.
    // An offset CE is also a little more expensive to look up and compute
    // than a simple CE.
    // If the range spans at least three UTrie2 block boundaries (> 64 code points),
    // then we take it.
    // If the range spans one or two block boundaries and there are
    // at least 4 code points on either side, then we take it.
    // (We could additionally require a minimum range length of, say, 16.)
    int32_t blockDelta = (end >> 5) - (start >> 5);
    if(2 <= step && step <= 0x7f &&
            (blockDelta >= 3 ||
            (blockDelta > 0 && (start & 0x1f) <= 0x1c && (end & 0x1f) >= 3))) {
        int64_t dataCE = ((int64_t)primary << 32) | (start << 8) | step;
        if(isCompressiblePrimary(primary)) { dataCE |= 0x80; }
        int32_t index = addCE(dataCE, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        if(index > 0xfffff) {
            errorCode = U_BUFFER_OVERFLOW_ERROR;
            return 0;
        }
        uint32_t offsetCE32 = makeSpecialCE32(Collation::OFFSET_TAG, index);
        utrie2_setRange32(trie, start, end, offsetCE32, TRUE, &errorCode);
        return TRUE;
    } else {
        return FALSE;
    }
}

uint32_t
CollationDataBuilder::setPrimaryRangeAndReturnNext(UChar32 start, UChar32 end,
                                                   uint32_t primary, int32_t step,
                                                   UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    UBool isCompressible = isCompressiblePrimary(primary);
    if(maybeSetPrimaryRange(start, end, primary, step, errorCode)) {
        return Collation::incThreeBytePrimaryByOffset(primary, isCompressible,
                                                      (end - start + 1) * step);
    } else {
        // Short range: Set individual CE32s.
        for(;;) {
            utrie2_set32(trie, start, makeLongPrimaryCE32(primary), &errorCode);
            ++start;
            primary = Collation::incThreeBytePrimaryByOffset(primary, isCompressible, step);
            if(start > end) { return primary; }
        }
    }
}

uint32_t
CollationDataBuilder::getCE32FromOffsetCE32(UChar32 c, uint32_t ce32) const {
    int64_t dataCE = ce64s.elementAti((int32_t)ce32 & 0xfffff);
    uint32_t p = Collation::getThreeBytePrimaryForOffsetData(c, dataCE);
    return makeLongPrimaryCE32(p);
}

UBool
CollationDataBuilder::isCompressibleLeadByte(uint32_t b) const {
    return base->isCompressibleLeadByte(b);
}

UBool
CollationDataBuilder::isAssigned(UChar32 c) const {
    uint32_t ce32 = utrie2_get32(trie, c);
    return ce32 != Collation::MIN_SPECIAL_CE32 && ce32 != Collation::UNASSIGNED_CE32;
}

uint32_t
CollationDataBuilder::getLongPrimaryIfSingleCE(UChar32 c) const {
    uint32_t ce32 = utrie2_get32(trie, c);
    if(Collation::isLongPrimaryCE32(ce32)) {
        return Collation::primaryFromLongPrimaryCE32(ce32);
    } else {
        return 0;
    }
}

int64_t
CollationDataBuilder::getSingleCE(UChar32 c, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    uint32_t ce32 = utrie2_get32(trie, c);
    if(ce32 == Collation::MIN_SPECIAL_CE32) {
        ce32 = base->getCE32(c);
    }
    for(;;) {  // Loop while ce32 is special.
        if(!Collation::isSpecialCE32(ce32)) {
            return Collation::ceFromCE32(ce32);
        }
        int32_t tag = Collation::getSpecialCE32Tag(ce32);
        if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
            errorCode = U_UNSUPPORTED_ERROR;
            return 0;
        }
        switch(tag) {
        case Collation::EXPANSION32_TAG:
            if((ce32 & 7) == 1) {
                ce32 = ce32s.elementAti((ce32 >> 3) & 0x1ffff);
                break;
            } else {
                errorCode = U_UNSUPPORTED_ERROR;
                return 0;
            }
        case Collation::EXPANSION_TAG: {
            if((ce32 & 7) == 1) {
                return ce64s.elementAti((ce32 >> 3) & 0x1ffff);
            } else {
                errorCode = U_UNSUPPORTED_ERROR;
                return 0;
            }
        }
        case Collation::PREFIX_TAG:
        case Collation::CONTRACTION_TAG:
        case Collation::HANGUL_TAG:
        case Collation::LEAD_SURROGATE_TAG:
            errorCode = U_UNSUPPORTED_ERROR;
            return 0;
        case Collation::DIGIT_TAG:
            // Fetch the non-CODAN CE32 and continue.
            ce32 = ce32s.elementAti((ce32 >> 4) & 0xffff);
            break;
        case Collation::RESERVED_TAG_11:
            errorCode = U_INTERNAL_PROGRAM_ERROR;
            return 0;
        case Collation::OFFSET_TAG:
            ce32 = getCE32FromOffsetCE32(c, ce32);
            break;
        case Collation::IMPLICIT_TAG:
            if((ce32 & 1) == 0) {
                U_ASSERT(c == 0);
                // Fetch the normal ce32 for U+0000 and continue.
                ce32 = ce32s.elementAti(0);
                break;
            } else {
                return Collation::unassignedCEFromCodePoint(c);
            }
        }
    }
}

int32_t
CollationDataBuilder::addCE(int64_t ce, UErrorCode &errorCode) {
    int32_t length = ce64s.size();
    for(int32_t i = 0; i < length; ++i) {
        if(ce == ce64s.elementAti(i)) { return i; }
    }
    ce64s.addElement(ce, errorCode);
    return length;
}

int32_t
CollationDataBuilder::addCE32(uint32_t ce32, UErrorCode &errorCode) {
    int32_t length = ce32s.size();
    for(int32_t i = 0; i < length; ++i) {
        if(ce32 == (uint32_t)ce32s.elementAti(i)) { return i; }
    }
    ce32s.addElement((int32_t)ce32, errorCode);
    return length;
}

int32_t
CollationDataBuilder::addConditionalCE32(const UnicodeString &context, uint32_t ce32,
                                         UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return -1; }
    int32_t index = conditionalCE32s.size();
    if(index > 0xfffff) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return -1;
    }
    ConditionalCE32 *cond = new ConditionalCE32(context, ce32);
    if(cond == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return -1;
    }
    conditionalCE32s.addElement(cond, errorCode);
    return index;
}

void
CollationDataBuilder::add(const UnicodeString &prefix, const UnicodeString &s,
                          const int64_t ces[], int32_t cesLength,
                          UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    // cesLength must be limited (e.g., to 31) so that the CollationIterator
    // can work with a fixed initial CEArray capacity for most cases.
    if(s.isEmpty() || cesLength <= 0 || cesLength > 31) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(trie == NULL || utrie2_isFrozen(trie)) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    UChar32 c = s.char32At(0);
    int32_t cLength = U16_LENGTH(c);
    // TODO: Validate prefix/c/suffix.
    // Valid UTF-16: No unpaired surrogates in either prefix or s.
    // Prefix must be FCD. s must be FCD. (Or make them.)
    // No FFFE
    // No FFFF
    // If prefix: cc(prefix[0])==cc(c)==0
    // Max lengths?
    // Forbid syllable-forming Jamos with/in expansions/contractions/prefixes, see design doc.
    uint32_t ce32 = encodeCEs(ces, cesLength, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    uint32_t oldCE32 = utrie2_get32(trie, c);
    // TODO: In a tailoring, if(!isContractionCE32(oldCE32)) then copy c's base CE32 and its context data.
    if(prefix.isEmpty() && s.length() == cLength) {
        // No prefix, no contraction.
        if(!isContractionCE32(oldCE32)) {
            utrie2_set32(trie, c, ce32, &errorCode);
        } else {
            ConditionalCE32 *cond = static_cast<ConditionalCE32 *>(
                conditionalCE32s[(int32_t)oldCE32 & 0xfffff]);
            cond->ce32 = ce32;
        }
    } else {
        ConditionalCE32 *cond;
        if(!isContractionCE32(oldCE32)) {
            // Replace the simple oldCE32 with a contraction CE32
            // pointing to a new ConditionalCE32 list head.
            int32_t index = addConditionalCE32(UnicodeString(), oldCE32, errorCode);
            if(U_FAILURE(errorCode)) { return; }
            utrie2_set32(trie, c, makeSpecialCE32(Collation::CONTRACTION_TAG, index), &errorCode);
            contextChars.add(c);
            cond = static_cast<ConditionalCE32 *>(conditionalCE32s[index]);
        } else {
            cond = static_cast<ConditionalCE32 *>(
                conditionalCE32s[(int32_t)oldCE32 & 0xfffff]);
        }
        UnicodeString suffix(s, cLength);
        UnicodeString context((UChar)prefix.length());
        context.append(prefix).append(suffix);
        unsafeBackwardSet.addAll(suffix);
        for(;;) {
            // invariant: context > cond->context
            int32_t next = cond->next;
            if(next < 0) {
                // Append a new ConditionalCE32 after cond.
                int32_t index = addConditionalCE32(context, ce32, errorCode);
                if(U_FAILURE(errorCode)) { return; }
                cond->next = index;
                break;
            }
            ConditionalCE32 *nextCond = static_cast<ConditionalCE32 *>(conditionalCE32s[next]);
            int8_t cmp = context.compare(nextCond->context);
            if(cmp < 0) {
                // Insert a new ConditionalCE32 between cond and nextCond.
                int32_t index = addConditionalCE32(context, ce32, errorCode);
                if(U_FAILURE(errorCode)) { return; }
                cond->next = index;
                static_cast<ConditionalCE32 *>(conditionalCE32s[index])->next = next;
                break;
            } else if(cmp == 0) {
                // Same context as before, overwrite its ce32.
                nextCond->ce32 = ce32;
                break;
            }
            cond = nextCond;
        }
    }
}

uint32_t
CollationDataBuilder::encodeOneCE(int64_t ce) {
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t t = (uint32_t)(ce & 0xffff);
    if((ce & 0xffff00ff00ff) == 0 && t > 0x100) {
        // normal form ppppsstt
        return p | ((uint32_t)(ce >> 16) & 0xff00u) | (t >> 8);
    } else if((ce & 0xffffffffff) == Collation::COMMON_SEC_AND_TER_CE) {
        // long-primary form pppppp01
        return p | 1u;
    } else if(p == 0 && (t & 0xff) == 0) {
        // long-secondary form sssstt00
        return (uint32_t)ce;
    }
    return Collation::UNASSIGNED_CE32;
}

uint32_t
CollationDataBuilder::encodeCEsAsCE32s(const int64_t ces[], int32_t cesLength,
                                       UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    UVector32 localCE32s(errorCode);
    if(cesLength > 7) {
        localCE32s.addElement(cesLength, errorCode);
    }
    for(int32_t i = 0; i < cesLength; ++i) {
        uint32_t ce32 = encodeOneCE(ces[i]);
        if(ce32 == Collation::UNASSIGNED_CE32) { return ce32; }
        localCE32s.addElement((int32_t)ce32, errorCode);
    }
    if(U_FAILURE(errorCode)) { return 0; }
    // See if this sequence of CE32s has already been stored.
    int32_t length7 = cesLength;
    int32_t length = cesLength;
    if(cesLength > 7) {
        length7 = 0;
        ++length;
    }
    int32_t ce32sMax = ce32s.size() - length;
    int32_t first = localCE32s.elementAti(0);
    for(int32_t i = 0; i <= ce32sMax; ++i) {
        if(first == ce32s.elementAti(i)) {
            if(i > 0x1ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return 0;
            }
            for(int32_t j = 1;; ++j) {
                if(j == length) {
                    return makeSpecialCE32(Collation::EXPANSION32_TAG, (i << 3) | length7);
                }
                if(ce32s.elementAti(i + j) != localCE32s.elementAti(j)) { break; }
            }
        }
    }
    // Store the new sequence.
    int32_t i = ce32s.size();
    if(i > 0x1ffff) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }
    for(int32_t j = 0; j < length; ++j) {
        ce32s.addElement(localCE32s.elementAti(j), errorCode);
    }
    return makeSpecialCE32(Collation::EXPANSION32_TAG, (i << 3) | length7);
}

uint32_t
CollationDataBuilder::encodeCEs(const int64_t ces[], int32_t cesLength,
                                UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    // TODO: Check CEs for validity.
    if(cesLength == 1) {
        // Try to encode one CE as one CE32.
        int64_t ce = ces[0];
        uint32_t ce32 = encodeOneCE(ce);
        if(ce32 != Collation::UNASSIGNED_CE32) { return ce32; }
        int32_t index = addCE(ce, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        if(index > 0x1ffff) {
            errorCode = U_BUFFER_OVERFLOW_ERROR;
            return 0;
        }
        return makeSpecialCE32(Collation::EXPANSION_TAG, (index << 3) | 1);
    } else if(cesLength == 2) {
        // Try to encode two CEs as one CE32.
        int64_t ce0 = ces[0];
        int64_t ce1 = ces[1];
        uint32_t p0 = (uint32_t)(ce0 >> 32);
        if((ce0 & 0xffffffffff00ff) == Collation::COMMON_SECONDARY_CE &&
                (ce1 & 0xffffffff00ffffff) == Collation::COMMON_TERTIARY_CE &&
                0 < p0 && p0 <= (Collation::MAX_LATIN_EXPANSION_TAG << 28)) {
            // Latin mini expansion
            return
                Collation::MIN_SPECIAL_CE32 | (p0 >> 8) |
                ((uint32_t)ce0 & 0xff00u) |
                (uint32_t)(ce1 >> 24);
        }
    }
    // Try to encode two or more CEs as CE32s.
    uint32_t ce32 = encodeCEsAsCE32s(ces, cesLength, errorCode);
    if(U_FAILURE(errorCode) || ce32 != Collation::UNASSIGNED_CE32) { return ce32; }
    // See if this sequence of two or more CEs has already been stored.
    int32_t length7 = cesLength;
    int32_t length = cesLength;
    int64_t first;
    if(cesLength <= 7) {
        first = ces[0];
    } else {
        first = cesLength;
        length7 = 0;
        ++length;
    }
    int32_t ce64sMax = ce64s.size() - length;
    for(int32_t i = 0; i <= ce64sMax; ++i) {
        if(first == ce64s.elementAti(i)) {
            if(i > 0x1ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return 0;
            }
            int32_t j = (cesLength <= 7) ? 1 : 0;  // j indexes into ces[]
            for(int32_t k = i + 1;; ++j, ++k) {  // k indexes into ce64s
                if(j == length) {
                    return makeSpecialCE32(Collation::EXPANSION_TAG, (i << 3) | length7);
                }
                if(ce64s.elementAti(k) != ces[j]) { break; }
            }
        }
    }
    // Store the new sequence.
    int32_t i = ce64s.size();
    if(i > 0x1ffff) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }
    ce64s.addElement(first, errorCode);
    for(int32_t j = (cesLength <= 7) ? 1 : 0; j < length; ++j) {
        ce64s.addElement(ces[j], errorCode);
    }
    return makeSpecialCE32(Collation::EXPANSION_TAG, (i << 3) | length7);
}

UBool
CollationDataBuilder::setJamoCEs(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return FALSE; }
    UBool anyJamoAssigned = FALSE;
    int32_t i;  // Count within Jamo types.
    int32_t j = 0;  // Count across Jamo types.
    UChar32 jamo = Hangul::JAMO_L_BASE;
    for(i = 0; i < Hangul::JAMO_L_COUNT; ++i, ++j, ++jamo) {
        anyJamoAssigned |= isAssigned(jamo);
        jamoCEs[j] = getSingleCE(jamo, errorCode);
    }
    jamo = Hangul::JAMO_V_BASE;
    for(i = 0; i < Hangul::JAMO_V_COUNT; ++i, ++j, ++jamo) {
        anyJamoAssigned |= isAssigned(jamo);
        jamoCEs[j] = getSingleCE(jamo, errorCode);
    }
    jamo = Hangul::JAMO_T_BASE + 1;  // Omit U+11A7 which is not a "T" Jamo.
    for(i = 1; i < Hangul::JAMO_T_COUNT; ++i, ++j, ++jamo) {
        anyJamoAssigned |= isAssigned(jamo);
        jamoCEs[j] = getSingleCE(jamo, errorCode);
    }
    return anyJamoAssigned;
}

U_CDECL_BEGIN

static UBool U_CALLCONV
enumRangeLeadValue(const void *context, UChar32 /*start*/, UChar32 /*end*/, uint32_t value) {
    int32_t *pValue = (int32_t *)context;
    if(value == Collation::UNASSIGNED_CE32) {
        value = 0;
    } else if(value == Collation::MIN_SPECIAL_CE32) {
        value = 1;
    } else {
        *pValue = 2;
        return FALSE;
    }
    if(*pValue < 0) {
        *pValue = (int32_t)value;
    } else if(*pValue != (int32_t)value) {
        *pValue = 2;
        return FALSE;
    }
    return TRUE;
}

U_CDECL_END

void
CollationDataBuilder::setLeadSurrogates(UErrorCode &errorCode) {
    for(UChar lead = 0xd800; lead < 0xdc00; ++lead) {
        int32_t value = -1;
        utrie2_enumForLeadSurrogate(trie, lead, NULL, enumRangeLeadValue, &value);
        utrie2_set32ForLeadSurrogateCodeUnit(
            trie, lead, makeSpecialCE32(Collation::LEAD_SURROGATE_TAG, value), &errorCode);
    }
}

CollationData *
CollationDataBuilder::buildTailoring(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }

    // Create a CollationData container of aliases to this builder's finalized data.
    LocalPointer<CollationData> cd(new CollationData(nfcImpl));
    if(cd.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    buildMappings(*cd, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }

    cd->variableTop = base->variableTop;
    cd->compressibleBytes = base->compressibleBytes;
    return cd.orphan();
}

void
CollationDataBuilder::buildMappings(CollationData &cd, UErrorCode &errorCode) {
    // TODO: Prevent build() after build().
    // TODO: Copy Latin-1 into each tailoring, but not 0..ff, rather 0..7f && c0..ff.

    buildContexts(errorCode);

    UBool anyJamoAssigned = setJamoCEs(errorCode);
    setLeadSurrogates(errorCode);

    // For U+0000, move its normal ce32 into CE32s[0] and set IMPLICIT_TAG with 0 data bits.
    ce32s.setElementAt((int32_t)utrie2_get32(trie, 0), 0);
    utrie2_set32(trie, 0, makeSpecialCE32(Collation::IMPLICIT_TAG, 0u), &errorCode);

    utrie2_freeze(trie, UTRIE2_32_VALUE_BITS, &errorCode);
    if(U_FAILURE(errorCode)) { return; }

    // Mark each lead surrogate as "unsafe"
    // if any of its 1024 associated supplementary code points is "unsafe".
    UChar32 c = 0x10000;
    for(UChar lead = 0xd800; lead < 0xdc00; ++lead, c += 0x400) {
        if(!unsafeBackwardSet.containsNone(c, c + 0x3ff)) {
            unsafeBackwardSet.add(lead);
        }
    }
    unsafeBackwardSet.freeze();

    cd.trie = trie;
    cd.ce32s = reinterpret_cast<const uint32_t *>(ce32s.getBuffer());
    cd.ces = ce64s.getBuffer();
    cd.contexts = contexts.getBuffer();
    cd.base = base;
    if(anyJamoAssigned || base == NULL) {
        cd.jamoCEs = jamoCEs;
    } else {
        cd.jamoCEs = base->jamoCEs;
    }
    cd.unsafeBackwardSet = &unsafeBackwardSet;
}

void
CollationDataBuilder::buildContexts(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    UnicodeSetIterator iter(contextChars);
    while(iter.next()) {
        buildContext(iter.getCodepoint(), errorCode);
    }
}

void
CollationDataBuilder::buildContext(UChar32 c, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    uint32_t ce32 = utrie2_get32(trie, c);
    if(!isContractionCE32(ce32)) {
        // Impossible: No context data for c in contextChars.
        errorCode = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    ConditionalCE32 *cond = static_cast<ConditionalCE32 *>(conditionalCE32s[ce32 & 0xfffff]);
    if(cond->next < 0) {
        // Impossible: No actual contexts after the list head.
        errorCode = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    // Default for no context match.
    uint32_t defaultCE32 = cond->ce32;
    // Entry for an empty prefix, to be stored before the trie.
    uint32_t emptyPrefixCE32 = defaultCE32;
    UCharsTrieBuilder prefixBuilder(errorCode);
    UCharsTrieBuilder contractionBuilder(errorCode);
    do {
        cond = static_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]);
        // The prefix or suffix can be empty, but not both.
        U_ASSERT(cond->context.length() > 1);
        int32_t prefixLength = cond->context[0];
        UnicodeString prefix(cond->context, 0, prefixLength + 1);
        // Collect all contraction suffixes for one prefix.
        ConditionalCE32 *firstCond = cond;
        ConditionalCE32 *lastCond = cond;
        while(cond->next >= 0 &&
                (cond = static_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]))
                    ->context.startsWith(prefix)) {
            lastCond = cond;
        }
        uint32_t ce32;
        int32_t suffixStart = prefixLength + 1;  // == prefix.length()
        if(lastCond->context.length() == suffixStart) {
            // One prefix without contraction suffix.
            U_ASSERT(firstCond == lastCond);
            ce32 = lastCond->ce32;
            cond = lastCond;
        } else {
            // Build the contractions trie.
            contractionBuilder.clear();
            // Entry for an empty suffix, to be stored before the trie.
            uint32_t emptySuffixCE32;
            cond = firstCond;
            if(cond->context.length() == suffixStart) {
                emptySuffixCE32 = cond->ce32;
                cond = static_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]);
            } else {
                emptySuffixCE32 = defaultCE32;
            }
            // Latin optimization: Flags bit 1 indicates whether
            // the first character of every contraction suffix is >=U+0300.
            // Short-circuits contraction matching when a normal Latin letter follows.
            int32_t flags = 0;
            if(cond->context[suffixStart] >= 0x300) { flags |= 2; }
            // Add all of the non-empty suffixes into the contraction trie.
            for(;;) {
                UnicodeString suffix(cond->context, suffixStart);
                uint16_t fcd16 = nfcImpl.getFCD16(suffix.char32At(suffix.length() - 1));
                if(fcd16 > 0xff) {
                    // The last suffix character has lccc!=0, allowing for discontiguous contractions.
                    flags |= 1;
                }
                contractionBuilder.add(suffix, (int32_t)cond->ce32, errorCode);
                if(cond == lastCond) { break; }
                cond = static_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]);
            }
            int32_t index = addContextTrie(emptySuffixCE32, contractionBuilder, errorCode);
            if(U_FAILURE(errorCode)) { return; }
            if(index > 0x3ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return;
            }
            ce32 = makeSpecialCE32(Collation::CONTRACTION_TAG, (index << 2) | flags);
        }
        U_ASSERT(cond == lastCond);
        if(prefixLength == 0) {
            if(cond->next < 0) {
                // No non-empty prefixes, only contractions.
                utrie2_set32(trie, c, ce32, &errorCode);
                return;
            } else {
                emptyPrefixCE32 = ce32;
            }
        } else {
            prefix.remove(0, 1);  // Remove the length unit.
            prefix.reverse();
            prefixBuilder.add(prefix, (int32_t)ce32, errorCode);
        }
    } while(cond->next >= 0);
    int32_t index = addContextTrie(emptyPrefixCE32, prefixBuilder, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    if(index > 0xfffff) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return;
    }
    utrie2_set32(trie, c, makeSpecialCE32(Collation::PREFIX_TAG, index), &errorCode);
}

int32_t
CollationDataBuilder::addContextTrie(uint32_t defaultCE32, UCharsTrieBuilder &trieBuilder,
                                     UErrorCode &errorCode) {
    UnicodeString context;
    context.append((UChar)(defaultCE32 >> 16)).append((UChar)defaultCE32);
    UnicodeString trieString;
    context.append(trieBuilder.buildUnicodeString(USTRINGTRIE_BUILD_SMALL, trieString, errorCode));
    if(U_FAILURE(errorCode)) { return -1; }
    int32_t index = contexts.indexOf(context);
    if(index < 0) {
        index = contexts.length();
        contexts.append(context);
    }
    return index;
}

int32_t
CollationDataBuilder::serializeTrie(void *data, int32_t capacity, UErrorCode &errorCode) const {
    return utrie2_serialize(trie, data, capacity, &errorCode);
}

int32_t
CollationDataBuilder::serializeUnsafeBackwardSet(uint16_t *data, int32_t capacity,
                                                 UErrorCode &errorCode) const {
    return unsafeBackwardSet.serialize(data, capacity, errorCode);
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(CollationDataBuilder)

// TODO: In CollationWeights allocator,
// try to treat secondary & tertiary weights as 3/4-byte weights with bytes 1 & 2 == 0.
// Natural secondary limit of 0x10000.

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
