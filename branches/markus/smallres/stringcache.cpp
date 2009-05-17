/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  stringcache.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009apr23
*   created by: Markus W. Scherer
*
*   Cache for compacted/compressed resource bundle strings.
*   Maps from resource item word to UTF-16 string.
*/

struct HashEntry {
    uint32_t res;
    uint32_t value;
}

class StringCache : public UMemory {
public:
    StringCache(int32_t numCompressedStrings,
                int32_t numDecompressedUChars,
                UErrorCode &errorCode)
        : fNumCompressedStrings(numCompressedStrings),
          fNumDecompressedUChars(numDecompressedUChars),
          fMediumBuffer(0),
          fLargeBuffer(0),
          fBuffer(fSmallBuffer),
          fCapacity(LENGTHOF(fSmallBuffer)-1),
          fRemainingUChars(numDecompressedUChars),
          fHashTable(0),
          fHashTableLength(0) {
        if(U_SUCCESS(errorCode)) {
            fSmallBuffer[0]=0;  // empty string
            if(numCompressedStrings<=LENGTHOF(fMiniHashTable)) {
                fHashTableLength=LENGTHOF(fMiniHashTable);
                fHashTable=fMiniHashTable;
            } else {
                fHashTableLength=2*numCompressedStrings;
                fHashTable=(HashEntry *)uprv_malloc(fHashTableLength*sizeof(HashEntry));
                if(fHashTable==NULL) {
                    errorCode=U_MEMORY_ALLOCATION_ERROR;
                }
            }
        }
    }
    ~StringCache() {
        if(fHashTable!=fMiniHashTable) {
            uprv_free(fHashTable);
        }
        uprv_free(fMediumBuffer);
        uprv_free(fLargeBuffer);
    }
    const UChar *getString(const int32_t *bundle,
                           uint32_t res,
                           int32_t &length) {
        uint32_t hashRes;
        int32_t hashIndex=getHashIndex(res);
        for(;;) {
            UMTX_CHECK(&rbMutex, fHashTable[hashIndex].res, hashRes);
            if(hashRes==res) {
                break;  // found the string in the cache
            } else if(hashRes==0) {
                // the string is not yet cached
                hashIndex=decompress(bundle, res, hashIndex, errorCode);
                if(U_FAILURE(errorCode)) {
                    length=0;
                    return NULL;
                }
                break;
            }
            // hash collision, go to the next hash index
            hashIndex=nextHashIndex(hashIndex);
        }
        uint32_t hashValue=fHashTable[hashIndex].value;
        const UChar *p=getBuffer((int32_t)(hashValue>>30));
        p+=hashValue&0x3fffffff;  // add intra-buffer offset
        length=*p++;  // get the length from one or two UChars
        if(length&0x8000) {
            length=((length&0x7fff)<<16)|*p++;
        }
        return p;
    }
private:
    int32_t getHashIndex(uint32_t res) {
        return (int32_t)(res^(res>>12))%fHashTableLength;
    }
    int32_t nextHashIndex(int32_t hashIndex) {
        // TODO: Need to make sure that gcd(kPrime, fHashTableLength)==1
        // so that all hash entries are reachable!
        // Idea: Choose fHashTableLength to be the next
        // power of 2 > numCompressedStrings?
        return (hashIndex+kPrime)%fHashTableLength;
    }
    UChar *getBuffer(int32_t bufferIndex) {
        switch(bufferIndex) {
            case 0: return fSmallBuffer;
            case 1: return fMediumBuffer;
            case 2: return fLargeBuffer;
            default: return NULL;
        }
    }
    int32_t decompress(const int32_t *bundle,
                       uint32_t res,
                       int32_t hashIndex,
                       UErrorCode &errorCode) {
        Mutex lock(&rbMutex);
        // Check if another thread has cached this string in the meantime,
        // or used this hash entry for a different string.
        for(;;) {
            uint32_t hashRes=fHashTable[hashIndex].res;
            if(hashRes==res) {
                return hashIndex;
            } else if(hashRes==0) {
                break;
            }
            // hash collision, go to the next hash index
            hashIndex=nextHashIndex(hashIndex);
        }
        uint32_t hashValue;
        if(isMiniString(res)) {
            res&=0xfffffff;
            if(res==0) {
                // empty string
                hashValue=0;
            } else {
                // 1..4 characters
                UChar temp[8];
                length=decodeMiniString(res, temp, errorCode);
                if(U_FAILURE(errorCode)) {
                    return 0;
                }
                UChar *p=allocString(length+2, hashValue, errorCode);
                *p++=(UChar)length;
                UChar *t=temp;
                do {
                    *p++=*t++;
                } while(--length>0);
                *p=0;
            }
        } else {
            const uint8_t *bytes=(const uint8_t *)(bundle+(res&0xfffffff));
            int32_t windowIndex=*bytes>>6;
            int32_t length=decodeLength(bytes);
            UChar *p=allocString(length+(length<=0x7fff ? 2 : 3), hashValue, errorCode);
            if(U_FAILURE(errorCode)) {
                return 0;
            }
            if(length<=0x7fff) {
                *p++=(UChar)length;
            } else {
                p[0]=(UChar)(0x8000|(length>>16));
                p[1]=(UChar)length;
                p+=2;
            }
            p[length]=0;
            if(windowIndex<=2) {
                decodeSingleByteMode(fWindows, windowIndex, bytes, p, length, errorCode);
            } else {
                decodeCJK(bytes, p, length, errorCode);
            }
            // TBD: back out the allocation if an error occurred during decoding
        }
        fHashTable[hashIndex].value=hashValue;
        return hashIndex;
    }
    // length must include the string-length and NUL UChars
    UChar *allocString(int32_t length, uint32_t &value, UErrorCode &errorCode) {
        const UChar *buffer;
        if(length<=fCapacity) {
            buffer=getBuffer(fBufferIndex);
        } else if(fMediumBuffer==NULL && length<(fRemainingUChars/4) && fRemainingUChars>1000) {
            int32_t capacity=fRemainingUChars/4;
            buffer=fMediumBuffer=(UChar *)uprv_malloc(capacity*U_SIZEOF_UCHAR);
            if(buffer==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }
            fBufferIndex=1;
            fCapacity=capacity;
            fBufferOffset=0;
        } else {
            buffer=fLargeBuffer=(UChar *)uprv_malloc(fRemainingUChars*U_SIZEOF_UCHAR);
            if(buffer==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }
            fBufferIndex=2;
            fCapacity=fRemainingUChars;
            fBufferOffset=0;
        }
        value=(uint32_t)((fBufferIndex<<30)|fBufferOffset);
        UChar *p=buffer+fBufferOffset;
        fBufferOffset+=length;
        fCapacity-=length;
        fRemainingUChars-=length;
        return p;
    }

    int32_t fNumCompressedStrings;
    int32_t fNumDecompressedUChars;

    // Cache of decompressed strings.
    UChar fSmallBuffer[120];  // arbitrary capacity >= 1
    UChar *fMediumBuffer, *fLargeBuffer;
    int32_t fBufferIndex;  // 0/1/2 for small/medium/large
    int32_t fBufferOffset;  // start of the unused remainder of the largest allocated buffer
    int32_t fCapacity;  // at fBufferOffset in the largest allocated buffer
    int32_t fRemainingUChars;  // not yet decompressed

    // Hash table from resource item words to strings.
    // First word is a resource item, or 0 if unused.
    // Second word is an offset into one of the strings buffers.
    // The upper two bits indicate in which buffer the string is stored.
    HashEntry fMiniHashTable[10];
    HashEntry *fHashTable;
    int32_t fHashTableLength;
};
