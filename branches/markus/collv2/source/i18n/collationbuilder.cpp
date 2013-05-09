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
    if(U_FAILURE(errorCode)) { return; }
    tailoring = new CollationTailoring();
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

namespace {

// TODO
// static const UChar BEFORE[] = { 0x5b, 0x62, 0x65, 0x66, 0x6f, 0x72, 0x65, 0 };  // "[before"
// const int32_t BEFORE_LENGTH = 7;

}  // namespace

CollationBuilder::CollationBuilder(const CollationData *base, UErrorCode &errorCode)
        : nfd(*Normalizer2::getNFDInstance(errorCode)),
          baseData(base),
          dataBuilder(errorCode),
          errorReason(NULL),
          cesLength(0),
          rootPrimaryIndexes(errorCode), nodes(errorCode) {
    // Preset some sentinel nodes so that we need not check for end-of-list indexes.
    // node 0: root/primary=0
    int64_t node = nodeFromNextIndex(1);
    nodes.addElement(node, errorCode);
    // TODO: Is it useful to preset the secondary/tertiary zero weight root nodes?
    // node 1: root/secondary=0
    node = nodeFromNextIndex(2) | nodeFromStrength(UCOL_SECONDARY);
    nodes.addElement(node, errorCode);
    // node 2: root/tertiary=0
    node = nodeFromPreviousIndex(1) | nodeFromNextIndex(3) | nodeFromStrength(UCOL_TERTIARY);
    nodes.addElement(node, errorCode);
    // node 3: root/primary=special (limit sentinel)
    node = nodeFromWeight24((int32_t)Collation::SPECIAL_BYTE << 16) | nodeFromPreviousIndex(2);
    nodes.addElement(node, errorCode);

    rootPrimaryIndexes.addElement(0, errorCode);
    rootPrimaryIndexes.addElement(3, errorCode);

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
    parser.setSink(this);
    parser.setImporter(importer);
    parser.parse(ruleString, tailoring.settings, outParseError, errorCode);
    errorReason = parser.getErrorReason();
    // TODO
}

void
CollationBuilder::addReset(int32_t strength, const UnicodeString &str,
                           const char *&parserErrorReason, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
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
    if(strength != UCOL_IDENTICAL) {
        errorCode = U_UNSUPPORTED_ERROR;  // TODO
        parserErrorReason = "TODO: support &[before n]";
        return;
    }
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
        modifyCEs(strength, errorCode);
        if(U_FAILURE(errorCode)) {
            parserErrorReason = "modifying collation elements";
            return;
        }
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

void
CollationBuilder::modifyCEs(int32_t strength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
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

    // Find the node index after which we insert the new tailored node.
    int32_t index;
    if(isTempCE(ce)) {
        index = indexFromTempCE(ce);
    } else {
        index = findOrInsertNodeForRootCE(ce, errorCode);
    }

    // Insert the new tailored node.
    index = insertNodeAfter(index, 0, TAILORED_NODE, strength, errorCode);
    ces[cesLength - 1] = tempCEFromIndexAndStrength(index, strength);
}

int32_t
CollationBuilder::findOrInsertNodeForRootCE(int64_t ce, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    return 0;  // TODO
}

int32_t
CollationBuilder::insertNodeAfter(int32_t index, int32_t weight24,
                                  int32_t type, int32_t strength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return 0; }
    return 0;  // TODO
}

#if 0
// TODO: move to Sink (CollationBuilder)
void
CollationBuilder::makeAndInsertToken(int32_t relation, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    U_ASSERT(NO_RELATION < (relation & STRENGTH_MASK) && (relation & STRENGTH_MASK) <= IDENTICAL);
    U_ASSERT((relation & ~RELATION_MASK) == 0);
    U_ASSERT(!str.isEmpty());
    // Find the tailoring in a previous rule chain.
    // Merge the new relation with any existing one for prefix+str.
    // If new, then encode the relation and tailoring strings into a token word,
    // and insert the token at tokenIndex.
    int32_t token = relation;
    if(!prefix.isEmpty()) {
        token |= HAS_PREFIX;
    }
    if(str.hasMoreChar32Than(0, 0x7fffffff, 1)) {
        token |= HAS_CONTRACTION;
    }
    if(!extension.isEmpty()) {
        token |= HAS_EXTENSION;
    }
    UChar32 c = str.char32At(0);
    if((token & HAS_STRINGS) == 0) {
        if(tailoredSet.contains(c)) {
            // TODO: search for existing token
        } else {
            tailoredSet.add(c);
        }
        token |= c << VALUE_SHIFT;
    } else {
        // The relation maps from str if preceded by the prefix. Look for a duplicate.
        // We ignore the extension because that adds to the CEs for the relation
        // but is not part of what is being tailored (what is mapped *from*).
        UnicodeString s(prefix);
        s.append(str);
        int32_t lengthsWord = prefix.length() | (str.length() << 5);
        if((token & HAS_CONTEXT) == 0 ? tailoredSet.contains(c) : tailoredSet.contains(s)) {
            // TODO: search for existing token
        } else if((token & HAS_CONTEXT) == 0) {
            tailoredSet.add(c);
        } else {
            tailoredSet.add(s);
        }
        int32_t value = tokenStrings.length();
        if(value > MAX_VALUE) {
            setParseError("total tailoring strings overflow", errorCode);
            return;
        }
        token |= ~value << VALUE_SHIFT;  // negative
        lengthsWord |= (extension.length() << 10);
        tokenStrings.append((UChar)lengthsWord).append(s).append(extension);
    }
    if((relation & DIFF) != 0) {
        // Postpone insertion:
        // Insert the new relation before the next token with a relation at least as strong.
        // Stops before resets and the sentinel.
        for(;;) {
            int32_t t = tokens.elementAti(tokenIndex);
            if((t & RELATION_MASK) <= relation) { break; }
            ++tokenIndex;
        }
    } else {
        // Append a new reset at the end of the token list,
        // before the sentinel, starting a new rule chain.
        tokenIndex = tokens.size() - 1;
    }
    tokens.insertElementAt(token, tokenIndex++, errorCode);
}
#endif

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
