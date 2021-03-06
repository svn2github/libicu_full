/*
*******************************************************************************
*                                                                             *
* COPYRIGHT:                                                                  *
*   (C) Copyright International Business Machines Corporation, 1998, 1999     *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.        *
*   US Government Users Restricted Rights - Use, duplication, or disclosure   *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                    *
*                                                                             *
*******************************************************************************
*
* File parse.c
*
* Modification History:
*
*   Date        Name        Description
*   05/26/99    stephen     Creation.
*******************************************************************************
*/

#include "parse.h"
#include "error.h"
#include "uhash.h"
#include "cmemory.h"
#include "read.h"
#include "ufile.h"
#include "ustdio.h"
#include "ustr.h"
#include "list.h"
#include "rblist.h"

/* Node IDs for the state transition table. */
enum ENode {
  eError,
  eInitial,   /* Next: Locale name */
  eGotLoc,    /* Next: { */
  eIdle,      /* Next: Tag name | } */
  eGotTag,    /* Next: { */
  eNode5,     /* Next: Data | Subtag */
  eNode6,     /* Next: } | { | , */
  eList,      /* Next: List data */
  eNode8,     /* Next: , */
  eTagList,   /* Next: Subtag data */
  eNode10,    /* Next: } */
  eNode11,    /* Next: Subtag */
  eNode12,    /* Next: { */
  e2dArray,   /* Next: Data | } */
  eNode14,    /* Next: , | } */
  eNode15,    /* Next: , | } */
  eNode16     /* Next: { | } */
};

/* Action codes for the state transtiion table. */
enum EAction {
  /* Generic actions */
  eNOP       = 0x0100, /* Do nothing */
  eOpen      = 0x0200, /* Open a new locale data block with the data
			  string as the locale name */
  eClose     = 0x0300, /* Close a locale data block */
  eSetTag    = 0x0400, /* Record the last string as the tag name */
  
  /* Comma-delimited lists */
  eBegList   = 0x1100, /* Start a new string list with the last string
			  as the first element */
  eEndList   = 0x1200, /* Close a string list being built */
  eListStr   = 0x1300, /* Record the last string as a data string and
			  increment the index */
  eStr       = 0x1400, /* Record the last string as a singleton string */
  
  /* 2-d lists */
  eBeg2dList = 0x2100, /* Start a new 2d string list with no elements as yet */
  eEnd2dList = 0x2200, /* Close a 2d string list being built */
  e2dStr     = 0x2300, /* Record the last string as a 2d string */
  eNewRow    = 0x2400, /* Start a new row */
  
  /* Tagged lists */
  eBegTagged = 0x3100, /* Start a new tagged list with the last
			  string as the first subtag */
  eEndTagged = 0x3200, /* Close a tagged list being build */
  eSubtag    = 0x3300, /* Record the last string as the subtag */
  eTaggedStr = 0x3400  /* Record the last string as a tagged string */
};

/* A struct which encapsulates a node ID and an action. */
struct STransition {
  enum ENode fNext;
  enum EAction fAction;
};

/* This table describes an ATM (state machine) which parses resource
   bundle text files rather strictly. Each row represents a node. The
   columns of that row represent transitions into other nodes. Most
   transitions are "eError" because most transitions are
   disallowed. For example, if the parser has just seen a tag name, it
   enters node 4 ("eGotTag"). The state table then marks only one
   valid transition, which is into node 5, upon seeing an eOpenBrace
   token. We allow an extra comma after the last element in a
   comma-delimited list (transition from eList to eIdle on
   kCloseBrace). */
static struct STransition gTransitionTable [] = {
  /* kString           kOpenBrace            kCloseBrace         kComma*/
  {eError,eNOP},       {eError,eNOP},        {eError,eNOP},      {eError,eNOP},
  
  {eGotLoc,eOpen},     {eError,eNOP},        {eError,eNOP},      {eError,eNOP},
  {eError,eNOP},       {eIdle,eNOP},         {eError,eNOP},      {eError,eNOP},
  
  {eGotTag,eSetTag},   {eError,eNOP},        {eInitial,eClose},  {eError,eNOP},
  {eError,eNOP},       {eNode5,eNOP},        {eError,eNOP},      {eError,eNOP},
  {eNode6,eNOP},       {e2dArray,eBeg2dList},{eError,eNOP},      {eError,eNOP},
  {eError,eNOP},       {eTagList,eBegTagged},{eIdle,eStr},       {eList,eBegList},
  
  {eNode8,eListStr},   {eError,eNOP},         {eIdle,eEndList},  {eError,eNOP},
  {eError,eNOP},       {eError,eNOP},         {eIdle,eEndList},  {eList,eNOP},
  
  {eNode10,eTaggedStr},{eError,eNOP},         {eError,eNOP},     {eError,eNOP},
  {eError,eNOP},       {eError,eNOP},         {eNode11,eNOP},    {eError,eNOP},
  {eNode12,eNOP},      {eError,eNOP},         {eIdle,eEndTagged},{eError,eNOP},
  {eError,eNOP},       {eTagList,eSubtag},    {eError,eNOP},     {eError,eNOP},

  {eNode14,e2dStr},    {eError,eNOP},         {eNode15,eNOP},    {eError,eNOP},
  {eError,eNOP},       {eError,eNOP},         {eNode15,eNOP},    {e2dArray,eNOP},
  {eError,eNOP},       {e2dArray,eNewRow},    {eIdle,eEnd2dList},{eNode16,eNOP},
  {eError,eNOP},       {e2dArray,eNewRow},    {eIdle,eEnd2dList},{eError,eNOP} 
};

/* Row length is 4 */
#define GETTRANSITION(row,col) (gTransitionTable[col + (row<<2)])

struct SRBItemList*
parse(FileStream *f,
      UErrorCode *status)
{
  struct UFILE *file;
  enum ETokenType type;
  enum ENode node;
  struct STransition t;

  struct UString token;
  struct UString tag;
  struct UString subtag;
  struct UString localeName;
  struct UString keyname;

  struct SRBItem *item;
  struct SRBItemList *list;
  struct SList *current;

  /* Hashtable for keeping track of seen tag names */
  struct UHashtable *data;


  if(FAILURE(*status)) return 0;

  /* setup */

  ustr_init(&token);
  ustr_init(&tag);
  ustr_init(&subtag);
  ustr_init(&localeName);
  ustr_init(&keyname);

  node = eInitial;
  data = 0;
  current = 0;
  item = 0;

  file = u_finit(f, status);
  list = rblist_open(status);
  if(FAILURE(*status)) goto finish;
  
  /* iterate through the stream */
  for(;;) {

    /* get next token from stream */
    type = getNextToken(file, &token, status);
    if(FAILURE(*status)) goto finish;

    switch(type) {
    case tok_EOF:
      *status = (node == eInitial) ? ZERO_ERROR : INVALID_FORMAT_ERROR;
      setErrorText("Unexpected EOF encountered");
      goto finish;
      /*break;*/

    case tok_error:
      *status = INVALID_FORMAT_ERROR;
      goto finish;
      /*break;*/
      
    default:
      break;
    }
    
    t = GETTRANSITION(node, type);
    node = t.fNext;
    
    if(node == eError) {
      *status = INVALID_FORMAT_ERROR;
      goto finish;
    }
    
    switch(t.fAction) {
    case eNOP:
      break;
      
      /* Record the last string as the tag name */
    case eSetTag:
      ustr_cpy(&tag, &token, status);
      if(FAILURE(*status)) goto finish;
      if(uhash_get(data, uhash_hashUString(tag.fChars)) != 0) {
	*status = INVALID_FORMAT_ERROR;
	setErrorText("Duplicate tag name detected");
	goto finish;
      }
      break;

      /* Record a singleton string */
    case eStr:
      if(current != 0) {
	*status = INTERNAL_PROGRAM_ERROR;
	goto finish;
      }
      current = strlist_open(status);
      strlist_add(current, token.fChars, status);
      item = make_rbitem(tag.fChars, current, status);
      rblist_add(list, item, status);
      uhash_put(data, tag.fChars, status);
      if(FAILURE(*status)) goto finish;
      current = 0;
      item = 0;
      break;

      /* Begin a string list */
    case eBegList:
      if(current != 0) {
	*status = INTERNAL_PROGRAM_ERROR;
	goto finish;
      }
      current = strlist_open(status);
      strlist_add(current, token.fChars, status);
      if(FAILURE(*status)) goto finish;
      break;
      
      /* Record a comma-delimited list string */      
    case eListStr:
      strlist_add(current, token.fChars, status);
      if(FAILURE(*status)) goto finish;
      break;
      
      /* End a string list */
    case eEndList:
      uhash_put(data, tag.fChars, status);
      item = make_rbitem(tag.fChars, current, status);
      rblist_add(list, item, status);
      if(FAILURE(*status)) goto finish;
      current = 0;
      item = 0;
      break;
      
    case eBeg2dList:
      if(current != 0) {
	*status = INTERNAL_PROGRAM_ERROR;
	goto finish;
      }
      current = strlist2d_open(status);
      if(FAILURE(*status)) goto finish;
      break;
      
    case eEnd2dList:
      uhash_put(data, tag.fChars, status);
      item = make_rbitem(tag.fChars, current, status);
      rblist_add(list, item, status);
      if(FAILURE(*status)) goto finish;
      current = 0;
      item = 0;
      break;
      
    case e2dStr:
      strlist2d_add(current, token.fChars, status);
      if(FAILURE(*status)) goto finish;
      break;
      
    case eNewRow:
      strlist2d_newRow(current, status);
      if(FAILURE(*status)) goto finish;
      break;
      
    case eBegTagged:
      if(current != 0) {
	*status = INTERNAL_PROGRAM_ERROR;
	goto finish;
      }
      current = taglist_open(status);
      ustr_cpy(&subtag, &token, status);
      if(FAILURE(*status)) goto finish;
      break;
      
    case eEndTagged:
      uhash_put(data, tag.fChars, status);
      item = make_rbitem(tag.fChars, current, status);
      rblist_add(list, item, status);
      if(FAILURE(*status)) goto finish;
      current = 0;
      item = 0;
      break;
      
    case eTaggedStr:
      taglist_add(current, subtag.fChars, token.fChars, status);
      if(FAILURE(*status)) goto finish;
      break;
      
      /* Record the last string as the subtag */
    case eSubtag:
      ustr_cpy(&subtag, &token, status);
      if(FAILURE(*status)) goto finish;
      if(taglist_get(current, subtag.fChars, status) != 0) {
	*status = INVALID_FORMAT_ERROR;
	setErrorText("Duplicate subtag found in tagged list");
	goto finish;
      }
      break;
      
    case eOpen:
      if(data != 0) {
	*status = INTERNAL_PROGRAM_ERROR;
	goto finish;
      }
      ustr_cpy(&localeName, &token, status);
      rblist_setlocale(list, localeName.fChars, status);
      if(FAILURE(*status)) goto finish;
      data = uhash_open(uhash_hashUString, status);
      break;
      
    case eClose:
      if(data == 0) {
	*status = INTERNAL_PROGRAM_ERROR;
	goto finish;
      }
      break;
    }
  }

 finish:

  /* clean  up */
  
  if(data != 0)
    uhash_close(data);

  if(item != 0)
    icu_free(item);

  ustr_deinit(&token);
  ustr_deinit(&tag);
  ustr_deinit(&subtag);
  ustr_deinit(&localeName);
  ustr_deinit(&keyname);

  if(file != 0)
    u_fclose(file);

  return list;
}
