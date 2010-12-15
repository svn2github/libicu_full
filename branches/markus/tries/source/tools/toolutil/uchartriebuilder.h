/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uchartriebuilder.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov14
*   created by: Markus W. Scherer
*
* Builder class for UCharTrie dictionary trie.
*/

#ifndef __UCHARTRIEBUILDER_H__
#define __UCHARTRIEBUILDER_H__

#include "unicode/utypes.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class UCharTrieElement;

class U_TOOLUTIL_API UCharTrieBuilder : public UMemory {
public:
    UCharTrieBuilder()
            : elements(NULL), elementsCapacity(0), elementsLength(0),
              uchars(NULL), ucharsCapacity(0), ucharsLength(0) {}
    ~UCharTrieBuilder();

    UCharTrieBuilder &add(const UnicodeString &s, int32_t value, UErrorCode &errorCode);

    UnicodeString build(UErrorCode &errorCode);

    UCharTrieBuilder &clear() {
        strings.remove();
        elementsLength=0;
        ucharsLength=0;
        return *this;
    }

private:
    void makeNode(int32_t start, int32_t limit, int32_t unitIndex);
    void makeBranchSubNode(int32_t start, int32_t limit, int32_t unitIndex, int32_t length);
    void makeContiguousBranchSubNode(int32_t start, int32_t limit, int32_t unitIndex, int32_t length);

    UBool ensureCapacity(int32_t length);
    void write(int32_t unit);
    void write(const UChar *s, int32_t length);
    void writeInt32(int32_t i);
    void writeValueAndFinal(int32_t i, UBool final);
    void writeDelta(int32_t i);

    UnicodeString strings;
    UCharTrieElement *elements;
    int32_t elementsCapacity;
    int32_t elementsLength;

    // UChar serialization of the trie.
    // Grows from the back: ucharsLength measures from the end of the buffer!
    UChar *uchars;
    int32_t ucharsCapacity;
    int32_t ucharsLength;
};

U_NAMESPACE_END

#endif  // __UCHARTRIEBUILDER_H__
