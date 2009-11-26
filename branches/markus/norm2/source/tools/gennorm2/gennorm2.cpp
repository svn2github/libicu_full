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
#include <string>
#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/putil.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "n2builder.h"
#include "uoptions.h"
#include "uparse.h"

// TODO: make public in uparse.h
// #define IS_INV_WHITESPACE(c) ((c)==' ' || (c)=='\t' || (c)=='\r' || (c)=='\n')

U_NAMESPACE_BEGIN

UBool beVerbose=FALSE, haveCopyright=TRUE;

U_DEFINE_LOCAL_OPEN_POINTER(LocalStdioFilePointer, FILE, fclose);

IcuToolErrorCode::~IcuToolErrorCode() {
    // Safe because our handleFailure() does not throw exceptions.
    if(isFailure()) { handleFailure(); }
}

void IcuToolErrorCode::handleFailure() const {
    fprintf(stderr, "error at %s: %s\n", location, errorName());
    exit(errorCode);
}

void parseFile(FILE *f, Normalizer2DataBuilder &builder);

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
    LocalPointer<Normalizer2DataBuilder> builder(new Normalizer2DataBuilder(errorCode));
    errorCode.assertSuccess();

    builder->setUnicodeVersion(options[UNICODE_VERSION].value);

    // prepare the filename beginning with the source dir
    std::string filename(options[SOURCEDIR].value);
    int32_t pathLength=filename.length();
    if( pathLength>0 &&
        filename[pathLength-1]!=U_FILE_SEP_CHAR &&
        filename[pathLength-1]!=U_FILE_ALT_SEP_CHAR
    ) {
        filename.push_back(U_FILE_SEP_CHAR);
        pathLength=filename.length();
    }

    for(int i=1; i<argc; ++i) {
        printf("gennorm2: processing %s\n", argv[i]);
        filename.append(argv[i]);
        LocalStdioFilePointer f(fopen(filename.c_str(), "r"));
        if(f==NULL) {
            fprintf(stderr, "gennorm2 error: unable to open %s\n", filename.c_str());
            exit(U_FILE_ACCESS_ERROR);
        }
        builder->setOverrideHandling(Normalizer2DataBuilder::OVERRIDE_PREVIOUS);
        parseFile(f.getAlias(), *builder);
        filename.erase(pathLength);
    }

    return errorCode.get();

#endif
}

#if !UCONFIG_NO_NORMALIZATION

void parseFile(FILE *f, Normalizer2DataBuilder &builder) {
    IcuToolErrorCode errorCode("gennorm2/parseFile()");
    char line[300];
    uint32_t startCP, endCP;
    while(NULL!=fgets(line, (int)sizeof(line), f)) {
        char *comment=(char *)strchr(line, '#');
        if(comment!=NULL) {
            *comment=0;
        }
        u_rtrim(line);
        if(line[0]==0) {
            continue;  // skip empty and comment-only lines
        }
        if(line[0]=='*') {
            continue;  // reserved syntax
        }
        char *delimiter=strchr(line, ':');
        if(delimiter) {
            *delimiter=0;
            u_parseCodePointRange(line, &startCP, &endCP, errorCode);
            errorCode.assertSuccess();

            const char *s=u_skipWhitespace(delimiter+1);
            char *end;
            unsigned long value=strtoul(s, &end, 10);
            if(end<=s || *u_skipWhitespace(end)!=0 || value>=0xff) {
                fprintf(stderr, "gennorm2 error: parsing ccc from %s\n", s);
                exit(U_PARSE_ERROR);
            }
            for(UChar32 c=(UChar32)startCP; c<=(UChar32)endCP; ++c) {
                builder.setCC(c, (uint8_t)value);
            }
            continue;
        }
    }
}

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
