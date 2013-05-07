/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationbuilder.h
*
* created on: 2013may06
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONBUILDER_H__
#define __COLLATIONBUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/unistr.h"
#include "collationruleparser.h"
#include "uvectr32.h"
#include "uvectr64.h"

struct UParseError;

U_NAMESPACE_BEGIN

struct CollationData;
struct CollationTailoring;

class U_I18N_API CollationBuilder : public CollationRuleParser::Sink {
public:
    CollationBuilder(const CollationData *base, UErrorCode &errorCode);
    virtual ~CollationBuilder();

    void parseAndBuild(const UnicodeString &ruleString,
                       CollationRuleParser::Importer *importer,
                       CollationTailoring &tailoring,
                       UParseError *outParseError,
                       UErrorCode &errorCode);

    const char *getErrorReason() const { return errorReason; }

    UBool modifiesSettings() const { return TRUE; }  // TODO
    UBool modifiesMappings() const { return TRUE; }  // TODO

private:
    /** Implements CollationRuleParser::Sink. */
    virtual void addReset(int32_t strength, const UnicodeString &str,
                          const char *&errorReason, UErrorCode &errorCode);

    /** Implements CollationRuleParser::Sink. */
    virtual void addRelation(int32_t strength, const UnicodeString &prefix,
                             const UnicodeString &str, const UnicodeString &extension,
                             const char *&errorReason, UErrorCode &errorCode);

    /** Implements CollationRuleParser::Sink. */
    virtual void suppressContractions(const UnicodeSet &set,
                                      const char *&errorReason, UErrorCode &errorCode);

    void setParseError(const char *reason, UErrorCode &errorCode);

    /**
     * Encodes "temporary CE" data into a CE that fits into the CE32 data structure,
     * with 2-byte primary, 1-byte secondary and 6-bit tertiary,
     * with valid CE byte values.
     *
     * The impossible case bits combination 11 is used to distinguish
     * temporary CEs from real CEs.
     */
    static inline int64_t tempCEFromIndexAndStrength(int32_t index, int32_t strength) {
        return
            // CE byte offsets, to ensure valid CE bytes, and case bits 11
            0x808000008000e000 |
            // index bits 17..12 -> primary byte 1 = CE bits 63..56 (byte values 80..BF)
            ((int64_t)(index & 0x3f000) << 44) |
            // index bits 11..6 -> primary byte 2 = CE bits 55..48 (byte values 80..BF)
            ((int64_t)(index & 0xfc0) << 42) |
            // index bits 5..0 -> seconary byte 1 = CE bits 31..24 (byte values 80..BF)
            ((index & 0x3f) << 24) |
            // strength bits 1..0 -> tertiary byte 1 = CE bits 13..8 (byte values 20..23)
            (strength << 8);
    }
    static inline int32_t indexFromTempCE(int64_t tempCE) {
        return
            ((int32_t)(tempCE >> 44) & 0x3f000) |
            ((int32_t)(tempCE >> 42) & 0xfc0) |
            ((int32_t)(tempCE >> 24) & 0x3f);
    }
    static inline int32_t strengthFromTempCE(int64_t tempCE) {
        return ((int32_t)tempCE >> 8) & 3;
    }

    static inline int64_t nodeFromWeight24(int32_t weight24) {
        return (int64_t)weight24 << 40;
    }
    static inline int64_t nodeFromPreviousIndex(int32_t previous) {
        return (int64_t)previous << 22;
    }
    static inline int64_t nodeFromNextIndex(int32_t next) {
        return next << 4;
    }
    static inline int64_t nodeFromType(int32_t type) {
        return type << 2;
    }
    static inline int64_t nodeFromStrength(int32_t strength) {
        return strength;
    }

    static inline int32_t weight24FromNode(int64_t node) {
        return (int32_t)(node >> 40) & 0xffffff;
    }
    static inline int32_t previousIndexFromNode(int64_t node) {
        return (int32_t)(node >> 22) & MAX_INDEX;
    }
    static inline int32_t nextIndexFromNode(int64_t node) {
        return ((int32_t)node >> 4) & MAX_INDEX;
    }
    static inline int32_t typeFromNode(int64_t node) {
        return ((int32_t)node >> 2) & 3;
    }
    static inline int32_t strengthFromNode(int64_t node) {
        return (int32_t)node & 3;
    }

    static inline int64_t changeNodePreviousIndex(int64_t node, int32_t previous) {
        return (node & 0xffffff00003fffff) | nodeFromPreviousIndex(previous);
    }
    static inline int64_t changeNodeNextIndex(int64_t node, int32_t next) {
        return (node & 0xffffffffffc0000f) | nodeFromNextIndex(next);
    }

    /** At most 256k nodes, limited by the 18 bits in node bit fields. */
    static const int32_t MAX_INDEX = 0x3ffff;
    static const int32_t ROOT_NODE = 0;
    static const int32_t DEFAULT_NODE = 1;
    static const int32_t TAILORED_NODE = 2;

    // TODO: const Normalizer2 &nfd, &fcc;

    const CollationData *baseData;
    // TODO: CollationSettings *settings;
    const char *errorReason;

    /**
     * Indexes of nodes with primary root weights, sorted by primary.
     * Compact form of a TreeMap from root primary to node index.
     *
     * This is a performance optimization for finding reset positions.
     * Without this, we would have to search through the entire nodes list.
     */
    UVector32 rootPrimaryIndexes;
    /**
     * Data structure for assigning tailored weights and CEs.
     * Doubly-linked list of nodes in mostly collation order.
     *
     * "Root" nodes store root collator weights.
     * "Default" nodes store default weak weights (e.g., secondary 02 or 05)
     * for stronger nodes (e.g., primary difference).
     * "Tailored" nodes create a difference of a certain strength from
     * the preceding node.
     *
     * Tailored CEs are initially represented in CollationData as temporary CEs
     * which point to a stable index in this list.
     * At the end, the tailored weights are allocated as necessary,
     * then the tailored nodes are replaced with final CEs,
     * and the CollationData is rewritten by replacing temporary CEs with final ones.
     *
     * We cannot simply insert new nodes in the middle of the list
     * because that would invalidate the indexes stored in existing temporary CEs.
     * We need to use a linked graph with stable indexes to existing nodes.
     * A doubly-linked list seems easiest to maintain.
     *
     * Each node is stored as an int64_t, with its fields stored as bit fields.
     * - a weight (if it is a root or default node): 24 bits 63..40
     * - index to the previous node: 18 bits 39..22
     * - index to the next node: 18 bits 21..4
     * - the type (root/default/tailored): 2 bits 3..2
     * - the difference strength (primary/secondary/tertiary/quaternary): 2 bits 1..0
     *
     * We could allocate structs with pointers, but we would have to store them
     * in a pointer list so that they can be indexed from temporary CEs,
     * and they would require more memory allocations.
     */
    UVector64 nodes;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONBUILDER_H__
