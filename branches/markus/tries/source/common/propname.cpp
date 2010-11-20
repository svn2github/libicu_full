/*
**********************************************************************
* Copyright (c) 2002-2010, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: October 30 2002
* Since: ICU 2.4
* 2010nov19 Markus Scherer  Rewrite for formatVersion 2.
**********************************************************************
*/
#include "propname.h"
#include "unicode/uchar.h"
#include "unicode/udata.h"
#include "umutex.h"
#include "cmemory.h"
#include "cstring.h"
#include "ucln_cmn.h"
#include "uarrsort.h"
#include "uinvchar.h"

#define INCLUDED_FROM_PROPNAME_CPP
#include "propname_data.h"

U_CDECL_BEGIN

/**
 * Get the next non-ignorable ASCII character from a property name
 * and lowercases it.
 * @return ((advance count for the name)<<8)|character
 */
static inline int32_t
getASCIIPropertyNameChar(const char *name) {
    int32_t i;
    char c;

    /* Ignore delimiters '-', '_', and ASCII White_Space */
    for(i=0;
        (c=name[i++])==0x2d || c==0x5f ||
        c==0x20 || (0x09<=c && c<=0x0d);
    ) {}

    if(c!=0) {
        return (i<<8)|(uint8_t)uprv_asciitolower((char)c);
    } else {
        return i<<8;
    }
}

/**
 * Get the next non-ignorable EBCDIC character from a property name
 * and lowercases it.
 * @return ((advance count for the name)<<8)|character
 */
static inline int32_t
getEBCDICPropertyNameChar(const char *name) {
    int32_t i;
    char c;

    /* Ignore delimiters '-', '_', and EBCDIC White_Space */
    for(i=0;
        (c=name[i++])==0x60 || c==0x6d ||
        c==0x40 || c==0x05 || c==0x15 || c==0x25 || c==0x0b || c==0x0c || c==0x0d;
    ) {}

    if(c!=0) {
        return (i<<8)|(uint8_t)uprv_ebcdictolower((char)c);
    } else {
        return i<<8;
    }
}

/**
 * Unicode property names and property value names are compared "loosely".
 *
 * UCD.html 4.0.1 says:
 *   For all property names, property value names, and for property values for
 *   Enumerated, Binary, or Catalog properties, use the following
 *   loose matching rule:
 *
 *   LM3. Ignore case, whitespace, underscore ('_'), and hyphens.
 *
 * This function does just that, for (char *) name strings.
 * It is almost identical to ucnv_compareNames() but also ignores
 * C0 White_Space characters (U+0009..U+000d, and U+0085 on EBCDIC).
 *
 * @internal
 */

U_CAPI int32_t U_EXPORT2
uprv_compareASCIIPropertyNames(const char *name1, const char *name2) {
    int32_t rc, r1, r2;

    for(;;) {
        r1=getASCIIPropertyNameChar(name1);
        r2=getASCIIPropertyNameChar(name2);

        /* If we reach the ends of both strings then they match */
        if(((r1|r2)&0xff)==0) {
            return 0;
        }

        /* Compare the lowercased characters */
        if(r1!=r2) {
            rc=(r1&0xff)-(r2&0xff);
            if(rc!=0) {
                return rc;
            }
        }

        name1+=r1>>8;
        name2+=r2>>8;
    }
}

U_CAPI int32_t U_EXPORT2
uprv_compareEBCDICPropertyNames(const char *name1, const char *name2) {
    int32_t rc, r1, r2;

    for(;;) {
        r1=getEBCDICPropertyNameChar(name1);
        r2=getEBCDICPropertyNameChar(name2);

        /* If we reach the ends of both strings then they match */
        if(((r1|r2)&0xff)==0) {
            return 0;
        }

        /* Compare the lowercased characters */
        if(r1!=r2) {
            rc=(r1&0xff)-(r2&0xff);
            if(rc!=0) {
                return rc;
            }
        }

        name1+=r1>>8;
        name2+=r2>>8;
    }
}

U_CDECL_END

U_NAMESPACE_BEGIN

int32_t PropNameData::findProperty(int32_t property) {
    int32_t i=1;  // valueMaps index, initially after numRanges
    for(int32_t numRanges=valueMaps[0]; numRanges>0; --numRanges) {
        // Read and skip the start and end of this range.
        int32_t start=valueMaps[i];
        int32_t end=valueMaps[i+1];
        i+=2;
        if(property<start) {
            break;
        }
        if(property<=end) {
            return i+(property-start)*2;
        }
        i+=((end-start)+1)*2;  // Skip all entries for this range.
    }
    return 0;
}

int32_t PropNameData::findPropertyValueNameGroup(int32_t valueMapIndex, int32_t value) {
    if(valueMapIndex==0) {
        return 0;  // The property does not have named values.
    }
    ++valueMapIndex;  // Skip the ByteTrie offset.
    int32_t numRanges=valueMaps[valueMapIndex++];
    if(numRanges<0x10) {
        // Ranges of values.
        for(; numRanges>0; --numRanges) {
            // Read and skip the start and end of this range.
            int32_t start=valueMaps[valueMapIndex];
            int32_t end=valueMaps[valueMapIndex+1];
            valueMapIndex+=2;
            if(value<start) {
                break;
            }
            if(value<=end) {
                return valueMaps[valueMapIndex+value-start];
            }
            valueMapIndex+=(end-start)+1;  // Skip all entries for this range.
        }
    } else {
        // List of values.
        int32_t valuesStart=valueMapIndex;
        int32_t nameGroupOffsetsStart=valueMapIndex+numRanges-0x10;
        do {
            int32_t v=valueMaps[valueMapIndex];
            if(value<v) {
                break;
            }
            if(value==v) {
                return valueMaps[nameGroupOffsetsStart+valueMapIndex-valuesStart];
            }
        } while(++valueMapIndex<nameGroupOffsetsStart);
    }
    return 0;
}

const char *PropNameData::getName(const char *nameGroup, int32_t nameIndex) {
    int32_t numNames=*nameGroup++;
    if(nameIndex<0 || numNames<=nameIndex || (nameIndex==0 && *nameGroup==0)) {
        return NULL;
    }
    // Skip nameIndex names.
    for(; nameIndex>0; --nameIndex) {
        nameGroup=uprv_strchr(nameGroup, 0)+1;
    }
    return nameGroup;
}

UBool PropNameData::containsName(ByteTrie &trie, const char *name) {
    if(name==NULL) {
        return FALSE;
    }
    char c;
    while((c=*name++)!=0) {
        c=uprv_invCharToLowercaseAscii(c);
        // Ignore delimiters '-', '_', and ASCII White_Space.
        if(c==0x2d || c==0x5f || c==0x20 || (0x09<=c && c<=0x0d)) {
            continue;
        }
        if(!trie.next((uint8_t)c)) {
            return FALSE;
        }
    }
    return trie.contains();
}

const char *PropNameData::getPropertyName(int32_t property, int32_t nameChoice) {
    int32_t valueMapIndex=findProperty(property);
    if(valueMapIndex==0) {
        return NULL;  // Not a known property.
    }
    return getName(nameGroups+valueMaps[valueMapIndex], nameChoice);
}

const char *PropNameData::getPropertyValueName(int32_t property, int32_t value, int32_t nameChoice) {
    int32_t valueMapIndex=findProperty(property);
    if(valueMapIndex==0) {
        return NULL;  // Not a known property.
    }
    int32_t nameGroupOffset=findPropertyValueNameGroup(valueMaps[valueMapIndex+1], value);
    if(nameGroupOffset==0) {
        return NULL;
    }
    return getName(nameGroups+nameGroupOffset, nameChoice);
}

int32_t PropNameData::getPropertyOrValueEnum(int32_t byteTrieOffset, const char *alias) {
    ByteTrie trie(byteTries+byteTrieOffset);
    if(containsName(trie, alias)) {
        return trie.getValue();
    } else {
        return UCHAR_INVALID_CODE;
    }
}

int32_t PropNameData::getPropertyEnum(const char *alias) {
    return getPropertyOrValueEnum(0, alias);
}

int32_t PropNameData::getPropertyValueEnum(int32_t property, const char *alias) {
    int32_t valueMapIndex=findProperty(property);
    if(valueMapIndex==0) {
        return UCHAR_INVALID_CODE;  // Not a known property.
    }
    valueMapIndex=valueMaps[valueMapIndex+1];
    if(valueMapIndex==0) {
        return UCHAR_INVALID_CODE;  // The property does not have named values.
    }
    // valueMapIndex is the start of the property's valueMap,
    // where the first word is the ByteTrie offset.
    return getPropertyOrValueEnum(valueMaps[valueMapIndex], alias);
}

U_NAMESPACE_END
U_NAMESPACE_USE  // TODO: extend to bottom of file

//----------------------------------------------------------------------
// Public API implementation

U_CAPI const char* U_EXPORT2
u_getPropertyName(UProperty property,
                  UPropertyNameChoice nameChoice) {
    return PropNameData::getPropertyName(property, nameChoice);
}

U_CAPI UProperty U_EXPORT2
u_getPropertyEnum(const char* alias) {
    return (UProperty)PropNameData::getPropertyEnum(alias);
}

U_CAPI const char* U_EXPORT2
u_getPropertyValueName(UProperty property,
                       int32_t value,
                       UPropertyNameChoice nameChoice) {
    return PropNameData::getPropertyValueName(property, value, nameChoice);
}

U_CAPI int32_t U_EXPORT2
u_getPropertyValueEnum(UProperty property,
                       const char* alias) {
    return PropNameData::getPropertyValueEnum(property, alias);
}

/* data swapping ------------------------------------------------------------ */

/*
 * Sub-structure-swappers use the temp array (which is as large as the
 * actual data) for intermediate storage,
 * as well as to indicate if a particular structure has been swapped already.
 * The temp array is initially reset to all 0.
 * pos is the byte offset of the sub-structure in the inBytes/outBytes/temp arrays.
 */

int32_t
EnumToOffset::swap(const UDataSwapper *ds,
                   const uint8_t *inBytes, int32_t length, uint8_t *outBytes,
                   uint8_t *temp, int32_t pos,
                   UErrorCode *pErrorCode) {
    const EnumToOffset *inMap;
    EnumToOffset *outMap, *tempMap;
    int32_t size;

    tempMap=(EnumToOffset *)(temp+pos);
    if(tempMap->enumStart!=0 || tempMap->enumLimit!=0) {
        /* this map was swapped already */
        size=tempMap->getSize();
        return size;
    }

    inMap=(const EnumToOffset *)(inBytes+pos);
    outMap=(EnumToOffset *)(outBytes+pos);

    tempMap->enumStart=udata_readInt32(ds, inMap->enumStart);
    tempMap->enumLimit=udata_readInt32(ds, inMap->enumLimit);
    size=tempMap->getSize();

    if(length>=0) {
        if(length<(pos+size)) {
            if(length<(int32_t)sizeof(PropertyAliases)) {
                udata_printError(ds, "upname_swap(EnumToOffset): too few bytes (%d after header)\n"
                                     "    for pnames.icu EnumToOffset{%d..%d} at %d\n",
                                 length, tempMap->enumStart, tempMap->enumLimit, pos);
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        }

        /* swap enumStart and enumLimit */
        ds->swapArray32(ds, inMap, 2*sizeof(EnumValue), outMap, pErrorCode);

        /* swap _offsetArray[] */
        ds->swapArray16(ds, inMap->getOffsetArray(), (tempMap->enumLimit-tempMap->enumStart)*sizeof(Offset),
                           outMap->getOffsetArray(), pErrorCode);
    }

    return size;
}

int32_t
NonContiguousEnumToOffset::swap(const UDataSwapper *ds,
                   const uint8_t *inBytes, int32_t length, uint8_t *outBytes,
                   uint8_t *temp, int32_t pos,
                   UErrorCode *pErrorCode) {
    const NonContiguousEnumToOffset *inMap;
    NonContiguousEnumToOffset *outMap, *tempMap;
    int32_t size;

    tempMap=(NonContiguousEnumToOffset *)(temp+pos);
    if(tempMap->count!=0) {
        /* this map was swapped already */
        size=tempMap->getSize();
        return size;
    }

    inMap=(const NonContiguousEnumToOffset *)(inBytes+pos);
    outMap=(NonContiguousEnumToOffset *)(outBytes+pos);

    tempMap->count=udata_readInt32(ds, inMap->count);
    size=tempMap->getSize();

    if(length>=0) {
        if(length<(pos+size)) {
            if(length<(int32_t)sizeof(PropertyAliases)) {
                udata_printError(ds, "upname_swap(NonContiguousEnumToOffset): too few bytes (%d after header)\n"
                                     "    for pnames.icu NonContiguousEnumToOffset[%d] at %d\n",
                                 length, tempMap->count, pos);
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        }

        /* swap count and _enumArray[] */
        length=(1+tempMap->count)*sizeof(EnumValue);
        ds->swapArray32(ds, inMap, length,
                           outMap, pErrorCode);

        /* swap _offsetArray[] */
        pos+=length;
        ds->swapArray16(ds, inBytes+pos, tempMap->count*sizeof(Offset),
                           outBytes+pos, pErrorCode);
    }

    return size;
}

struct NameAndIndex {
    Offset name, index;
};

U_CDECL_BEGIN
typedef int32_t U_CALLCONV PropNameCompareFn(const char *name1, const char *name2);

struct CompareContext {
    const char *chars;
    PropNameCompareFn *propCompare;
};

static int32_t U_CALLCONV
upname_compareRows(const void *context, const void *left, const void *right) {
    CompareContext *cmp=(CompareContext *)context;
    return cmp->propCompare(cmp->chars+((const NameAndIndex *)left)->name,
                            cmp->chars+((const NameAndIndex *)right)->name);
}
U_CDECL_END

int32_t
NameToEnum::swap(const UDataSwapper *ds,
                   const uint8_t *inBytes, int32_t length, uint8_t *outBytes,
                   uint8_t *temp, int32_t pos,
                   UErrorCode *pErrorCode) {
    const NameToEnum *inMap;
    NameToEnum *outMap, *tempMap;

    const EnumValue *inEnumArray;
    EnumValue *outEnumArray;

    const Offset *inNameArray;
    Offset *outNameArray;

    NameAndIndex *sortArray;
    CompareContext cmp;

    int32_t i, size, oldIndex;

    tempMap=(NameToEnum *)(temp+pos);
    if(tempMap->count!=0) {
        /* this map was swapped already */
        size=tempMap->getSize();
        return size;
    }

    inMap=(const NameToEnum *)(inBytes+pos);
    outMap=(NameToEnum *)(outBytes+pos);

    tempMap->count=udata_readInt32(ds, inMap->count);
    size=tempMap->getSize();

    if(length>=0) {
        if(length<(pos+size)) {
            if(length<(int32_t)sizeof(PropertyAliases)) {
                udata_printError(ds, "upname_swap(NameToEnum): too few bytes (%d after header)\n"
                                     "    for pnames.icu NameToEnum[%d] at %d\n",
                                 length, tempMap->count, pos);
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        }

        /* swap count */
        ds->swapArray32(ds, inMap, 4, outMap, pErrorCode);

        inEnumArray=inMap->getEnumArray();
        outEnumArray=outMap->getEnumArray();

        inNameArray=(const Offset *)(inEnumArray+tempMap->count);
        outNameArray=(Offset *)(outEnumArray+tempMap->count);

        if(ds->inCharset==ds->outCharset) {
            /* no need to sort, just swap the enum/name arrays */
            ds->swapArray32(ds, inEnumArray, tempMap->count*4, outEnumArray, pErrorCode);
            ds->swapArray16(ds, inNameArray, tempMap->count*2, outNameArray, pErrorCode);
            return size;
        }

        /*
         * The name and enum arrays are sorted by names and must be resorted
         * if inCharset!=outCharset.
         * We use the corresponding part of the temp array to sort an array
         * of pairs of name offsets and sorting indexes.
         * Then the sorting indexes are used to permutate-swap the name and enum arrays.
         *
         * The outBytes must already contain the swapped strings.
         */
        sortArray=(NameAndIndex *)tempMap->getEnumArray();
        for(i=0; i<tempMap->count; ++i) {
            sortArray[i].name=udata_readInt16(ds, inNameArray[i]);
            sortArray[i].index=(Offset)i;
        }

        /*
         * use a stable sort to avoid shuffling of equal strings,
         * which makes testing harder
         */
        cmp.chars=(const char *)outBytes;
        if (ds->outCharset==U_ASCII_FAMILY) {
            cmp.propCompare=uprv_compareASCIIPropertyNames;
        }
        else {
            cmp.propCompare=uprv_compareEBCDICPropertyNames;
        }
        uprv_sortArray(sortArray, tempMap->count, sizeof(NameAndIndex),
                       upname_compareRows, &cmp,
                       TRUE, pErrorCode);
        if(U_FAILURE(*pErrorCode)) {
            udata_printError(ds, "upname_swap(NameToEnum).uprv_sortArray(%d items) failed\n",
                             tempMap->count);
            return 0;
        }

        /* copy/swap/permutate _enumArray[] and _nameArray[] */
        if(inEnumArray!=outEnumArray) {
            for(i=0; i<tempMap->count; ++i) {
                oldIndex=sortArray[i].index;
                ds->swapArray32(ds, inEnumArray+oldIndex, 4, outEnumArray+i, pErrorCode);
                ds->swapArray16(ds, inNameArray+oldIndex, 2, outNameArray+i, pErrorCode);
            }
        } else {
            /*
             * in-place swapping: need to permutate into a temporary array
             * and then copy back to not destroy the data
             */
            EnumValue *tempEnumArray;
            Offset *oldIndexes;

            /* write name offsets directly from sortArray */
            for(i=0; i<tempMap->count; ++i) {
                ds->writeUInt16((uint16_t *)outNameArray+i, (uint16_t)sortArray[i].name);
            }

            /*
             * compress the oldIndexes into a separate array to make space for tempEnumArray
             * the tempMap _nameArray becomes oldIndexes[], getting the index
             *   values from the 2D sortArray[],
             * while sortArray=tempMap _enumArray[] becomes tempEnumArray[]
             * this saves us allocating more memory
             *
             * it works because sizeof(NameAndIndex)<=sizeof(EnumValue)
             * and because the nameArray[] can be used for oldIndexes[]
             */
            tempEnumArray=(EnumValue *)sortArray;
            oldIndexes=(Offset *)(sortArray+tempMap->count);

            /* copy sortArray[].index values into oldIndexes[] */
            for(i=0; i<tempMap->count; ++i) {
                oldIndexes[i]=sortArray[i].index;
            }

            /* permutate inEnumArray[] into tempEnumArray[] */
            for(i=0; i<tempMap->count; ++i) {
                ds->swapArray32(ds, inEnumArray+oldIndexes[i], 4, tempEnumArray+i, pErrorCode);
            }

            /* copy tempEnumArray[] to outEnumArray[] */
            uprv_memcpy(outEnumArray, tempEnumArray, tempMap->count*4);
        }
    }

    return size;
}

int32_t
PropertyAliases::swap(const UDataSwapper *ds,
                      const uint8_t *inBytes, int32_t length, uint8_t *outBytes,
                      UErrorCode *pErrorCode) {
    const PropertyAliases *inAliases;
    PropertyAliases *outAliases;
    PropertyAliases aliases;

    const ValueMap *inValueMaps;
    ValueMap *outValueMaps;
    ValueMap valueMap;

    int32_t i;

    inAliases=(const PropertyAliases *)inBytes;
    outAliases=(PropertyAliases *)outBytes;

    /* read the input PropertyAliases - all 16-bit values */
    for(i=0; i<(int32_t)sizeof(PropertyAliases)/2; ++i) {
        ((uint16_t *)&aliases)[i]=ds->readUInt16(((const uint16_t *)inBytes)[i]);
    }

    if(length>=0) {
        if(length<aliases.total_size) {
            udata_printError(ds, "upname_swap(): too few bytes (%d after header) for all of pnames.icu\n",
                             length);
            *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }

        /* copy the data for inaccessible bytes */
        if(inBytes!=outBytes) {
            uprv_memcpy(outBytes, inBytes, aliases.total_size);
        }

        /* swap the PropertyAliases class fields */
        ds->swapArray16(ds, inAliases, sizeof(PropertyAliases), outAliases, pErrorCode);

        /* swap the name groups */
        ds->swapArray16(ds, inBytes+aliases.nameGroupPool_offset,
                                aliases.stringPool_offset-aliases.nameGroupPool_offset,
                           outBytes+aliases.nameGroupPool_offset, pErrorCode);

        /* swap the strings */
        udata_swapInvStringBlock(ds, inBytes+aliases.stringPool_offset,
                                        aliases.total_size-aliases.stringPool_offset,
                                    outBytes+aliases.stringPool_offset, pErrorCode);

        /*
         * alloc uint8_t temp[total_size] and reset it
         * swap each top-level struct, put at least the count fields into temp
         *   use subclass-specific swap() functions
         * enumerate value maps, for each
         *   if temp does not have count!=0 yet
         *     read count, put it into temp
         *     swap the array(s)
         *     resort strings in name->enum maps
         * swap value maps
         */
        LocalMemory<uint8_t> temp;
        if(temp.allocateInsteadAndReset(aliases.total_size)==NULL) {
            udata_printError(ds, "upname_swap(): unable to allocate temp memory (%d bytes)\n",
                             aliases.total_size);
            *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
            return 0;
        }

        /* swap properties->name groups map */
        NonContiguousEnumToOffset::swap(ds, inBytes, length, outBytes,
                                        temp.getAlias(), aliases.enumToName_offset, pErrorCode);

        /* swap name->properties map */
        NameToEnum::swap(ds, inBytes, length, outBytes,
                         temp.getAlias(), aliases.nameToEnum_offset, pErrorCode);

        /* swap properties->value maps map */
        NonContiguousEnumToOffset::swap(ds, inBytes, length, outBytes,
                                        temp.getAlias(), aliases.enumToValue_offset, pErrorCode);

        /* enumerate all ValueMaps and swap them */
        inValueMaps=(const ValueMap *)(inBytes+aliases.valueMap_offset);
        outValueMaps=(ValueMap *)(outBytes+aliases.valueMap_offset);

        for(i=0; i<aliases.valueMap_count; ++i) {
            valueMap.enumToName_offset=udata_readInt16(ds, inValueMaps[i].enumToName_offset);
            valueMap.ncEnumToName_offset=udata_readInt16(ds, inValueMaps[i].ncEnumToName_offset);
            valueMap.nameToEnum_offset=udata_readInt16(ds, inValueMaps[i].nameToEnum_offset);

            if(valueMap.enumToName_offset!=0) {
                EnumToOffset::swap(ds, inBytes, length, outBytes,
                                   temp.getAlias(), valueMap.enumToName_offset,
                                   pErrorCode);
            } else if(valueMap.ncEnumToName_offset!=0) {
                NonContiguousEnumToOffset::swap(ds, inBytes, length, outBytes,
                                                temp.getAlias(), valueMap.ncEnumToName_offset,
                                                pErrorCode);
            }
            if(valueMap.nameToEnum_offset!=0) {
                NameToEnum::swap(ds, inBytes, length, outBytes,
                                 temp.getAlias(), valueMap.nameToEnum_offset,
                                 pErrorCode);
            }
        }

        /* swap the ValueMaps array itself */
        ds->swapArray16(ds, inValueMaps, aliases.valueMap_count*sizeof(ValueMap),
                           outValueMaps, pErrorCode);

        /* name groups and strings were swapped above */
    }

    return aliases.total_size;
}

U_CAPI int32_t U_EXPORT2
upname_swap(const UDataSwapper *ds,
            const void *inData, int32_t length, void *outData,
            UErrorCode *pErrorCode) {
    const UDataInfo *pInfo;
    int32_t headerSize;

    const uint8_t *inBytes;
    uint8_t *outBytes;

    /* udata_swapDataHeader checks the arguments */
    headerSize=udata_swapDataHeader(ds, inData, length, outData, pErrorCode);
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return 0;
    }

    /* check data format and format version */
    pInfo=(const UDataInfo *)((const char *)inData+4);
    if(!(
        pInfo->dataFormat[0]==0x70 &&   /* dataFormat="pnam" */
        pInfo->dataFormat[1]==0x6e &&
        pInfo->dataFormat[2]==0x61 &&
        pInfo->dataFormat[3]==0x6d &&
        pInfo->formatVersion[0]==1
    )) {
        udata_printError(ds, "upname_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as pnames.icu\n",
                         pInfo->dataFormat[0], pInfo->dataFormat[1],
                         pInfo->dataFormat[2], pInfo->dataFormat[3],
                         pInfo->formatVersion[0]);
        *pErrorCode=U_UNSUPPORTED_ERROR;
        return 0;
    }

    inBytes=(const uint8_t *)inData+headerSize;
    outBytes=(uint8_t *)outData+headerSize;

    if(length>=0) {
        length-=headerSize;
        if(length<(int32_t)sizeof(PropertyAliases)) {
            udata_printError(ds, "upname_swap(): too few bytes (%d after header) for pnames.icu\n",
                             length);
            *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }
    }

    return headerSize+PropertyAliases::swap(ds, inBytes, length, outBytes, pErrorCode);
}

//eof
