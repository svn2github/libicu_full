/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  gennorm2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov25
*   created by: Markus W. Scherer
*
*   This program reads text files that define Unicode normalization,
*   parses them, and builds a binary data file.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/putil.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "n2builder.h"
#include "uoptions.h"
#include "uparse.h"

U_NAMESPACE_BEGIN

UBool beVerbose=FALSE, haveCopyright=TRUE;

IcuToolErrorCode::~IcuToolErrorCode() {
    // Safe because our handleFailure() does not throw exceptions.
    if(isFailure()) { handleFailure(); }
}

void IcuToolErrorCode::handleFailure() const {
    fprintf(stderr, "error at %s: %s\n", location, errorName());
    exit(errorCode);
}

/* -------------------------------------------------------------------------- */

enum {
    HELP_H,
    HELP_QUESTION_MARK,
    VERBOSE,
    COPYRIGHT,
    SOURCEDIR,
    OUTPUT_FILENAME,
    UNICODE_VERSION
};

static UOption options[]={
    UOPTION_HELP_H,
    UOPTION_HELP_QUESTION_MARK,
    UOPTION_VERBOSE,
    UOPTION_COPYRIGHT,
    UOPTION_SOURCEDIR,
    UOPTION_DEF("output", 'o', UOPT_REQUIRES_ARG),
    UOPTION_DEF("unicode", 'u', UOPT_REQUIRES_ARG)
};

extern "C" int
main(int argc, char* argv[]) {
    U_MAIN_INIT_ARGS(argc, argv);

    /* preset then read command line options */
    options[SOURCEDIR].value="";
    options[UNICODE_VERSION].value=U_UNICODE_VERSION;
    argc=u_parseArgs(argc, argv, sizeof(options)/sizeof(options[HELP_H]), options);

    /* error handling, printing usage message */
    if(argc<0) {
        fprintf(stderr,
            "error in command line argument \"%s\"\n",
            argv[-argc]);
    }
    if(!options[OUTPUT_FILENAME].doesOccur) {
        argc=-1;
    }
    if( argc<0 ||
        options[HELP_H].doesOccur || options[HELP_QUESTION_MARK].doesOccur
    ) {
        /*
         * Broken into chunks because the C89 standard says the minimum
         * required supported string length is 509 bytes.
         */
        fprintf(stderr,
            "Usage: %s [-options] infiles+ -o outputfilename\n"
            "\n"
            "Reads the infiles with normalization data and\n"
            "creates a binary file (outputfilename) with the data.\n"
            "\n",
            argv[0]);
        fprintf(stderr,
            "Options:\n"
            "\t-h or -? or --help  this usage text\n"
            "\t-v or --verbose     verbose output\n"
            "\t-c or --copyright   include a copyright notice\n"
            "\t-u or --unicode     Unicode version, followed by the version like 5.2.0\n");
        fprintf(stderr,
            "\t-s or --sourcedir   source directory, followed by the path\n"
            "\t-o or --output      output filename\n");
        return argc<0 ? U_ILLEGAL_ARGUMENT_ERROR : U_ZERO_ERROR;
    }

    beVerbose=options[VERBOSE].doesOccur;
    haveCopyright=options[COPYRIGHT].doesOccur;

#if UCONFIG_NO_NORMALIZATION

    fprintf(stderr,
        "gennorm2 writes a dummy binary data file "
        "because UCONFIG_NO_NORMALIZATION is set, \n"
        "see icu/source/common/unicode/uconfig.h\n");
    // TODO: generateData(destDir, options[CSOURCE].doesOccur);
    return U_UNSUPPORTED_ERROR;

#else

    IcuToolErrorCode errorCode("gennorm2/main()");
    LocalPointer<Normalizer2DataBuilder> n2builder(new Normalizer2DataBuilder(errorCode));
    errorCode.assertSuccess();

    n2builder->setUnicodeVersion(options[UNICODE_VERSION].value);

    // prepare the filename beginning with the source dir
    char filename[300];
    strcpy(filename, options[SOURCEDIR].value);
    char *basename=filename+strlen(filename);
    if(basename>filename && *(basename-1)!=U_FILE_SEP_CHAR && *(basename-1)!=U_FILE_ALT_SEP_CHAR) {
        *basename++=U_FILE_SEP_CHAR;
    }
    return errorCode.get();

#endif
}

#if !UCONFIG_NO_NORMALIZATION

#endif // !UCONFIG_NO_NORMALIZATION

U_NAMESPACE_END

/*
 * Hey, Emacs, please set the following:
 *
 * Local Variables:
 * indent-tabs-mode: nil
 * End:
 *
 */
