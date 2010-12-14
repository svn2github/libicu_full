/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetriebuilder.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010sep25
*   created by: Markus W. Scherer
*
* Builder class for ByteTrie dictionary trie.
*/

#ifndef __BYTETRIEBUILDER_H__
#define __BYTETRIEBUILDER_H__

#include "unicode/utypes.h"
#include "unicode/stringpiece.h"
#include "charstr.h"

U_NAMESPACE_BEGIN

class ByteTrieElement;

class U_TOOLUTIL_API ByteTrieBuilder : public UMemory {
public:
    ByteTrieBuilder()
            : elements(NULL), elementsCapacity(0), elementsLength(0),
              bytes(NULL), bytesCapacity(0), bytesLength(0) {}
    ~ByteTrieBuilder();

    ByteTrieBuilder &add(const StringPiece &s, int32_t value, UErrorCode &errorCode);

    StringPiece build(UErrorCode &errorCode);

    ByteTrieBuilder &clear() {
        strings.clear();
        elementsLength=0;
        bytesLength=0;
        return *this;
    }

private:
    void makeNode(int32_t start, int32_t limit, int32_t byteIndex);
    void makeBranchSubNode(int32_t start, int32_t limit, int32_t byteIndex, int32_t length);

    UBool ensureCapacity(int32_t length);
    void write(int32_t byte);
    void write(const char *b, int32_t length);
    void writeValueAndFinal(int32_t i, UBool final);
    void writeDelta(int32_t i);

    CharString strings;
    ByteTrieElement *elements;
    int32_t elementsCapacity;
    int32_t elementsLength;

    // Byte serialization of the trie.
    // Grows from the back: bytesLength measures from the end of the buffer!
    char *bytes;
    int32_t bytesCapacity;
    int32_t bytesLength;
};

U_NAMESPACE_END

#endif  // __BYTETRIEBUILDER_H__
