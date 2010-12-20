/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uchartrie.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov14
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "uassert.h"
#include "uchartrie.h"

U_NAMESPACE_BEGIN

Appendable &
Appendable::appendCodePoint(UChar32 c) {
    if(c<=0xffff) {
        return append((UChar)c);
    } else {
        return append(U16_LEAD(c)).append(U16_TRAIL(c));
    }
}

Appendable &
Appendable::append(const UChar *s, int32_t length) {
    if(s!=NULL && length!=0) {
        if(length<0) {
            UChar c;
            while((c=*s++)!=0) {
                append(c);
            }
        } else {
            const UChar *limit=s+length;
            while(s<limit) {
                append(*s++);
            }
        }
    }
    return *this;
}

UOBJECT_DEFINE_NO_RTTI_IMPLEMENTATION(Appendable)

UDictTrieResult
UCharTrie::current() const {
    const UChar *pos=pos_;
    if(haveValue_) {
        return pos==NULL ? UDICTTRIE_HAS_FINAL_VALUE : UDICTTRIE_HAS_VALUE;
    } else if(pos==NULL) {
        return UDICTTRIE_NO_MATCH;
    } else {
        int32_t node;
        return (remainingMatchLength_<0 && (node=*pos)>=kMinValueLead) ?
            (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
    }
}

UDictTrieResult
UCharTrie::branchNext(const UChar *pos, int32_t length, int32_t uchar) {
    // Branch according to the current unit.
    if(length==0) {
        length=*pos++;
    }
    ++length;
    // The length of the branch is the number of units to select from.
    // The data structure encodes a binary search.
    while(length>kMaxBranchLinearSubNodeLength) {
        if(uchar<*pos++) {
            length>>=1;
            // int32_t delta=readDelta(pos);
            int32_t delta=*pos++;
            if(delta>=kMinTwoUnitDeltaLead) {
                if(delta==kThreeUnitDeltaLead) {
                    delta=(pos[0]<<16)|pos[1];
                    pos+=2;
                } else {
                    delta=((delta-kMinTwoUnitDeltaLead)<<16)|*pos++;
                }
            }
            // end readDelta()
            pos+=delta;
        } else {
            length=length-(length>>1);
            pos=skipDelta(pos);
        }
    }
    // Drop down to linear search for the last few units.
    // length>=2 because the loop body above sees length>kMaxBranchLinearSubNodeLength>=3
    // and divides length by 2.
    do {
        if(uchar==*pos++) {
            UDictTrieResult result;
            int32_t node=*pos;
            U_ASSERT(node>=kMinValueLead);
            if(node&kValueIsFinal) {
                // Leave the final value for getValue() to read.
                result=UDICTTRIE_HAS_FINAL_VALUE;
            } else {
                // Use the non-final value as the jump delta.
                ++pos;
                // int32_t delta=readValue(pos, node>>1);
                node>>=1;
                int32_t delta;
                if(node<kMinTwoUnitValueLead) {
                    delta=node-kMinOneUnitValueLead;
                } else if(node<kThreeUnitValueLead) {
                    delta=((node-kMinTwoUnitValueLead)<<16)|*pos++;
                } else {
                    delta=(pos[0]<<16)|pos[1];
                    pos+=2;
                }
                // end readValue()
                pos+=delta;
                node=*pos;
                result= node>=kMinValueLead ?
                    (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
            }
            pos_=pos;
            return result;
        }
        --length;
        pos=skipValue(pos);
    } while(length>1);
    if(uchar==*pos++) {
        pos_=pos;
        int32_t node=*pos;
        return node>=kMinValueLead ?
            (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
    } else {
        stop();
        return UDICTTRIE_NO_MATCH;
    }
}

UDictTrieResult
UCharTrie::nextImpl(const UChar *pos, int32_t uchar) {
    for(;;) {
        int32_t node=*pos++;
        if(node<kMinLinearMatch) {
            return branchNext(pos, node, uchar);
        } else if(node<kMinValueLead) {
            // Match the first of length+1 units.
            int32_t length=node-kMinLinearMatch;  // Actual match length minus 1.
            if(uchar==*pos++) {
                remainingMatchLength_=--length;
                pos_=pos;
                return (length<0 && (node=*pos)>=kMinValueLead) ?
                    (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
            } else {
                // No match.
                break;
            }
        } else if(node&kValueIsFinal) {
            // No further matching units.
            break;
        } else {
            // Skip intermediate value.
            pos=skipValue(pos, node);
            // The next node must not also be a value node.
            U_ASSERT(*pos<kMinValueLead);
        }
    }
    stop();
    return UDICTTRIE_NO_MATCH;
}

UDictTrieResult
UCharTrie::next(int32_t uchar) {
    haveValue_=FALSE;
    const UChar *pos=pos_;
    if(pos==NULL) {
        return UDICTTRIE_NO_MATCH;
    }
    int32_t length=remainingMatchLength_;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Remaining part of a linear-match node.
        if(uchar==*pos++) {
            remainingMatchLength_=--length;
            pos_=pos;
            int32_t node;
            return (length<0 && (node=*pos)>=kMinValueLead) ?
                (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
        } else {
            stop();
            return UDICTTRIE_NO_MATCH;
        }
    }
    return nextImpl(pos, uchar);
}

UDictTrieResult
UCharTrie::next(const UChar *s, int32_t sLength) {
    if(sLength<0 ? *s==0 : sLength==0) {
        // Empty input.
        return current();
    }
    haveValue_=FALSE;
    const UChar *pos=pos_;
    if(pos==NULL) {
        return UDICTTRIE_NO_MATCH;
    }
    int32_t length=remainingMatchLength_;  // Actual remaining match length minus 1.
    for(;;) {
        // Fetch the next input unit, if there is one.
        // Continue a linear-match node without rechecking sLength<0.
        int32_t uchar;
        if(sLength<0) {
            for(;;) {
                if((uchar=*s++)==0) {
                    remainingMatchLength_=length;
                    pos_=pos;
                    int32_t node;
                    return (length<0 && (node=*pos)>=kMinValueLead) ?
                        (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
                }
                if(length<0) {
                    remainingMatchLength_=length;
                    break;
                }
                if(uchar!=*pos) {
                    stop();
                    return UDICTTRIE_NO_MATCH;
                }
                ++pos;
                --length;
            }
        } else {
            for(;;) {
                if(sLength==0) {
                    remainingMatchLength_=length;
                    pos_=pos;
                    int32_t node;
                    return (length<0 && (node=*pos)>=kMinValueLead) ?
                        (UDictTrieResult)(UDICTTRIE_HAS_VALUE+(node&kValueIsFinal)) : UDICTTRIE_NO_VALUE;
                }
                uchar=*s++;
                --sLength;
                if(length<0) {
                    remainingMatchLength_=length;
                    break;
                }
                if(uchar!=*pos) {
                    stop();
                    return UDICTTRIE_NO_MATCH;
                }
                ++pos;
                --length;
            }
        }
        for(;;) {
            int32_t node=*pos++;
            if(node<kMinLinearMatch) {
                UDictTrieResult result=branchNext(pos, node, uchar);
                if(result==UDICTTRIE_NO_MATCH) {
                    return UDICTTRIE_NO_MATCH;
                }
                pos=pos_;  // branchNext() advanced pos and wrote it to pos_ .
                // Fetch the next input unit, if there is one.
                if(sLength<0) {
                    if((uchar=*s++)==0) {
                        return result;
                    }
                } else {
                    if(sLength==0) {
                        return result;
                    }
                    uchar=*s++;
                    --sLength;
                }
                if(result==UDICTTRIE_HAS_FINAL_VALUE) {
                    // No further matching units.
                    stop();
                    return UDICTTRIE_NO_MATCH;
                }
            } else if(node<kMinValueLead) {
                // Match length+1 units.
                length=node-kMinLinearMatch;  // Actual match length minus 1.
                if(uchar!=*pos) {
                    stop();
                    return UDICTTRIE_NO_MATCH;
                }
                ++pos;
                --length;
                break;
            } else if(node&kValueIsFinal) {
                // No further matching units.
                stop();
                return UDICTTRIE_NO_MATCH;
            } else {
                // Skip intermediate value.
                pos=skipValue(pos, node);
                // The next node must not also be a value node.
                U_ASSERT(*pos<kMinValueLead);
            }
        }
    }
}

UBool
UCharTrie::hasUniqueValue() {
#if 0
    if(pos==NULL) {
        return FALSE;
    }
    const UChar *originalPos=pos;
    uniqueValue=value;
    haveUniqueValue=haveValue;

    if(remainingMatchLength>=0) {
        // Skip the rest of a pending linear-match node.
        pos+=remainingMatchLength+1;
    }
    haveValue=findUniqueValue();
    // If haveValue is true, then value is already set to the final value
    // of the last-visited branch.
    // Restore original state, except for value/haveValue.
    pos=originalPos;
    return haveValue;
#endif
    return FALSE;
}

UBool
UCharTrie::findUniqueValueFromBranchEntry(int32_t node) {
#if 0
    value=readFixedInt(node);
    if(node&kFixedIntIsFinal) {
        // Final value directly in the branch entry.
        if(!isUniqueValue()) {
            return FALSE;
        }
    } else {
        // Use the non-final value as the jump delta.
        if(!findUniqueValueAt(value)) {
            return FALSE;
        }
    }
    return TRUE;
#endif
    return FALSE;
}

UBool
UCharTrie::findUniqueValue() {
#if 0
    for(;;) {
        int32_t node=*pos++;
        if(node<kMinLinearMatch) {
            while(node>=kMinSplitBranch) {
                // split-branch node
                ++pos;  // ignore the comparison unit
                // less-than branch
                int32_t delta=readFixedInt(node);
                if(!findUniqueValueAt(delta)) {
                    return FALSE;
                }
                // greater-or-equal branch
                node=*pos++;
                U_ASSERT(node<kMinLinearMatch);
            }
            // list-branch node
            int32_t length=(node>>kMaxListBranchLengthShift)+1;  // Actual list length minus 1.
            if(length>=kMaxListBranchSmallLength) {
                // For 7..14 pairs, read the next unit as well.
                node=(node<<16)|*pos++;
            }
            do {
                ++pos;  // ignore a comparison unit
                // handle its value
                if(!findUniqueValueFromBranchEntry(node)) {
                    return FALSE;
                }
                node>>=2;
            } while(--length>0);
            ++pos;  // ignore the last comparison unit
        } else if(node<kMinValueLead) {
            // linear-match node
            pos+=node-kMinLinearMatch+1;  // Ignore the match units.
        } else {
            UBool isFinal=readCompactInt(node);
            if(!isUniqueValue()) {
                return FALSE;
            }
            if(isFinal) {
                return TRUE;
            }
            U_ASSERT(*pos<kMinValueLead);
        }
    }
#endif
    return FALSE;
}

int32_t
UCharTrie::getNextUChars(Appendable &out) {
#if 0
    if(pos==NULL) {
        return 0;
    }
    if(remainingMatchLength>=0) {
        out.append(*pos);  // Next byte of a pending linear-match node.
        return 1;
    }
    const UChar *originalPos=pos;
    int32_t node=*pos;
    if(node>=kMinValueLead) {
        if(node&kValueIsFinal) {
            return 0;
        } else {
            ++pos;  // Skip the lead unit.
            skipValue(node);
            node=*pos;
            U_ASSERT(node<kMinValueLead);
        }
    }
    int32_t count;
    if(node<kMinLinearMatch) {
        count=getNextBranchUChars(out);
    } else {
        // First byte of the linear-match node.
        out.append(pos[1]);
        count=1;
    }
    pos=originalPos;
    return count;
#endif
    out.append(0);
    return 0;
}

int32_t
UCharTrie::getNextBranchUChars(Appendable &out) {
#if 0
    int32_t count=0;
    int32_t node=*pos++;
    U_ASSERT(node<kMinLinearMatch);
    while(node>=kMinSplitBranch) {
        // split-branch node
        ++pos;  // ignore the comparison unit
        // less-than branch
        int32_t delta=readFixedInt(node);
        const UChar *currentPos=pos;
        pos+=delta;
        count+=getNextBranchUChars(out);
        pos=currentPos;
        // greater-or-equal branch
        node=*pos++;
        U_ASSERT(node<kMinLinearMatch);
    }
    // list-branch node
    int32_t length=(node>>kMaxListBranchLengthShift)+1;  // Actual list length minus 1.
    if(length>=kMaxListBranchSmallLength) {
        // For 7..14 pairs, read the next unit as well.
        node=(node<<16)|*pos++;
    }
    count+=length+1;
    do {
        out.append(*pos++);
        pos+=(node&kFixedInt32)+1;
        node>>=2;
    } while(--length>0);
    out.append(*pos);
    return count;
#endif
    out.append(0);
    return 0;
}

U_NAMESPACE_END
