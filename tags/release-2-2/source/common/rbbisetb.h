//
//  rbbisetb.h
/*
**********************************************************************
*   Copyright (c) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#ifndef RBBISETB_H
#define RBBISETB_H

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "rbbirb.h"
#include "uvector.h"
#include "uhash.h"

struct  UNewTrie;

U_NAMESPACE_BEGIN

//
//  RBBISetBuilder   Derives the character categories used by the runtime RBBI engine
//                   from the Unicode Sets appearing in the source  RBBI rules, and
//                   creates the TRIE table used to map from Unicode to the
//                   character categories.
//


//
//  RangeDescriptor
//
//     Each of the non-overlapping character ranges gets one of these descriptors.
//     All of them are strung together in a linked list, which is kept in order
//     (by character)
//
class RangeDescriptor : public UObject {
public:
    UChar32            fStartChar;      // Start of range, unicode 32 bit value.
    UChar32            fEndChar;        // End of range, unicode 32 bit value.
    int32_t            fNum;            // runtime-mapped input value for this range.
    UVector           *fIncludesSets;   // vector of the the original
                                        //   Unicode sets that include this range.
                                        //    (Contains ptrs to uset nodes)
    RangeDescriptor   *fNext;           // Next RangeDescriptor in the linked list.

    RangeDescriptor(UErrorCode &status);
    RangeDescriptor(const RangeDescriptor &other, UErrorCode &status);
    ~RangeDescriptor();
    void split(UChar32 where, UErrorCode &status);   // Spit this range in two at "where", with
                                        //   where appearing in the second (higher) part.
    void setDictionaryFlag();           // Check whether this range appears as part of
                                        //   the Unicode set named "dictionary"

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 2.2
     */
    virtual inline UClassID getDynamicClassID() const { return getStaticClassID(); }

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 2.2
     */
    static inline UClassID getStaticClassID() { return (UClassID)&fgClassID; }

private:
    /**
     * The address of this static class variable serves as this class's ID
     * for ICU "poor man's RTTI".
     */
    static const char fgClassID;
};


//
//  RBBISetBuilder   Handles processing of Unicode Sets from RBBI rules.
//
//      Starting with the rules parse tree from the scanner,
//
//                   -  Enumerate the set of UnicodeSets that are referenced
//                      by the RBBI rules.
//                   -  compute a derived set of non-overlapping UnicodeSets
//                      that will correspond to columns in the state table for
//                      the RBBI execution engine.
//                   -  construct the trie table that maps input characters
//                      to set numbers in the non-overlapping set of sets.
//


class RBBISetBuilder : public UObject {
public:
    RBBISetBuilder(RBBIRuleBuilder *rb);
    ~RBBISetBuilder();

    void     build();
    void     addValToSets(UVector *sets, uint32_t val);
    int32_t  getNumCharCategories();   // CharCategories are the same as input symbol set to the
                                   //    runtime state machine, which are the same as
                                   //    columns in the DFA state table
    int32_t  getTrieSize();        // Size in bytes of the serialized Trie.
    void     serializeTrie(uint8_t *where);  // write out the serialized Trie.
    void     printSets();
    void     printRanges();
    void     printRangeGroups();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @draft ICU 2.2
     */
    virtual inline UClassID getDynamicClassID() const { return getStaticClassID(); }

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @draft ICU 2.2
     */
    static inline UClassID getStaticClassID() { return (UClassID)&fgClassID; }

private:
    void           numberSets();

    RBBIRuleBuilder       *fRB;             // The RBBI Rule Compiler that owns us.
    UErrorCode            *fStatus;

    RangeDescriptor       *fRangeList;      // Head of the linked list of RangeDescriptors

    UNewTrie              *fTrie;           // The mapping TRIE that is the end result of processing
    uint32_t              fTrieSize;        //  the Unicode Sets.

    // Groups correspond to character categories -
    //       groups of ranges that are in the same original UnicodeSets.
    //       fGroupCount is the index of the last used group.
    //       The value is also the number of columns in the RBBI state table being compiled.
    //       Index 0 is not used.  Funny counting.
    int32_t               fGroupCount;

    /**
     * The address of this static class variable serves as this class's ID
     * for ICU "poor man's RTTI".
     */
    static const char fgClassID;
};



U_NAMESPACE_END
#endif
