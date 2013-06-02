/*
*******************************************************************************
* Copyright (C) 2012-2013, International Business Machines
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
#include "unicode/uchar.h"
#include "unicode/ucharstrie.h"
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

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

CollationDataBuilder::CEModifier::~CEModifier() {}

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

    inline UBool hasContext() const { return context.length() > 1; }
    inline int32_t prefixLength() const { return context.charAt(0); }

    /**
     * "\0" for the first entry for any code point, with its default CE32.
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
          base(NULL), baseSettings(NULL),
          trie(NULL),
          ce32s(errorCode), ce64s(errorCode), conditionalCE32s(errorCode),
          modified(FALSE) {
    // Reserve the first CE32 for U+0000.
    ce32s.addElement(0, errorCode);
    conditionalCE32s.setDeleter(uprv_deleteConditionalCE32);
}

CollationDataBuilder::~CollationDataBuilder() {
    utrie2_close(trie);
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
        uint32_t offsetCE32 = Collation::makeSpecialCE32(Collation::OFFSET_TAG, index);
        utrie2_setRange32(trie, start, end, offsetCE32, TRUE, &errorCode);
        modified = TRUE;
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
            utrie2_set32(trie, start, Collation::makeLongPrimaryCE32(primary), &errorCode);
            ++start;
            primary = Collation::incThreeBytePrimaryByOffset(primary, isCompressible, step);
            if(start > end) { return primary; }
        }
        modified = TRUE;
    }
}

uint32_t
CollationDataBuilder::getCE32FromOffsetCE32(UBool fromBase, UChar32 c, uint32_t ce32) const {
    int32_t i = Collation::getOffsetIndex(ce32);
    int64_t dataCE = fromBase ? base->ces[i] : ce64s.elementAti(i);
    uint32_t p = Collation::getThreeBytePrimaryForOffsetData(c, dataCE);
    return Collation::makeLongPrimaryCE32(p);
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
    UBool fromBase = FALSE;
    uint32_t ce32 = utrie2_get32(trie, c);
    if(ce32 == Collation::MIN_SPECIAL_CE32) {
        fromBase = TRUE;
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
            if(Collation::getExpansionLength(ce32) == 1) {
                int32_t i = Collation::getExpansionIndex(ce32);
                ce32 = fromBase ? base->ce32s[i] : ce32s.elementAti(i);
                break;
            } else {
                errorCode = U_UNSUPPORTED_ERROR;
                return 0;
            }
        case Collation::EXPANSION_TAG: {
            if(Collation::getExpansionLength(ce32) == 1) {
                int32_t i = Collation::getExpansionIndex(ce32);
                return fromBase ? base->ces[i] : ce64s.elementAti(i);
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
            // Fetch the non-numeric-collation CE32 and continue.
            ce32 = ce32s.elementAti(Collation::getDigitIndex(ce32));
            break;
        case Collation::RESERVED_TAG_11:
            errorCode = U_INTERNAL_PROGRAM_ERROR;
            return 0;
        case Collation::OFFSET_TAG:
            ce32 = getCE32FromOffsetCE32(fromBase, c, ce32);
            break;
        case Collation::IMPLICIT_TAG:
            if((ce32 & 1) == 0) {
                U_ASSERT(c == 0);
                // Fetch the normal ce32 for U+0000 and continue.
                ce32 = fromBase ? base->ce32s[0] : ce32s.elementAti(0);
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
    U_ASSERT(!context.isEmpty());
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
    if(s.isEmpty() || cesLength < 0 || cesLength > Collation::MAX_EXPANSION_LENGTH) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(trie == NULL || utrie2_isFrozen(trie)) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    // TODO: Validate prefix/c/suffix.
    // Forbid syllable-forming Jamos with/in expansions/contractions/prefixes, see design doc.
    uint32_t ce32 = encodeCEs(ces, cesLength, errorCode);
    addCE32(prefix, s, ce32, errorCode);
}

void
CollationDataBuilder::addCE32(const UnicodeString &prefix, const UnicodeString &s,
                              uint32_t ce32, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    UChar32 c = s.char32At(0);
    int32_t cLength = U16_LENGTH(c);
    uint32_t oldCE32 = utrie2_get32(trie, c);
    UBool hasContext = !prefix.isEmpty() || s.length() > cLength;
    if(oldCE32 == Collation::MIN_SPECIAL_CE32) {
        // First tailoring for c.
        // If c has contextual base mappings or if we add a contextual mapping,
        // then copy the base mappings.
        // Otherwise we just override the base mapping.
        uint32_t baseCE32 = base->getFinalCE32(base->getCE32(c));
        if(hasContext || Collation::ce32HasContext(baseCE32)) {
            oldCE32 = copyFromBaseCE32(c, baseCE32, errorCode);
            utrie2_set32(trie, c, oldCE32, &errorCode);
            if(U_FAILURE(errorCode)) { return; }
        }
    }
    if(!hasContext) {
        // No prefix, no contraction.
        if(!Collation::isContractionCE32(oldCE32)) {
            utrie2_set32(trie, c, ce32, &errorCode);
        } else {
            ConditionalCE32 *cond = getConditionalCE32ForCE32(oldCE32);
            cond->ce32 = ce32;
        }
    } else {
        ConditionalCE32 *cond;
        if(!Collation::isContractionCE32(oldCE32)) {
            // Replace the simple oldCE32 with a contraction CE32
            // pointing to a new ConditionalCE32 list head.
            int32_t index = addConditionalCE32(UnicodeString((UChar)0), oldCE32, errorCode);
            if(U_FAILURE(errorCode)) { return; }
            uint32_t contractionCE32 = Collation::makeSpecialCE32(Collation::CONTRACTION_TAG, index);
            utrie2_set32(trie, c, contractionCE32, &errorCode);
            contextChars.add(c);
            cond = getConditionalCE32(index);
        } else {
            cond = getConditionalCE32ForCE32(oldCE32);
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
            ConditionalCE32 *nextCond = getConditionalCE32(next);
            int8_t cmp = context.compare(nextCond->context);
            if(cmp < 0) {
                // Insert a new ConditionalCE32 between cond and nextCond.
                int32_t index = addConditionalCE32(context, ce32, errorCode);
                if(U_FAILURE(errorCode)) { return; }
                cond->next = index;
                getConditionalCE32(index)->next = next;
                break;
            } else if(cmp == 0) {
                // Same context as before, overwrite its ce32.
                nextCond->ce32 = ce32;
                break;
            }
            cond = nextCond;
        }
    }
    modified = TRUE;
}

uint32_t
CollationDataBuilder::encodeOneCEAsCE32(int64_t ce) {
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t lower32 = (uint32_t)ce;
    uint32_t t = (uint32_t)(ce & 0xffff);
    if((ce & 0xffff00ff00ff) == 0 && t > 0x100) {
        // normal form ppppsstt
        return p | (lower32 >> 16) | (t >> 8);
    } else if((ce & 0xffffffffff) == Collation::COMMON_SEC_AND_TER_CE) {
        // long-primary form pppppp01
        return p | 1u;
    } else if(p == 0 && (t & 0xff) == 0 && (lower32 < Collation::MIN_SPECIAL_CE32)) {
        // long-secondary form sssstt00
        // Must be < MIN_SPECIAL_CE32 so that it does not look like a special CE32.
        // TODO: If the addition of the long-secondary form costs any performance,
        // for probably a small data size improvement,
        // then remove it and just test t!=0 for the normal form vs. long-primary.
        return lower32;
    }
    return Collation::UNASSIGNED_CE32;
}

uint32_t
CollationDataBuilder::encodeOneCE(int64_t ce, UErrorCode &errorCode) {
    // Try to encode one CE as one CE32.
    uint32_t ce32 = encodeOneCEAsCE32(ce);
    if(ce32 != Collation::UNASSIGNED_CE32) { return ce32; }
    int32_t index = addCE(ce, errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    if(index > 0x1ffff) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }
    return Collation::makeSpecialCE32(Collation::EXPANSION_TAG, (index << 3) | 1);
}

uint32_t
CollationDataBuilder::encodeCEs(const int64_t ces[], int32_t cesLength,
                                UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    if(cesLength == 0) {
        // Convenience: We cannot map to nothing, but we can map to a completely ignorable CE.
        // Do this here so that callers need not do it.
        return encodeOneCEAsCE32(0);
    } else if(cesLength == 1) {
        return encodeOneCE(ces[0], errorCode);
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
    int32_t newCE32s[Collation::MAX_EXPANSION_LENGTH];
    for(int32_t i = 0;; ++i) {
        if(i == cesLength) {
            return encodeExpansion32(newCE32s, cesLength, errorCode);
        }
        uint32_t ce32 = encodeOneCEAsCE32(ces[i]);
        if(ce32 == Collation::UNASSIGNED_CE32) { break; }
        newCE32s[i] = (int32_t)ce32;
    }
    return encodeExpansion(ces, cesLength, errorCode);
}

uint32_t
CollationDataBuilder::encodeExpansion(const int64_t ces[], int32_t length, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    // See if this sequence of CEs has already been stored.
    int32_t length7;
    int32_t offset;
    int64_t first;
    if(length <= 7) {
        length7 = length;
        offset = 0;
        first = ces[0];
    } else {
        length7 = 0;
        offset = 1;
        first = length;
    }
    int32_t ce64sMax = ce64s.size() - length - offset;
    for(int32_t i = 0; i <= ce64sMax; ++i) {
        if(first == ce64s.elementAti(i)) {
            if(i > 0x1ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return 0;
            }
            for(int32_t j = 1 - offset;; ++j) {
                if(j == length) {
                    return Collation::makeSpecialCE32(Collation::EXPANSION_TAG, (i << 3) | length7);
                }
                if(ce64s.elementAti(i + offset + j) != ces[j]) { break; }
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
    for(int32_t j = 1 - offset; j < length; ++j) {
        ce64s.addElement(ces[j], errorCode);
    }
    return Collation::makeSpecialCE32(Collation::EXPANSION_TAG, (i << 3) | length7);
}

uint32_t
CollationDataBuilder::encodeExpansion32(const int32_t newCE32s[], int32_t length,
                                        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    // See if this sequence of CE32s has already been stored.
    int32_t length7;
    int32_t offset;
    int32_t first;
    if(length <= 7) {
        length7 = length;
        offset = 0;
        first = newCE32s[0];
    } else {
        length7 = 0;
        offset = 1;
        first = length;
    }
    int32_t ce32sMax = ce32s.size() - length - offset;
    for(int32_t i = 0; i <= ce32sMax; ++i) {
        if(first == ce32s.elementAti(i)) {
            if(i > 0x1ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return 0;
            }
            for(int32_t j = 1 - offset;; ++j) {
                if(j == length) {
                    return Collation::makeSpecialCE32(Collation::EXPANSION32_TAG, (i << 3) | length7);
                }
                if(ce32s.elementAti(i + offset + j) != newCE32s[j]) { break; }
            }
        }
    }
    // Store the new sequence.
    int32_t i = ce32s.size();
    if(i > 0x1ffff) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }
    ce32s.addElement(first, errorCode);
    for(int32_t j = 1 - offset; j < length; ++j) {
        ce32s.addElement(newCE32s[j], errorCode);
    }
    return Collation::makeSpecialCE32(Collation::EXPANSION32_TAG, (i << 3) | length7);
}

uint32_t
CollationDataBuilder::copyFromBaseCE32(UChar32 c, uint32_t ce32, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    if(!Collation::isSpecialCE32(ce32)) { return ce32; }
    int32_t tag = Collation::getSpecialCE32Tag(ce32);
    if(tag <= Collation::MAX_LATIN_EXPANSION_TAG) {
        // copy as is
    } else if(tag == Collation::EXPANSION32_TAG) {
        const uint32_t *baseCE32s = base->ce32s + Collation::getExpansionIndex(ce32);
        int32_t length = Collation::getExpansionLength(ce32);
        if(length == 0) { length = (int32_t)*baseCE32s++; }
        ce32 = encodeExpansion32(
            reinterpret_cast<const int32_t *>(baseCE32s), length, errorCode);
    } else if(tag == Collation::EXPANSION_TAG) {
        const int64_t *baseCEs = base->ces + Collation::getExpansionIndex(ce32);
        int32_t length = Collation::getExpansionLength(ce32);
        if(length == 0) { length = (int32_t)*baseCEs++; }
        ce32 = encodeExpansion(baseCEs, length, errorCode);
    } else if(tag == Collation::HANGUL_TAG) {
        errorCode = U_UNSUPPORTED_ERROR;  // TODO: revisit tailoring of Hangul syllables
    } else if(tag == Collation::OFFSET_TAG) {
        ce32 = getCE32FromOffsetCE32(TRUE, c, ce32);
    } else if(tag == Collation::IMPLICIT_TAG) {
        ce32 = Collation::unassignedCEFromCodePoint(c);
    } else if(tag == Collation::PREFIX_TAG) {
        // Flatten prefixes and nested suffixes (contractions)
        // into a linear list of ConditionalCE32.
        const UChar *p = base->contexts + Collation::getPrefixIndex(ce32);
        ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no prefix match.
        ConditionalCE32 head(UnicodeString(), 0);
        UnicodeString context((UChar)0);
        int32_t index;
        if(Collation::isContractionCE32(ce32)) {
            index = copyContractionsFromBaseCE32(context, c, ce32, &head, errorCode);
        } else {
            ce32 = copyFromBaseCE32(c, ce32, errorCode);
            head.next = index = addConditionalCE32(context, ce32, errorCode);
        }
        if(U_FAILURE(errorCode)) { return 0; }
        ConditionalCE32 *cond = getConditionalCE32(index);  // the last ConditionalCE32 so far
        UCharsTrie::Iterator prefixes(p + 2, 0, errorCode);
        while(prefixes.next(errorCode)) {
            const UnicodeString &prefix = prefixes.getString();
            context.remove().append((UChar)prefix.length()).append(prefix);
            ce32 = (uint32_t)prefixes.getValue();
            if(Collation::isContractionCE32(ce32)) {
                index = copyContractionsFromBaseCE32(context, c, ce32, cond, errorCode);
            } else {
                ce32 = copyFromBaseCE32(c, ce32, errorCode);
                cond->next = index = addConditionalCE32(context, ce32, errorCode);
            }
            if(U_FAILURE(errorCode)) { return 0; }
            cond = getConditionalCE32(index);
        }
        ce32 = Collation::makeSpecialCE32(Collation::CONTRACTION_TAG, head.next);
        contextChars.add(c);
    } else if(tag == Collation::CONTRACTION_TAG) {
        ConditionalCE32 head(UnicodeString(), 0);
        UnicodeString context((UChar)0);
        copyContractionsFromBaseCE32(context, c, ce32, &head, errorCode);
        ce32 = Collation::makeSpecialCE32(Collation::CONTRACTION_TAG, head.next);
        contextChars.add(c);
    } else {
        U_ASSERT(FALSE);  // require ce32 == base->getFinalCE32(ce32)
    }
    return ce32;
}

int32_t
CollationDataBuilder::copyContractionsFromBaseCE32(UnicodeString &context, UChar32 c, uint32_t ce32,
                                                   ConditionalCE32 *cond, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    const UChar *p = base->contexts + Collation::getContractionIndex(ce32);
    ce32 = ((uint32_t)p[0] << 16) | p[1];  // Default if no suffix match.
    // Ignore the default mapping if it falls back to another set of contractions:
    // In that case, we are underneath a prefix, and the empty prefix
    // maps to the same contractions.
    int32_t index;
    if(Collation::isContractionCE32(ce32)) {
        U_ASSERT(context.length() > 1);
        index = -1;
    } else {
        ce32 = copyFromBaseCE32(c, ce32, errorCode);
        cond->next = index = addConditionalCE32(context, ce32, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        cond = getConditionalCE32(index);
    }

    int32_t suffixStart = context.length();
    UCharsTrie::Iterator suffixes(p + 2, 0, errorCode);
    while(suffixes.next(errorCode)) {
        context.append(suffixes.getString());
        ce32 = copyFromBaseCE32(c, (uint32_t)suffixes.getValue(), errorCode);
        cond->next = index = addConditionalCE32(context, ce32, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        // No need to update the unsafeBackwardSet because the tailoring set
        // is already a copy of the base set.
        cond = getConditionalCE32(index);
        context.truncate(suffixStart);
    }
    U_ASSERT(index >= 0);
    return index;
}

class CopyHelper {
public:
    CopyHelper(const CollationDataBuilder &s, CollationDataBuilder &d,
               const CollationDataBuilder::CEModifier &m, UErrorCode &initialErrorCode)
            : src(s), dest(d), modifier(m),
              errorCode(initialErrorCode) {}

    UBool copyRangeCE32(UChar32 start, UChar32 end, uint32_t ce32) {
        ce32 = copyCE32(ce32);
        utrie2_setRange32(dest.trie, start, end, ce32, TRUE, &errorCode);
        if(Collation::isContractionCE32(ce32)) {
            dest.contextChars.add(start, end);
        }
        return U_SUCCESS(errorCode);
    }

    uint32_t copyCE32(uint32_t ce32) {
        if(!Collation::isSpecialCE32(ce32)) {
            int64_t ce = modifier.modifyCE32(ce32);
            if(ce != Collation::NO_CE) {
                ce32 = dest.encodeOneCE(ce, errorCode);
            }
        } else {
            int32_t tag = Collation::getSpecialCE32Tag(ce32);
            if(tag == Collation::EXPANSION32_TAG) {
                const uint32_t *srcCE32s = reinterpret_cast<uint32_t *>(src.ce32s.getBuffer());
                srcCE32s += Collation::getExpansionIndex(ce32);
                int32_t length = Collation::getExpansionLength(ce32);
                if(length == 0) { length = (int32_t)*srcCE32s++; }
                // Inspect the source CE32s. Just copy them if none are modified.
                // Otherwise copy to modifiedCEs, with modifications.
                UBool isModified = FALSE;
                for(int32_t i = 0; i < length; ++i) {
                    ce32 = srcCE32s[i];
                    int64_t ce = modifier.modifyCE32(srcCE32s[i]);
                    if(ce == Collation::NO_CE) {
                        if(isModified) {
                            modifiedCEs[i] = Collation::ceFromCE32(ce32);
                        }
                    } else {
                        if(!isModified) {
                            for(int32_t j = 0; j < i; ++j) {
                                modifiedCEs[j] = Collation::ceFromCE32(srcCE32s[j]);
                            }
                            isModified = TRUE;
                        }
                        modifiedCEs[i] = ce;
                    }
                }
                if(isModified) {
                    ce32 = dest.encodeCEs(modifiedCEs, length, errorCode);
                } else {
                    ce32 = dest.encodeExpansion32(
                        reinterpret_cast<const int32_t *>(srcCE32s), length, errorCode);
                }
            } else if(tag == Collation::EXPANSION_TAG) {
                const int64_t *srcCEs = src.ce64s.getBuffer();
                srcCEs += Collation::getExpansionIndex(ce32);
                int32_t length = Collation::getExpansionLength(ce32);
                if(length == 0) { length = (int32_t)*srcCEs++; }
                // Inspect the source CEs. Just copy them if none are modified.
                // Otherwise copy to modifiedCEs, with modifications.
                UBool isModified = FALSE;
                for(int32_t i = 0; i < length; ++i) {
                    int64_t srcCE = srcCEs[i];
                    int64_t ce = modifier.modifyCE(srcCE);
                    if(ce == Collation::NO_CE) {
                        if(isModified) {
                            modifiedCEs[i] = srcCE;
                        }
                    } else {
                        if(!isModified) {
                            for(int32_t j = 0; j < i; ++j) {
                                modifiedCEs[j] = srcCEs[j];
                            }
                            isModified = TRUE;
                        }
                        modifiedCEs[i] = ce;
                    }
                }
                if(isModified) {
                    ce32 = dest.encodeCEs(modifiedCEs, length, errorCode);
                } else {
                    ce32 = dest.encodeExpansion(srcCEs, length, errorCode);
                }
            } else if(tag == Collation::CONTRACTION_TAG) {
                // Copy the list of ConditionalCE32.
                ConditionalCE32 *cond = src.getConditionalCE32ForCE32(ce32);
                U_ASSERT(!cond->hasContext());
                int32_t destIndex = dest.addConditionalCE32(
                        cond->context, copyCE32(cond->ce32), errorCode);
                ce32 = Collation::makeSpecialCE32(Collation::CONTRACTION_TAG, destIndex);
                while(cond->next >= 0) {
                    cond = src.getConditionalCE32(cond->next);
                    ConditionalCE32 *prevDestCond = dest.getConditionalCE32(destIndex);
                    destIndex = dest.addConditionalCE32(
                            cond->context, copyCE32(cond->ce32), errorCode);
                    int32_t suffixStart = cond->context.charAt(0) + 1;
                    dest.unsafeBackwardSet.addAll(cond->context.tempSubString(suffixStart));
                    prevDestCond->next = destIndex;
                }
            } else {
                // Just copy Latin mini expansions as is,
                // assuming that the modifier would not modify them.
                U_ASSERT(tag <= Collation::MAX_LATIN_EXPANSION_TAG);
            }
        }
        return ce32;
    }

    const CollationDataBuilder &src;
    CollationDataBuilder &dest;
    const CollationDataBuilder::CEModifier &modifier;
    int64_t modifiedCEs[Collation::MAX_EXPANSION_LENGTH];
    UErrorCode errorCode;
};

U_CDECL_BEGIN

static UBool U_CALLCONV
enumRangeForCopy(const void *context, UChar32 start, UChar32 end, uint32_t value) {
    return
        value == Collation::UNASSIGNED_CE32 || value == Collation::MIN_SPECIAL_CE32 ||
        ((CopyHelper *)context)->copyRangeCE32(start, end, value);
}

U_CDECL_END

void
CollationDataBuilder::copyFrom(const CollationDataBuilder &src, const CEModifier &modifier,
                               UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie == NULL || utrie2_isFrozen(trie)) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    CopyHelper helper(src, *this, modifier, errorCode);
    utrie2_enum(src.trie, NULL, enumRangeForCopy, &helper);
    errorCode = helper.errorCode;
    // Update the contextChars and the unsafeBackwardSet while copying,
    // in case a character had conditional mappings in the source builder
    // and they were removed later.
    modified |= src.modified;
}

void
CollationDataBuilder::optimize(const UnicodeSet &set, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || set.isEmpty()) { return; }
    // TODO: exclude Hangul syllables? see other TODOs about Hangul
    UnicodeSetIterator iter(set);
    while(iter.next()) {
        UChar32 c = iter.getCodepoint();
        uint32_t ce32 = utrie2_get32(trie, c);
        if(ce32 == Collation::MIN_SPECIAL_CE32) {
            ce32 = base->getFinalCE32(base->getCE32(c));
            ce32 = copyFromBaseCE32(c, ce32, errorCode);
            utrie2_set32(trie, c, ce32, &errorCode);
        }
    }
    modified = TRUE;
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

void
CollationDataBuilder::setDigitTags(UErrorCode &errorCode) {
    UnicodeSet digits(UNICODE_STRING_SIMPLE("[:Nd:]"), errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UnicodeSetIterator iter(digits);
    while(iter.next()) {
        UChar32 c = iter.getCodepoint();
        uint32_t ce32 = utrie2_get32(trie, c);
        if(ce32 != Collation::MIN_SPECIAL_CE32 && ce32 != Collation::UNASSIGNED_CE32) {
            int32_t index = addCE32(ce32, errorCode);
            if(index > 0xffff) {
                // Note: If we overflow with digit CE32s due to their few index bits,
                // then we can either try to reserve as many low-index slots as we have tailored Nd
                // (in CollationBuilder::finalizeCEs() where we copy from
                // a temporary builder to the final one before building it),
                // or change the encoding and store two CE32s per digit, one with the digit value.
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return;
            }
            int32_t value = (index << 4) | u_charDigitValue(c);
            ce32 = Collation::makeSpecialCE32(Collation::DIGIT_TAG, value);
            utrie2_set32(trie, c, ce32, &errorCode);
        }
    }
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
            trie, lead, Collation::makeSpecialCE32(Collation::LEAD_SURROGATE_TAG, value), &errorCode);
    }
}

void
CollationDataBuilder::buildMappings(CollationData &data, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie == NULL || utrie2_isFrozen(trie)) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }

    // TODO: Maybe always optimize Jamo and set HANGUL_TAG?

    buildContexts(errorCode);

    UBool anyJamoAssigned = setJamoCEs(errorCode);
    int32_t jamoIndex = -1;
    if(anyJamoAssigned || base == NULL) {
        jamoIndex = ce64s.size();
        for(int32_t i = 0; i < LENGTHOF(jamoCEs); ++i) {
            ce64s.addElement(jamoCEs[i], errorCode);
        }
    }

    setDigitTags(errorCode);
    setLeadSurrogates(errorCode);

    // For U+0000, move its normal ce32 into CE32s[0] and set IMPLICIT_TAG with 0 data bits.
    ce32s.setElementAt((int32_t)utrie2_get32(trie, 0), 0);
    utrie2_set32(trie, 0, Collation::makeSpecialCE32(Collation::IMPLICIT_TAG, 0u), &errorCode);

    utrie2_freeze(trie, UTRIE2_32_VALUE_BITS, &errorCode);
    if(U_FAILURE(errorCode)) { return; }

    // Mark each lead surrogate as "unsafe"
    // if any of its 1024 associated supplementary code points is "unsafe".
    UChar32 c = 0x10000;
    for(UChar lead = 0xd800; lead < 0xdc00; ++lead, c += 0x400) {
        if(unsafeBackwardSet.containsSome(c, c + 0x3ff)) {
            unsafeBackwardSet.add(lead);
        }
    }
    unsafeBackwardSet.freeze();

    data.trie = trie;
    data.ce32s = reinterpret_cast<const uint32_t *>(ce32s.getBuffer());
    data.ces = ce64s.getBuffer();
    data.contexts = contexts.getBuffer();
    data.base = base;
    if(jamoIndex >= 0) {
        data.jamoCEs = data.ces + jamoIndex;
    } else {
        data.jamoCEs = base->jamoCEs;
    }
    data.unsafeBackwardSet = &unsafeBackwardSet;
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
    if(!Collation::isContractionCE32(ce32)) {
        // Impossible: No context data for c in contextChars.
        errorCode = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    ConditionalCE32 *cond = getConditionalCE32ForCE32(ce32);
    if(cond->next < 0) {
        // Impossible: No actual contexts after the list head.
        errorCode = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    // Entry for an empty prefix, to be stored before the trie.
    // Starts with the no-context CE32.
    // If there are no-prefix contractions, then this changes to their CE32.
    uint32_t emptyPrefixCE32 = cond->ce32;
    UCharsTrieBuilder prefixBuilder(errorCode);
    UCharsTrieBuilder contractionBuilder(errorCode);
    do {
        cond = getConditionalCE32(cond->next);
        // The prefix or suffix can be empty, but not both.
        U_ASSERT(cond->hasContext());
        int32_t prefixLength = cond->context[0];
        UnicodeString prefix(cond->context, 0, prefixLength + 1);
        // Collect all contraction suffixes for one prefix.
        ConditionalCE32 *firstCond = cond;
        ConditionalCE32 *lastCond = cond;
        while(cond->next >= 0 &&
                (cond = getConditionalCE32(cond->next))->context.startsWith(prefix)) {
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
                // There is a mapping for the prefix and the single character c. (p|c)
                // If no other suffix matches, then we return this value.
                emptySuffixCE32 = cond->ce32;
                cond = getConditionalCE32(cond->next);
            } else {
                // There is no mapping for the prefix and just the single character.
                // (There is no p|c, only p|cd, p|ce etc.)
                // When the prefix matches but none of the prefix-specific suffixes,
                // we fall back to no prefix, which might be another set of contractions.
                // For example, if there are mappings for ch, p|cd, p|ce, but not for p|c,
                // then in text "pch" we find the ch contraction.
                emptySuffixCE32 = emptyPrefixCE32;
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
                cond = getConditionalCE32(cond->next);
            }
            int32_t index = addContextTrie(emptySuffixCE32, contractionBuilder, errorCode);
            if(U_FAILURE(errorCode)) { return; }
            if(index > 0x3ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return;
            }
            ce32 = Collation::makeSpecialCE32(Collation::CONTRACTION_TAG, (index << 2) | flags);
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
    utrie2_set32(trie, c, Collation::makeSpecialCE32(Collation::PREFIX_TAG, index), &errorCode);
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

uint32_t
CollationDataBuilder::getCE32FromContext(const UnicodeString &s, uint32_t ce32,
                                         int32_t sIndex, UnicodeSet &consumed) const {
    U_ASSERT(Collation::isContractionCE32(ce32));
    ConditionalCE32 *cond = getConditionalCE32ForCE32(ce32);
    int32_t cpStart = sIndex - U16_LENGTH(s.char32At(sIndex - 1));
    if(cpStart == 0 && sIndex == s.length()) { return cond->ce32; }  // single-character string

    // Find the longest matching prefix.
    int32_t matchingPrefixLength = 0;
    ConditionalCE32 *firstNoPrefixCond = cond;
    ConditionalCE32 *lastNoPrefixCond = cond;
    ConditionalCE32 *firstPrefixCond = NULL;
    ConditionalCE32 *lastPrefixCond = NULL;
    U_ASSERT(cond->next >= 0);
    do {
        cond = getConditionalCE32(cond->next);
        // The prefix or suffix can be empty, but not both.
        U_ASSERT(cond->hasContext());
        int32_t prefixLength = cond->context[0];
        if(prefixLength == 0) {
            lastNoPrefixCond = cond;
        } else if(prefixLength == matchingPrefixLength) {
            if(cond->context.compare(1, prefixLength,
                                     firstPrefixCond->context, 1, prefixLength) == 0) {
                lastPrefixCond = cond;
            }
        } else if(prefixLength > matchingPrefixLength && prefixLength <= cpStart &&
                s.compare(cpStart - prefixLength, prefixLength,
                          cond->context, 1, prefixLength) == 0) {
            firstPrefixCond = lastPrefixCond = cond;
            matchingPrefixLength = prefixLength;
        }
    } while(cond->next >= 0);

    if(firstPrefixCond != NULL) {
        // Try the contractions for the longest matching prefix.
        uint32_t contractionCE32 =
            getCE32FromContraction(s, sIndex, consumed, firstPrefixCond, lastPrefixCond);
        if(contractionCE32 != Collation::UNASSIGNED_CE32) {
            return contractionCE32;
        }
    }

    // Try the no-prefix contractions.
    // Falls back to the no-context value.
    return getCE32FromContraction(s, sIndex, consumed, firstNoPrefixCond, lastNoPrefixCond);
}

uint32_t
CollationDataBuilder::getCE32FromContraction(const UnicodeString &s,
                                             int32_t sIndex, UnicodeSet &consumed,
                                             ConditionalCE32 *firstCond,
                                             ConditionalCE32 *lastCond) const {
    int32_t matchLength = 1 + firstCond->prefixLength();
    UnicodeString match(firstCond->context, 0, matchLength);
    uint32_t ce32;
    if(firstCond->context.length() == matchLength) {
        ce32 = firstCond->ce32;
    } else {
        ce32 = Collation::UNASSIGNED_CE32;
    }

    // Find the longest contiguous match.
    int32_t i = sIndex;
    while(i < s.length()) {
        UChar32 c = s.char32At(i);
        int32_t nextIndex = i + U16_LENGTH(c);
        if(consumed.contains(i)) {
            i = nextIndex;
            continue;
        }
        match.append(c);
        if(match > lastCond->context) { break; }
        ConditionalCE32 *cond = firstCond;
        while(match > cond->context) {
            cond = getConditionalCE32(cond->next);
        }
        if(match == cond->context) {
            // contiguous match
            consumed.add(sIndex, nextIndex - 1);
            sIndex = nextIndex;
            matchLength = match.length();
            ce32 = cond->ce32;
            firstCond = cond;
        } else if(!cond->context.startsWith(match)) {
            // not even a partial match
            break;
        }
        // partial match, continue
        i = nextIndex;
    }
    match.truncate(matchLength);

    // Look for a discontiguous match.
    // Mark matching combining marks as consumed, rather than removing them,
    // so that we do not disturb later prefix matching.
    uint8_t prevCC = 0;
    i = sIndex;
    while(i < s.length()) {
        UChar32 c = s.char32At(i);
        int32_t nextIndex = i + U16_LENGTH(c);
        if(consumed.contains(i)) {
            i = nextIndex;
            continue;
        }
        uint8_t cc = nfcImpl.getCC(nfcImpl.getNorm16(c));
        if(cc == 0) { break; }
        if(cc == prevCC) {
            // c is blocked
            i = nextIndex;
            continue;
        }
        prevCC = cc;
        match.append(c);
        if(match > lastCond->context) {
            match.truncate(matchLength);
            i = nextIndex;
            continue;
        }
        ConditionalCE32 *cond = firstCond;
        while(match > cond->context) {
            cond = getConditionalCE32(cond->next);
        }
        if(match == cond->context) {
            // extend the match by c
            consumed.add(i);
            matchLength = match.length();
            ce32 = cond->ce32;
            firstCond = cond;
        } else {
            // no match for c
            match.truncate(matchLength);
        }
        i = nextIndex;
    }

    return ce32;
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

UTrie2 *
CollationDataBuilder::orphanTrie() {
    UTrie2 *orphan = trie;
    trie = NULL;
    return orphan;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
