/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2001, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CDANTST.H
*
* Modification History:
*        Name                     Description            
*     Madhu Katragadda            Converted to C
*********************************************************************************/
/**
 * CollationDanishTest is a third level test class.  This tests the locale
 * specific primary, secondary and tertiary rules.  For example, the ignorable
 * character '-' in string "black-bird".  The en_US locale uses the default
 * collation rules as its sorting sequence.
 */

#ifndef _CDANCOLLTST
#define _CDANCOLLTST


#include "cintltst.h"

#define  MAX_TOKEN_LEN  128


    /* performs test with strength PRIMARY */
 static   void TestPrimary(void);

    /* perform test with strength TERTIARY*/
 static  void TestTertiary(void);


    

    

#endif
