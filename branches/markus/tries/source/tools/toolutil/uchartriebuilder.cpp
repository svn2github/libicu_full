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

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "uarrsort.h"
#include "uchartrie.h"
#include "uchartriebuilder.h"

U_NAMESPACE_BEGIN

/*
 * Note: This builder implementation stores (string, value) pairs with full copies
 * of the 16-bit-unit sequences, until the UCharTrie is built.
 * It might(!) take less memory if we collected the data in a temporary, dynamic trie.
 */

class UCharTrieElement : public UMemory {
public:
    // Use compiler's default constructor, initializes nothing.

    void setTo(const UnicodeString &s, int32_t val, UnicodeString &strings, UErrorCode &errorCode);

    UnicodeString getString(const UnicodeString &strings) const {
        int32_t length=strings[stringOffset];
        return strings.tempSubString(stringOffset+1, length);
    }
    int32_t getStringLength(const UnicodeString &strings) const {
        return strings[stringOffset];
    }

    UChar charAt(int32_t index, const UnicodeString &strings) const {
        return strings[stringOffset+1+index];
    }

    int32_t getValue() const { return value; }

    int32_t compareStringTo(const UCharTrieElement &o, const UnicodeString &strings) const;

private:
    // The first strings unit contains the string length.
    // (Compared with a stringLength field here, this saves 2 bytes per string.)
    int32_t stringOffset;
    int32_t value;
};

void
UCharTrieElement::setTo(const UnicodeString &s, int32_t val,
                        UnicodeString &strings, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }
    int32_t length=s.length();
    if(length>0xffff) {
        // Too long: We store the length in 1 unit.
        errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }
    stringOffset=strings.length();
    strings.append((UChar)length);
    value=val;
    strings.append(s);
}

int32_t
UCharTrieElement::compareStringTo(const UCharTrieElement &other, const UnicodeString &strings) const {
    return getString(strings).compare(other.getString(strings));
}

UCharTrieBuilder::~UCharTrieBuilder() {
    delete[] elements;
    uprv_free(uchars);
}

UCharTrieBuilder &
UCharTrieBuilder::add(const UnicodeString &s, int32_t value, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return *this;
    }
    if(ucharsLength>0) {
        // Cannot add elements after building.
        errorCode=U_NO_WRITE_PERMISSION;
        return *this;
    }
    ucharsCapacity+=s.length()+1;  // Crude uchars preallocation estimate.
    if(elementsLength==elementsCapacity) {
        int32_t newCapacity;
        if(elementsCapacity==0) {
            newCapacity=1024;
        } else {
            newCapacity=4*elementsCapacity;
        }
        UCharTrieElement *newElements=new UCharTrieElement[newCapacity];
        if(newElements==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        }
        if(elementsLength>0) {
            uprv_memcpy(newElements, elements, elementsLength*sizeof(UCharTrieElement));
        }
        delete[] elements;
        elements=newElements;
        elementsCapacity=newCapacity;
    }
    elements[elementsLength++].setTo(s, value, strings, errorCode);
    if(U_SUCCESS(errorCode) && strings.isBogus()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
    }
    return *this;
}

U_CDECL_BEGIN

static int32_t U_CALLCONV
compareElementStrings(const void *context, const void *left, const void *right) {
    const UnicodeString *strings=reinterpret_cast<const UnicodeString *>(context);
    const UCharTrieElement *leftElement=reinterpret_cast<const UCharTrieElement *>(left);
    const UCharTrieElement *rightElement=reinterpret_cast<const UCharTrieElement *>(right);
    return leftElement->compareStringTo(*rightElement, *strings);
}

U_CDECL_END

UnicodeString
UCharTrieBuilder::build(UErrorCode &errorCode) {
    UnicodeString result;
    if(U_FAILURE(errorCode)) {
        return result;
    }
    if(ucharsLength>0) {
        // Already built.
        result.setTo(FALSE, uchars+(ucharsCapacity-ucharsLength), ucharsLength);
        return result;
    }
    if(elementsLength==0) {
        errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
        return result;
    }
    if(strings.isBogus()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return result;
    }
    uprv_sortArray(elements, elementsLength, (int32_t)sizeof(UCharTrieElement),
                   compareElementStrings, &strings,
                   FALSE,  // need not be a stable sort
                   &errorCode);
    if(U_FAILURE(errorCode)) {
        return result;
    }
    // Duplicate strings are not allowed.
    UnicodeString prev=elements[0].getString(strings);
    for(int32_t i=1; i<elementsLength; ++i) {
        UnicodeString current=elements[i].getString(strings);
        if(prev==current) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return result;
        }
        prev.fastCopyFrom(current);
    }
    // Create and UChar-serialize the trie for the elements.
    if(ucharsCapacity<1024) {
        ucharsCapacity=1024;
    }
    uchars=reinterpret_cast<UChar *>(uprv_malloc(ucharsCapacity*2));
    if(uchars==NULL) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return result;
    }
    makeNode(0, elementsLength, 0);
    if(uchars==NULL) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
    } else {
        result.setTo(FALSE, uchars+(ucharsCapacity-ucharsLength), ucharsLength);
    }
    return result;
}

// Requires start<limit,
// and all strings of the [start..limit[ elements must be sorted and
// have a common prefix of length unitIndex.
void
UCharTrieBuilder::makeNode(int32_t start, int32_t limit, int32_t unitIndex) {
    if(unitIndex==elements[start].getStringLength(strings)) {
        // An intermediate or final value.
        int32_t value=elements[start++].getValue();
        UBool final= start==limit;
        if(!final) {
            makeNode(start, limit, unitIndex);
        }
        writeCompactInt(value, final);
        return;
    }
    // Now all [start..limit[ strings are longer than unitIndex.
    int32_t minUnit=(uint8_t)elements[start].charAt(unitIndex, strings);
    int32_t maxUnit=(uint8_t)elements[limit-1].charAt(unitIndex, strings);
    if(minUnit==maxUnit) {
        // Linear-match node: All strings have the same character at unitIndex.
        int32_t lastUnitIndex=unitIndex;
        int32_t length=0;
        do {
            ++lastUnitIndex;
            ++length;
        } while(length<UCharTrie::kMaxLinearMatchLength &&
                elements[start].getStringLength(strings)>lastUnitIndex &&
                elements[start].charAt(lastUnitIndex, strings)==
                    elements[limit-1].charAt(lastUnitIndex, strings));
        makeNode(start, limit, lastUnitIndex);
        write(elements[start].getString(strings).getBuffer()+unitIndex, length);
        write(UCharTrie::kMinLinearMatch+length-1);
        return;
    }
    // Branch node.
    int32_t length=0;  // Number of different units at unitIndex.
    int32_t i=start;
    do {
        UChar unit=elements[i++].charAt(unitIndex, strings);
        while(i<limit && unit==elements[i].charAt(unitIndex, strings)) {
            ++i;
        }
        ++length;
    } while(i<limit);
    // length>=2 because minUnit!=maxUnit.
    if(length<=UCharTrie::kMaxListBranchLength) {
        makeListBranchNode(start, limit, unitIndex, length);
    } else {
        makeSplitBranchNode(start, limit, unitIndex, length);
    }
}

// start<limit && all strings longer than unitIndex &&
// 2..kMaxListBranchLength different units at unitIndex
void
UCharTrieBuilder::makeListBranchNode(int32_t start, int32_t limit, int32_t unitIndex, int32_t length) {
    // List of unit-value pairs where values are either final values
    // or jumps to other parts of the trie.
    int32_t starts[UCharTrie::kMaxListBranchLength];
    UBool final[UCharTrie::kMaxListBranchLength-1];
    // For each unit except the last one, find its elements array start and its value if final.
    int32_t unitNumber=0;
    do {
        int32_t i=starts[unitNumber]=start;
        UChar unit=elements[i++].charAt(unitIndex, strings);
        while(unit==elements[i].charAt(unitIndex, strings)) {
            ++i;
        }
        final[unitNumber]= start==i-1 && unitIndex+1==elements[start].getStringLength(strings);
        start=i;
    } while(++unitNumber<length-1);
    // unitNumber==length-1, and the maxUnit elements range is [start..limit[
    starts[unitNumber]=start;

    // Write the sub-nodes in reverse order: The jump lengths are deltas from
    // after their own positions, so if we wrote the minUnit sub-node first,
    // then its jump delta would be larger.
    // Instead we write the minUnit sub-node last, for a shorter delta.
    int32_t jumpTargets[UCharTrie::kMaxListBranchLength-1];
    do {
        --unitNumber;
        if(!final[unitNumber]) {
            makeNode(starts[unitNumber], starts[unitNumber+1], unitIndex+1);
            jumpTargets[unitNumber]=ucharsLength;
        }
    } while(unitNumber>0);
    // The maxUnit sub-node is written as the very last one because we do
    // not jump for it at all.
    unitNumber=length-1;
    makeNode(start, limit, unitIndex+1);
    write(elements[start].charAt(unitIndex, strings));
    // Write the rest of this node's unit-value pairs.
    int32_t valueFlags=0;
    while(--unitNumber>=0) {
        valueFlags<<=2;
        start=starts[unitNumber];
        int32_t value;
        if(final[unitNumber]) {
            // Write the final value for the one string ending with this unit.
            value=elements[start].getValue();
            valueFlags|=UCharTrie::kFixedIntIsFinal;
        } else {
            // Write the delta to the start position of the sub-node.
            value=ucharsLength-jumpTargets[unitNumber];
        }
        valueFlags|=writeFixedInt(value)-1;
        write(elements[start].charAt(unitIndex, strings));
    }
    // Write the node lead units.
    if(length>UCharTrie::kMaxListBranchSmallLength) {
        write(valueFlags);
        valueFlags>>=16;
    }
    write(((length-2)<<UCharTrie::kMaxListBranchLengthShift)|valueFlags);
}

// start<limit && all strings longer than unitIndex &&
// at least four different units at unitIndex
// (At least four because the left and right outbound edges
// must lead to branch nodes again, thus each side must have at least two units to branch on.)
void
UCharTrieBuilder::makeSplitBranchNode(int32_t start, int32_t limit, int32_t unitIndex, int32_t length) {
    // Three-way branch on the middle unit.
    // Find the middle unit.
    length/=2;  // >=1
    int32_t i=start;
    UChar unit;
    do {
        unit=elements[i++].charAt(unitIndex, strings);
        while(unit==elements[i].charAt(unitIndex, strings)) {
            ++i;
        }
    } while(--length>0);
    unit=elements[i].charAt(unitIndex, strings);  // middle unit
    // Encode the less-than branch first.
    makeNode(start, i, unitIndex);
    int32_t leftNode=ucharsLength;
    // Encode the greater-or-equal branch last because we do not jump for it at all.
    makeNode(i, limit, unitIndex);
    // Write this node.
    int32_t unitsForLessThan=writeFixedInt(ucharsLength-leftNode);  // less-than
    write(unit);
    write(UCharTrie::kMinSplitBranch+unitsForLessThan-1);
}

UBool
UCharTrieBuilder::ensureCapacity(int32_t length) {
    if(uchars==NULL) {
        return FALSE;  // previous memory allocation had failed
    }
    if(length>ucharsCapacity) {
        int32_t newCapacity=ucharsCapacity;
        do {
            newCapacity*=2;
        } while(newCapacity<=length);
        UChar *newUChars=reinterpret_cast<UChar *>(uprv_malloc(newCapacity*2));
        if(newUChars==NULL) {
            // unable to allocate memory
            uprv_free(uchars);
            uchars=NULL;
            return FALSE;
        }
        uprv_memcpy(newUChars+(newCapacity-ucharsLength),
                    uchars+(ucharsCapacity-ucharsLength), ucharsLength);
        uprv_free(uchars);
        uchars=newUChars;
        ucharsCapacity=newCapacity;
    }
    return TRUE;
}

void
UCharTrieBuilder::write(int32_t unit) {
    int32_t newLength=ucharsLength+1;
    if(ensureCapacity(newLength)) {
        ucharsLength=newLength;
        uchars[ucharsCapacity-ucharsLength]=(UChar)unit;
    }
}

void
UCharTrieBuilder::write(const UChar *s, int32_t length) {
    int32_t newLength=ucharsLength+length;
    if(ensureCapacity(newLength)) {
        ucharsLength=newLength;
        u_memcpy(uchars+(ucharsCapacity-ucharsLength), s, length);
    }
}

void
UCharTrieBuilder::writeCompactInt(int32_t i, UBool final) {
    UChar intUnits[3];
    int32_t length;
    if(i<0 || i>UCharTrie::kMaxTwoUnitValue) {
        intUnits[0]=(UChar)(UCharTrie::kThreeUnitLead);
        intUnits[1]=(UChar)(i>>16);
        intUnits[2]=(UChar)i;
        length=3;
    } else if(i<=UCharTrie::kMaxOneUnitValue) {
        intUnits[0]=(UChar)(UCharTrie::kMinOneUnitLead+i);
        length=1;
    } else {
        intUnits[0]=(UChar)(UCharTrie::kMinTwoUnitLead+(i>>16));
        intUnits[1]=(UChar)i;
        length=2;
    }
    intUnits[0]=(UChar)((intUnits[0]<<1)|final);
    write(intUnits, length);
}

int32_t
UCharTrieBuilder::writeFixedInt(int32_t i) {
    UChar intUnits[2];
    int32_t length;
    if(i<0 || i>0xffff) {
        intUnits[0]=(UChar)(i>>16);
        length=1;  // last unit below
    } else {
        length=0;
    }
    intUnits[length++]=(UChar)i;
    write(intUnits, length);
    return length;
}

U_NAMESPACE_END
