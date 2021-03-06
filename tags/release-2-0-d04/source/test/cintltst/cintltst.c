/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2001, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CINTLTST.C
*
* Modification History:
*        Name                     Description
*     Madhu Katragadda               Creation
*********************************************************************************
*/

/*The main root for C API tests*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "unicode/utypes.h"
#include "unicode/putil.h"

#include "cintltst.h"
#include "umutex.h"

#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/ucnv.h"
#include "unicode/ures.h"
#include "unicode/uclean.h"

#ifdef XP_MAC_CONSOLE
#   include <console.h>
#endif

#define CTST_LEAK_CHECK 1
#ifdef CTST_LEAK_CHECK
U_CFUNC void ctst_freeAll(void);
U_CFUNC void ctst_init(void);
#endif

static char* _testDirectory=NULL;

/*
 *  Forward Declarations
 */
void ctest_setICU_DATA(void);

int main(int argc, const char* const argv[])
{
    int nerrors;
    TestNode *root;

    /* initial check for the default converter */
    UErrorCode errorCode = U_ZERO_ERROR;
    UResourceBundle *rb;
    UConverter *cnv;

#ifdef CTST_LEAK_CHECK
    ctst_init();
#endif

    /* If no ICU_DATA environment was set, try to fake up one. */
    ctest_setICU_DATA();

#ifdef XP_MAC_CONSOLE
    argc = ccommand((char***)&argv);
#endif

    cnv  = ucnv_open(NULL, &errorCode);
    if(cnv != NULL) {
        /* ok */
        ucnv_close(cnv);
    } else {
        fprintf(stderr,
            "*** Failure! The default converter cannot be opened.\n"
            "*** Check the ICU_DATA environment variable and \n"
            "*** check that the data files are present.\n");
        return 1;
    }

    /* try more data */
    cnv = ucnv_open("iso-8859-7", &errorCode);
    if(cnv != 0) {
        /* ok */
        ucnv_close(cnv);
    } else {
        fprintf(stderr,
                "*** Failure! The converter for iso-8859-7 cannot be opened.\n"
                "*** Check the ICU_DATA environment variable and \n"
                "*** check that the data files are present.\n");
        return 1;
    }

    rb = ures_open(NULL, "en", &errorCode);
    if(U_SUCCESS(errorCode)) {
        /* ok */
        ures_close(rb);
    } else {
        fprintf(stderr,
                "*** Failure! The \"en\" locale resource bundle cannot be opened.\n"
                "*** Check the ICU_DATA environment variable and \n"
                "*** check that the data files are present.\n");
        return 1;
    }

    fprintf(stdout, "Default locale for this run is %s\n", uloc_getDefault());

    root = NULL;
    addAllTests(&root);
    nerrors = processArgs(root, argc, argv);
    cleanUpTestTree(root);
#ifdef CTST_LEAK_CHECK
    ctst_freeAll();

    /* To check for leaks */

    u_cleanup(); /* nuke the hashtable.. so that any still-open cnvs are leaked */
#endif

    return nerrors ? 1 : 0;
}

void
ctest_pathnameInContext( char* fullname, int32_t maxsize, const char* relPath )
{
    char mainDirBuffer[1024];
    char* mainDir = NULL;
    const char *dataDirectory = u_getDataDirectory();
    const char inpSepChar = '|';
    char* tmp;
    int32_t lenMainDir;
    int32_t lenRelPath;

#ifdef XP_MAC
    Str255 volName;
    int16_t volNum;
    OSErr err = GetVol( volName, &volNum );
    if (err != noErr)
        volName[0] = 0;
    mainDir = (char*) &(volName[1]);
    mainDir[volName[0]] = 0;
#else
    if (dataDirectory != NULL) {
        strcpy(mainDirBuffer, dataDirectory);
        strcat(mainDirBuffer, ".." U_FILE_SEP_STRING);
    } else {
        mainDirBuffer[0]='\0';
    }
    mainDir = mainDirBuffer;
#endif

    lenMainDir = (int32_t)strlen(mainDir);
    if(lenMainDir > 0 && mainDir[lenMainDir - 1] != U_FILE_SEP_CHAR) {
        mainDir[lenMainDir++] = U_FILE_SEP_CHAR;
        mainDir[lenMainDir] = 0;
    }

    if (relPath[0] == '|')
        relPath++;
    lenRelPath = (int32_t)strlen(relPath);
    if (maxsize < lenMainDir + lenRelPath + 2) {
        fullname[0] = 0;
        return;
    }
    strcpy(fullname, mainDir);
    /*strcat(fullname, U_FILE_SEP_STRING);*/
    strcat(fullname, relPath);
    strchr(fullname, inpSepChar);
    tmp = strchr(fullname, inpSepChar);
    while (tmp) {
        *tmp = U_FILE_SEP_CHAR;
        tmp = strchr(tmp+1, inpSepChar);
    }
}

const char*
ctest_getTestDirectory()
{
    if (_testDirectory == NULL)
    {
        /* always relative to icu/source/data/.. */
        ctest_setTestDirectory("test|testdata|");
    }
    return _testDirectory;
}

void
ctest_setTestDirectory(const char* newDir)
{
    char newTestDir[256];
    ctest_pathnameInContext(newTestDir, (int32_t)sizeof(newTestDir), newDir);
    if(_testDirectory != NULL)
        free(_testDirectory);
    _testDirectory = (char*) ctst_malloc(sizeof(char*) * (strlen(newTestDir) + 1));
    strcpy(_testDirectory, newTestDir);
}



/*  ctest_setICU_DATA  - if the ICU_DATA environment variable is not already
 *                       set, try to deduce the directory in which ICU was built,
 *                       and set ICU_DATA to "icu/source/data" in that location.
 *                       The intent is to allow the tests to have a good chance
 *                       of running without requiring that the user manually set
 *                       ICU_DATA.  Common data isn't a problem, since it is
 *                       picked up via a static (build time) reference, but the
 *                       tests dynamically load some data.
 */
void ctest_setICU_DATA() {
    const char *original_ICU_DATA = getenv("ICU_DATA");

    if (original_ICU_DATA != NULL && *original_ICU_DATA != 0) {
        /*  If the user set ICU_DATA, don't second-guess the person. */
        return;
    }

    /* U_TOPBUILDDIR is set by the makefiles on UNIXes when building cintltst and intltst
	 //              to point to the top of the build hierarchy, which may or
	 //              may not be the same as the source directory, depending on
	 //              the configure options used.  At any rate,
	 //              set the data path to the built data from this directory.
	 //              The value is complete with quotes, so it can be used
	 //              as-is as a string constant.
     */
#if defined (U_TOPBUILDDIR)
    {
        static char env_string[] = U_TOPBUILDDIR  "/data/";
        u_setDataDirectory(env_string);
        return;
    }
#else

    /* On Windows, the file name obtained from __FILE__ includes a full path.
     *             This file is "wherever\icu\source\test\cintltst\cintltst.c"
     *             Change to    "wherever\icu\source\data"
     */
    {
        char p[sizeof(__FILE__) + 10];
        char *pBackSlash;
        int i;

        strcpy(p, __FILE__);
        /* We want to back over three '\' chars.                            */
        /*   Only Windows should end up here, so looking for '\' is safe.   */
        for (i=1; i<=3; i++) {
            pBackSlash = strrchr(p, U_FILE_SEP_CHAR);
            if (pBackSlash != NULL) {
                *pBackSlash = 0;        /* Truncate the string at the '\'   */
            }
        }

        if (pBackSlash != NULL) {
            /* We found and truncated three names from the path.
             *  Now append "source\data" and set the environment
             */
            strcpy(pBackSlash, U_FILE_SEP_STRING "data" U_FILE_SEP_STRING);
            u_setDataDirectory(p);     /*  p is "ICU_DATA=wherever\icu\source\data"    */
            return;
        }
    }
#endif

    /* No location for the data dir was identifiable.
     *   Add other fallbacks for the test data location here if the need arises
     */
}




char *austrdup(const UChar* unichars)
{
    int   length;
    char *newString;

    length    = u_strlen ( unichars );
    /*newString = (char*)malloc  ( sizeof( char ) * 4 * ( length + 1 ) );*/ /* this leaks for now */
    newString = (char*)ctst_malloc  ( sizeof( char ) * 4 * ( length + 1 ) ); /* this shouldn't */

    if ( newString == NULL )
        return NULL;

    u_austrcpy ( newString, unichars );

    return newString;
}

char *aescstrdup(const UChar* unichars){
    int length;
    char *newString,*targetLimit,*target;
    UConverterFromUCallback cb;
    const void *p;
    UErrorCode errorCode = U_ZERO_ERROR;
    UConverter* conv = ucnv_open("US-ASCII",&errorCode);

    length = u_strlen( unichars);
    newString = (char*)ctst_malloc ( sizeof(char) * 8 * (length +1));
    target = newString;
    targetLimit = newString+sizeof(char) * 8 * (length +1);
    ucnv_setFromUCallBack(conv, UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_JAVA, &cb, &p, &errorCode);
    ucnv_fromUnicode(conv,&target,targetLimit, &unichars, (UChar*)(unichars+length),NULL,TRUE,&errorCode);
    ucnv_close(conv);
    *target = '\0';
    return newString;
}



#define CTST_MAX_ALLOC 10000
/* Array used as a queue */
static void * ctst_allocated_stuff[CTST_MAX_ALLOC];
static int ctst_allocated = 0;
static UBool ctst_free = FALSE;

void ctst_init(void) {
    int i;
    for(i=0; i<CTST_MAX_ALLOC; i++) {
        ctst_allocated_stuff[i] = NULL;
    }
}

void *ctst_malloc(size_t size) {
    if(ctst_allocated >= CTST_MAX_ALLOC - 1) {
        ctst_allocated = 0;
        ctst_free = TRUE;
    }
    if(ctst_allocated_stuff[ctst_allocated]) {
        free(ctst_allocated_stuff[ctst_allocated]);
    }
    return ctst_allocated_stuff[ctst_allocated++] = malloc(size);
}

#ifdef CTST_LEAK_CHECK
void ctst_freeAll() {
    int i;
    if(ctst_free == 0) {
        for(i=0; i<ctst_allocated; i++) {
            free(ctst_allocated_stuff[i]);
        }
    } else {
        for(i=0; i<CTST_MAX_ALLOC; i++) {
            free(ctst_allocated_stuff[i]);
        }
    }
}
#endif
