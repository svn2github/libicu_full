/*
*******************************************************************************
*
*   Copyright (C) 1998-2000, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* Private implementation header for C collation
*   file name:  ucolimp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000dec11
*   created by: Vladimir Weinstein
*/

#ifndef UCOL_IMP_H
#define UCOL_IMP_H

#include "unicode/ucol.h"
#include "ucmp32.h"

/* This is the size of the stack allocated buffer for sortkey generation and similar operations */
/* if it is too small, heap allocation will occur.*/
/* You can expect stack usage in following multiples */
/* getSortkey 6*UCOL_MAX_BUFFER */
/* strcoll 26 - 32 */
/* strcollInc 14 - 46 */
/* you can change this value if you need memory - it will affect the performance, though, since we're going to malloc */
#define UCOL_MAX_BUFFER 256

#define UCOL_NORMALIZATION_GROWTH 2

/* This writable buffer is used if we encounter Thai and need to reorder the string on the fly */
/* Sometimes we already have a writable buffer (like in case of normalized strings). */
/* you can change this value if you need memory - it will affect the performance, though, since we're going to malloc */
#define UCOL_WRITABLE_BUFFER_SIZE 256

/* This is the size of the buffer for expansion CE's */
/* In reality we should not have to deal with expm sequences longer then 16 */
/* you can change this value if you need memory */
/* WARNING THIS BUFFER DOES NOT HAVE MALLOC FALLBACK. If you make it too small, you'll get in trouble */
/* Reasonable small value is around 10, if you don't do Arabic or other funky collations that have long expansion sequence */
/* This is the longest expansion sequence we can handle without bombing out */
#define UCOL_EXPAND_CE_BUFFER_SIZE 64

struct collIterate {
  UChar *string; /* Original string */
  UChar *len;   /* Original string length */
  UChar *pos; /* This is position in the string */
  uint32_t *toReturn; /* This is the CE from CEs buffer that should be returned */
  uint32_t *CEpos; /* This is the position to which we have stored processed CEs */
  uint32_t CEs[UCOL_EXPAND_CE_BUFFER_SIZE]; /* This is where we store CEs */
  UBool isThai; /* Have we already encountered a Thai prevowel */
  UBool isWritable; /* is the source buffer writable? */
  UChar stackWritableBuffer[UCOL_WRITABLE_BUFFER_SIZE]; /* A writable buffer. */
  UChar *writableBuffer;
};

struct incrementalContext {
    UCharForwardIterator *source; 
    void *sourceContext;
    UChar currentChar;
    UChar lastChar;
    UChar stackString[UCOL_MAX_BUFFER]; /* Original string */
    UChar *stringP; /* This is position in the string */
    UChar *len;   /* Original string length */
    UChar *capacity; /* This is capacity */
    uint32_t *toReturn; /* This is the CE from CEs buffer that should be returned */
    uint32_t *CEpos; /* This is the position to which we have stored processed CEs */
    uint32_t CEs[UCOL_EXPAND_CE_BUFFER_SIZE]; /* This is where we store CEs */
    UBool panic; /* can't handle it any more - we have to call the cavalry */
};

/* Fixup table a la Markus */
/* see http://www.ibm.com/software/developer/library/utf16.html for further explanation */
static uint8_t utf16fixup[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0x20, 0xf8, 0xf8, 0xf8, 0xf8
};

/* from coleiterator */
#define UCOL_UNMAPPEDCHARVALUE 0x7fff0000     

#define UCOL_LEVELTERMINATOR 1
/* need look up in .commit() */
#define UCOL_CHARINDEX 0x70000000             
/* Expand index follows */
#define UCOL_EXPANDCHARINDEX 0x7E000000       
/* contract indexes follows */
#define UCOL_CONTRACTCHARINDEX 0x7F000000     
/* unmapped character values */
#define UCOL_UNMAPPED 0xFFFFFFFF              
/* primary strength increment */
#define UCOL_PRIMARYORDERINCREMENT 0x00010000 
/* secondary strength increment */
#define UCOL_SECONDARYORDERINCREMENT 0x00000100 
/* tertiary strength increment */
#define UCOL_TERTIARYORDERINCREMENT 0x00000001 
/* maximum ignorable char order value */
#define UCOL_MAXIGNORABLE 0x00010000          
/* mask off anything but primary order */
#define UCOL_PRIMARYORDERMASK 0xffff0000      
/* mask off anything but secondary order */
#define UCOL_SECONDARYORDERMASK 0x0000ff00    
/* mask off anything but tertiary order */
#define UCOL_TERTIARYORDERMASK 0x000000ff     
/* mask off secondary and tertiary order */
#define UCOL_SECONDARYRESETMASK 0x0000ffff    
/* mask off ignorable char order */
#define UCOL_IGNORABLEMASK 0x0000ffff         
/* use only the primary difference */
#define UCOL_PRIMARYDIFFERENCEONLY 0xffff0000 
/* use only the primary and secondary difference */
#define UCOL_SECONDARYDIFFERENCEONLY 0xffffff00  
/* primary order shift */
#define UCOL_PRIMARYORDERSHIFT 16             
/* secondary order shift */
#define UCOL_SECONDARYORDERSHIFT 8            
/* minimum sort key offset */
#define UCOL_SORTKEYOFFSET 2                  
/* Indicates the char is a contract char */
#define UCOL_CONTRACTCHAROVERFLOW 0x7FFFFFFF  


/* starting value for collation elements */
#define UCOL_COLELEMENTSTART 0x02020202       
/* testing mask for primary low element */
#define UCOL_PRIMARYLOWZEROMASK 0x00FF0000    
/* reseting value for secondaries and tertiaries */
#define UCOL_RESETSECONDARYTERTIARY 0x00000202
/* reseting value for tertiaries */
#define UCOL_RESETTERTIARY 0x00000002         

#define UCOL_IGNORABLE 0x02020202
#define UCOL_PRIMIGNORABLE 0x0202
#define UCOL_SECIGNORABLE 0x02
#define UCOL_TERIGNORABLE 0x02

/* get weights from a CE */
#define UCOL_PRIMARYORDER(order) (((order) & UCOL_PRIMARYORDERMASK)>> UCOL_PRIMARYORDERSHIFT)
#define UCOL_SECONDARYORDER(order) (((order) & UCOL_SECONDARYORDERMASK)>> UCOL_SECONDARYORDERSHIFT)
#define UCOL_TERTIARYORDER(order) ((order) & UCOL_TERTIARYORDERMASK)

/**
 * Determine if a character is a Thai vowel (which sorts after
 * its base consonant).
 */
#define UCOL_ISTHAIPREVOWEL(ch) ((uint32_t)(ch) - 0xe40) <= (0xe44 - 0xe40)

/**
 * Determine if a character is a Thai base consonant
 */
#define UCOL_ISTHAIBASECONSONANT(ch) ((uint32_t)(ch) - 0xe01) <= (0xe2e - 0xe01)

/* initializes collIterate structure */
/* made as macro to speed up things */
#define init_collIterate(sourceString, sourceLen, s, isSourceWritable) { \
    (s)->string = (s)->pos = (UChar *)(sourceString); \
    (s)->len = (UChar *)(sourceString)+(sourceLen); \
    (s)->CEpos = (s)->toReturn = (s)->CEs; \
	(s)->isThai = TRUE; \
	(s)->isWritable = (isSourceWritable); \
	(s)->writableBuffer = (s)->stackWritableBuffer; \
}

/* a macro that gets a simple CE */
/* for more complicated CEs it resorts to getComplicatedCE (what else) */
#define UCOL_GETNEXTCE(order, coll, collationSource, status) { \
  if (U_FAILURE((status)) || ((collationSource).pos>=(collationSource).len \
      && (collationSource).CEpos <= (collationSource).toReturn)) { \
    (order) = UCOL_NULLORDER; \
  } else if ((collationSource).CEpos > (collationSource).toReturn) { \
    (order) = *((collationSource).toReturn++); \
  } else {\
    (collationSource).CEpos = (collationSource).toReturn = (collationSource).CEs; \
    *((collationSource).CEpos)  = ucmp32_get(((RuleBasedCollator *)(coll))->data->mapping, *((collationSource).pos)); \
    if(*((collationSource).CEpos) < UCOL_EXPANDCHARINDEX && !(UCOL_ISTHAIPREVOWEL(*((collationSource).pos)))) { \
      (collationSource).pos++; \
      (order)=(*((collationSource).CEpos)); \
    } else { \
	  (order) = getComplicatedCE((coll), &(collationSource), &(status)); \
    } \
  } \
}


int32_t getComplicatedCE(const UCollator *coll, collIterate *source, UErrorCode *status);
void incctx_cleanUpContext(incrementalContext *ctx);
UChar incctx_appendChar(incrementalContext *ctx, UChar c);

/* function used by C++ getCollationKey to prevent restarting the calculation */
U_CFUNC uint8_t *ucol_getSortKeyWithAllocation(const UCollator *coll, 
        const    UChar        *source,
        int32_t            sourceLength, int32_t *resultLen);

/* get some memory */
void *ucol_getABuffer(const UCollator *coll, uint32_t size);

/* worker function for generating sortkeys */
int32_t
ucol_calcSortKey(const    UCollator    *coll,
        const    UChar        *source,
        int32_t        sourceLength,
        uint8_t        **result,
        int32_t        resultLength,
        UBool allocatePrimary);

/**
 * Makes a copy of the Collator's rule data. The format is
 * that of .col files.
 * 
 * @param length returns the length of the data, in bytes.
 * @param status the error status
 * @return memory, owned by the caller, of size 'length' bytes.
 * @internal INTERNAL USE ONLY
 */
U_CAPI uint8_t *
ucol_cloneRuleData(UCollator *coll, int32_t *length, UErrorCode *status);

#define UCOL_SPECIAL_FLAG 0xF0000000
#define UCOL_TAG_SHIFT 24
#define UCOL_TAG_MASK 0x0F000000
#define INIT_EXP_TABLE_SIZE 1024
#define UCOL_NOT_FOUND 0xF0000000
#define UCOL_EXPANSION 0xF1000000
#define UCOL_CONTRACTION 0xF2000000
#define UCOL_THAI 0xF3000000

#define isSpecial(CE) ((((CE)&UCOL_SPECIAL_FLAG)>>28)==0xF)
#define getCETag(CE) (((CE)&UCOL_TAG_MASK)>>UCOL_TAG_SHIFT)
#define constructContractCE(CE) (UCOL_SPECIAL_FLAG | (CONTRACTION_TAG<<UCOL_TAG_SHIFT) | ((CE))&0xFFFFFF)
#define getContractOffset(CE) ((CE)&0xFFFFFF)

#define UCA_DATA_TYPE "dat"
#define UCA_DATA_NAME "UCATable"

typedef struct {
      int32_t size;
      /* all the offsets are in bytes */
      /* to get the address add to the header address and cast properly */
      uint32_t CEindex;          /* uint16_t *CEindex;              */
      uint32_t CEvalues;         /* int32_t *CEvalues;              */
      uint32_t mappingPosition;  /* const uint8_t *mappingPosition; */
      uint32_t expansion;        /* uint32_t *expansion;            */
      uint32_t contractionIndex; /* UChar *contractionIndex;        */
      uint32_t contractionCEs;   /* uint32_t *contractionCEs;       */
      uint32_t latinOneMapping;  /* fast track to latin1 chars      */
      CompactIntArray *mapping;
      int32_t CEcount;
      UChar variableTopValue;
      UVersionInfo version;
      UColAttributeValue frenchCollation;
      UColAttributeValue alternateHandling; /* attribute for handling variable elements*/
      UColAttributeValue caseFirst;         /* who goes first, lower case or uppercase */
      UColAttributeValue caseLevel;         /* do we have an extra case level */
      UColAttributeValue normalizationMode; /* attribute for normalization */
      UColAttributeValue strength;          /* attribute for strength */
} UCATableHeader;

struct UCollatorNew {
    const UCATableHeader *image;
    CompactIntArray *mapping; 
    const uint32_t *latinOneMapping;
    const uint32_t *expansion;            
    const UChar *contractionIndex;        
    const uint32_t *contractionCEs;       
    UChar variableTopValue;
    UColAttributeValue frenchCollation;
    UColAttributeValue alternateHandling; /* attribute for handling variable elements*/
    UColAttributeValue caseFirst;         /* who goes first, lower case or uppercase */
    UColAttributeValue caseLevel;         /* do we have an extra case level */
    UColAttributeValue normalizationMode; /* attribute for normalization */
    UColAttributeValue strength;          /* attribute for strength */
    UChar variableTopValueDefault;
    UColAttributeValue frenchCollationDefault;
    UColAttributeValue alternateHandlingDefault; /* attribute for handling variable elements*/
    UColAttributeValue caseFirstDefault;         /* who goes first, lower case or uppercase */
    UColAttributeValue caseLevelDefault;         /* do we have an extra case level */
    UColAttributeValue normalizationModeDefault; /* attribute for normalization */
    UColAttributeValue strengthDefault;          /* attribute for strength */
};

#endif

