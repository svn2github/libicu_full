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

const int8_t ByteTrie::bytesPerLead[kFiveByteLead+1]={
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5
};

int32_t
ByteTrie::readValue(int32_t leadByte) {  // lead byte already shifted right by 1.
    int32_t v;
    if(leadByte<kMinTwoByteLead) {
        return leadByte-kMinOneByteLead;
    } else if(leadByte<kMinThreeByteLead) {
        return ((leadByte-kMinTwoByteLead)<<8)|*pos++;
    } else if(leadByte<kFourByteLead) {
        v=((leadByte-kMinThreeByteLead)<<16)|(pos[0]<<8)|pos[1];
        pos+=2;
    } else if(leadByte==kFourByteLead) {
        v=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        pos+=3;
    } else {
        v=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        pos+=4;
    }
    return v;
}

int32_t
ByteTrie::readFixedInt(int32_t bytesPerValue) {
    int32_t fixedInt=*pos;
    switch(bytesPerValue) {  // Actually number of bytes minus 1.
    case 0:
        break;
    case 1:
        fixedInt=(fixedInt<<8)|pos[1];
        break;
    case 2:
        fixedInt=(fixedInt<<16)|(pos[1]<<8)|pos[2];
        break;
    case 3:
        fixedInt=(fixedInt<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        break;
    }
    pos+=bytesPerValue+1;
    return fixedInt;
}

const int8_t ByteTrie::bytesPerDeltaLead[0x100]={
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5
};

int32_t
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
#if 0
    int numBytes=bytesPerDeltaLead[delta]-1;  // -1: lead byte was already consumed.
    switch(numBytes) {
    case 0:
        // nothing to do
        return delta;
    case 1:
        delta=((delta-kMinTwoByteDeltaLead)<<8)|*pos;
        break;
    case 2:
        delta=((delta-kMinThreeByteDeltaLead)<<16)|(pos[0]<<8)|pos[1];
        break;
    case 3:
        delta=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        break;
    case 4:
        delta=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        break;
    }
    pos+=numBytes;
#endif
    return delta;
}

UBool
ByteTrie::branchNext(int32_t length, int32_t inByte) {
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
            int32_t delta=readDelta();
            pos+=delta;
        } else {
            length=length-(length>>1);
            skipDelta();
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
                int32_t delta=readValue(node>>1);
                pos+=delta;
            }
            return TRUE;
        }
        --length;
        skipValueAndFinal();
    } while(length>1);
    if(inByte==*pos++) {
        return TRUE;
    } else {
        stop();
        return FALSE;
    }
}

UBool
ByteTrie::next(int32_t inByte) {
    if(pos==NULL) {
        return FALSE;
    }
    haveValue=FALSE;
    int32_t length=remainingMatchLength;  // Actual remaining match length minus 1.
    if(length>=0) {
        // Remaining part of a linear-match node.
        if(inByte==*pos) {
            remainingMatchLength=length-1;
            ++pos;
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
            return branchNext(node, inByte);
        } else if(node<kMinValueLead) {
            // Match the first of length+1 bytes.
            length=node-kMinLinearMatch;  // Actual match length minus 1.
            if(inByte==*pos) {
                remainingMatchLength=length-1;
                ++pos;
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
            skipValueAndFinal(node);
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
    if(pos==NULL) {
        return FALSE;
    }
    haveValue=FALSE;
    int32_t length=remainingMatchLength;  // Actual remaining match length minus 1.
    for(;;) {
        // Fetch the next input byte, if there is one.
        // Continue a linear-match node without rechecking sLength<0.
        int32_t inByte;
        if(sLength<0) {
            for(;;) {
                if((inByte=*s++)==0) {
                    remainingMatchLength=length;
                    return TRUE;
                }
                if(length<0) {
                    remainingMatchLength=length;
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
                    remainingMatchLength=length;
                    return TRUE;
                }
                inByte=*s++;
                --sLength;
                if(length<0) {
                    remainingMatchLength=length;
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
                if(!branchNext(node, inByte)) {
                    return FALSE;
                }
                // Fetch the next input byte, if there is one.
                if(sLength<0) {
                    if((inByte=*s++)==0) {
                        return TRUE;
                    }
                } else {
                    if(sLength==0) {
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
                skipValueAndFinal(node);
                // The next node must not also be a value node.
                U_ASSERT(*pos<kMinValueLead);
            }
        }
    }
}

UBool
ByteTrie::hasValue() {
    int32_t node;
    if(haveValue) {
        return TRUE;
    } else if(pos!=NULL && remainingMatchLength<0 && (node=*pos)>=kMinValueLead) {
        // Deliver value for the matching bytes.
        ++pos;
        if(readCompactInt(node)) {
            stop();
        } else {
            // The next node must not also be a value node.
            U_ASSERT(*pos<kMinValueLead);
        }
        haveValue=TRUE;
        return TRUE;
    }
    return FALSE;
}

UBool
ByteTrie::hasUniqueValue() {
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
}

UBool
ByteTrie::findUniqueValueFromBranchEntry() {
    int32_t node=*pos++;
    U_ASSERT(node>=kMinValueLead);
    if(readCompactInt(node)) {
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
}

UBool
ByteTrie::findUniqueValue() {
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
}

int32_t
ByteTrie::getNextBytes(ByteSink &out) {
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
}

int32_t
ByteTrie::getNextBranchBytes(ByteSink &out) {
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
}

void
ByteTrie::append(ByteSink &out, int c) {
    char ch=(char)c;
    out.Append(&ch, 1);
}

U_NAMESPACE_END
