/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetrie.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010sep25
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/bytestream.h"
#include "unicode/uobject.h"
#include "uassert.h"
#include "bytetrie.h"

U_NAMESPACE_BEGIN

int32_t
ByteTrie::readValue(const uint8_t *pos, int32_t leadByte) {  // lead byte already shifted right by 1.
    int32_t value;
    if(leadByte<kMinTwoByteValueLead) {
        value=leadByte-kMinOneByteValueLead;
    } else if(leadByte<kMinThreeByteValueLead) {
        value=((leadByte-kMinTwoByteValueLead)<<8)|*pos++;
    } else if(leadByte<kFourByteValueLead) {
        value=((leadByte-kMinThreeByteValueLead)<<16)|(pos[0]<<8)|pos[1];
        pos+=2;
    } else if(leadByte==kFourByteValueLead) {
        value=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        pos+=3;
    } else {
        value=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        pos+=4;
    }
    pos_=pos;
    return value;
}

/* int32_t
ByteTrie::readDelta() {
    int32_t delta=*pos++;
    if(delta<kMinTwoByteDeltaLead) {
        return delta;
    } else if(delta<kMinThreeByteDeltaLead) {
        return ((delta-kMinTwoByteDeltaLead)<<8)|*pos++;
    } else if(delta<kFourByteDeltaLead) {
        delta=((delta-kMinThreeByteDeltaLead)<<16)|(pos[0]<<8)|pos[1];
        pos+=2;
    } else if(delta==kFourByteDeltaLead) {
        delta=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        pos+=3;
    } else {
        delta=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        pos+=4;
    }
    return delta;
} */

const uint8_t *
ByteTrie::branchNext(const uint8_t *pos, int32_t length, int32_t inByte) {
    // Branch according to the current byte.
    if(length==0) {
        length=*pos++;
    }
    ++length;
    // The length of the branch is the number of bytes to select from.
    // The data structure encodes a binary search.
    while(length>kMaxBranchLinearSubNodeLength) {
        if(inByte<*pos++) {
            length>>=1;
            // int32_t delta=readDelta(pos);
            int32_t delta=*pos++;
            if(delta<kMinTwoByteDeltaLead) {
                // nothing to do
            } else if(delta<kMinThreeByteDeltaLead) {
                delta=((delta-kMinTwoByteDeltaLead)<<8)|*pos++;
            } else if(delta<kFourByteDeltaLead) {
                delta=((delta-kMinThreeByteDeltaLead)<<16)|(pos[0]<<8)|pos[1];
                pos+=2;
            } else if(delta==kFourByteDeltaLead) {
                delta=(pos[0]<<16)|(pos[1]<<8)|pos[2];
                pos+=3;
            } else {
                delta=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
                pos+=4;
            }
            // end readDelta()
            pos+=delta;
        } else {
            length=length-(length>>1);
            pos=skipDelta(pos);
        }
    }
    // Drop down to linear search for the last few bytes.
    // length>=2 because the loop body above sees length>kMaxBranchLinearSubNodeLength>=3
    // and divides length by 2.
    do {
        if(inByte==*pos++) {
            int32_t node=*pos;
            U_ASSERT(node>=kMinValueLead);
            if(node&kValueIsFinal) {
                // Leave the final value for hasValue() to read.
            } else {
                // Use the non-final value as the jump delta.
                ++pos;
                // int32_t delta=readValue(pos, node>>1);
                node>>=1;
                int32_t delta;
                if(node<kMinTwoByteValueLead) {
                    delta=node-kMinOneByteValueLead;
                } else if(node<kMinThreeByteValueLead) {
                    delta=((node-kMinTwoByteValueLead)<<8)|*pos++;
                } else if(node<kFourByteValueLead) {
                    delta=((node-kMinThreeByteValueLead)<<16)|(pos[0]<<8)|pos[1];
                    pos+=2;
                } else if(node==kFourByteValueLead) {
                    delta=(pos[0]<<16)|(pos[1]<<8)|pos[2];
                    pos+=3;
                } else {
                    delta=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
                    pos+=4;
                }
                // end readValue()
                pos+=delta;
            }
            return pos;
        }
        --length;
        pos=skipValueAndFinal(pos);
    } while(length>1);
    if(inByte==*pos++) {
        return pos;
    } else {
        return NULL;
    }
}

UBool
ByteTrie::next(int32_t inByte) {
    const uint8_t *pos=pos_;
    if(pos==NULL) {
        return FALSE;
    }
    haveValue_=FALSE;
    int32_t length=remainingMatchLength_;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Remaining part of a linear-match node.
        if(inByte==*pos) {
            remainingMatchLength_=length-1;
            pos_=pos+1;
            return TRUE;
        } else {
            // No match.
            stop();
            return FALSE;
        }
    }
    for(;;) {
        int32_t node=*pos++;
        if(node<kMinLinearMatch) {
            pos_=pos=branchNext(pos, node, inByte);
            return pos!=NULL;
        } else if(node<kMinValueLead) {
            // Match the first of length+1 bytes.
            length=node-kMinLinearMatch;  // Actual match length minus 1.
            if(inByte==*pos) {
                remainingMatchLength_=length-1;
                pos_=pos+1;
                return TRUE;
            } else {
                // No match.
                stop();
                return FALSE;
            }
        } else if(node&kValueIsFinal) {
            // No further matching bytes.
            stop();
            return FALSE;
        } else {
            // Skip intermediate value.
            pos=skipValueAndFinal(pos, node);
            // The next node must not also be a value node.
            U_ASSERT(*pos<kMinValueLead);
        }
    }
}

UBool
ByteTrie::next(const char *s, int32_t sLength) {
    if(sLength<0 ? *s==0 : sLength==0) {
        // Empty input: Do nothing at all, see API doc.
        return TRUE;
    }
    const uint8_t *pos=pos_;
    if(pos==NULL) {
        return FALSE;
    }
    haveValue_=FALSE;
    int32_t length=remainingMatchLength_;  // Actual remaining match length minus 1.
    for(;;) {
        // Fetch the next input byte, if there is one.
        // Continue a linear-match node without rechecking sLength<0.
        int32_t inByte;
        if(sLength<0) {
            for(;;) {
                if((inByte=*s++)==0) {
                    remainingMatchLength_=length;
                    pos_=pos;
                    return TRUE;
                }
                if(length<0) {
                    remainingMatchLength_=length;
                    break;
                }
                if(inByte!=*pos) {
                    stop();
                    return FALSE;
                }
                ++pos;
                --length;
            }
        } else {
            for(;;) {
                if(sLength==0) {
                    remainingMatchLength_=length;
                    pos_=pos;
                    return TRUE;
                }
                inByte=*s++;
                --sLength;
                if(length<0) {
                    remainingMatchLength_=length;
                    break;
                }
                if(inByte!=*pos) {
                    stop();
                    return FALSE;
                }
                ++pos;
                --length;
            }
        }
        for(;;) {
            int32_t node=*pos++;
            if(node<kMinLinearMatch) {
                pos=branchNext(pos, node, inByte);
                if(pos==NULL) {
                    stop();
                    return FALSE;
                }
                // Fetch the next input byte, if there is one.
                if(sLength<0) {
                    if((inByte=*s++)==0) {
                        pos_=pos;
                        return TRUE;
                    }
                } else {
                    if(sLength==0) {
                        pos_=pos;
                        return TRUE;
                    }
                    inByte=*s++;
                    --sLength;
                }
            } else if(node<kMinValueLead) {
                // Match length+1 bytes.
                length=node-kMinLinearMatch;  // Actual match length minus 1.
                if(inByte!=*pos) {
                    // No match.
                    stop();
                    return FALSE;
                }
                ++pos;
                --length;
                break;
            } else if(node&kValueIsFinal) {
                // No further matching bytes.
                stop();
                return FALSE;
            } else {
                // Skip intermediate value.
                pos=skipValueAndFinal(pos, node);
                // The next node must not also be a value node.
                U_ASSERT(*pos<kMinValueLead);
            }
        }
    }
}

UBool
ByteTrie::hasValue() {
    int32_t node;
    if(haveValue_) {
        return TRUE;
    }
    const uint8_t *pos=pos_;
    if(pos!=NULL && remainingMatchLength_<0 && (node=*pos)>=kMinValueLead) {
        // Deliver value for the matching bytes.
        if(readValueAndFinal(pos+1, node)) {
            stop();
        } else {
            // The next node must not also be a value node.
            U_ASSERT(*pos_<kMinValueLead);
        }
        haveValue_=TRUE;
        return TRUE;
    }
    return FALSE;
}

UBool
ByteTrie::hasUniqueValue() {
#if 0
    if(pos==NULL) {
        return FALSE;
    }
    const uint8_t *originalPos=pos;
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
ByteTrie::findUniqueValueFromBranchEntry() {
#if 0
    int32_t node=*pos++;
    U_ASSERT(node>=kMinValueLead);
    if(readValueAndFinal(node)) {
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
ByteTrie::findUniqueValue() {
#if 0
    for(;;) {
        int32_t node=*pos++;
        if(node<kMinLinearMatch) {
            while(node>=kMinSplitBranch) {
                // split-branch node
                node-=kMinSplitBranch;
                ++pos;  // ignore the comparison byte
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
            int32_t length=node+1;  // Actual list length minus 1.
            do {
                ++pos;  // ignore a comparison byte
                // handle its value
                if(!findUniqueValueFromBranchEntry()) {
                    return FALSE;
                }
            } while(--length>0);
            ++pos;  // ignore the last comparison byte
        } else if(node<kMinValueLead) {
            // linear-match node
            pos+=node-kMinLinearMatch+1;  // Ignore the match bytes.
        } else {
            UBool isFinal=readValueAndFinal(node);
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
ByteTrie::getNextBytes(ByteSink &out) {
#if 0
    if(pos==NULL) {
        return 0;
    }
    if(remainingMatchLength>=0) {
        append(out, *pos);  // Next byte of a pending linear-match node.
        return 1;
    }
    const uint8_t *originalPos=pos;
    int32_t node=*pos;
    if(node>=kMinValueLead) {
        if(node&kValueIsFinal) {
            return 0;
        } else {
            pos+=bytesPerLead[node>>1];
            node=*pos;
            U_ASSERT(node<kMinValueLead);
        }
    }
    int32_t count;
    if(node<kMinLinearMatch) {
        count=getNextBranchBytes(out);
    } else {
        // First byte of the linear-match node.
        append(out, pos[1]);
        count=1;
    }
    pos=originalPos;
    return count;
#endif
    append(out, 0);
    return 0;
}

int32_t
ByteTrie::getNextBranchBytes(ByteSink &out) {
#if 0
    int32_t count=0;
    int32_t node=*pos++;
    U_ASSERT(node<kMinLinearMatch);
    while(node>=kMinSplitBranch) {
        // split-branch node
        node-=kMinSplitBranch;
        ++pos;  // ignore the comparison byte
        // less-than branch
        int32_t delta=readFixedInt(node);
        const uint8_t *currentPos=pos;
        pos+=delta;
        count+=getNextBranchBytes(out);
        pos=currentPos;
        // greater-or-equal branch
        node=*pos++;
        U_ASSERT(node<kMinLinearMatch);
    }
    // list-branch node
    int32_t length=node+1;  // Actual list length minus 1.
    count+=length+1;
    do {
        append(out, *pos++);
        pos+=bytesPerLead[*pos>>1];
    } while(--length>0);
    append(out, *pos);
    return count;
#endif
    append(out, 0);
    return 0;
}

void
ByteTrie::append(ByteSink &out, int c) {
    char ch=(char)c;
    out.Append(&ch, 1);
}

U_NAMESPACE_END
