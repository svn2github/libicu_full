/*
**********************************************************************
*   Copyright (C) 2008-2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "cmemory.h"
#include "udatamem.h"
#include "umutex.h"
#include "udataswp.h"
#include "uassert.h"
#include "uspoof_impl.h"


U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SpoofImpl)

SpoofImpl::SpoofImpl(SpoofData *data, UErrorCode &/*status*/) {
	fMagic = USPOOF_MAGIC;
	fSpoofData = data;
	fChecks = 0;
}

SpoofImpl::SpoofImpl() {
}

SpoofImpl::~SpoofImpl() {
	fMagic = 0;   // head off application errors by preventing use of
	              //    of deleted objects.
}

//
//  Incoming parameter check on Status and the SpoofChecker object
//    received from the C API.
//
SpoofImpl *SpoofImpl::validateThis(USpoofChecker *sc, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (sc == NULL) {
        return NULL;
    };
    SpoofImpl *This = (SpoofImpl *)sc;
    if (This->fMagic != USPOOF_MAGIC) {
        return NULL;
    }
    return This;
}

const SpoofImpl *SpoofImpl::validateThis(const USpoofChecker *sc, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (sc == NULL) {
        return NULL;
    };
    const SpoofImpl *This = (const SpoofImpl *)sc;
    if (This->fMagic != USPOOF_MAGIC) {
        return NULL;
    }
    return This;
}


int32_t SpoofImpl::confusableLookup(UChar32 inChar, int32_t tableMask, UChar *destBuf) const {

    // Binary search the spoof data key table for the inChar
    int32_t  *low   = fSpoofData->fCFUKeys;
    int32_t  *mid   = NULL;
    int32_t  *limit = low + fSpoofData->fRawData->fCFUKeysSize;
    UChar     midc;
    do {
        int32_t delta = (limit-low)/2;
        mid = low + delta;
        midc = *mid & 0x1fffff;
        if (inChar == midc) {
            goto foundChar;
        } else if (inChar < midc) {
            limit = mid;
        } else {
            low = mid;
        }
    } while (low < limit-1);
    mid = low;
    midc = *mid & 0x1fffff;
    if (inChar != midc) {
        // Char not found.  It maps to itself.
        int i = 0;
        U16_APPEND_UNSAFE(destBuf, i, inChar)
        return i;
    } 
  foundChar:
    int32_t keyFlags = *mid & 0xff000000;
    if ((keyFlags & tableMask) == 0) {
        // We found the right key char, but the entry doesn't pertain to the
        //  table we need.  See if there is an adjacent key that does
        if (keyFlags & USPOOF_KEY_MULTIPLE_VALUES) {
            int32_t *altMid;
            for (altMid = mid-1; (*altMid&0x00ffffff) == inChar; altMid--) {
                keyFlags = *altMid & 0xff000000;
                if (keyFlags & tableMask) {
                    mid = altMid;
                    goto foundKey;
                }
            }
            for (altMid = mid+1; (*altMid&0x00ffffff) == inChar; altMid++) {
                keyFlags = *altMid & 0xff000000;
                if (keyFlags & tableMask) {
                    mid = altMid;
                    goto foundKey;
                }
            }
        }
        // No key entry for this char & table.
        // The input char maps to itself.
        int i = 0;
        U16_APPEND_UNSAFE(destBuf, i, inChar)
        return i;
    }

  foundKey:
    int32_t  stringLen = USPOOF_KEY_LENGTH_FIELD(keyFlags) + 1;
    int32_t keyTableIndex = mid - fSpoofData->fCFUKeys;

    // Value is either a UChar  (for strings of length 1) or
    //                 an index into the string table (for longer strings)
    uint16_t value = fSpoofData->fCFUValues[keyTableIndex];
    if (stringLen == 1) {
        destBuf[0] = value;
        return 1;
    }

    // String length of 4 is used for all strings of length >= 4.
    // Get the real length from the string lengths table,
    //   which maps string table indexes to lengths.
    //   (All strings of the same length are stored contiguously in the string table)

    if (stringLen == 4) {
        // TODO:
    }

    UChar *src = &fSpoofData->fCFUStrings[value];
    for (int32_t idx=0; idx<stringLen; idx++) {
        destBuf[idx] = src[idx];
    }
    return stringLen;
}


// Convert a text format hex number.  Utility function used by builder code.  Static.
// Input: UChar *string text.  Output: a UChar32
// Input has been pre-checked, and will have no non-hex chars.
// The number must fall in the code point range of 0..0x10ffff
// Static Function.
UChar32 SpoofImpl::ScanHex(const UChar *s, int32_t start, int32_t limit, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    U_ASSERT(limit-start > 0);
    uint32_t val = 0;
    int i;
    for (i=start; i<limit; i++) {
        int digitVal = s[i] - 0x30;
        if (digitVal>9) {
            digitVal = 0xa + (s[i] - 0x41);  // Upper Case 'A'
        }
        if (digitVal>15) {
            digitVal = 0xa + (s[i] - 0x61);  // Lower Case 'a'
        }
        U_ASSERT(digitVal <= 0xf);
        val <<= 4;
        val += digitVal;
    }
    if (val > 0x10ffff) {
        status = U_PARSE_ERROR;
        val = 0;
    }
    return (UChar32)val;
}



//
//  poofData::getDefault() - return a wrapper around the spoof data that is
//                           baked into the default ICU data.
//
SpoofData *SpoofData::getDefault(UErrorCode &/*status*/) {
    // TODO:
    return NULL;
}


// Spoof Data constructor for use from data builder.
//   Initializes a new, empty data area that will be populated later.
SpoofData::SpoofData(UErrorCode &status) {
    fRawData = NULL;
    fDataOwned = true;
    fRefCount = 1;

    if (U_FAILURE(status)) {
        return;
    }

    // The spoof header should already be sized to be a multiple of 16 bytes.
    // Just in case it's not, round it up.
    uint32_t initialSize = (sizeof(SpoofDataHeader) + 15) & ~15;
    U_ASSERT(initialSize == sizeof(SpoofDataHeader));
    
    fRawData = static_cast<SpoofDataHeader *>(uprv_malloc(initialSize));
    fMemLimit = initialSize;
    if (fRawData == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uprv_memset(fRawData, 0, initialSize);

    fRawData->fMagic = USPOOF_MAGIC;
    initPtrs();
}

//  initPtrs  Initialize the pointers to the various sections of the raw data.
//            This operation may need to be repeated during the data build process
//            when adding to the data causes a realloc(), which then moves everything.
//
//            The pointers for non-existent data sections (identified by an offset of 0)
//            are set to NULL.
//
void SpoofData::initPtrs() {
    fCFUKeys = NULL;
    fCFUValues = NULL;
    fCFUStringLengths = NULL;
    fCFUStrings = NULL;
    if (fRawData->fCFUKeys != 0) {
        fCFUKeys = (int32_t *)((char *)fRawData + fRawData->fCFUKeys);
    }
    if (fRawData->fCFUStringIndex != 0) {
        fCFUValues = (uint16_t *)((char *)fRawData + fRawData->fCFUStringIndex);
    }
    if (fRawData->fCFUStringLengths != 0) {
        fCFUStringLengths = (SpoofStringLengthsElement *)((char *)fRawData + fRawData->fCFUStringLengths);
    }
    if (fRawData->fCFUStringTable != 0) {
        fCFUStrings = (UChar *)((char *)fRawData + fRawData->fCFUStringTable);
    }
    
    // NOTE:  do not fix up the pointers to the Tries for Whole Script Confusables.
    //        These will point to the actual deserialized trie objects,
    //        not to the serialized trie2 data.
    if (fRawData->fScriptSets != 0) {
        fScriptSets = (ScriptSet *)((char *)fRawData + fRawData->fCFUStringTable);
    }
}


SpoofData::~SpoofData() {
    if (fDataOwned) {
        uprv_free(fRawData);
    }
    fRawData = NULL;
}


void SpoofData::removeReference() {
    if (umtx_atomic_dec(&fRefCount) == 0) {
        delete this;
    }
}


void *SpoofData::reserveSpace(int32_t numBytes,  UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (!fDataOwned) {
        U_ASSERT(FALSE);
        status = U_INTERNAL_PROGRAM_ERROR;
        return NULL;
    }

    numBytes = (numBytes + 15) & ~15;   // Round up to a multiple of 16
    uint32_t returnOffset = fMemLimit;
    fMemLimit += numBytes;
    fRawData = static_cast<SpoofDataHeader *>(uprv_realloc(fRawData, fMemLimit));
    initPtrs();
    return (char *)fRawData + returnOffset;
}


//----------------------------------------------------------------------------
//
//  ScriptSet implementation
//
//----------------------------------------------------------------------------
ScriptSet::ScriptSet() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
    }
}

ScriptSet::~ScriptSet() {
}

UBool ScriptSet::operator == (const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        if (bits[i] != other.bits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

void ScriptSet::add(UScriptCode script) {
    uint32_t index = script / 32;
    uint32_t bit   = 1 << (script & 31);
    U_ASSERT(index < sizeof(bits)*4);
    bits[index] |= bit;
}


void ScriptSet::Union(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] |= other.bits[i];
    }
}

void ScriptSet::intersect(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] &= other.bits[i];
    }
}

ScriptSet & ScriptSet::operator =(const ScriptSet &other) {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = other.bits[i];
    }
    return *this;
}


void ScriptSet::setAll() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 1;
    }
}


void ScriptSet::resetAll() {
    for (uint32_t i=0; i<sizeof(bits)/sizeof(uint32_t); i++) {
        bits[i] = 0;
    }
}



U_NAMESPACE_END

U_NAMESPACE_USE

//-----------------------------------------------------------------------------
//
//  uspoof_swap   -  byte swap and char encoding swap of spoof data
//
//-----------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uspoof_swap(const UDataSwapper *ds, const void *inData, int32_t length, void *outData,
           UErrorCode *status) {

    if (status == NULL || U_FAILURE(*status)) {
        return 0;
    }
    if(ds==NULL || inData==NULL || length<-1 || (length>0 && outData==NULL)) {
        *status=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    //
    //  Check that the data header is for spoof data.
    //    (Header contents are defined in genbrk.cpp)
    //
    const UDataInfo *pInfo = (const UDataInfo *)((const char *)inData+4);
    if(!(  pInfo->dataFormat[0]==0x63 &&   /* dataFormat="cfu " */
           pInfo->dataFormat[1]==0x66 &&
           pInfo->dataFormat[2]==0x6b &&
           pInfo->dataFormat[3]==0x20 &&
           pInfo->formatVersion[0]==1  )) {
        udata_printError(ds, "uspoof_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized\n",
                         pInfo->dataFormat[0], pInfo->dataFormat[1],
                         pInfo->dataFormat[2], pInfo->dataFormat[3],
                         pInfo->formatVersion[0]);
        *status=U_UNSUPPORTED_ERROR;
        return 0;
    }

    //
    // Swap the data header.  (This is the generic ICU Data Header, not the uspoof Specific
    //                         header).  This swap also conveniently gets us
    //                         the size of the ICU d.h., which lets us locate the start
    //                         of the uspoof specific data.
    //
    int32_t headerSize=udata_swapDataHeader(ds, inData, length, outData, status);


    //
    // Get the Spoof Data Header, and check that it appears to be OK.
    //
    //
    const uint8_t   *inBytes =(const uint8_t *)inData+headerSize;
    SpoofDataHeader *spoofDH = (SpoofDataHeader *)inBytes;
    if (ds->readUInt32(spoofDH->fMagic)   != 0x5b0f ||
        ds->readUInt32(spoofDH->fLength)  <  sizeof(SpoofDataHeader)) 
    {
        udata_printError(ds, "uspoof_swap(): Spoof Data header is invalid.\n");
        *status=U_UNSUPPORTED_ERROR;
        return 0;
    }

    //
    // Prefight operation?  Just return the size
    //
    int32_t spoofDataLength = ds->readUInt32(spoofDH->fLength);
    int32_t totalSize = headerSize + spoofDataLength;
    if (length < 0) {
        return totalSize;
    }

    //
    // Check that length passed in is consistent with length from RBBI data header.
    //
    if (length < totalSize) {
        udata_printError(ds, "uspoof_swap(): too few bytes (%d after ICU Data header) for spoof data.\n",
                            spoofDataLength);
        *status=U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
        }


    //
    // Swap the Data.  Do the data itself first, then the Spoof Data Header, because
    //                 we need to reference the header to locate the data, and an
    //                 inplace swap of the header leaves it unusable.
    //
    uint8_t          *outBytes = (uint8_t *)outData + headerSize;
    SpoofDataHeader  *outputDH = (SpoofDataHeader *)outBytes;

    int32_t   sectionStart;
    int32_t   sectionLength;

    //
    // If not swapping in place, zero out the output buffer before starting.
    //    Gaps may exist between the individual sections, and these must be zeroed in
    //    the output buffer.  The simplest way to do that is to just zero the whole thing.
    //
    if (inBytes != outBytes) {
        uprv_memset(outBytes, 0, spoofDataLength);
    }

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUStringLengths);
    sectionLength = ds->readUInt32(spoofDH->fCFUStringLengthsSize) * 4;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUKeys);
    sectionLength = ds->readUInt32(spoofDH->fCFUKeysSize) * 4;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUStringIndex);
    sectionLength = ds->readUInt32(spoofDH->fCFUStringIndexSize) * 2;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUStringTable);
    sectionLength = ds->readUInt32(spoofDH->fCFUStringTableLen) * 2;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);


    // And, last, swap the header itself.
    //   The entire header consists of int32_t values.
    //
    ds->swapArray32(ds, inBytes, sizeof(SpoofDataHeader), outBytes, status);


    return totalSize;
}


