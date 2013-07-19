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

#ifdef DEBUG_COLLATION_BUILDER
#include <stdio.h>
#endif

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/normalizer2.h"
#include "unicode/parseerr.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationbuilder.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationroot.h"
#include "collationrootelements.h"
#include "collationruleparser.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "collationweights.h"
#include "rulebasedcollator.h"
#include "uassert.h"
#include "utf16collationiterator.h"

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
    if(U_FAILURE(errorCode)) { return; }
    CollationBuilder builder(CollationRoot::getRoot(errorCode), errorCode);
    LocalPointer<CollationTailoring> t(builder.parseAndBuild(rules, NULL /* TODO: importer */,
                                                             outParseError, errorCode));
    if(U_FAILURE(errorCode)) { return; }
    if(strength != UCOL_DEFAULT) {
        t->settings.setStrength(strength, 0, errorCode);
    }
    if(decompositionMode != UCOL_DEFAULT) {
        t->settings.setFlag(CollationSettings::CHECK_FCD, decompositionMode, 0, errorCode);
    }
    if(U_FAILURE(errorCode)) { return; }
    data = t->data;
    settings = &t->settings;
    t->addRef();
    tailoring = t.orphan();
}

// CollationBuilder implementation ----------------------------------------- ***

CollationBuilder::CollationBuilder(const CollationTailoring *b, UErrorCode &errorCode)
        : nfd(*Normalizer2::getNFDInstance(errorCode)),
          fcc(*Normalizer2::getInstance(NULL, "nfc", UNORM2_COMPOSE_CONTIGUOUS, errorCode)),
          base(b),
          baseData(b->data),
          rootElements(b->data->rootElements, b->data->rootElementsLength),
          variableTop(0),
          dataBuilder(new CollationDataBuilder(errorCode)),
          errorReason(NULL),
          cesLength(0),
          rootPrimaryIndexes(errorCode), nodes(errorCode) {
    if(U_FAILURE(errorCode)) {
        errorReason = "CollationBuilder fields initialization failed";
        return;
    }
    if(dataBuilder == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    dataBuilder->initForTailoring(baseData, errorCode);
    if(U_FAILURE(errorCode)) {
        errorReason = "CollationBuilder initialization failed";
    }
}

CollationBuilder::~CollationBuilder() {
}

CollationTailoring *
CollationBuilder::parseAndBuild(const UnicodeString &ruleString,
                                CollationRuleParser::Importer *importer,
                                UParseError *outParseError,
                                UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    if(baseData->rootElements == NULL) {
        errorCode = U_MISSING_RESOURCE_ERROR;
        errorReason = "missing root elements data, tailoring not supported";
        return NULL;
    }
    LocalPointer<CollationTailoring> tailoring(new CollationTailoring(&base->settings));
    if(tailoring.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    CollationRuleParser parser(baseData, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    // Note: This always bases &[last variable] and &[first regular]
    // on the root collator's maxVariable/variableTop.
    // If we wanted this to change after [maxVariable x], then we would keep
    // the tailoring.settings pointer here and read its variableTop when we need it.
    // See http://unicode.org/cldr/trac/ticket/6070
    variableTop = base->settings.variableTop;
    parser.setSink(this);
    parser.setImporter(importer);
    parser.parse(ruleString, *tailoring, outParseError, errorCode);
    errorReason = parser.getErrorReason();
    if(dataBuilder->hasMappings()) {
        makeTailoredCEs(errorCode);
        // TODO: canonical closure
        finalizeCEs(errorCode);
        // Copy all of ASCII, and Latin-1 letters, into each tailoring.
        optimizeSet.add(0, 0x7f);
        optimizeSet.add(0xc0, 0xff);
        dataBuilder->optimize(optimizeSet, errorCode);
        tailoring->ensureOwnedData(errorCode);
        if(U_FAILURE(errorCode)) { return NULL; }
        dataBuilder->build(*tailoring->ownedData, errorCode);
        tailoring->builder = dataBuilder;
        dataBuilder = NULL;
    } else {
        tailoring->data = baseData;
    }
    CollationSettings::MaxVariable maxVariable = tailoring->settings.getMaxVariable();
    if(maxVariable != base->settings.getMaxVariable()) {
        uint32_t variableTop = tailoring->data->getVariableTopForMaxVariable(maxVariable);
        U_ASSERT(variableTop != 0);  // The rule parser should enforce valid settings.
        tailoring->settings.variableTop = variableTop;
    }
    // TODO: remember if any settings were modified, if useful for suppressing them in writing .res file data
    // (otherwise just detect that they are the same as the root collator settings)
    if(U_FAILURE(errorCode)) { return NULL; }
    tailoring->rules = ruleString;
    // TODO: tailoring->version: maybe root version xor rules.hashCode() xor strength xor decomp (if not default)
    return tailoring.orphan();
}

void
CollationBuilder::addReset(int32_t strength, const UnicodeString &str,
                           const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    U_ASSERT(!str.isEmpty());
    if(str.charAt(0) == CollationRuleParser::POS_LEAD) {
        ces[0] = getSpecialResetPosition(str, parserErrorReason, errorCode);
        cesLength = 1;
        if(U_FAILURE(errorCode)) { return; }
        U_ASSERT((ces[0] & Collation::CASE_AND_QUATERNARY_MASK) == 0);
    } else {
        // normal reset to a character or string
        UnicodeString nfdString = nfd.normalize(str, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "normalizing the reset position";
            return;
        }
        cesLength = dataBuilder->getCEs(nfdString, ces, 0);
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
    if(U_FAILURE(errorCode)) { return; }

    int64_t node = nodes.elementAti(index);
    // If the index is for a "weaker" tailored node,
    // then skip backwards over this and further "weaker" nodes.
    while(strengthFromNode(node) > strength) {
        index = previousIndexFromNode(node);
        node = nodes.elementAti(index);
    }

    // Find or insert a node whose index we will put into a temporary CE.
    if(strengthFromNode(node) == strength && isTailoredNode(node)) {
        // Reset to just before this same-strength tailored node.
        index = previousIndexFromNode(node);
    } else if(strength == UCOL_PRIMARY) {
        // root primary node (has no previous index)
        uint32_t p = weight32FromNode(node);
        if(p == 0) {
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "reset primary-before ignorable not possible";
            return;
        }
        if(p <= rootElements.getFirstPrimary()) {
            // There is no primary gap between ignorables and the space-first-primary.
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "reset primary-before first non-ignorable not supported";
            return;
        }
        if(p == Collation::FIRST_TRAILING_PRIMARY) {
            // We do not support tailoring to an unassigned-implicit CE.
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "reset primary-before [first trailing] not supported";
            return;
        }
        p = rootElements.getPrimaryBefore(p, baseData->isCompressiblePrimary(p));
        index = findOrInsertNodeForPrimary(p, errorCode);
        // Go to the last node in this list:
        // Tailor after the last node between adjacent root nodes.
        for(;;) {
            node = nodes.elementAti(index);
            int32_t nextIndex = nextIndexFromNode(node);
            if(nextIndex == 0) { break; }
            index = nextIndex;
        }
    } else {
        // &[before 2] or &[before 3]
        index = findCommonNode(index, UCOL_SECONDARY);
        if(strength >= UCOL_TERTIARY) {
            index = findCommonNode(index, UCOL_TERTIARY);
        }
        node = nodes.elementAti(index);
        if(strengthFromNode(node) == strength) {
            // Found a same-strength node with an explicit weight.
            uint32_t weight16 = weight16FromNode(node);
            if(weight16 == 0) {
                errorCode = U_UNSUPPORTED_ERROR;
                if(strength == UCOL_SECONDARY) {
                    parserErrorReason = "reset secondary-before secondary ignorable not possible";
                } else {
                    parserErrorReason = "reset tertiary-before completely ignorable not possible";
                }
                return;
            }
            U_ASSERT(weight16 >= Collation::COMMON_WEIGHT16);
            int32_t previousIndex = previousIndexFromNode(node);
            if(weight16 == Collation::COMMON_WEIGHT16) {
                // Reset to just before this same-strength common-weight node.
                index = previousIndex;
            } else {
                // A non-common weight is only possible from a root CE.
                // Find the higher-level weights, which must all be explicit,
                // and then find the preceding weight for this level.
                uint32_t previousWeight16 = 0;
                int32_t previousWeightIndex = -1;
                int32_t i = index;
                if(strength == UCOL_SECONDARY) {
                    uint32_t p;
                    do {
                        i = previousIndexFromNode(node);
                        node = nodes.elementAti(i);
                        if(strengthFromNode(node) == UCOL_SECONDARY && !isTailoredNode(node) &&
                                previousWeightIndex < 0) {
                            previousWeightIndex = i;
                            previousWeight16 = weight16FromNode(node);
                        }
                    } while(strengthFromNode(node) > UCOL_PRIMARY);
                    U_ASSERT(!isTailoredNode(node));
                    p = weight32FromNode(node);
                    weight16 = rootElements.getSecondaryBefore(p, weight16);
                } else {
                    uint32_t p, s;
                    do {
                        i = previousIndexFromNode(node);
                        node = nodes.elementAti(i);
                        if(strengthFromNode(node) == UCOL_TERTIARY && !isTailoredNode(node) &&
                                previousWeightIndex < 0) {
                            previousWeightIndex = i;
                            previousWeight16 = weight16FromNode(node);
                        }
                    } while(strengthFromNode(node) > UCOL_SECONDARY);
                    U_ASSERT(!isTailoredNode(node));
                    if(strengthFromNode(node) == UCOL_SECONDARY) {
                        s = weight16FromNode(node);
                        do {
                            i = previousIndexFromNode(node);
                            node = nodes.elementAti(i);
                        } while(strengthFromNode(node) > UCOL_PRIMARY);
                        U_ASSERT(!isTailoredNode(node));
                    } else {
                        U_ASSERT(!nodeHasBefore2(node));
                        s = Collation::COMMON_WEIGHT16;
                    }
                    p = weight32FromNode(node);
                    weight16 = rootElements.getTertiaryBefore(p, s, weight16);
                    U_ASSERT((weight16 & ~Collation::ONLY_TERTIARY_MASK) == 0);
                }
                // Find or insert the new explicit weight before the current one.
                if(previousWeightIndex >= 0 && weight16 == previousWeight16) {
                    // Tailor after the last node between adjacent root nodes.
                    index = previousIndex;
                } else {
                    node = nodeFromWeight16(weight16) | nodeFromStrength(strength);
                    index = insertNodeBetween(previousIndex, index, node, errorCode);
                }
            }
        } else {
            // Found a stronger node with implied strength-common weight.
            int64_t hasBefore3 = 0;
            if(strength == UCOL_SECONDARY) {
                U_ASSERT(!nodeHasBefore2(node));
                // Move the HAS_BEFORE3 flag from the parent node
                // to the new secondary common node.
                hasBefore3 = node & HAS_BEFORE3;
                node = (node & ~(int64_t)HAS_BEFORE3) | HAS_BEFORE2;
            } else {
                U_ASSERT(!nodeHasBefore3(node));
                node |= HAS_BEFORE3;
            }
            nodes.setElementAt(node, index);
            int32_t nextIndex = nextIndexFromNode(node);
            // Insert default nodes with weights 02 and 05, reset to the 02 node.
            node = nodeFromWeight16(BEFORE_WEIGHT16) | nodeFromStrength(strength);
            index = insertNodeBetween(index, nextIndex, node, errorCode);
            node = nodeFromWeight16(Collation::COMMON_WEIGHT16) | hasBefore3 |
                    nodeFromStrength(strength);
            insertNodeBetween(index, nextIndex, node, errorCode);
        }
        // Strength of the temporary CE = strength of its reset position.
        // Code above raises an error if the before-strength is stronger.
        strength = ceStrength(ces[cesLength - 1]);
    }
    if(U_FAILURE(errorCode)) {
        parserErrorReason = "inserting reset position for &[before n]";
        return;
    }
    ces[cesLength - 1] = tempCEFromIndexAndStrength(index, strength);
}

int64_t
CollationBuilder::getSpecialResetPosition(const UnicodeString &str,
                                          const char *&parserErrorReason, UErrorCode &errorCode) {
    U_ASSERT(str.length() == 2);
    int64_t ce;
    int32_t strength = UCOL_PRIMARY;
    UChar32 pos = str.charAt(1) - CollationRuleParser::POS_BASE;
    U_ASSERT(0 <= pos && pos <= CollationRuleParser::LAST_TRAILING);
    switch(pos) {
    case CollationRuleParser::FIRST_TERTIARY_IGNORABLE:
        // Quaternary CEs are not supported.
        // Non-zero quaternary weights are possible only on tertiary or stronger CEs.
        return 0;
    case CollationRuleParser::LAST_TERTIARY_IGNORABLE:
        return 0;
    case CollationRuleParser::FIRST_SECONDARY_IGNORABLE: {
        // Look for a tailored tertiary node after [0, 0, 0].
        int32_t index = findOrInsertNodeForRootCE(0, UCOL_TERTIARY, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        int64_t node = nodes.elementAti(index);
        if((index = nextIndexFromNode(node)) != 0) {
            node = nodes.elementAti(index);
            U_ASSERT(strengthFromNode(node) <= UCOL_TERTIARY);
            if(isTailoredNode(node) && strengthFromNode(node) == UCOL_TERTIARY) {
                return tempCEFromIndexAndStrength(index, UCOL_TERTIARY);
            }
        }
        return rootElements.getFirstTertiaryCE();
        // No need to look for nodeHasAnyBefore() on a tertiary node.
    }
    case CollationRuleParser::LAST_SECONDARY_IGNORABLE:
        ce = rootElements.getLastTertiaryCE();
        strength = UCOL_TERTIARY;
        break;
    case CollationRuleParser::FIRST_PRIMARY_IGNORABLE: {
        // Look for a tailored secondary node after [0, 0, *].
        int32_t index = findOrInsertNodeForRootCE(0, UCOL_SECONDARY, errorCode);
        if(U_FAILURE(errorCode)) { return 0; }
        int64_t node = nodes.elementAti(index);
        while((index = nextIndexFromNode(node)) != 0) {
            node = nodes.elementAti(index);
            strength = strengthFromNode(node);
            if(strength < UCOL_SECONDARY) { break; }
            if(strength == UCOL_SECONDARY) {
                if(isTailoredNode(node)) {
                    if(nodeHasBefore3(node)) {
                        index = nextIndexFromNode(nodes.elementAti(nextIndexFromNode(node)));
                        U_ASSERT(isTailoredNode(nodes.elementAti(index)));
                    }
                    return tempCEFromIndexAndStrength(index, UCOL_SECONDARY);
                } else {
                    break;
                }
            }
        }
        ce = rootElements.getFirstSecondaryCE();
        strength = UCOL_SECONDARY;
        break;
    }
    case CollationRuleParser::LAST_PRIMARY_IGNORABLE:
        ce = rootElements.getLastSecondaryCE();
        strength = UCOL_SECONDARY;
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
        // Use the Hani-first-primary rather than the actual last "regular" CE before it,
        // for backward compatibility with behavior before the introduction of
        // script-first-primary CEs in the root collator.
        ce = rootElements.firstCEWithPrimaryAtLeast(
            baseData->getFirstPrimaryForGroup(USCRIPT_HAN));
        break;
    case CollationRuleParser::FIRST_IMPLICIT: {
        uint32_t ce32 = baseData->getCE32(0x4e00);
        U_ASSERT(Collation::hasCE32Tag(ce32, Collation::OFFSET_TAG));
        int64_t dataCE = baseData->ces[Collation::indexFromCE32(ce32)];
        ce = Collation::makeCE(Collation::getThreeBytePrimaryForOffsetData(0x4e00, dataCE));
        break;
    }
    case CollationRuleParser::LAST_IMPLICIT:
        // We do not support tailoring to an unassigned-implicit CE.
        errorCode = U_UNSUPPORTED_ERROR;
        parserErrorReason = "reset to [last implicit] not supported";
        return 0;
    case CollationRuleParser::FIRST_TRAILING:
        ce = Collation::makeCE(Collation::FIRST_TRAILING_PRIMARY);
        break;
    case CollationRuleParser::LAST_TRAILING:
        ce = rootElements.lastCEWithPrimaryBefore(Collation::FFFD_PRIMARY);
        break;
    default:
        U_ASSERT(FALSE);
        return 0;
    }

    int32_t index = findOrInsertNodeForRootCE(ce, strength, errorCode);
    if(U_FAILURE(errorCode)) { return 0; }
    int64_t node = nodes.elementAti(index);
    if((pos & 1) == 0) {
        // even pos = [first xyz]
        if(nodeHasAnyBefore(node)) {
            // Get the first node that was tailored before the [first xyz]
            // at a weaker strength.
            if(nodeHasBefore2(node)) {
                index = nextIndexFromNode(nodes.elementAti(nextIndexFromNode(node)));
                node = nodes.elementAti(index);
            }
            if(nodeHasBefore3(node)) {
                index = nextIndexFromNode(nodes.elementAti(nextIndexFromNode(node)));
            }
            U_ASSERT(isTailoredNode(nodes.elementAti(index)));
            ce = tempCEFromIndexAndStrength(index, strength);
        }
    } else {
        // odd pos = [last xyz]
        // Find the last node that was tailored after the [last xyz]
        // at a strength no greater than the position's strength.
        for(;;) {
            int32_t nextIndex = nextIndexFromNode(node);
            if(nextIndex == 0) { break; }
            int32_t nextNode = nodes.elementAti(nextIndex);
            if(strengthFromNode(nextNode) < strength) { break; }
            index = nextIndex;
            node = nextNode;
        }
        // Do not make a temporary CE for a root node.
        // This last node might be the node for the root CE itself,
        // or a node with a common secondary or tertiary weight.
        if(isTailoredNode(node)) {
            ce = tempCEFromIndexAndStrength(index, strength);
        }
    }
    return ce;
}

void
CollationBuilder::addRelation(int32_t strength, const UnicodeString &prefix,
                              const UnicodeString &str, const UnicodeString &extension,
                              const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    UnicodeString nfdPrefix, fccPrefix;
    if(!prefix.isEmpty()) {
        nfd.normalize(prefix, nfdPrefix, errorCode);
        fcc.normalize(prefix, fccPrefix, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "normalizing the relation prefix";
            return;
        }
    }
    UnicodeString nfdString = nfd.normalize(str, errorCode);
    UnicodeString fccString = fcc.normalize(str, errorCode);
    if(U_FAILURE(errorCode)) {
        parserErrorReason = "normalizing the relation string";
        return;
    }

    UBool hasContext = !nfdPrefix.isEmpty() || nfdString.hasMoreChar32Than(0, 0x7fffffff, 1);
    if(hasContext && (containsJamo(nfdPrefix) || containsJamo(nfdString))) {
        errorCode = U_UNSUPPORTED_ERROR;
        parserErrorReason = "contextual mappings with conjoining Jamo (hst=L/V/T) not supported";
        return;
    }

    if(strength != UCOL_IDENTICAL) {
        // Find the node index after which we insert the new tailored node.
        int32_t index = findOrInsertNodeForCEs(strength, parserErrorReason, errorCode);
        U_ASSERT(cesLength > 0);
        int64_t ce = ces[cesLength - 1];
        if(strength == UCOL_PRIMARY && !isTempCE(ce) && (uint32_t)(ce >> 32) == 0) {
            // There is no primary gap between ignorables and the space-first-primary.
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "tailoring primary after ignorables not supported";
            return;
        }
        if(strength == UCOL_QUATERNARY && ce == 0) {
            // The CE data structure does not support non-zero quaternary weights
            // on tertiary ignorables.
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "tailoring quaternary after tertiary ignorables not supported";
            return;
        }
        // Insert the new tailored node.
        index = insertTailoredNodeAfter(index, strength, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "modifying collation elements";
            return;
        }
        // Strength of the temporary CE:
        // The new relation may yield a stronger CE but not a weaker one.
        int32_t tempStrength = ceStrength(ce);
        if(strength < tempStrength) { tempStrength = strength; }
        ces[cesLength - 1] = tempCEFromIndexAndStrength(index, tempStrength);
    }

    if(!hasContext && cesLength > 1) {
        int32_t hst = u_getIntPropertyValue(nfdString.charAt(0), UCHAR_HANGUL_SYLLABLE_TYPE);
        if(hst == U_HST_LEADING_JAMO || hst == U_HST_VOWEL_JAMO || hst == U_HST_TRAILING_JAMO) {
            errorCode = U_UNSUPPORTED_ERROR;
            parserErrorReason = "expansions for conjoining Jamo (hst=L/V/T) not supported";
            return;
        }
    }

    setCaseBits(nfdString, parserErrorReason, errorCode);
    if(U_FAILURE(errorCode)) { return; }

    int32_t cesLengthBeforeExtension = cesLength;
    if(!extension.isEmpty()) {
        UnicodeString nfdExtension = nfd.normalize(extension, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "normalizing the relation extension";
            return;
        }
        cesLength = dataBuilder->getCEs(nfdExtension, ces, cesLength);
        if(cesLength > Collation::MAX_EXPANSION_LENGTH) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            parserErrorReason =
                "extension string adds too many collation elements (more than 31 total)";
            return;
        }
    }
    // Map from the NFD input to the CEs.
    dataBuilder->add(nfdPrefix, nfdString, ces, cesLength, errorCode);
    if(fccPrefix != nfdPrefix || fccString != nfdString) {
        // Also right away map from the FCC input to the CEs.
        // Do not map from un-normalized strings that may not pass the FCD check.
        dataBuilder->add(fccPrefix, fccString, ces, cesLength, errorCode);
    }
    if(U_FAILURE(errorCode)) {
        parserErrorReason = "writing collation elements";
        return;
    }
    cesLength = cesLengthBeforeExtension;
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
        if(ceStrength(ce) <= strength) { break; }
    }

    if(isTempCE(ce)) {
        // No need to findCommonNode() here for lower levels
        // because insertTailoredNodeAfter() will do that anyway.
        return indexFromTempCE(ce);
    }

    // root CE
    if((uint8_t)(ce >> 56) == Collation::UNASSIGNED_IMPLICIT_BYTE) {
        errorCode = U_UNSUPPORTED_ERROR;
        parserErrorReason = "tailoring relative to an unassigned code point not supported";
        return 0;
    }
    return findOrInsertNodeForRootCE(ce, strength, errorCode);
}

int32_t
CollationBuilder::findOrInsertNodeForRootCE(int64_t ce, int32_t strength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    U_ASSERT((uint8_t)(ce >> 56) != Collation::UNASSIGNED_IMPLICIT_BYTE);

    // Find or insert the node for each of the root CE's weights,
    // down to the requested level/strength.
    // Root CEs must have common=zero quaternary weights (for which we never insert any nodes).
    U_ASSERT((ce & 0xc0) == 0);
    int32_t index = findOrInsertNodeForPrimary((uint32_t)(ce >> 32) , errorCode);
    if(strength >= UCOL_SECONDARY) {
        uint32_t lower32 = (uint32_t)ce;
        index = findOrInsertWeakNode(index, lower32 >> 16, UCOL_SECONDARY, errorCode);
        if(strength >= UCOL_TERTIARY) {
            index = findOrInsertWeakNode(index, lower32 & Collation::ONLY_TERTIARY_MASK,
                                         UCOL_TERTIARY, errorCode);
        }
    }
    return index;
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
    if(length == 0) { return ~0; }
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
CollationBuilder::findOrInsertNodeForPrimary(uint32_t p, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }

    int32_t rootIndex = binarySearchForRootPrimaryNode(
        rootPrimaryIndexes.getBuffer(), rootPrimaryIndexes.size(), nodes.getBuffer(), p);
    if(rootIndex >= 0) {
        return rootPrimaryIndexes.elementAti(rootIndex);
    } else {
        // Start a new list of nodes with this primary.
        int32_t index = nodes.size();
        nodes.addElement(nodeFromWeight32(p), errorCode);
        rootPrimaryIndexes.insertElementAt(index, ~rootIndex, errorCode);
        return index;
    }
}

int32_t
CollationBuilder::findOrInsertWeakNode(int32_t index, uint32_t weight16, int32_t level, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    U_ASSERT(0 <= index && index < nodes.size());

    U_ASSERT(weight16 == 0 || weight16 >= Collation::COMMON_WEIGHT16);
    // Only reset-before inserts common weights.
    if(weight16 == Collation::COMMON_WEIGHT16) {
        return findCommonNode(index, level);
    }
    // Find the root CE's weight for this level.
    // Postpone insertion if not found:
    // Insert the new root node before the next stronger node,
    // or before the next root node with the same strength and a larger weight.
    int64_t node = nodes.elementAti(index);
    int32_t nextIndex;
    while((nextIndex = nextIndexFromNode(node)) != 0) {
        node = nodes.elementAti(nextIndex);
        int32_t nextStrength = strengthFromNode(node);
        if(nextStrength <= level) {
            // Insert before a stronger node.
            if(nextStrength < level) { break; }
            // nextStrength == level
            if(!isTailoredNode(node)) {
                uint32_t nextWeight16 = weight16FromNode(node);
                if(nextWeight16 == weight16) {
                    // Found the node for the root CE up to this level.
                    return nextIndex;
                }
                // Insert before a node with a larger same-strength weight.
                if(nextWeight16 > weight16) { break; }
            }
        }
        // Skip the next node.
        index = nextIndex;
    }
    node = nodeFromWeight16(weight16) | nodeFromStrength(level);
    return insertNodeBetween(index, nextIndex, node, errorCode);
}

int32_t
CollationBuilder::insertTailoredNodeAfter(int32_t index, int32_t strength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    U_ASSERT(0 <= index && index < nodes.size());
    if(strength >= UCOL_SECONDARY) {
        index = findCommonNode(index, UCOL_SECONDARY);
        if(strength >= UCOL_TERTIARY) {
            index = findCommonNode(index, UCOL_TERTIARY);
        }
    }
    // Postpone insertion:
    // Insert the new node before the next one with a strength at least as strong.
    int64_t node = nodes.elementAti(index);
    int32_t nextIndex;
    while((nextIndex = nextIndexFromNode(node)) != 0) {
        node = nodes.elementAti(nextIndex);
        if(strengthFromNode(node) <= strength) { break; }
        // Skip the next node which has a weaker (larger) strength than the new one.
        index = nextIndex;
    }
    node = IS_TAILORED | nodeFromStrength(strength);
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

int32_t
CollationBuilder::findCommonNode(int32_t index, int32_t strength) const {
    U_ASSERT(UCOL_SECONDARY <= strength && strength <= UCOL_TERTIARY);
    int64_t node = nodes.elementAti(index);
    if(strengthFromNode(node) >= strength) {
        // The current node is no stronger.
        return index;
    }
    if(strength == UCOL_SECONDARY ? !nodeHasBefore2(node) : !nodeHasBefore3(node)) {
        // The current node implies the strength-common weight.
        return index;
    }
    index = nextIndexFromNode(node);
    node = nodes.elementAti(index);
    U_ASSERT(!isTailoredNode(node) && strengthFromNode(node) == strength &&
            weight16FromNode(node) == BEFORE_WEIGHT16);
    // Skip to the explicit common node.
    do {
        index = nextIndexFromNode(node);
        node = nodes.elementAti(index);
        U_ASSERT(strengthFromNode(node) >= strength);
    } while(isTailoredNode(node) || strengthFromNode(node) > strength);
    U_ASSERT(weight16FromNode(node) == Collation::COMMON_WEIGHT16);
    return index;
}

UBool
CollationBuilder::containsJamo(const UnicodeString &s) {
    for(int32_t i = 0; i < s.length();) {
        UChar32 c = s.char32At(i);
        int32_t hst = u_getIntPropertyValue(c, UCHAR_HANGUL_SYLLABLE_TYPE);
        if(hst == U_HST_LEADING_JAMO || hst == U_HST_VOWEL_JAMO || hst == U_HST_TRAILING_JAMO) {
            return TRUE;
        }
        i += U16_LENGTH(c);
    }
    return FALSE;
}

void
CollationBuilder::setCaseBits(const UnicodeString &nfdString,
                              const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t numTailoredPrimaries = 0;
    for(int32_t i = 0; i < cesLength; ++i) {
        if(ceStrength(ces[i]) == UCOL_PRIMARY) { ++numTailoredPrimaries; }
    }
    // We should not be able to get too many case bits because
    // cesLength<=31==MAX_EXPANSION_LENGTH.
    // 31 pairs of case bits fit into an int64_t without setting its sign bit.
    U_ASSERT(numTailoredPrimaries <= 31);

    int64_t cases = 0;
    if(numTailoredPrimaries > 0) {
        const UChar *s = nfdString.getBuffer();
        UTF16CollationIterator baseCEs(baseData, FALSE, s, s, s + nfdString.length());
        int32_t baseCEsLength = baseCEs.fetchCEs(errorCode) - 1;
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "fetching root CEs for tailored string";
            return;
        }
        U_ASSERT(baseCEsLength >= 0 && baseCEs.getCE(baseCEsLength) == Collation::NO_CE);

        uint32_t lastCase = 0;
        int32_t numBasePrimaries = 0;
        for(int32_t i = 0; i < baseCEsLength; ++i) {
            int64_t ce = baseCEs.getCE(i);
            if((ce >> 32) != 0) {
                ++numBasePrimaries;
                uint32_t c = ((uint32_t)ce >> 14) & 3;
                U_ASSERT(c == 0 || c == 2);  // lowercase or uppercase, no mixed case in any base CE
                if(numBasePrimaries < numTailoredPrimaries) {
                    cases |= (int64_t)c << ((numBasePrimaries - 1) * 2);
                } else if(numBasePrimaries == numTailoredPrimaries) {
                    lastCase = c;
                } else if(c != lastCase) {
                    // There are more base primary CEs than tailored primaries.
                    // Set mixed case if the case bits of the remainder differ.
                    lastCase = 1;
                    // Nothing more can change.
                    break;
                }
            }
        }
        if(numBasePrimaries >= numTailoredPrimaries) {
            cases |= (int64_t)lastCase << ((numTailoredPrimaries - 1) * 2);
        }
    }

    for(int32_t i = 0; i < cesLength; ++i) {
        int64_t ce = ces[i] & 0xffffffffffff3fff;  // clear old case bits
        int32_t strength = ceStrength(ce);
        if(strength == UCOL_PRIMARY) {
            ce |= (cases & 3) << 14;
            cases >>= 2;
        } else if(strength == UCOL_TERTIARY) {
            // Tertiary CEs must have uppercase bits.
            // See the LDML spec, and comments in class CollationCompare.
            ce |= 0x8000;
        }
        // Tertiary ignorable CEs must have 0 case bits.
        // We set 0 case bits for secondary CEs too
        // since currently only U+0345 is cased and maps to a secondary CE,
        // and it is lowercase. Other secondaries are uncased.
        // See [[:Cased:]&[:uca1=:]] where uca1 queries the root primary weight.
        ces[i] = ce;
    }
}

void
CollationBuilder::suppressContractions(const UnicodeSet &set, const char *&parserErrorReason,
                                       UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    dataBuilder->suppressContractions(set, errorCode);
    if(U_FAILURE(errorCode)) {
        parserErrorReason = "application of [suppressContractions [set]] failed";
    }
    // TODO: add the set of suppressed base context strings to the canonical closure;
    // try to remove suppressed builder context strings
}

void
CollationBuilder::optimize(const UnicodeSet &set, const char *& /* parserErrorReason */,
                           UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    optimizeSet.addAll(set);
}

#ifdef DEBUG_COLLATION_BUILDER

uint32_t
alignWeightRight(uint32_t w) {
    if(w != 0) {
        while((w & 0xff) == 0) { w >>= 8; }
    }
    return w;
}

#endif

void
CollationBuilder::makeTailoredCEs(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }

    CollationWeights primaries, secondaries, tertiaries;
    int64_t *nodesArray = nodes.getBuffer();

    for(int32_t rpi = 0; rpi < rootPrimaryIndexes.size(); ++rpi) {
        int32_t i = rootPrimaryIndexes.elementAti(rpi);
        int64_t node = nodesArray[i];
        uint32_t p = weight32FromNode(node);
        uint32_t s = p == 0 ? 0 : Collation::COMMON_WEIGHT16;
        uint32_t t = s;
        uint32_t q = 0;
        UBool pIsTailored = FALSE;
        UBool sIsTailored = FALSE;
        UBool tIsTailored = FALSE;
#ifdef DEBUG_COLLATION_BUILDER
        printf("\nprimary     %lx\n", (long)alignWeightRight(p));
#endif
        int32_t pIndex = p == 0 ? 0 : rootElements.findPrimary(p);
        int32_t nextIndex = nextIndexFromNode(node);
        while(nextIndex != 0) {
            i = nextIndex;
            node = nodesArray[i];
            nextIndex = nextIndexFromNode(node);
            int32_t strength = strengthFromNode(node);
            if(strength == UCOL_QUATERNARY) {
                U_ASSERT(isTailoredNode(node));
#ifdef DEBUG_COLLATION_BUILDER
                printf("      quat+     ");
#endif
                if(q == 3) {
                    errorCode = U_BUFFER_OVERFLOW_ERROR;
                    errorReason = "quaternary tailoring gap too small";
                    return;
                }
                ++q;
            } else {
                if(strength == UCOL_TERTIARY) {
                    if(isTailoredNode(node)) {
#ifdef DEBUG_COLLATION_BUILDER
                        printf("    ter+        ");
#endif
                        if(!tIsTailored) {
                            // First tailored tertiary node for [p, s].
                            int32_t tCount = countTailoredNodes(nodesArray, nextIndex,
                                                                UCOL_TERTIARY) + 1;
                            uint32_t tLimit;
                            if(t == 0) {
                                // Gap at the beginning of the tertiary CE range.
                                t = rootElements.getTertiaryBoundary() - 0x100;
                                tLimit = rootElements.getFirstTertiaryCE() & Collation::ONLY_TERTIARY_MASK;
                            } else if(t == BEFORE_WEIGHT16) {
                                tLimit = Collation::COMMON_WEIGHT16;
                            } else if(!pIsTailored && !sIsTailored) {
                                // p and s are root weights.
                                tLimit = rootElements.getTertiaryAfter(pIndex, s, t);
                            } else {
                                // [p, s] is tailored.
                                U_ASSERT(t == Collation::COMMON_WEIGHT16);
                                tLimit = rootElements.getTertiaryBoundary();
                            }
                            U_ASSERT(tLimit == 0x4000 || (tLimit & ~Collation::ONLY_TERTIARY_MASK) == 0);
                            tertiaries.initForTertiary();
                            if(!tertiaries.allocWeights(t, tLimit, tCount)) {
                                errorCode = U_BUFFER_OVERFLOW_ERROR;
                                errorReason = "tertiary tailoring gap too small";
                                return;
                            }
                            tIsTailored = TRUE;
                        }
                        t = tertiaries.nextWeight();
                        U_ASSERT(t != 0xffffffff);
                    } else {
                        t = weight16FromNode(node);
                        tIsTailored = FALSE;
#ifdef DEBUG_COLLATION_BUILDER
                        printf("    ter     %lx\n", (long)alignWeightRight(t));
#endif
                    }
                } else {
                    if(strength == UCOL_SECONDARY) {
                        if(isTailoredNode(node)) {
#ifdef DEBUG_COLLATION_BUILDER
                            printf("  sec+          ");
#endif
                            if(!sIsTailored) {
                                // First tailored secondary node for p.
                                int32_t sCount = countTailoredNodes(nodesArray, nextIndex,
                                                                    UCOL_SECONDARY) + 1;
                                uint32_t sLimit;
                                if(s == 0) {
                                    // Gap at the beginning of the secondary CE range.
                                    s = rootElements.getSecondaryBoundary() - 0x100;
                                    sLimit = rootElements.getFirstSecondaryCE() >> 16;
                                } else if(s == BEFORE_WEIGHT16) {
                                    sLimit = Collation::COMMON_WEIGHT16;
                                } else if(!pIsTailored) {
                                    // p is a root primary.
                                    sLimit = rootElements.getSecondaryAfter(pIndex, s);
                                } else {
                                    // p is a tailored primary.
                                    U_ASSERT(s == Collation::COMMON_WEIGHT16);
                                    sLimit = rootElements.getSecondaryBoundary();
                                }
                                if(s == Collation::COMMON_WEIGHT16) {
                                    // Do not tailor into the getSortKey() range of
                                    // compressed common secondaries.
                                    s = rootElements.getLastCommonSecondary();
                                }
                                secondaries.initForSecondary();
                                if(!secondaries.allocWeights(s, sLimit, sCount)) {
                                    errorCode = U_BUFFER_OVERFLOW_ERROR;
                                    errorReason = "secondary tailoring gap too small";
                                    return;
                                }
                                sIsTailored = TRUE;
                            }
                            s = secondaries.nextWeight();
                            U_ASSERT(s != 0xffffffff);
                        } else {
                            s = weight16FromNode(node);
                            sIsTailored = FALSE;
#ifdef DEBUG_COLLATION_BUILDER
                            printf("  sec       %lx\n", (long)alignWeightRight(s));
#endif
                        }
                    } else /* UCOL_PRIMARY */ {
                        U_ASSERT(isTailoredNode(node));
#ifdef DEBUG_COLLATION_BUILDER
                        printf("pri+            ");
#endif
                        if(!pIsTailored) {
                            // First tailored primary node in this list.
                            int32_t pCount = countTailoredNodes(nodesArray, nextIndex,
                                                                UCOL_PRIMARY) + 1;
                            UBool isCompressible = baseData->isCompressiblePrimary(p);
                            uint32_t pLimit =
                                rootElements.getPrimaryAfter(p, pIndex, isCompressible);
                            primaries.initForPrimary(isCompressible);
                            if(!primaries.allocWeights(p, pLimit, pCount)) {
                                errorCode = U_BUFFER_OVERFLOW_ERROR;  // TODO: introduce a more specific UErrorCode?
                                errorReason = "primary tailoring gap too small";
                                return;
                            }
                            pIsTailored = TRUE;
                        }
                        p = primaries.nextWeight();
                        U_ASSERT(p != 0xffffffff);
                        s = Collation::COMMON_WEIGHT16;
                        sIsTailored = FALSE;
                    }
                    t = s == 0 ? 0 : Collation::COMMON_WEIGHT16;
                    tIsTailored = FALSE;
                }
                q = 0;
            }
            if(isTailoredNode(node)) {
                nodesArray[i] = Collation::makeCE(p, s, t, q);
#ifdef DEBUG_COLLATION_BUILDER
                printf("%016llx\n", (long long)nodesArray[i]);
#endif
            }
        }
    }
}

int32_t
CollationBuilder::countTailoredNodes(const int64_t *nodesArray, int32_t i, int32_t strength) {
    int32_t count = 0;
    for(;;) {
        if(i == 0) { break; }
        int64_t node = nodesArray[i];
        if(strengthFromNode(node) < strength) { break; }
        if(strengthFromNode(node) == strength) {
            if(isTailoredNode(node)) {
                ++count;
            } else {
                break;
            }
        }
        i = nextIndexFromNode(node);
    }
    return count;
}

class CEFinalizer : public CollationDataBuilder::CEModifier {
public:
    CEFinalizer(const int64_t *ces) : finalCEs(ces) {}
    virtual ~CEFinalizer();
    virtual int64_t modifyCE32(uint32_t ce32) const {
        U_ASSERT(!Collation::isSpecialCE32(ce32));
        if(CollationBuilder::isTempCE32(ce32)) {
            // retain case bits
            return finalCEs[CollationBuilder::indexFromTempCE32(ce32)] | ((ce32 & 0xc0) << 8);
        } else {
            return Collation::NO_CE;
        }
    }
    virtual int64_t modifyCE(int64_t ce) const {
        if(CollationBuilder::isTempCE(ce)) {
            // retain case bits
            return finalCEs[CollationBuilder::indexFromTempCE(ce)] | (ce & 0xc000);
        } else {
            return Collation::NO_CE;
        }
    }

private:
    const int64_t *finalCEs;
};

CEFinalizer::~CEFinalizer() {}

void
CollationBuilder::finalizeCEs(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    LocalPointer<CollationDataBuilder> newBuilder(new CollationDataBuilder(errorCode));
    if(newBuilder.isNull()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    newBuilder->initForTailoring(baseData, errorCode);
    CEFinalizer finalizer(nodes.getBuffer());
    newBuilder->copyFrom(*dataBuilder, finalizer, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    delete dataBuilder;
    dataBuilder = newBuilder.orphan();
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

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
