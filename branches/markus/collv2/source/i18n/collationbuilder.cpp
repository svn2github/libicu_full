/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationbuilder.cpp
*
* created on: 2013may06
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/normalizer2.h"
#include "unicode/parseerr.h"
#include "unicode/ucol.h"
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationbuilder.h"
#include "collationdata.h"
#include "collationroot.h"
#include "collationrootelements.h"
#include "collationruleparser.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "collationtailoringdatabuilder.h"
#include "rulebasedcollator.h"
#include "uassert.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

U_NAMESPACE_BEGIN

// RuleBasedCollator implementation ---------------------------------------- ***

// These methods are here, rather than in rulebasedcollator.cpp,
// for modularization:
// Most code using Collator does not need to build a Collator from rules.
// By moving these constructors and helper methods to a separate file,
// most code will not have a static dependency on the builder code.

RuleBasedCollator2::RuleBasedCollator2(const UnicodeString &rules, UErrorCode &errorCode)
        : data(NULL),
          settings(NULL),
          reader(NULL),
          tailoring(NULL),
          ownedSettings(NULL),
          ownedReorderCodesCapacity(0),
          explicitlySetAttributes(0) {
    buildTailoring(rules, UCOL_DEFAULT, UCOL_DEFAULT, NULL, errorCode);
}

RuleBasedCollator2::RuleBasedCollator2(const UnicodeString &rules, ECollationStrength strength,
                                       UErrorCode &errorCode)
        : data(NULL),
          settings(NULL),
          reader(NULL),
          tailoring(NULL),
          ownedSettings(NULL),
          ownedReorderCodesCapacity(0),
          explicitlySetAttributes(0) {
    buildTailoring(rules, strength, UCOL_DEFAULT, NULL, errorCode);
}

RuleBasedCollator2::RuleBasedCollator2(const UnicodeString &rules,
                                       UColAttributeValue decompositionMode,
                                       UErrorCode &errorCode)
        : data(NULL),
          settings(NULL),
          reader(NULL),
          tailoring(NULL),
          ownedSettings(NULL),
          ownedReorderCodesCapacity(0),
          explicitlySetAttributes(0) {
    buildTailoring(rules, UCOL_DEFAULT, decompositionMode, NULL, errorCode);
}

RuleBasedCollator2::RuleBasedCollator2(const UnicodeString &rules,
                                       ECollationStrength strength,
                                       UColAttributeValue decompositionMode,
                                       UErrorCode &errorCode)
        : data(NULL),
          settings(NULL),
          reader(NULL),
          tailoring(NULL),
          ownedSettings(NULL),
          ownedReorderCodesCapacity(0),
          explicitlySetAttributes(0) {
    buildTailoring(rules, strength, decompositionMode, NULL, errorCode);
}

void
RuleBasedCollator2::buildTailoring(const UnicodeString &rules,
                                   int32_t strength,
                                   UColAttributeValue decompositionMode,
                                   UParseError *outParseError, UErrorCode &errorCode) {
    const CollationData *baseData = CollationRoot::getBaseData(errorCode);
    const CollationSettings *baseSettings = CollationRoot::getBaseSettings(errorCode);
    if(U_FAILURE(errorCode)) { return; }
    tailoring = new CollationTailoring(*baseSettings);
    if(tailoring == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    CollationBuilder builder(baseData, errorCode);
    builder.parseAndBuild(rules, NULL /* TODO: importer */, *tailoring, outParseError, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    tailoring->rules = rules;
    data = tailoring->data;
    settings = &tailoring->settings;
    // TODO: tailoring->version: maybe root version xor rules.hashCode() xor strength xor decomp (if not default)
    // TODO: tailoring->isDataOwned
    if(strength != UCOL_DEFAULT) {
        tailoring->settings.setStrength(strength, 0, errorCode);
    }
    if(decompositionMode != UCOL_DEFAULT) {
        tailoring->settings.setFlag(CollationSettings::CHECK_FCD, decompositionMode, 0, errorCode);
    }
}

// CollationBuilder implementation ----------------------------------------- ***

CollationBuilder::CollationBuilder(const CollationData *base, UErrorCode &errorCode)
        : nfd(*Normalizer2::getNFDInstance(errorCode)),
          baseData(base),
          rootElements(base->rootElements, base->rootElementsLength),
          variableTop(0),
          firstImplicitCE(0),
          dataBuilder(errorCode),
          errorReason(NULL),
          cesLength(0),
          rootPrimaryIndexes(errorCode), nodes(errorCode) {
    // Preset node 0 as the start of a list for root primary 0.
    nodes.addElement(0, errorCode);
    rootPrimaryIndexes.addElement(0, errorCode);

    // Look up [first implicit] before tailoring the relevant character.
    int32_t length = dataBuilder.getCEs(UnicodeString((UChar)0x4e00), ces, 0);
    U_ASSERT(length == 1);
    firstImplicitCE = ces[0];

    if(U_FAILURE(errorCode)) {
        errorReason = "CollationBuilder initialization failed";
    }
}

CollationBuilder::~CollationBuilder() {
}

void
CollationBuilder::parseAndBuild(const UnicodeString &ruleString,
                                CollationRuleParser::Importer *importer,
                                CollationTailoring &tailoring,
                                UParseError *outParseError,
                                UErrorCode &errorCode) {
    CollationRuleParser parser(baseData, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    // TODO: This always bases &[last variable] and &[first regular]
    // on the root collator's maxVariable/variableTop.
    // Discuss whether we would want this to change after [maxVariable x],
    // in which case we would keep the tailoring.settings pointer here
    // and read its variableTop when we need it.
    // See http://unicode.org/cldr/trac/ticket/6070
    variableTop = tailoring.settings.variableTop;
    parser.setSink(this);
    parser.setImporter(importer);
    parser.parse(ruleString, tailoring.settings, outParseError, errorCode);
    errorReason = parser.getErrorReason();
    if(U_FAILURE(errorCode)) { return; }
    // TODO
}

void
CollationBuilder::addReset(int32_t strength, const UnicodeString &str,
                           const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    U_ASSERT(!str.isEmpty());
    if(str.charAt(0) == CollationRuleParser::POS_LEAD) {
        // special reset position
        U_ASSERT(str.length() == 2);
        UChar32 pos = str.charAt(1) - CollationRuleParser::POS_BASE;
        U_ASSERT(0 <= pos && pos <= CollationRuleParser::LAST_TRAILING);
        int64_t ce = 0;
        switch(pos) {
        case CollationRuleParser::FIRST_TERTIARY_IGNORABLE:
            // ce = 0;
            break;
        case CollationRuleParser::LAST_TERTIARY_IGNORABLE:
            // ce = 0;
            break;
        case CollationRuleParser::FIRST_SECONDARY_IGNORABLE:
            ce = rootElements.getFirstTertiaryCE();
            break;
        case CollationRuleParser::LAST_SECONDARY_IGNORABLE:
            ce = rootElements.getLastTertiaryCE();
            break;
        case CollationRuleParser::FIRST_PRIMARY_IGNORABLE:
            ce = rootElements.getFirstSecondaryCE();
            break;
        case CollationRuleParser::LAST_PRIMARY_IGNORABLE:
            ce = rootElements.getLastSecondaryCE();
            break;
        case CollationRuleParser::FIRST_VARIABLE:
            ce = rootElements.getFirstPrimaryCE();
            break;
        case CollationRuleParser::LAST_VARIABLE:
            ce = rootElements.lastCEWithPrimaryBefore(variableTop + 1);
            break;
        case CollationRuleParser::FIRST_REGULAR:
            ce = rootElements.firstCEWithPrimaryAtLeast(variableTop + 1);
            break;
        case CollationRuleParser::LAST_REGULAR:
            // Use [first Hani] rather than the actual last "regular" CE before it,
            // for backward compatibility with behavior before the introduction of
            // script-first primary CEs in the root collator.
            ce = rootElements.firstCEWithPrimaryAtLeast(
                baseData->getFirstPrimaryForGroup(USCRIPT_HAN));
            break;
        case CollationRuleParser::FIRST_IMPLICIT:
            ce = firstImplicitCE;
            break;
        case CollationRuleParser::LAST_IMPLICIT:
            // We do not support tailoring to an unassigned-implicit CE.
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "reset to [last implicit] not supported";
            return;
        case CollationRuleParser::FIRST_TRAILING:
            ce = Collation::makeCE(Collation::FIRST_TRAILING_PRIMARY);
            break;
        case CollationRuleParser::LAST_TRAILING:
            ce = rootElements.lastCEWithPrimaryBefore(Collation::FFFD_PRIMARY);
            break;
        }
        ces[0] = ce;
        cesLength = 1;
    } else {
        // normal reset to a character or string
        UnicodeString nfdString = nfd.normalize(str, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "NFD(reset position)";
            return;
        }
        cesLength = dataBuilder.getCEs(nfdString, ces, 0);
        if(cesLength > Collation::MAX_EXPANSION_LENGTH) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            parserErrorReason = "reset position maps to too many collation elements (more than 31)";
            return;
        }
    }
    if(strength == UCOL_IDENTICAL) { return; }  // simple reset-at-position

    // &[before strength]position
    U_ASSERT(UCOL_PRIMARY <= strength && strength <= UCOL_TERTIARY);
    int32_t index = findOrInsertNodeForCEs(strength, parserErrorReason, errorCode);
    int64_t ce = ces[cesLength - 1];
    if(strength == UCOL_PRIMARY && ce == rootElements.getFirstPrimaryCE()) {
        // There is no primary gap between ignorables and [first space].
        errorCode = U_UNSUPPORTED_ERROR;
        parserErrorReason = "reset primary-before first non-ignorable not supported";
        return;
    }
    if(strength == UCOL_PRIMARY && ce == Collation::makeCE(Collation::FIRST_TRAILING_PRIMARY)) {
        // We do not support tailoring to an unassigned-implicit CE.
        errorCode = U_UNSUPPORTED_ERROR;
        parserErrorReason = "reset primary-before [first trailing] not supported";
        return;
    }
    errorCode = U_UNSUPPORTED_ERROR;  // TODO
    parserErrorReason = "TODO: support &[before n]";
}

void
CollationBuilder::addRelation(int32_t strength, const UnicodeString &prefix,
                              const UnicodeString &str, const UnicodeString &extension,
                              const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    UnicodeString nfdPrefix;
    if(!prefix.isEmpty()) {
        nfd.normalize(prefix, nfdPrefix, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "NFD(prefix)";
            return;
        }
    }
    UnicodeString nfdString = nfd.normalize(str, errorCode);
    if(U_FAILURE(errorCode)) {
        parserErrorReason = "NFD(string)";
        return;
    }
    if(strength != UCOL_IDENTICAL) {
        // Find the node index after which we insert the new tailored node.
        int32_t index = findOrInsertNodeForCEs(strength, parserErrorReason, errorCode);
        if(index == 0) {
            // There is no primary gap between ignorables and [first space].
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "tailoring primary after ignorables not supported";
            return;
        }
        // Insert the new tailored node.
        index = insertTailoredNodeAfter(index, strength, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "modifying collation elements";
            return;
        }
        ces[cesLength - 1] = tempCEFromIndexAndStrength(index, strength);
    }
    int32_t totalLength = cesLength;
    if(!extension.isEmpty()) {
        UnicodeString nfdExtension = nfd.normalize(extension, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "NFD(extension)";
            return;
        }
        totalLength = dataBuilder.getCEs(nfdExtension, ces, cesLength);
        if(totalLength > Collation::MAX_EXPANSION_LENGTH) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            parserErrorReason =
                "extension string adds too many collation elements (more than 31 total)";
            return;
        }
    }
    // Map from the NFD input to the CEs.
    dataBuilder.add(nfdPrefix, nfdString, ces, cesLength, errorCode);
    if(prefix != nfdPrefix || str != nfdString) {
        // Also right away map from the FCC input to the CEs.
        dataBuilder.add(prefix, str, ces, cesLength, errorCode);
    }
    if(U_FAILURE(errorCode)) {
        parserErrorReason = "writing collation elements";
        return;
    }
}

int32_t
CollationBuilder::ceStrength(int64_t ce) {
    return
        isTempCE(ce) ? strengthFromTempCE(ce) :
        (ce & 0xff00000000000000) != 0 ? UCOL_PRIMARY :
        ((uint32_t)ce & 0xff000000) != 0 ? UCOL_SECONDARY :
        ce != 0 ? UCOL_TERTIARY :
        UCOL_IDENTICAL;
}

int32_t
CollationBuilder::findOrInsertNodeForCEs(int32_t strength, const char *&parserErrorReason,
                                         UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    U_ASSERT(UCOL_PRIMARY <= strength && strength <= UCOL_QUATERNARY);

    // Find the last CE that is at least as "strong" as the requested difference.
    // Note: Stronger is smaller (UCOL_PRIMARY=0).
    int64_t ce;
    for(;; --cesLength) {
        if(cesLength == 0) {
            ce = ces[0] = 0;
            cesLength = 1;
            break;
        } else {
            ce = ces[cesLength - 1];
        }
        if(ceStrength(ce) <= strength) {
            break;
        }
    }

    if(isTempCE(ce)) { return indexFromTempCE(ce); }

    if((uint8_t)(ce >> 56) == Collation::UNASSIGNED_IMPLICIT_BYTE) {
        errorCode = U_UNSUPPORTED_ERROR;
        parserErrorReason = "tailoring relative to an unassigned code point not supported";
        return 0;
    }
    return findOrInsertNodeForRootCE(ce, strength, errorCode);
}

namespace {

/**
 * Like Java Collections.binarySearch(List, key, Comparator).
 *
 * @return the index>=0 where the item was found,
 *         or the index<0 for inserting the string at ~index in sorted order
 *         (index into rootPrimaryIndexes)
 */
int32_t
binarySearchForRootPrimaryNode(const int32_t *rootPrimaryIndexes, int32_t length,
                               const int64_t *nodes, uint32_t p) {
    U_ASSERT(length > 0);
    int32_t start = 0;
    int32_t limit = length;
    for (;;) {
        int32_t i = (start + limit) / 2;
        int64_t node = nodes[rootPrimaryIndexes[i]];
        uint32_t nodePrimary = (uint32_t)(node >> 32);  // weight32FromNode(node)
        if (p == nodePrimary) {
            return i;
        } else if (p < nodePrimary) {
            if (i == start) {
                return ~start;  // insert s before i
            }
            limit = i;
        } else {
            if (i == start) {
                return ~(start + 1);  // insert s after i
            }
            start = i;
        }
    }
}

}  // namespace

int32_t
CollationBuilder::findOrInsertNodeForRootCE(int64_t ce, int32_t strength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }

    // Find or insert the node for the root CE's primary weight.
    uint32_t p = (uint32_t)(ce >> 32);
    int32_t rootIndex = binarySearchForRootPrimaryNode(
        rootPrimaryIndexes.getBuffer(), rootPrimaryIndexes.size(), nodes.getBuffer(), p);
    int32_t index;
    if(rootIndex >= 0) {
        index = rootPrimaryIndexes.elementAti(rootIndex);
    } else {
        // Start a new list of nodes with this primary.
        index = nodes.size();
        nodes.addElement(nodeFromWeight32(p), errorCode);
        rootPrimaryIndexes.insertElementAt(index, ~rootIndex, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
    }

    // Find or insert the node for each of the root CE's lower-level weights,
    // down to the requested level/strength.
    for(int32_t level = UCOL_SECONDARY; level <= strength; ++level) {
        uint32_t lower32 = (uint32_t)ce;
        uint32_t weight16 =
            level == UCOL_SECONDARY ? lower32 >> 16 :
            level == UCOL_TERTIARY ? lower32 & Collation::ONLY_TERTIARY_MASK : 0;
        // Find the root CE's weight for this level.
        // Postpone insertion if not found:
        // Insert the new root node before the next stronger node,
        // or before the next root node with the same strength and a larger weight.
        int64_t node = nodes.elementAti(index);
        int32_t nextIndex;
        for(;;) {
            nextIndex = nextIndexFromNode(node);
            node = nodes.elementAti(nextIndex);
            int32_t nextStrength = strengthFromNode(node);
            if(nextStrength <= strength) {
                // Insert before a stronger node.
                if(nextStrength < strength) { break; }
                // nextStrength == strength
                if((node & TAILORED_NODE) != 0) {
                    uint32_t nextWeight16 = weight16FromNode(node);
                    if(nextWeight16 == weight16) {
                        // Found the node for the root CE up to this level.
                        index = nextIndex;
                        nextIndex = -1;  // no insertion (continue outer loop)
                        break;
                    }
                    // Insert before a node with a larger same-strength weight.
                    if(nextWeight16 > weight16) { break; }
                }
            }
            // Skip the next node.
            index = nextIndex;
        }
        if(nextIndex >= 0) {
            node = nodeFromWeight16(weight16) | nodeFromStrength(level);
            index = insertNodeBetween(index, nextIndex, node, errorCode);
            if(U_FAILURE(errorCode)) { return 0; }
        }
    }
    return index;
}

int32_t
CollationBuilder::insertTailoredNodeAfter(int32_t index, int32_t strength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    // Postpone insertion:
    // Insert the new node before the next one with a strength at least as strong.
    int64_t node = nodes.elementAti(index);
    int32_t nextIndex;
    for(;;) {
        nextIndex = nextIndexFromNode(node);
        if(nextIndex == 0) { break; }
        node = nodes.elementAti(nextIndex);
        if(strengthFromNode(node) <= strength) { break; }
        // Skip the next node which has a weaker (larger) strength than the new one.
        index = nextIndex;
    }
    node = TAILORED_NODE | nodeFromStrength(strength);
    return insertNodeBetween(index, nextIndex, node, errorCode);
}

int32_t
CollationBuilder::insertNodeBetween(int32_t index, int32_t nextIndex, int64_t node,
                                    UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    U_ASSERT(previousIndexFromNode(node) == 0);
    U_ASSERT(nextIndexFromNode(node) == 0);
    U_ASSERT(nextIndexFromNode(nodes.elementAti(index)) == nextIndex);
    // Append the new node and link it to the existing nodes.
    int32_t newIndex = nodes.size();
    node |= nodeFromPreviousIndex(index) | nodeFromNextIndex(nextIndex);
    nodes.addElement(node, errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    // nodes[index].nextIndex = newIndex
    node = nodes.elementAti(index);
    nodes.setElementAt(changeNodeNextIndex(node, newIndex), index);
    // nodes[nextIndex].previousIndex = newIndex
    if(nextIndex != 0) {
        node = nodes.elementAti(nextIndex);
        nodes.setElementAt(changeNodePreviousIndex(node, newIndex), nextIndex);
    }
    return newIndex;
}

void
CollationBuilder::suppressContractions(const UnicodeSet &set,
                                       const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(!set.isEmpty()) {
        errorCode = U_UNSUPPORTED_ERROR;  // TODO
        parserErrorReason = "TODO: support [suppressContractions [set]]";
        return;
    }
    // TODO
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
