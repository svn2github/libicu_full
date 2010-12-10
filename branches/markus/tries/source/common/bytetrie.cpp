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

UBool
ByteTrie::readCompactInt(int32_t leadByte) {
    UBool isFinal=(UBool)(leadByte&kValueIsFinal);
    leadByte>>=1;
    int numBytes=bytesPerLead[leadByte]-1;  // -1: lead byte was already consumed.
    switch(numBytes) {
    case 0:
        value=leadByte-kMinOneByteLead;
        break;
    case 1:
        value=((leadByte-kMinTwoByteLead)<<8)|*pos;
        break;
    case 2:
        value=((leadByte-kMinThreeByteLead)<<16)|(pos[0]<<8)|pos[1];
        break;
    case 3:
        value=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        break;
    case 4:
        value=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        break;
    }
    pos+=numBytes;
    return isFinal;
}

int32_t
ByteTrie::readFixedInt(int32_t bytesPerValue) {
    int32_t fixedInt;
    switch(bytesPerValue) {  // Actually number of bytes minus 1.
    case 0:
        fixedInt=*pos;
        break;
    case 1:
        fixedInt=(pos[0]<<8)|pos[1];
        break;
    case 2:
        fixedInt=(pos[0]<<16)|(pos[1]<<8)|pos[2];
        break;
    case 3:
        fixedInt=(pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3];
        break;
    }
    pos+=bytesPerValue+1;
    return fixedInt;
}

UBool
ByteTrie::branchNext(int32_t node, int32_t inByte) {
    // Branch according to the current byte.
    while(node>=kMinThreeWayBranch) {
        // Branching on a byte value,
        // with a jump delta for less-than, a compact int for equals,
        // and continuing for greater-than.
        // The less-than and greater-than branches must lead to branch nodes again.
        node-=kMinThreeWayBranch;
        uint8_t trieByte=*pos++;
        if(inByte<trieByte) {
            int32_t delta=readFixedInt(node);
            pos+=delta;
        } else {
            pos+=node+1;  // Skip fixed-width integer.
            node=*pos;
            U_ASSERT(node>=kMinValueLead);
            if(inByte==trieByte) {
                if(node&kValueIsFinal) {
                    // Leave the final value for hasValue() to read.
                } else {
                    // Use the non-final value as the jump delta.
                    ++pos;
                    readCompactInt(node);
                    pos+=value;
                }
                return TRUE;
            } else {  // inByte>trieByte
                pos+=bytesPerLead[node>>1];
            }
        }
        node=*pos++;
        U_ASSERT(node<kMinLinearMatch);
    }
    // Branch node with a list of key-value pairs where
    // values are compact integers: either final values or jump deltas.
    // If the last key byte matches, just continue after it rather
    // than jumping.
    int32_t length=node+1;  // Actual list length minus 1.
    for(;;) {
        uint8_t trieByte=*pos++;
        U_ASSERT(length==0 || *pos>=kMinValueLead);
        if(inByte==trieByte) {
            if(length>0) {
                node=*pos;
                if(node&kValueIsFinal) {
                    // Leave the final value for hasValue() to read.
                } else {
                    // Use the non-final value as the jump delta.
                    ++pos;
                    readCompactInt(node);
                    pos+=value;
                }
            }
            return TRUE;
        }
        if(inByte<trieByte || length--==0) {
            stop();
            return FALSE;
        }
        pos+=bytesPerLead[*pos>>1];
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
    int32_t node=*pos;
    if(node>=kMinValueLead) {
        if(node&kValueIsFinal) {
            // No further matching bytes.
            stop();
            return FALSE;
        } else {
            // Skip intermediate value.
            pos+=bytesPerLead[node>>1];
            // The next node must not also be a value node.
            node=*pos;
            U_ASSERT(node<kMinValueLead);
        }
    }
    ++pos;
    if(node<kMinLinearMatch) {
        return branchNext(node, inByte);
    } else {
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
    }
}

UBool
ByteTrie::next(const char *s, int32_t sLength) {
    if(pos==NULL) {
        // Return FALSE if there are input bytes,
        // but TRUE if not, so that next(string) is equivalent to
        // for(each byte b)
        //     if(!next(b)) return FALSE;
        return sLength==0 || (sLength<0 && *s==0);
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
        int32_t node=*pos;
        if(node>=kMinValueLead) {
            if(node&kValueIsFinal) {
                // No further matching bytes.
                stop();
                return FALSE;
            } else {
                // Skip intermediate value.
                pos+=bytesPerLead[node>>1];
                // The next node must not also be a value node.
                node=*pos;
                U_ASSERT(node<kMinValueLead);
            }
        }
        ++pos;
        if(node<kMinLinearMatch) {
            if(!branchNext(node, inByte)) {
                return FALSE;
            }
        } else {
            // Match length+1 bytes.
            length=node-kMinLinearMatch;  // Actual match length minus 1.
            if(inByte!=*pos) {
                // No match.
                stop();
                return FALSE;
            }
            ++pos;
            --length;
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
        if(node>=kMinValueLead) {
            UBool isFinal=readCompactInt(node);
            if(!isUniqueValue()) {
                return FALSE;
            }
            if(isFinal) {
                return TRUE;
            }
            node=*pos++;
            U_ASSERT(node<kMinValueLead);
        }
        if(node<kMinLinearMatch) {
            while(node>=kMinThreeWayBranch) {
                // three-way-branch node
                node-=kMinThreeWayBranch;
                ++pos;  // ignore the comparison byte
                // less-than branch
                int32_t delta=readFixedInt(node);
                if(!findUniqueValueAt(delta)) {
                    return FALSE;
                }
                // equals branch
                if(!findUniqueValueFromBranchEntry()) {
                    return FALSE;
                }
                // greater-than branch
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
        } else {
            // linear-match node
            pos+=node-kMinLinearMatch+1;  // Ignore the match bytes.
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
    while(node>=kMinThreeWayBranch) {
        // three-way-branch node
        node-=kMinThreeWayBranch;
        uint8_t trieByte=*pos++;
        // less-than branch
        int32_t delta=readFixedInt(node);
        const uint8_t *currentPos=pos;
        pos+=delta;
        count+=getNextBranchBytes(out);
        pos=currentPos;
        // equals branch
        append(out, trieByte);
        ++count;
        node=*pos;
        U_ASSERT(node>=kMinValueLead);
        pos+=bytesPerLead[node>>1];
        // greater-than branch
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
