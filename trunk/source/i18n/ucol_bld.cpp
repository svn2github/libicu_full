/*
*******************************************************************************
*
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucol_tok.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created 02/22/2001
*   created by: Vladimir Weinstein
*
* This module builds a collator based on the rule set.
* 
*/

#include "ucol_bld.h" 

static const UChar *rulesToParse = 0;
static const InverseTableHeader* invUCA = NULL;

static UBool U_CALLCONV
isAcceptableInvUCA(void * /*context*/, 
             const char * /*type*/, const char * /*name*/,
             const UDataInfo *pInfo){
  /* context, type & name are intentionally not used */
    if( pInfo->size>=20 &&
        pInfo->isBigEndian==U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily==U_CHARSET_FAMILY &&
        pInfo->dataFormat[0]==0x49 &&   /* dataFormat="InvC" */
        pInfo->dataFormat[1]==0x6e &&
        pInfo->dataFormat[2]==0x76 &&
        pInfo->dataFormat[3]==0x43 &&
        pInfo->formatVersion[0]==1 &&
        pInfo->dataVersion[0]==3 &&
        pInfo->dataVersion[1]==0 &&
        pInfo->dataVersion[2]==0 &&
        pInfo->dataVersion[3]==0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

int32_t ucol_inv_findCE(uint32_t CE, uint32_t SecondCE) {
  uint32_t bottom = 0, top = invUCA->tableSize;
  uint32_t i = 0;
  uint32_t first = 0, second = 0;
  uint32_t *CETable = (uint32_t *)((uint8_t *)invUCA+invUCA->table);

  while(bottom < top-1) {
    i = (top+bottom)/2;
    first = *(CETable+3*i);
    second = *(CETable+3*i+1);
    if(first > CE) {
      top = i;
    } else if(first < CE) {
      bottom = i;
    } else {
        if(second > SecondCE) {
          top = i;
        } else if(second < SecondCE) {
          bottom = i;
        } else {
          break;
        }
    }
  }

  if((first == CE && second == SecondCE)) {
    return i;
  } else {
    return -1;
  }
}

static uint32_t strengthMask[UCOL_CE_STRENGTH_LIMIT] = {
  0xFFFF0000,
  0xFFFFFF00,
  0xFFFFFFFF
};

U_CAPI int32_t U_EXPORT2 ucol_inv_getNextCE(uint32_t CE, uint32_t contCE, 
                                            uint32_t *nextCE, uint32_t *nextContCE, 
                                            uint32_t strength) {
  uint32_t *CETable = (uint32_t *)((uint8_t *)invUCA+invUCA->table);
  int32_t iCE;

  iCE = ucol_inv_findCE(CE, contCE);

  if(iCE<0) {
    *nextCE = UCOL_NOT_FOUND;
    return -1;
  }

  CE &= strengthMask[strength];
  contCE &= strengthMask[strength];

  *nextCE = CE;
  *nextContCE = contCE;

  while((*nextCE  & strengthMask[strength]) == CE 
    && (*nextContCE  & strengthMask[strength]) == contCE) {
    *nextCE = (*(CETable+3*(++iCE)));
    *nextContCE = (*(CETable+3*(iCE)+1));
  }

  return iCE;
}

U_CAPI int32_t U_EXPORT2 ucol_inv_getPrevCE(uint32_t CE, uint32_t contCE, 
                                            uint32_t *prevCE, uint32_t *prevContCE, 
                                            uint32_t strength) {
  uint32_t *CETable = (uint32_t *)((uint8_t *)invUCA+invUCA->table);
  int32_t iCE;

  iCE = ucol_inv_findCE(CE, contCE);

  if(iCE<0) {
    *prevCE = UCOL_NOT_FOUND;
    return -1;
  }

  CE &= strengthMask[strength];
  contCE &= strengthMask[strength];

  *prevCE = CE;
  *prevContCE = contCE;

  while((*prevCE  & strengthMask[strength]) == CE 
    && (*prevContCE  & strengthMask[strength])== contCE) {
    *prevCE = (*(CETable+3*(--iCE)));
    *prevContCE = (*(CETable+3*(iCE)+1));
  }

  return iCE;
}

int32_t ucol_inv_getPrevious(UColTokListHeader *lh, uint32_t strength) {

  uint32_t CE = lh->baseCE;
  uint32_t SecondCE = lh->baseContCE; 

  uint32_t *CETable = (uint32_t *)((uint8_t *)invUCA+invUCA->table);
  uint32_t previousCE, previousContCE;
  int32_t iCE;

  iCE = ucol_inv_findCE(CE, SecondCE);

  if(iCE<0) {
    return -1;
  }

  CE &= strengthMask[strength];
  SecondCE &= strengthMask[strength];

  previousCE = CE;
  previousContCE = SecondCE;

  while((previousCE  & strengthMask[strength]) == CE && (previousContCE  & strengthMask[strength])== SecondCE) {
    previousCE = (*(CETable+3*(--iCE)));
    previousContCE = (*(CETable+3*(iCE)+1));
  }
  lh->previousCE = previousCE;
  lh->previousContCE = previousContCE;

  return iCE;
}

int32_t ucol_inv_getNext(UColTokListHeader *lh, uint32_t strength) {
  uint32_t CE = lh->baseCE;
  uint32_t SecondCE = lh->baseContCE; 

  uint32_t *CETable = (uint32_t *)((uint8_t *)invUCA+invUCA->table);
  uint32_t nextCE, nextContCE;
  int32_t iCE;

  iCE = ucol_inv_findCE(CE, SecondCE);

  if(iCE<0) {
    return -1;
  }

  CE &= strengthMask[strength];
  SecondCE &= strengthMask[strength];

  nextCE = CE;
  nextContCE = SecondCE;

  while((nextCE  & strengthMask[strength]) == CE 
    && (nextContCE  & strengthMask[strength]) == SecondCE) {
    nextCE = (*(CETable+3*(++iCE)));
    nextContCE = (*(CETable+3*(iCE)+1));
  }

  lh->nextCE = nextCE;
  lh->nextContCE = nextContCE;

  return iCE;
}

U_CFUNC void ucol_inv_getGapPositions(/*UColTokenParser *src,*/ UColTokListHeader *lh, UErrorCode *status) {
  /* reset all the gaps */
  int32_t i = 0;
  uint32_t *CETable = (uint32_t *)((uint8_t *)invUCA+invUCA->table);
  uint32_t st = 0;
  uint32_t t1, t2;
  int32_t pos;


  UColToken *tok = lh->first;
  uint32_t tokStrength = tok->strength;

  for(i = 0; i<3; i++) {
    lh->gapsHi[3*i] = 0;
    lh->gapsHi[3*i+1] = 0;
    lh->gapsHi[3*i+2] = 0;
    lh->gapsLo[3*i] = 0;
    lh->gapsLo[3*i+1] = 0;
    lh->gapsLo[3*i+2] = 0;
    lh->numStr[i] = 0;
    lh->fStrToken[i] = NULL;
    lh->lStrToken[i] = NULL;
    lh->pos[i] = -1;
  }

  if(lh->baseCE >= PRIMARY_IMPLICIT_MIN && lh->baseCE < PRIMARY_IMPLICIT_MAX ) { /* implicits - */ 
    lh->pos[0] = 0;
    t1 = lh->baseCE;
    t2 = lh->baseContCE;
    lh->gapsLo[0] = (t1 & UCOL_PRIMARYMASK) | (t2 & UCOL_PRIMARYMASK) >> 16;
    lh->gapsLo[1] = (t1 & UCOL_SECONDARYMASK) << 16 | (t2 & UCOL_SECONDARYMASK) << 8;
    lh->gapsLo[2] = (UCOL_TERTIARYORDER(t1)) << 24 | (UCOL_TERTIARYORDER(t2)) << 16;
    if(lh->baseCE < 0xEF000000) {
    /* first implicits have three byte primaries, with a gap of one */
    /* so we esentially need to add 2 to the top byte in lh->baseContCE */
      t2 += 0x02000000;
    } else {
    /* second implicits have four byte primaries, with a gap of IMPLICIT_LAST2_MULTIPLIER_ */
    /* Now, this guy is not really accessible here, so until we find a better way to pass it */
    /* around, we'll assume that the gap is 1 */
      t2 += 0x00020000;
    }
    lh->gapsHi[0] = (t1 & UCOL_PRIMARYMASK) | (t2 & UCOL_PRIMARYMASK) >> 16;
    lh->gapsHi[1] = (t1 & UCOL_SECONDARYMASK) << 16 | (t2 & UCOL_SECONDARYMASK) << 8;
    lh->gapsHi[2] = (UCOL_TERTIARYORDER(t1)) << 24 | (UCOL_TERTIARYORDER(t2)) << 16;
  } else if(lh->baseCE == UCOL_RESET_TOP_VALUE && lh->baseContCE == 0) {
    lh->pos[0] = 0;
    t1 = UCOL_RESET_TOP_VALUE;
    t2 = 0;
    lh->gapsLo[0] = (t1 & UCOL_PRIMARYMASK);
    lh->gapsLo[1] = (t1 & UCOL_SECONDARYMASK) << 16;
    lh->gapsLo[2] = (UCOL_TERTIARYORDER(t1)) << 24;
    t1 = UCOL_NEXT_TOP_VALUE;
    t2 = 0;
    lh->gapsHi[0] = (t1 & UCOL_PRIMARYMASK);
    lh->gapsHi[1] = (t1 & UCOL_SECONDARYMASK) << 16;
    lh->gapsHi[2] = (UCOL_TERTIARYORDER(t1)) << 24;
  } else {
    for(;;) {
      if(tokStrength < UCOL_CE_STRENGTH_LIMIT) {
        if((lh->pos[tokStrength] = ucol_inv_getNext(lh, tokStrength)) >= 0) {
          lh->fStrToken[tokStrength] = tok;
        } else { /* The CE must be implicit, since it's not in the table */
          /* Error */
          *status = U_INTERNAL_PROGRAM_ERROR;
        }
      }

      while(tok != NULL && tok->strength >= tokStrength) {
        if(tokStrength < UCOL_CE_STRENGTH_LIMIT) {
          lh->lStrToken[tokStrength] = tok;
        }
        tok = tok->next;
      }
      if(tokStrength < UCOL_CE_STRENGTH_LIMIT-1) {
        /* check if previous interval is the same and merge the intervals if it is so */
        if(lh->pos[tokStrength] == lh->pos[tokStrength+1]) {
          lh->fStrToken[tokStrength] = lh->fStrToken[tokStrength+1];
          lh->fStrToken[tokStrength+1] = NULL;
          lh->lStrToken[tokStrength+1] = NULL;
          lh->pos[tokStrength+1] = -1;
        }
      }
      if(tok != NULL) {
        tokStrength = tok->strength;
      } else {
        break;
      }
    }
    for(st = 0; st < 3; st++) {
      if((pos = lh->pos[st]) >= 0) {
        t1 = *(CETable+3*(pos));
        t2 = *(CETable+3*(pos)+1);
        lh->gapsHi[3*st] = (t1 & UCOL_PRIMARYMASK) | (t2 & UCOL_PRIMARYMASK) >> 16;
        lh->gapsHi[3*st+1] = (t1 & UCOL_SECONDARYMASK) << 16 | (t2 & UCOL_SECONDARYMASK) << 8;
        //lh->gapsHi[3*st+2] = (UCOL_TERTIARYORDER(t1)) << 24 | (UCOL_TERTIARYORDER(t2)) << 16;
        lh->gapsHi[3*st+2] = (t1&0x3f) << 24 | (t2&0x3f) << 16;
        pos--;
        t1 = *(CETable+3*(pos));
        t2 = *(CETable+3*(pos)+1);
        lh->gapsLo[3*st] = (t1 & UCOL_PRIMARYMASK) | (t2 & UCOL_PRIMARYMASK) >> 16;
        lh->gapsLo[3*st+1] = (t1 & UCOL_SECONDARYMASK) << 16 | (t2 & UCOL_SECONDARYMASK) << 8;
        lh->gapsLo[3*st+2] = (t1&0x3f) << 24 | (t2&0x3f) << 16;
      }
    }
  }
}


#define ucol_countBytes(value, noOfBytes)   \
{                               \
  uint32_t mask = 0xFFFFFFFF;   \
  (noOfBytes) = 0;              \
  while(mask != 0) {            \
    if(((value) & mask) != 0) { \
      (noOfBytes)++;            \
    }                           \
    mask >>= 8;                 \
  }                             \
}

U_CFUNC uint32_t ucol_getNextGenerated(ucolCEGenerator *g, UErrorCode *status) {
  if(U_SUCCESS(*status)) {
  g->current = ucol_nextWeight(g->ranges, &g->noOfRanges);
  }
  return g->current;
}

U_CFUNC uint32_t ucol_getSimpleCEGenerator(ucolCEGenerator *g, UColToken *tok, uint32_t strength, UErrorCode *status) {
/* TODO: rename to enum names */
  uint32_t high, low, count=1;
  uint32_t maxByte = (strength == UCOL_TERTIARY)?0x3F:0xFF;

  if(strength == UCOL_SECONDARY) {
    low = UCOL_COMMON_TOP2<<24;
    high = 0xFFFFFFFF;
    count = 0xFF - UCOL_COMMON_TOP2;
  } else {
    low = UCOL_BYTE_COMMON << 24; //0x05000000;
    high = 0x40000000;
    count = 0x40 - UCOL_BYTE_COMMON;
  }

  if(tok->next != NULL && tok->next->strength == strength) {
    count = tok->next->toInsert;
  } 

  g->noOfRanges = ucol_allocWeights(low, high, count, maxByte, g->ranges);
  g->current = UCOL_BYTE_COMMON<<24;

  if(g->noOfRanges == 0) {
    *status = U_INTERNAL_PROGRAM_ERROR;
  }
  return g->current;
}

U_CFUNC uint32_t ucol_getCEGenerator(ucolCEGenerator *g, uint32_t* lows, uint32_t* highs, UColToken *tok, uint32_t fStrength, UErrorCode *status) {
  uint32_t strength = tok->strength;
  uint32_t low = lows[fStrength*3+strength];
  uint32_t high = highs[fStrength*3+strength];
  uint32_t maxByte = (strength == UCOL_TERTIARY)?0x3F:0xFF;

  uint32_t count = tok->toInsert;

  if(low == high && strength > UCOL_PRIMARY) {
    int32_t s = strength;
    for(;;) {
      s--;
      if(lows[fStrength*3+s] != highs[fStrength*3+s]) {
        if(strength == UCOL_SECONDARY) {
          low = UCOL_COMMON_TOP2<<24;
          high = 0xFFFFFFFF;
        } else {
          //low = 0x02000000;
          high = 0x40000000;
        }
        break;
      }
      if(s<0) {
        *status = U_INTERNAL_PROGRAM_ERROR;
        return 0;
      }
    }
  } 

  if(low == 0) {
    low = 0x01000000;
  }

  if(strength == UCOL_SECONDARY) { /* similar as simple */
    if(low >= (UCOL_COMMON_BOT2<<24) && low < (uint32_t)(UCOL_COMMON_TOP2<<24)) {
      low = UCOL_COMMON_TOP2<<24;
    }
    if(high > (UCOL_COMMON_BOT2<<24) && high < (uint32_t)(UCOL_COMMON_TOP2<<24)) {
      high = UCOL_COMMON_TOP2<<24;
    } 
    if(low < UCOL_COMMON_BOT2<<24) {
      g->noOfRanges = ucol_allocWeights(UCOL_COMMON_TOP2<<24, high, count, maxByte, g->ranges);
      g->current = UCOL_COMMON_BOT2;
      return g->current;
    }
  } 

  g->noOfRanges = ucol_allocWeights(low, high, count, maxByte, g->ranges);
  if(g->noOfRanges == 0) {
    *status = U_INTERNAL_PROGRAM_ERROR;
  }
  g->current = ucol_nextWeight(g->ranges, &g->noOfRanges);
  return g->current;
}

U_CFUNC void ucol_doCE(uint32_t *CEparts, UColToken *tok) {
  /* this one makes the table and stuff */
  uint32_t noOfBytes[3];
  uint32_t i;

  for(i = 0; i<3; i++) {
    ucol_countBytes(CEparts[i], noOfBytes[i]);
  }

  /* Here we have to pack CEs from parts */

  uint32_t CEi = 0;
  uint32_t value = 0;

  while(2*CEi<noOfBytes[0] || CEi<noOfBytes[1] || CEi<noOfBytes[2]) {
    if(CEi > 0) {
      value = UCOL_CONTINUATION_MARKER; /* Continuation marker */
    } else {
      value = 0;
    }

    if(2*CEi<noOfBytes[0]) {
      value |= ((CEparts[0]>>(32-16*(CEi+1))) & 0xFFFF) << 16;
    }
    if(CEi<noOfBytes[1]) {
      value |= ((CEparts[1]>>(32-8*(CEi+1))) & 0xFF) << 8;
    }
    if(CEi<noOfBytes[2]) {
      value |= ((CEparts[2]>>(32-8*(CEi+1))) & 0x3F);
    }
    tok->CEs[CEi] = value;
    CEi++;
  }
  if(CEi == 0) { /* totally ignorable */
    tok->noOfCEs = 1;
    tok->CEs[0] = 0;
  } else { /* there is at least something */
    tok->noOfCEs = CEi;
  }

#if UCOL_DEBUG==2
  fprintf(stderr, "%04X str: %i, [%08X, %08X, %08X]: tok: ", tok->debugSource, tok->strength, CEparts[0] >> (32-8*noOfBytes[0]), CEparts[1] >> (32-8*noOfBytes[1]), CEparts[2]>> (32-8*noOfBytes[2]));
  for(i = 0; i<tok->noOfCEs; i++) {
    fprintf(stderr, "%08X ", tok->CEs[i]);
  }
  fprintf(stderr, "\n");
#endif
}

U_CFUNC void ucol_initBuffers(/*UColTokenParser *src,*/ UColTokListHeader *lh, UErrorCode *status) {

  ucolCEGenerator Gens[UCOL_CE_STRENGTH_LIMIT];
  uint32_t CEparts[UCOL_CE_STRENGTH_LIMIT];

  uint32_t i = 0;

  UColToken *tok = lh->last;
  uint32_t t[UCOL_STRENGTH_LIMIT];

  for(i=0; i<UCOL_STRENGTH_LIMIT; i++) {
    t[i] = 0;
  }

  tok->toInsert = 1;
  t[tok->strength] = 1;

  while(tok->previous != NULL) {
    if(tok->previous->strength < tok->strength) { /* going up */
      t[tok->strength] = 0;
      t[tok->previous->strength]++;
    } else if(tok->previous->strength > tok->strength) { /* going down */
      t[tok->previous->strength] = 1;
    } else {
      t[tok->strength]++;
    }
    tok=tok->previous;
    tok->toInsert = t[tok->strength];
  } 

  tok->toInsert = t[tok->strength];
  ucol_inv_getGapPositions(lh, status);

#if UCOL_DEBUG
  fprintf(stderr, "BaseCE: %08X %08X\n", lh->baseCE, lh->baseContCE);
  int32_t j = 2;
  for(j = 2; j >= 0; j--) {
    fprintf(stderr, "gapsLo[%i] [%08X %08X %08X]\n", j, lh->gapsLo[j*3], lh->gapsLo[j*3+1], lh->gapsLo[j*3+2]);
    fprintf(stderr, "gapsHi[%i] [%08X %08X %08X]\n", j, lh->gapsHi[j*3], lh->gapsHi[j*3+1], lh->gapsHi[j*3+2]);
  }
  tok=lh->first[UCOL_TOK_POLARITY_POSITIVE];

  do {
    fprintf(stderr,"%i", tok->strength);
    tok = tok->next;
  } while(tok != NULL);
  fprintf(stderr, "\n");

  tok=lh->first[UCOL_TOK_POLARITY_POSITIVE];

  do {  
    fprintf(stderr,"%i", tok->toInsert);
    tok = tok->next;
  } while(tok != NULL);
#endif

  tok = lh->first;
  uint32_t fStrength = UCOL_IDENTICAL;
  uint32_t initStrength = UCOL_IDENTICAL;


  CEparts[UCOL_PRIMARY] = (lh->baseCE & UCOL_PRIMARYMASK) | (lh->baseContCE & UCOL_PRIMARYMASK) >> 16;
  CEparts[UCOL_SECONDARY] = (lh->baseCE & UCOL_SECONDARYMASK) << 16 | (lh->baseContCE & UCOL_SECONDARYMASK) << 8;
  CEparts[UCOL_TERTIARY] = (UCOL_TERTIARYORDER(lh->baseCE)) << 24 | (UCOL_TERTIARYORDER(lh->baseContCE)) << 16;

  while (tok != NULL && U_SUCCESS(*status)) {
    fStrength = tok->strength;
    if(fStrength < initStrength) {
      initStrength = fStrength;
      if(lh->pos[fStrength] == -1) {
        while(lh->pos[fStrength] == -1 && fStrength > 0) {
          fStrength--;
        }
        if(lh->pos[fStrength] == -1) {
          *status = U_INTERNAL_PROGRAM_ERROR;
          return;
        }
      }
      if(initStrength == UCOL_TERTIARY) { /* starting with tertiary */
        CEparts[UCOL_PRIMARY] = lh->gapsLo[fStrength*3];
        CEparts[UCOL_SECONDARY] = lh->gapsLo[fStrength*3+1];
        /*CEparts[UCOL_TERTIARY] = ucol_getCEGenerator(&Gens[2], lh->gapsLo[fStrength*3+2], lh->gapsHi[fStrength*3+2], tok, UCOL_TERTIARY); */
        CEparts[UCOL_TERTIARY] = ucol_getCEGenerator(&Gens[UCOL_TERTIARY], lh->gapsLo, lh->gapsHi, tok, fStrength, status); 
      } else if(initStrength == UCOL_SECONDARY) { /* secondaries */
        CEparts[UCOL_PRIMARY] = lh->gapsLo[fStrength*3];
        /*CEparts[1] = ucol_getCEGenerator(&Gens[1], lh->gapsLo[fStrength*3+1], lh->gapsHi[fStrength*3+1], tok, 1);*/
        CEparts[UCOL_SECONDARY] = ucol_getCEGenerator(&Gens[UCOL_SECONDARY], lh->gapsLo, lh->gapsHi, tok, fStrength,  status);
        CEparts[UCOL_TERTIARY] = ucol_getSimpleCEGenerator(&Gens[UCOL_TERTIARY], tok, UCOL_TERTIARY, status);
      } else { /* primaries */
        /*CEparts[UCOL_PRIMARY] = ucol_getCEGenerator(&Gens[0], lh->gapsLo[0], lh->gapsHi[0], tok, UCOL_PRIMARY);*/
        CEparts[UCOL_PRIMARY] = ucol_getCEGenerator(&Gens[UCOL_PRIMARY], lh->gapsLo, lh->gapsHi, tok, fStrength,  status);
        CEparts[UCOL_SECONDARY] = ucol_getSimpleCEGenerator(&Gens[UCOL_SECONDARY], tok, UCOL_SECONDARY, status);
        CEparts[UCOL_TERTIARY] = ucol_getSimpleCEGenerator(&Gens[UCOL_TERTIARY], tok, UCOL_TERTIARY, status);
      }
    } else {
      if(tok->strength == UCOL_TERTIARY) {
        CEparts[UCOL_TERTIARY] = ucol_getNextGenerated(&Gens[UCOL_TERTIARY], status);
      } else if(tok->strength == UCOL_SECONDARY) {
        CEparts[UCOL_SECONDARY] = ucol_getNextGenerated(&Gens[UCOL_SECONDARY], status);
        CEparts[UCOL_TERTIARY] = ucol_getSimpleCEGenerator(&Gens[UCOL_TERTIARY], tok, UCOL_TERTIARY, status);
      } else if(tok->strength == UCOL_PRIMARY) {
        CEparts[UCOL_PRIMARY] = ucol_getNextGenerated(&Gens[UCOL_PRIMARY], status);
        CEparts[UCOL_SECONDARY] = ucol_getSimpleCEGenerator(&Gens[UCOL_SECONDARY], tok, UCOL_SECONDARY, status);
        CEparts[UCOL_TERTIARY] = ucol_getSimpleCEGenerator(&Gens[UCOL_TERTIARY], tok, UCOL_TERTIARY, status);
      }
    }
    ucol_doCE(CEparts, tok);
    tok = tok->next;
  }
}

uint32_t u_toLargeKana(const UChar *source, const uint32_t sourceLen, UChar *resBuf, const uint32_t resLen, UErrorCode *status) {
  uint32_t i = 0; 
  UChar c;

  if(U_FAILURE(*status)) {
    return 0;
  }

  if(sourceLen > resLen) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
  }
  
  for(i = 0; i < sourceLen; i++) {
    c = source[i];
    if(0x3042 < c && c < 0x30ef) { /* Kana range */
      switch(c - 0x3000) {
      case 0x41: case 0x43: case 0x45: case 0x47: case 0x49: case 0x63: case 0x83: case 0x85: case 0x8E:
      case 0xA1: case 0xA3: case 0xA5: case 0xA7: case 0xA9: case 0xC3: case 0xE3: case 0xE5: case 0xEE:
        c++;
        break;
      case 0xF5:
        c = 0x30AB;
        break;
      case 0xF6:
        c = 0x30B1;
        break;
      }
    }
    resBuf[i] = c;
  }
  return sourceLen;
}

uint32_t u_toSmallKana(const UChar *source, const uint32_t sourceLen, UChar *resBuf, const uint32_t resLen, UErrorCode *status) {
  uint32_t i = 0; 
  UChar c;

  if(U_FAILURE(*status)) {
    return 0;
  }

  if(sourceLen > resLen) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
  }
  
  for(i = 0; i < sourceLen; i++) {
    c = source[i];
    if(0x3042 < c && c < 0x30ef) { /* Kana range */
      switch(c - 0x3000) {
      case 0x42: case 0x44: case 0x46: case 0x48: case 0x4A: case 0x64: case 0x84: case 0x86: case 0x8F:
      case 0xA2: case 0xA4: case 0xA6: case 0xA8: case 0xAA: case 0xC4: case 0xE4: case 0xE6: case 0xEF:
        c--;
        break;
      case 0xAB:
        c = 0x30F5;
        break;
      case 0xB1:
        c = 0x30F6;
        break;
      }
    }
    resBuf[i] = c;
  }
  return sourceLen;
}

uint8_t ucol_uprv_getCaseBits(const UCollator *UCA, const UChar *src, uint32_t len, UErrorCode *status) {
  uint32_t i = 0;
  UChar n[128];
  uint32_t nLen = 0;
  uint32_t uCount = 0, lCount = 0;

  collIterate s;
  uint32_t order = 0;

  if(U_FAILURE(*status)) {
    return UCOL_LOWER_CASE;
  }

  nLen = unorm_normalize(src, len, UNORM_NFKD, 0, n, 128, status);

  for(i = 0; i < nLen; i++) {
    init_collIterate(UCA, &n[i], 1, &s);
    order = ucol_getNextCE(UCA, &s, status);
    if(isContinuation(order)) {
      *status = U_INTERNAL_PROGRAM_ERROR;
      return UCOL_LOWER_CASE;
    }
    if((order&UCOL_CASE_BIT_MASK)== UCOL_UPPER_CASE) {
      uCount++;
    } else {
      if(u_islower(n[i])) {
        lCount++;
      } else {
        UChar sk[1], lk[1];
        u_toSmallKana(&n[i], 1, sk, 1, status);
        u_toLargeKana(&n[i], 1, lk, 1, status);
        if(sk[0] == n[i] && lk[0] != n[i]) {
          lCount++;
        }
      }
    }
  }

  if(uCount != 0 && lCount != 0) {
    return UCOL_MIXED_CASE;
  } else if(uCount != 0) {
    return UCOL_UPPER_CASE;
  } else {
    return UCOL_LOWER_CASE;
  }
}

U_CFUNC void ucol_createElements(UColTokenParser *src, tempUCATable *t, UColTokListHeader *lh, UErrorCode *status) {
  UCAElements el;
  UColToken *tok = lh->first;
  UColToken *expt = NULL;
  uint32_t i = 0, j = 0;

  while(tok != NULL) {
    /* first, check if there are any expansions */
    /* if there are expansions, we need to do a little bit more processing */
    /* since parts of expansion can be tailored, while others are not */
    if(tok->expansion != 0) {
      uint32_t len = tok->expansion >> 24;
      uint32_t currentSequenceLen = len;
      uint32_t expOffset = tok->expansion & 0x00FFFFFF;
      uint32_t exp = currentSequenceLen | expOffset;

      while(len > 0) {
        currentSequenceLen = len;
        while(currentSequenceLen > 0) {
          exp = (currentSequenceLen << 24) | expOffset;
          if((expt = (UColToken *)uhash_get(src->tailored, (void *)exp)) != NULL && expt->strength != UCOL_TOK_RESET) { /* expansion is tailored */
            uint32_t noOfCEsToCopy = expt->noOfCEs;
            for(j = 0; j<noOfCEsToCopy; j++) {
              tok->expCEs[tok->noOfExpCEs + j] = expt->CEs[j];
            }
            tok->noOfExpCEs += noOfCEsToCopy;
            // Smart people never try to add codepoints and CEs.
            // For some odd reason, it won't work.
            expOffset += currentSequenceLen; //noOfCEsToCopy;
            len -= currentSequenceLen; //noOfCEsToCopy;
            break;
          } else {
            currentSequenceLen--;
          }
        }
        if(currentSequenceLen == 0) { /* couldn't find any tailored subsequence */
          /* will have to get one from UCA */
          /* first, get the UChars from the rules */
          /* then pick CEs out until there is no more and stuff them into expansion */
          collIterate s;
          uint32_t order = 0;
          init_collIterate(src->UCA, expOffset + src->source, 1, &s);

          for(;;) {
            order = ucol_getNextCE(src->UCA, &s, status);
            if(order == UCOL_NO_MORE_CES) {
                break;
            }
            tok->expCEs[tok->noOfExpCEs++] = order;
          }
          expOffset++;
          len--;
        }
      }
    } else {
      tok->noOfExpCEs = 0;
    }

    /* set the ucaelement with obtained values */
    el.noOfCEs = tok->noOfCEs + tok->noOfExpCEs;
    /* copy CEs */
    for(i = 0; i<tok->noOfCEs; i++) {
      el.CEs[i] = tok->CEs[i];
    }
    for(i = 0; i<tok->noOfExpCEs; i++) {
      el.CEs[i+tok->noOfCEs] = tok->expCEs[i];
    }

    /* copy UChars */

    //UChar buff[128];
    //uint32_t decompSize;
    //uprv_memcpy(buff, (tok->source & 0x00FFFFFF) + src->source, (tok->source >> 24)*sizeof(UChar));
    //decompSize = unorm_normalize(buff, tok->source >> 24, UNORM_NFD, 0, el.uchars, 128, status);
    //el.cSize = decompSize; /*(tok->source >> 24); *//* + (tok->expansion >> 24);*/
    el.cSize = (tok->source >> 24); 
    uprv_memcpy(el.uchars, (tok->source & 0x00FFFFFF) + src->source, el.cSize*sizeof(UChar));
    el.cPoints = el.uchars;

    if(UCOL_ISTHAIPREVOWEL(el.cPoints[0])) {
      el.isThai = TRUE;
    } else {
      el.isThai = FALSE;
    }

    if(src->UCA != NULL) {
      for(i = 0; i<el.cSize; i++) {
        if(UCOL_ISJAMO(el.cPoints[i])) {
          t->image->jamoSpecial = TRUE;
        }
      }
    }

    // Case bits handling 
    el.CEs[0] &= 0xFFFFFF3F; // Clean the case bits field
    if(el.cSize > 1) {
      // Do it manually
      el.CEs[0] |= ucol_uprv_getCaseBits(src->UCA, el.cPoints, el.cSize, status);
    } else {
      // Copy it from the UCA
      uint32_t caseCE = ucol_getFirstCE(src->UCA, el.cPoints[0], status);
      el.CEs[0] |= (caseCE & 0xC0);
    }

    /* and then, add it */
#if UCOL_DEBUG==2
    fprintf(stderr, "Adding: %04X with %08X\n", el.cPoints[0], el.CEs[0]);
#endif
    uprv_uca_addAnElement(t, &el, status);
#if UCOL_DEBUG_DUPLICATES
    if(*status != U_ZERO_ERROR) {
      fprintf(stderr, "replaced CE for %04X with CE for %04X\n", el.cPoints[0], tok->debugSource);
      *status = U_ZERO_ERROR;
    }
#endif

    tok = tok->next;
  }
}


  
UCATableHeader *ucol_assembleTailoringTable(UColTokenParser *src, UErrorCode *status) {
  uint32_t i = 0;
  if(U_FAILURE(*status)) {
    return NULL;
  }
/*
2.  Eliminate the negative lists by doing the following for each non-null negative list: 
    o   if previousCE(baseCE, strongestN) != some ListHeader X's baseCE, 
    create new ListHeader X 
    o   reverse the list, add to the end of X's positive list. Reset the strength of the 
    first item you add, based on the stronger strength levels of the two lists. 
*/
/*
3.  For each ListHeader with a non-null positive list: 
*/
/*
    o   Find all character strings with CEs between the baseCE and the 
    next/previous CE, at the strength of the first token. Add these to the 
    tailoring. 
      ? That is, if UCA has ...  x <<< X << x' <<< X' < y ..., and the 
      tailoring has & x < z... 
      ? Then we change the tailoring to & x  <<< X << x' <<< X' < z ... 
*/
  /* It is possible that this part should be done even while constructing list */
  /* The problem is that it is unknown what is going to be the strongest weight */
  /* So we might as well do it here */

/*
    o   Allocate CEs for each token in the list, based on the total number N of the 
    largest level difference, and the gap G between baseCE and nextCE at that 
    level. The relation * between the last item and nextCE is the same as the 
    strongest strength. 
    o   Example: baseCE < a << b <<< q << c < d < e * nextCE(X,1) 
      ? There are 3 primary items: a, d, e. Fit them into the primary gap. 
      Then fit b and c into the secondary gap between a and d, then fit q 
      into the tertiary gap between b and c. 

    o   Example: baseCE << b <<< q << c * nextCE(X,2) 
      ? There are 2 secondary items: b, c. Fit them into the secondary gap. 
      Then fit q into the tertiary gap between b and c. 
    o   When incrementing primary values, we will not cross high byte 
    boundaries except where there is only a single-byte primary. That is to 
    ensure that the script reordering will continue to work. 
*/
  rulesToParse = src->source;
  UCATableHeader *image = (UCATableHeader *)uprv_malloc(sizeof(UCATableHeader));
  uprv_memcpy(image, src->UCA->image, sizeof(UCATableHeader));

  for(i = 0; i<src->resultLen; i++) {
    /* now we need to generate the CEs */ 
    /* We stuff the initial value in the buffers, and increase the appropriate buffer */
    /* According to strength                                                          */
    if(U_SUCCESS(*status)) {
      ucol_initBuffers(&src->lh[i], status);
    }
  }

  if(src->varTop != NULL) { /* stuff the variable top value */
    src->opts->variableTopValue = (*(src->varTop->CEs))>>16;
    /* remove it from the list */
    if(src->varTop->listHeader->first == src->varTop) { /* first in list */
      src->varTop->listHeader->first = src->varTop->next;
    }
    if(src->varTop->listHeader->last == src->varTop) { /* first in list */
      src->varTop->listHeader->last = src->varTop->previous;    
    }
    if(src->varTop->next != NULL) {
      src->varTop->next->previous = src->varTop->previous;
    }
    if(src->varTop->previous != NULL) {
      src->varTop->previous->next = src->varTop->next;
    }
  }


  tempUCATable *t = uprv_uca_initTempTable(image, src->opts, src->UCA, status);


  /* After this, we have assigned CE values to all regular CEs      */
  /* now we will go through list once more and resolve expansions,  */
  /* make UCAElements structs and add them to table                 */
  for(i = 0; i<src->resultLen; i++) {
    /* now we need to generate the CEs */ 
    /* We stuff the initial value in the buffers, and increase the appropriate buffer */
    /* According to strength                                                          */
    if(U_SUCCESS(*status)) {
      ucol_createElements(src, t, &src->lh[i], status);
    }
  }

  {
    UChar decomp[256];
    uint32_t noOfDec = 0, CE = UCOL_NOT_FOUND;
    UChar u = 0;
    UCAElements el;
    el.isThai = FALSE;
    collIterate colIt;

    /* add latin-1 stuff */
    if(U_SUCCESS(*status)) {
      for(u = 0; u<0x100; u++) {
        if((CE = ucmp32_get(t->mapping, u)) == UCOL_NOT_FOUND 
          /* this test is for contractions that are missing the starting element. Looks like latin-1 should be done before assembling */
          /* the table, even if it results in more false closure elements */
          || ((isContraction(CE)) &&
          (uprv_cnttab_getCE(t->contractions, CE, 0, status) == UCOL_NOT_FOUND))
          ) {
          decomp[0] = (UChar)u;
          el.uchars[0] = (UChar)u;
          el.cPoints = el.uchars;
          el.cSize = 1;
          el.noOfCEs = 0;
          init_collIterate(src->UCA, decomp, 1, &colIt);
          while(CE != UCOL_NO_MORE_CES) {
            CE = ucol_getNextCE(src->UCA, &colIt, status);
            if(CE != UCOL_NO_MORE_CES) {
              el.CEs[el.noOfCEs++] = CE;
            }
          }
          uprv_uca_addAnElement(t, &el, status);
        }
      }
    }

    if(U_SUCCESS(*status)) {
      /* copy contractions from the UCA - this is felt mostly for cyrillic*/

      uint32_t tailoredCE = UCOL_NOT_FOUND;
      UChar *conts = (UChar *)((uint8_t *)src->UCA->image + src->UCA->image->contractionUCACombos);
      UCollationElements *ucaEl = ucol_openElements(src->UCA, NULL, 0, status);
      while(*conts != 0) {
        tailoredCE = ucmp32_get(t->mapping, *conts);
        if(tailoredCE != UCOL_NOT_FOUND) {         
          UBool needToAdd = TRUE;
          if(isContraction(tailoredCE)) {
            if(uprv_cnttab_isTailored(t->contractions, tailoredCE, conts+1, status) == TRUE) {
              needToAdd = FALSE;
            }
          }

          if(needToAdd == TRUE) { // we need to add if this contraction is not tailored.
            el.cPoints = el.uchars;
            el.noOfCEs = 0;
            el.uchars[0] = *conts;
            el.uchars[1] = *(conts+1);
            if(*(conts+2)!=0) {
              el.uchars[2] = *(conts+2);
              el.cSize = 3;
            } else {
              el.cSize = 2;
            }
            ucol_setText(ucaEl, el.uchars, el.cSize, status);
            while ((el.CEs[el.noOfCEs] = ucol_next(ucaEl, status)) != UCOL_NULLORDER) {
              el.noOfCEs++;
            }
            uprv_uca_addAnElement(t, &el, status);
          }

        }
        conts+=3;
      }
      ucol_closeElements(ucaEl);

      UCollator *tempColl = NULL;
      if(U_SUCCESS(*status)) {
        tempUCATable *tempTable = uprv_uca_cloneTempTable(t, status);

        UCATableHeader *tempData = uprv_uca_assembleTable(tempTable, status);
        tempColl = ucol_initCollator(tempData, 0, status);

        if(U_SUCCESS(*status)) {
          tempColl->rb = NULL;
          tempColl->hasRealData = TRUE;
        }
        uprv_uca_closeTempTable(tempTable);    
      }

      /* produce canonical closure */
      UCollationElements* colEl = ucol_openElements(tempColl, NULL, 0, status);
      for(u = 0; u < 0xFFFF; u++) {
        if((noOfDec = unorm_normalize(&u, 1, UNORM_NFD, 0, decomp, 256, status)) > 1
          || (noOfDec == 1 && *decomp != (UChar)u))
        {
          if(ucol_strcoll(tempColl, (UChar *)&u, 1, decomp, noOfDec) != UCOL_EQUAL) {
            el.uchars[0] = (UChar)u;
            el.cPoints = el.uchars;
            el.cSize = 1;
            el.noOfCEs = 0;

            ucol_setText(colEl, decomp, noOfDec, status);
            while((el.CEs[el.noOfCEs] = ucol_next(colEl, status)) != UCOL_NULLORDER) {
              el.noOfCEs++;
            }

            uprv_uca_addAnElement(t, &el, status);
          }
        }
      }
      ucol_closeElements(colEl);
      ucol_close(tempColl);
    }
  }

    /* still need to produce compatibility closure */

  UCATableHeader *myData = uprv_uca_assembleTable(t, status);  

  uprv_uca_closeTempTable(t);    
  uprv_free(image);

  return myData;
}

const InverseTableHeader *ucol_initInverseUCA(UErrorCode *status) {
  if(U_FAILURE(*status)) return NULL;

  if(invUCA == NULL) {
    InverseTableHeader *newInvUCA = NULL;
    UDataMemory *result = udata_openChoice(NULL, INVC_DATA_TYPE, INVC_DATA_NAME, isAcceptableInvUCA, NULL, status);

    if(U_FAILURE(*status)) {
        udata_close(result);
        uprv_free(newInvUCA);
    }

    if(result != NULL) { /* It looks like sometimes we can fail to find the data file */
      newInvUCA = (InverseTableHeader *)udata_getMemory(result);

      umtx_lock(NULL);
      if(invUCA == NULL) {
          invUCA = newInvUCA;
          newInvUCA = NULL;
      }
      umtx_unlock(NULL);

      if(newInvUCA != NULL) {
          udata_close(result);
          uprv_free(newInvUCA);
      }
    }

  }
  return invUCA;
}

#if 0
/* This function handles the special CEs like contractions, expansions, surrogates, Thai */
/* It is called by both getNextCE and getNextUCA                                         */
uint32_t uprv_getSpecialDynamicCE(const tempUCATable *t, uint32_t CE, collIterate *source, UErrorCode *status) {
  uint32_t i = 0; /* general counter */
  uint32_t firstCE = UCOL_NOT_FOUND;
  UChar *firstUChar = source->pos;
  //uint32_t CE = *source->CEpos;
  for (;;) {
    const uint32_t *CEOffset = NULL;
    const UChar *UCharOffset = NULL;
    UChar schar, tchar;
    uint32_t size = 0;
    switch(getCETag(CE)) {
    case NOT_FOUND_TAG:
      /* This one is not found, and we'll let somebody else bother about it... no more games */
      return CE;
    case CHARSET_TAG:
    case SURROGATE_TAG:
      return UCOL_NOT_FOUND;
    case CONTRACTION_TAG:
      /* This should handle contractions */
      for (;;) {
        /* First we position ourselves at the begining of contraction sequence */
        /*const UChar *ContractionStart = UCharOffset = (UChar *)coll->image+getContractOffset(CE);*/
        ContractionTable *ctb = t->contractions->elements[getContractOffset(CE)];
        const UChar *ContractionStart = UCharOffset = ctb->codePoints;

        if (source->pos>=source->endp) { 
                                           /* this is the end of string.  (Null terminated handled later,
                                            when the null doesn't match the contraction sequence.)     */
          {
            /*CE = *(coll->contractionCEs + (UCharOffset - coll->contractionIndex));*/ /* So we'll pick whatever we have at the point... */
            CE = *(ctb->CEs+(UCharOffset - ContractionStart)); /* So we'll pick whatever we have at the point... */
            if (CE == UCOL_NOT_FOUND) {
              source->pos = firstUChar; /* spit all the not found chars, which led us in this contraction */
              if(firstCE != UCOL_NOT_FOUND) {
                CE = firstCE;
              }
            }
          }
          break;
        }

        /* we need to convey the notion of having a backward search - most probably through the context object */
        /* if (backwardsSearch) offset += contractionUChars[(int16_t)offset]; else UCharOffset++;  */
        UCharOffset++; /* skip the backward offset, see above */


        schar = *source->pos++;
        while(schar > (tchar = *UCharOffset)) { /* since the contraction codepoints should be ordered, we skip all that are smaller */
          UCharOffset++;
        }
        if(schar != tchar) { /* we didn't find the correct codepoint. We can use either the first or the last CE */
          UCharOffset = ContractionStart; /* We're not at the end, bailed out in the middle. Better use starting CE */
          /*source->pos = firstUChar; *//* spit all the not found chars, which led us in this contraction */
          source->pos--; /* Spit out the last char of the string, wasn't tasty enough */
        } 
        /*CE = *(coll->contractionCEs + (UCharOffset - coll->contractionIndex));*/
        CE = *(ctb->CEs + (UCharOffset - ContractionStart));

        if(CE == UCOL_NOT_FOUND) {
          source->pos = firstUChar; /* spit all the not found chars, which led us in this contraction */
          if(firstCE != UCOL_NOT_FOUND) {
            CE = firstCE;
          }
          break;
        } else if(isContraction(CE)) { /* fix for the bug. Other places need to be checked */
        /* this is contraction, and we will continue. However, we can fail along the */
        /* th road, which means that we have part of contraction correct */
          /*uint32_t tempCE = *(coll->contractionCEs + (ContractionStart - coll->contractionIndex));*/
          uint32_t tempCE = *(ctb->CEs);
          if(tempCE != UCOL_NOT_FOUND) {
            firstCE = *(ctb->CEs);
            /*firstCE = *(coll->contractionCEs + (ContractionStart - coll->contractionIndex));*/
            firstUChar = source->pos-1;
          }
        } else {
          break;
        }
      }
      break;
    case EXPANSION_TAG:
    case THAI_TAG:
      /* This should handle expansion. */
      /* NOTE: we can encounter both continuations and expansions in an expansion! */
      /* I have to decide where continuations are going to be dealt with */
      CEOffset = t->expansions->CEs+(getExpansionOffset(CE) - (headersize>>2)); /* find the offset to expansion table */
      size = getExpansionCount(CE);
      CE = *CEOffset++;
      if(size != 0) { /* if there are less than 16 elements in expansion, we don't terminate */
        for(i = 1; i<size; i++) {
          *(source->CEpos++) = *CEOffset++;
        }
      } else { /* else, we do */
        while(*CEOffset != 0) {
          *(source->CEpos++) = *CEOffset++;
        }
      }
      return CE;
    default:
      *status = U_INTERNAL_PROGRAM_ERROR;
      CE=0;
      break;
    }
    if (CE <= UCOL_NOT_FOUND) break;
  }
  return CE;
}

uint32_t uprv_ucol_getNextDynamicCE(tempUCATable *t, collIterate *collationSource, UErrorCode *status) {
  uint32_t order;
  if (collationSource->CEpos > collationSource->toReturn) {       /* Are there any CEs from previous expansions? */
    order = *(collationSource->toReturn++);                         /* if so, return them */
    if(collationSource->CEpos == collationSource->toReturn) {
      collationSource->CEpos = collationSource->toReturn = collationSource->CEs; 
    }
    return order;
  }

  UChar ch;

  if (collationSource->pos >= collationSource->endp) {
      // Ran off of the end of the main source string.  We're done.
      return UCOL_NO_MORE_CES;
  }
  ch = *collationSource->pos++;

  order = ucmp32_get(t->mapping, ch);                        /* we'll go for slightly slower trie */

  if(order >= UCOL_NOT_FOUND) {                                   /* if a CE is special */
    order = uprv_getSpecialDynamicCE(t, order, collationSource, status);       /* and try to get the special CE */

    if(order == UCOL_NOT_FOUND) {   /* We couldn't find a good CE in the tailoring */
      order = ucol_getNextUCA(ch, collationSource, status);
    }
  } 

  return order; /* return the CE */
}

uint32_t ucol_getDynamicCEs(UColTokenParser *src, tempUCATable *t, UChar *decomp, uint32_t noOfDec, uint32_t *result, uint32_t resultSize, UErrorCode *status) {
  uint32_t resLen = 0;
  collIterate colIt;

  init_collIterate(src->UCA, decomp, noOfDec, &colIt);

  result[resLen] = uprv_ucol_getNextDynamicCE(t, &colIt, status);
  while(result[resLen] != UCOL_NO_MORE_CES) {
    resLen++;
    result[resLen] = uprv_ucol_getNextDynamicCE(t, &colIt, status);
  }

  return resLen;
}
#endif
