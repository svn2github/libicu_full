/*
**********************************************************************
*   Copyright (C) 1998-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "cmemory.h"
#include "udatamem.h"
#include "udataswp.h"
#include "uspoofim.h"


U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SpoofImpl)

static const int SPOOF_MAGIC = 0x8345fdef; 
SpoofImpl::SpoofImpl(UErrorCode &status) {
	fMagic = SPOOF_MAGIC;
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
    if (This->fMagic != SPOOF_MAGIC) {
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
    if (This->fMagic != SPOOF_MAGIC) {
        return NULL;
    }
    return This;
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
    //  Check that the data header is for for break data.
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
    sectionStart  = ds->readUInt32(spoofDH->fStringLengthsOffset);
    sectionLength = ds->readUInt32(spoofDH->fStringLengthsSize) * 4;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fKeysTableOffset);
    sectionLength = ds->readUInt32(spoofDH->fKeyTableSize) * 4;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fStringIndexesOffset);
    sectionLength = ds->readUInt32(spoofDH->fStringIndexesSize) * 2;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fStringTableOffset);
    sectionLength = ds->readUInt32(spoofDH->fStringTableSize) * 2;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);


    // And, last, swap the header itself.
    //   The entire header consists of int32_t values.
    //
    ds->swapArray32(ds, inBytes, sizeof(SpoofDataHeader), outBytes, status);


    return totalSize;
}


