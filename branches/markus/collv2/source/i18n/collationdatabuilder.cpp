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

// TODO: Move to utrie2.h.
// TODO: Used here?
U_DEFINE_LOCAL_OPEN_POINTER(LocalUTrie2Pointer, UTrie2, utrie2_close);

U_NAMESPACE_BEGIN

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
    delete reinterpret_cast<ConditionalCE32 *>(obj);
}

U_CDECL_END

CollationDataBuilder::CollationDataBuilder(UErrorCode &errorCode)
        : nfcImpl(*Normalizer2Factory::getNFCImpl(errorCode)),
          base(NULL), trie(NULL),
          ce32s(errorCode), ce64s(errorCode), conditionalCE32s(errorCode),
          fcd16_F00(NULL), compressibleBytes(NULL) {
    // Reserve the first CE32 for U+0000.
    ce32s.addElement(0, errorCode);
    conditionalCE32s.setDeleter(uprv_deleteConditionalCE32);
}

CollationDataBuilder::~CollationDataBuilder() {
    utrie2_close(trie);
    uprv_free(fcd16_F00);
    uprv_free(compressibleBytes);
}

void
CollationDataBuilder::initBase(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }

    fcd16_F00 = (uint16_t *)uprv_malloc(0xf00 * 2);
    if(fcd16_F00 == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    for(UChar32 c = 0; c < 0xf00; ++c) {
        fcd16_F00[c] = nfcImpl.getFCD16(c);
    }

    compressibleBytes = (UBool *)uprv_malloc(256);
    if(compressibleBytes == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    // Not compressible:
    // - digits
    // - Latin
    // - Hani
    // - trail weights
    // Some scripts are compressible, some are not.
    uprv_memset(compressibleBytes, FALSE, 256);
    compressibleBytes[Collation::UNASSIGNED_IMPLICIT_BYTE] = TRUE;

    // For a base, the default is to compute an unassigned-character implicit CE.
    // This includes surrogate code points; see the last option in
    // UCA section 7.1.1 Handling Ill-Formed Code Unit Sequences.
    trie = utrie2_open(Collation::UNASSIGNED_CE32, 0, &errorCode);

    utrie2_set32(trie, 0xfffe, Collation::MERGE_SEPARATOR_CE32, &errorCode);
    utrie2_set32(trie, 0xffff, Collation::MAX_REGULAR_CE32, &errorCode);

    initHanRanges(errorCode);
    initHanCompat(errorCode);
    // TODO
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

    if(U_FAILURE(errorCode)) { return; }
    // TODO
}

void
CollationDataBuilder::initHanRanges(UErrorCode &errorCode) {
    // Preset the Han ranges as ranges of offset CE32s.
    // See http://www.unicode.org/reports/tr10/#Implicit_Weights
    UnicodeSet han(UNICODE_STRING_SIMPLE("[:Unified_Ideograph:]"), errorCode);
    if(U_FAILURE(errorCode)) { return; }
    // Multiply the number of code points by (gap+1).
    // Add one for tailoring after the last Han character.
    int32_t gap = 1;
    int32_t step = gap + 1;
    int32_t numHan = han.size() * step + 1;
    // Numbers of Han primaries per lead byte determined by
    // numbers of 2nd (not compressible) times 3rd primary byte values.
    int32_t numHanPerLeadByte = 253 * 253;
    int32_t numHanLeadBytes = (numHan + numHanPerLeadByte - 1) / numHanPerLeadByte;
    uint32_t hanPrimary = (uint32_t)(Collation::UNASSIGNED_IMPLICIT_BYTE - numHanLeadBytes) << 24;
    hanPrimary |= 0x30300;
    // TODO: Save [fixed first implicit byte xx] and [first implicit [hanPrimary, 05, 05]]
    // TODO: Set the range in invuca.
    UnicodeSetIterator hanIter(han);
    // Unihan extension A sorts after the other BMP ranges.
    UChar32 endOfExtA;
    if(!hanIter.nextRange() ||
        hanIter.getCodepoint() != 0x3400 || (endOfExtA = hanIter.getCodepointEnd()) < 0x4d00
    ) {
        // The first range does not look like Unihan extension A.
        errorCode = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    while(hanIter.nextRange()) {
        UChar32 start = hanIter.getCodepoint();
        UChar32 end = hanIter.getCodepointEnd();
        if(start > 0xffff && endOfExtA >= 0) {
            // Insert extension A.
            hanPrimary = setThreeByteOffsetRange(0x3400, endOfExtA, hanPrimary, step, errorCode);
            endOfExtA = -1;
        }
        hanPrimary = setThreeByteOffsetRange(start, end, hanPrimary, step, errorCode);
    }
}

void
CollationDataBuilder::initHanCompat(UErrorCode &errorCode) {
    // Set the compatibility ideographs which decompose to regular ones.
    UnicodeSet compat(UNICODE_STRING_SIMPLE("[[:Hani:]&[:L:]&[:NFD_QC=No:]]"), errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UnicodeSetIterator iter(compat);
    UChar buffer[4];
    int32_t length;
    while(iter.next()) {
        // Get the singleton decomposition Han character.
        UChar32 c = iter.getCodepoint();
        const UChar *decomp = nfcImpl.getDecomposition(c, buffer, length);
        U_ASSERT(length > 0);
        int32_t i = 0;
        UChar32 han;
        U16_NEXT_UNSAFE(decomp, i, han);
        U_ASSERT(i == length);  // Expect a singleton decomposition.
        // Give c its decomposition's regular CE32.
        uint32_t ce32 = utrie2_get32(trie, han);
        if(Collation::isSpecialCE32(ce32)) {
            if(Collation::getSpecialCE32Tag(ce32) != Collation::OFFSET_TAG) {
                errorCode = U_INTERNAL_PROGRAM_ERROR;
                return;
            }
            ce32 = getCE32FromOffsetCE32(han, ce32);
        } else {
            if(!Collation::isLongPrimaryCE32(ce32)) {
                errorCode = U_INTERNAL_PROGRAM_ERROR;
                return;
            }
        }
        utrie2_set32(trie, c, ce32, &errorCode);
    }
}

uint32_t
CollationDataBuilder::setThreeByteOffsetRange(UChar32 start, UChar32 end,
                                              uint32_t primary, int32_t step,
                                              UErrorCode &errorCode) {
    U_ASSERT(start <= end);
    U_ASSERT((start >> 16) == (end >> 16));  // Range does not span a Unicode plane boundary.
    U_ASSERT(1 <= step && step <= 16);
    // TODO: Do we need to check what values are currently set for start..end?
    // We always set a normal CE32 for the start code point.
    utrie2_set32(trie, start, makeLongPrimaryCE32(primary), &errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    // An offset range is worth it only if we can achieve an overlap between
    // adjacent UTrie2 blocks of 32 code points each.
    // An offset CE is also a little more expensive to look up and compute
    // than a simple CE.
    // We could calculate how many code points are in each block,
    // but a simple minimum range length check should suffice.
    UChar32 limit = end + 1;
    UBool isCompressible = isCompressiblePrimary(primary);
    if((limit - start) >= 40) {  // >= 40 code points in the range
        uint32_t offsetCE32 = makeSpecialCE32(
            Collation::OFFSET_TAG,
            ((uint32_t)(start & 0xffff) << 4) | (uint32_t)(step - 1));
        utrie2_setRange32(trie, start + 1, end, offsetCE32, TRUE, &errorCode);
        return Collation::incThreeBytePrimaryByOffset(primary, isCompressible,
                                                      (limit - start) * step);
    } else {
        // Short range: Set individual CE32s.
        for(;;) {
            ++start;
            primary = Collation::incThreeBytePrimaryByOffset(primary, isCompressible, step);
            if(start > end) { return primary; }
            utrie2_set32(trie, start, makeLongPrimaryCE32(primary), &errorCode);
        }
    }
}

uint32_t
CollationDataBuilder::getCE32FromOffsetCE32(UChar32 c, uint32_t ce32) const {
    UChar32 baseCp = (c & 0x1f0000) | (((UChar32)ce32 >> 4) & 0xffff);
    int32_t offset = (c - baseCp) * ((ce32 & 0xf) + 1);  // delta * increment
    ce32 = utrie2_get32(trie, baseCp);
    // ce32 must be a long-primary pppppp01.
    U_ASSERT(Collation::isLongPrimaryCE32(ce32));
    uint32_t p = Collation::primaryFromLongPrimaryCE32(ce32);
    return makeLongPrimaryCE32(
        Collation::incThreeBytePrimaryByOffset(p, isCompressiblePrimary(p), offset));
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
    if(s.isEmpty() || cesLength <= 0) {
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
    // No FFFE
    // No FFFF
    // If prefix: cc(prefix[0])==cc(c)==0
    // Max lengths?
    uint32_t ce32 = encodeCEs(ces, cesLength, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    uint32_t oldCE32 = utrie2_get32(trie, c);
    // TODO: In a tailoring, if(!isContractionCE32(oldCE32)) then copy c's base CE32 and its context data.
    if(prefix.isEmpty() && s.length() == cLength) {
        // No prefix, no contraction.
        if(!isContractionCE32(oldCE32)) {
            utrie2_set32(trie, c, ce32, &errorCode);
        } else {
            ConditionalCE32 *cond = reinterpret_cast<ConditionalCE32 *>(
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
        } else {
            cond = reinterpret_cast<ConditionalCE32 *>(
                conditionalCE32s[(int32_t)oldCE32 & 0xfffff]);
        }
        UnicodeString context((UChar)prefix.length());
        context.append(prefix).append(s, cLength, 0x7fffffff);
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
            ConditionalCE32 *nextCond = reinterpret_cast<ConditionalCE32 *>(conditionalCE32s[next]);
            int8_t cmp = context.compare(nextCond->context);
            if(cmp < 0) {
                // Insert a new ConditionalCE32 between cond and nextCond.
                int32_t index = addConditionalCE32(context, ce32, errorCode);
                if(U_FAILURE(errorCode)) { return; }
                cond->next = index;
                reinterpret_cast<ConditionalCE32 *>(conditionalCE32s[index])->next = next;
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
        // See if this CE has already been stored.
        int32_t length = ce64s.size();
        int32_t i;
        for(i = 0; i < length && ce != ce64s.elementAti(i); ++i) {}
        if(i > 0x1ffff) {
            errorCode = U_BUFFER_OVERFLOW_ERROR;
            return 0;
        }
        if(i == length) { ce64s.addElement(ce, errorCode); }
        return makeSpecialCE32(Collation::EXPANSION_TAG, (i << 3) | 1);
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

void
CollationDataBuilder::setHiragana(UErrorCode &errorCode) {
    UnicodeSet hira(UNICODE_STRING_SIMPLE("[[:Hira:][\\u3099-\\u309C]]"), errorCode);
    if(U_FAILURE(errorCode)) { return; }
    UnicodeSetIterator iter(hira);
    while(iter.next()) {
        UChar32 c = iter.getCodepoint();
        uint32_t ce32 = utrie2_get32(trie, c);
        if(ce32 == Collation::MIN_SPECIAL_CE32) { continue; }  // Only override a real CE32.
        int32_t index = addCE32(ce32, errorCode);
        if(index > 0xfffff) {
            errorCode = U_BUFFER_OVERFLOW_ERROR;
            return;
        }
        uint32_t hiraCE32 = makeSpecialCE32(Collation::HIRAGANA_TAG, index);
        utrie2_set32(trie, c, hiraCE32, &errorCode);
    }
}

CollationData *
CollationDataBuilder::build(UErrorCode &errorCode) {
    buildContexts(errorCode);
    setHiragana(errorCode);

    // TODO: Copy Latin-1 into each tailoring, but not 0..ff, rather 0..7f && c0..ff.

    // For U+0000, move its normal ce32 into CE32s[0] and set IMPLICIT_TAG with 0 data bits.
    ce32s.setElementAt((int32_t)utrie2_get32(trie, 0), 0);
    utrie2_set32(trie, 0, makeSpecialCE32(Collation::IMPLICIT_TAG, 0u), &errorCode);

    utrie2_freeze(trie, UTRIE2_32_VALUE_BITS, &errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }

    // Create a CollationData container of aliases to this builder's finalized data.
    LocalPointer<CollationData> cd(new CollationData(nfcImpl));
    if(cd.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    cd->trie = trie;
    cd->ce32s = reinterpret_cast<const uint32_t *>(ce32s.getBuffer());
    cd->ces = ce64s.getBuffer();
    cd->contexts = contexts.getBuffer();
    cd->base = base;
    cd->fcd16_F00 = (fcd16_F00 != NULL) ? fcd16_F00 : base->fcd16_F00;
    cd->compressibleBytes = compressibleBytes;
    return cd.orphan();
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
    ConditionalCE32 *cond = reinterpret_cast<ConditionalCE32 *>(
        conditionalCE32s[(int32_t)ce32 & 0xfffff]);
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
        // Collect all contraction suffixes for one prefix.
        ConditionalCE32 *firstCond =
            reinterpret_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]);
        ConditionalCE32 *lastCond = firstCond;
        // The prefix or suffix can be empty, but not both.
        U_ASSERT(firstCond->context.length() > 1);
        int32_t prefixLength = firstCond->context[0];
        UnicodeString prefix(firstCond->context, 0, prefixLength + 1);
        while(lastCond->next >= 0 &&
                (cond = reinterpret_cast<ConditionalCE32 *>(
                    conditionalCE32s[lastCond->next]))->context.startsWith(prefix)) {
            lastCond = cond;
        }
        uint32_t ce32;
        int32_t suffixStart = prefixLength + 1;  // == prefix.length()
        if(lastCond->context.length() == suffixStart) {
            // One prefix without contraction suffix.
            U_ASSERT(firstCond == lastCond);
            ce32 = lastCond->ce32;
        } else {
            // Build the contractions trie.
            contractionBuilder.clear();
            // Entry for an empty suffix, to be stored before the trie.
            uint32_t emptySuffixCE32;
            cond = firstCond;
            if(cond->context.length() == suffixStart) {
                emptySuffixCE32 = cond->ce32;
                cond = reinterpret_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]);
            } else {
                emptySuffixCE32 = defaultCE32;
            }
            // Latin optimization: Flags bit 1 indicates whether
            // the first character of any contraction suffix is >=U+0300.
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
                cond = reinterpret_cast<ConditionalCE32 *>(conditionalCE32s[cond->next]);
            }
            int32_t index = addContextTrie(emptySuffixCE32, contractionBuilder, errorCode);
            if(U_FAILURE(errorCode)) { return; }
            if(index > 0x3ffff) {
                errorCode = U_BUFFER_OVERFLOW_ERROR;
                return;
            }
            ce32 = makeSpecialCE32(Collation::CONTRACTION_TAG, (index << 2) | flags);
        }
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

// TODO: In CollationWeights allocator,
// try to treat secondary & tertiary weights as 3/4-byte weights with bytes 1 & 2 == 0.
// Natural secondary limit of 0x10000.

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
