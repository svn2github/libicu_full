/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  dicttriebuilder.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010dec24
*   created by: Markus W. Scherer
*
* Base class for dictionary-trie builder classes.
*/

#ifndef __DICTTRIEBUILDER_H__
#define __DICTTRIEBUILDER_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "uhash.h"

U_NAMESPACE_BEGIN

class U_TOOLUTIL_API DictTrieBuilder : public UMemory {
public:
    /** @internal */
    static UBool hashNode(const void *node);
    /** @internal */
    static UBool equalNodes(const void *left, const void *right);

protected:
    DictTrieBuilder();
    ~DictTrieBuilder();

    class Node;

    void createCompactBuilder(int32_t sizeGuess, UErrorCode &errorCode);
    void deleteCompactBuilder();

    /**
     * Makes sure that there is only one unique node registered that is
     * equivalent to newNode.
     * @param newNode Input node. The builder takes ownership.
     * @param errorCode ICU in/out UErrorCode.
                        Set to U_MEMORY_ALLOCATION_ERROR if it was success but newNode==NULL.
     * @return newNode if it is the first of its kind, or
     *         an equivalent node if newNode is a duplicate.
     */
    Node *registerNode(Node *newNode, UErrorCode &errorCode);
    /**
     * Makes sure that there is only one unique FinalValueNode registered
     * with this value.
     * Avoids creating a node if the value is a duplicate.
     * @param value A final value.
     * @param errorCode ICU in/out UErrorCode.
                        Set to U_MEMORY_ALLOCATION_ERROR if it was success but newNode==NULL.
     * @return A FinalValueNode with the given value.
     */
    Node *registerFinalValue(int32_t value, UErrorCode &errorCode);

    /*
     * C++ note:
     * registerNode() and registerFinalValue() take ownership of their input nodes,
     * and only return owned nodes.
     * If they see a failure UErrorCode, they will delete the input node.
     * If they get a NULL pointer, they will record a U_MEMORY_ALLOCATION_ERROR.
     * If there is a failure, they return NULL.
     *
     * NULL Node pointers can be safely passed into other Nodes because
     * they call the static Node::hashCode() which checks for a NULL pointer first.
     *
     * Therefore, as long as builder functions register a new node,
     * they need to check for failures only before explicitly dereferencing
     * a Node pointer, or before setting a new UErrorCode.
     */

    virtual Node *createFinalValueNode(int32_t value) const = 0;

    // Hash set of nodes, maps from nodes to integer 1.
    UHashtable *nodes;

    class Node : public UObject {
    public:
        Node(int32_t initialHash) : hash(initialHash), offset(0) {}
        inline int32_t hashCode() const { return hash; }
        // Handles node==NULL.
        static inline int32_t hashCode(const Node *node) { return node==NULL ? 0 : node->hashCode(); }
        // Base class operator==() compares the actual class types.
        virtual UBool operator==(const Node &other) const;
        inline UBool operator!=(const Node &other) const { return !operator==(other); }
        // write() must set the offset.
        virtual void write(DictTrieBuilder &builder) = 0;
        inline int32_t writeOrGetOffset(DictTrieBuilder &builder) {
            if(offset==0) {
                write(builder);
            }
            return offset;
        }
    protected:
        int32_t hash;
        int32_t offset;
    private:
        // No ICU "poor man's RTTI" for this class nor its subclasses.
        virtual UClassID getDynamicClassID() const;
    };

    class FinalValueNode : public Node {
    public:
        FinalValueNode(int32_t v) : Node(0x111111*37+v), value(v) {}
        virtual UBool operator==(const Node &other) const;
        // Dummy default implementation, must be overridden for real writing.
        virtual void write(DictTrieBuilder & /*builder*/) {}
    protected:
        int32_t value;
    };

    class ValueNode : public Node {
    public:
        ValueNode(int32_t initialHash) : Node(initialHash), hasValue(FALSE), value(0) {}
        virtual UBool operator==(const Node &other) const;
        void setValue(int32_t v) {
            hasValue=TRUE;
            value=v;
            hash=hash*37+v;
        }
    protected:
        UBool hasValue;
        int32_t value;
    };

    class LinearMatchNode : public ValueNode {
    public:
        LinearMatchNode(int32_t len, Node *nextNode)
                : ValueNode((0x333333*37+len)*37+hashCode(nextNode)),
                  length(len), next(nextNode) {}
        virtual UBool operator==(const Node &other) const;
    protected:
        int32_t length;
        Node *next;
    };

    class ListBranchNode : public Node {
    public:
        ListBranchNode() : Node(0x444444), length(0) {}
        virtual UBool operator==(const Node &other) const;
        // Adds a unit with a final value.
        void add(int32_t c, int32_t value) {
            units[length]=(UChar)c;
            equal[length]=NULL;
            values[length]=value;
            ++length;
            hash=(hash*37+c)*37+value;
        }
        // Adds a unit which leads to another match node.
        void add(int32_t c, Node *node) {
            units[length]=(UChar)c;
            equal[length]=node;
            values[length]=0;
            ++length;
            hash=(hash*37+c)*37+hashCode(node);
        }
    protected:
        // TODO: 10 -> max(BT/UCT max list lengths)
        Node *equal[10];  // NULL means "has final value".
        int32_t length;
        int32_t values[10];
        UChar units[10];
    };

    class SplitBranchNode : public Node {
    public:
        SplitBranchNode(UChar middleUnit, Node *lessThanNode, Node *greaterOrEqualNode)
                : Node(((0x555555*37+middleUnit)*37+
                        hashCode(lessThanNode))*37+hashCode(greaterOrEqualNode)),
                  unit(middleUnit), lessThan(lessThanNode), greaterOrEqual(greaterOrEqualNode) {}
        virtual UBool operator==(const Node &other) const;
    protected:
        UChar unit;
        Node *lessThan;
        Node *greaterOrEqual;
    };

    // Branch head node, for writing the actual node lead unit.
    class BranchNode : public ValueNode {
    public:
        BranchNode(int32_t len, Node *subNode)
                : ValueNode((0x666666*37+len)*37+hashCode(subNode)),
                  length(len), next(subNode) {}
        virtual UBool operator==(const Node &other) const;
    protected:
        int32_t length;
        Node *next;  // A branch sub-node.
    };
};

U_NAMESPACE_END

#endif  // __DICTTRIEBUILDER_H__
