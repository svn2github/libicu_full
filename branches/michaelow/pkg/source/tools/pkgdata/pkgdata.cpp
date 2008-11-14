/******************************************************************************
 *   Copyright (C) 2000-2008, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 *   file name:  pkgdata.c
 *   encoding:   ANSI X3.4 (1968)
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2000may15
 *   created by: Steven \u24C7 Loomis
 *
 *   This program packages the ICU data into different forms
 *   (DLL, common data, etc.)
 */

/*
 * We define _XOPEN_SOURCE so that we can get popen and pclose.
 */
#if !defined(_XOPEN_SOURCE)
#if __STDC_VERSION__ >= 199901L
/* It is invalid to compile an XPG3, XPG4, XPG4v2 or XPG5 application using c99 on Solaris */
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 4
#endif
#endif

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "toolutil.h"
#include "unicode/uclean.h"
#include "unewdata.h"
#include "uoptions.h"
#include "putilimp.h"
#include "package.h"
#include "pkg_icu.h"
#include "pkg_genc.h"


#if U_HAVE_POPEN
# include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>

U_CDECL_BEGIN
#include "pkgtypes.h"
#include "makefile.h"
U_CDECL_END

static void loadLists(UPKGOptions *o, UErrorCode *status);

static int32_t pkg_executeOptions(UPKGOptions *o);

/* always have this fcn, just might not do anything */
static void fillInMakefileFromICUConfig(UOption *option);

/* This sets the modes that are available */
static struct
{
    const char *name, *alt_name;
    UPKGMODE   *fcn;
    const char *desc;
} modes[] =
{
    { "files", 0, pkg_mode_files, "Uses raw data files (no effect). Installation copies all files to the target location." },
#ifdef U_MAKE_IS_NMAKE
    { "dll",    "library", pkg_mode_windows,    "Generates one common data file and one shared library, <package>.dll"},
    { "common", "archive", pkg_mode_windows,    "Generates just the common file, <package>.dat"},
    { "static", "static",  pkg_mode_windows,    "Generates one statically linked library, " LIB_PREFIX "<package>" UDATA_LIB_SUFFIX }
#else /*#ifdef U_MAKE_IS_NMAKE*/
#ifdef UDATA_SO_SUFFIX
    { "dll",    "library", pkg_mode_dll,    "Generates one shared library, <package>" UDATA_SO_SUFFIX },
#endif
    { "common", "archive", pkg_mode_common, "Generates one common data file, <package>.dat" },
    { "static", "static",  pkg_mode_static, "Generates one statically linked library, " LIB_PREFIX "<package>" UDATA_LIB_SUFFIX }
#endif /*#ifdef U_MAKE_IS_NMAKE*/
};

enum {
    NAME,
    BLDOPT,
    MODE,
    HELP,
    HELP_QUESTION_MARK,
    VERBOSE,
    COPYRIGHT,
    COMMENT,
    DESTDIR,
    CLEAN,
    NOOUTPUT,
    REBUILD,
    TEMPDIR,
    INSTALL,
    SOURCEDIR,
    ENTRYPOINT,
    REVISION,
    MAKEARG,
    FORCE_PREFIX,
    LIBNAME,
    QUIET
};

static UOption options[]={
    /*00*/    UOPTION_DEF( "name",    'p', UOPT_REQUIRES_ARG),
    /*01*/    UOPTION_DEF( "bldopt",  'O', UOPT_REQUIRES_ARG), /* on Win32 it is release or debug */
    /*02*/    UOPTION_DEF( "mode",    'm', UOPT_REQUIRES_ARG),
    /*03*/    UOPTION_HELP_H,                                   /* -h */
    /*04*/    UOPTION_HELP_QUESTION_MARK,                       /* -? */
    /*05*/    UOPTION_VERBOSE,                                  /* -v */
    /*06*/    UOPTION_COPYRIGHT,                                /* -c */
    /*07*/    UOPTION_DEF( "comment", 'C', UOPT_REQUIRES_ARG),
    /*08*/    UOPTION_DESTDIR,                                  /* -d */
    /*09*/    UOPTION_DEF( "clean",   'k', UOPT_NO_ARG),
    /*10*/    UOPTION_DEF( "nooutput",'n', UOPT_NO_ARG),
    /*11*/    UOPTION_DEF( "rebuild", 'F', UOPT_NO_ARG),
    /*12*/    UOPTION_DEF( "tempdir", 'T', UOPT_REQUIRES_ARG),
    /*13*/    UOPTION_DEF( "install", 'I', UOPT_REQUIRES_ARG),
    /*14*/    UOPTION_SOURCEDIR ,
    /*15*/    UOPTION_DEF( "entrypoint", 'e', UOPT_REQUIRES_ARG),
    /*16*/    UOPTION_DEF( "revision", 'r', UOPT_REQUIRES_ARG),
    /*17*/    UOPTION_DEF( "makearg", 'M', UOPT_REQUIRES_ARG),
    /*18*/    UOPTION_DEF( "force-prefix", 'f', UOPT_NO_ARG),
    /*19*/    UOPTION_DEF( "libname", 'L', UOPT_REQUIRES_ARG),
    /*20*/    UOPTION_DEF( "quiet", 'q', UOPT_NO_ARG)
};

enum {
    GENCCODE_ASSEMBLY_TYPE,
    SO_EXT,
    SOBJ_EXT,
    A_EXT,
    LIBPREFIX,
    LIB_EXT_ORDER,
    COMPILER,
    LIBFLAGS,
    GENLIB,
    LDICUDTFLAGS,
    LD_SONAME,
    RPATH_FLAGS,
    BIR_FLAGS,
    AR,
    ARFLAGS,
    RANLIB,
    PKGDATA_FLAGS_SIZE
};

static char pkgDataFlags[PKGDATA_FLAGS_SIZE][512];

static int32_t pkg_readInFlags(const char* fileName);

static void pkg_checkFlag(UPKGOptions *o);

const char options_help[][320]={
    "Set the data name",
#ifdef U_MAKE_IS_NMAKE
    "The directory where the ICU is located (e.g. <ICUROOT> which contains the bin directory)",
#else
    "Specify options for the builder. (Autdetected if icu-config is available)",
#endif
    "Specify the mode of building (see below; default: common)",
    "This usage text",
    "This usage text",
    "Make the output verbose",
    "Use the standard ICU copyright",
    "Use a custom comment (instead of the copyright)",
    "Specify the destination directory for files",
    "Clean out generated & temporary files",
    "Suppress output of data, just list files to be created",
    "Force rebuilding of all data",
    "Specify temporary dir (default: output dir)",
    "Install the data (specify target)",
    "Specify a custom source directory",
    "Specify a custom entrypoint name (default: short name)",
    "Specify a version when packaging in DLL or static mode",
    "Pass the next argument to make(1)",
    "Add package to all file names if not present",
    "Library name to build (if different than package name)",
    "Quite mode. (e.g. Do not output a readme file for static libraries)"
};

const char  *progname = "PKGDATA";

int
main(int argc, char* argv[]) {
    int result = 0;
    FileStream  *out;
    UPKGOptions  o;
    CharList    *tail;
    UBool        needsHelp = FALSE;
    UErrorCode   status = U_ZERO_ERROR;
    char         tmp[1024];
    int32_t i;

    U_MAIN_INIT_ARGS(argc, argv);

    progname = argv[0];

    options[MODE].value = "common";
    options[MAKEARG].value = "";

    /* read command line options */
    argc=u_parseArgs(argc, argv, sizeof(options)/sizeof(options[0]), options);

    /* error handling, printing usage message */
    /* I've decided to simply print an error and quit. This tool has too
    many options to just display them all of the time. */

    if(options[HELP].doesOccur || options[HELP_QUESTION_MARK].doesOccur) {
        needsHelp = TRUE;
    }
    else {
        if(!needsHelp && argc<0) {
            fprintf(stderr,
                "%s: error in command line argument \"%s\"\n",
                progname,
                argv[-argc]);
            fprintf(stderr, "Run '%s --help' for help.\n", progname);
            return 1;
        }

        if(!options[BLDOPT].doesOccur) {
            /* Try to fill in from icu-config or equivalent */
            fillInMakefileFromICUConfig(&options[1]);
        }
#ifdef U_MAKE_IS_NMAKE
        else {
            fprintf(stderr, "Warning: You are using the deprecated -O option\n"
                            "\tYou can fix this warning by installing pkgdata, gencmn and genccode\n"
                            "\tinto the same directory and not specifying the -O option to pkgdata.\n");
        }
#endif

        if(!options[BLDOPT].doesOccur) {
            fprintf(stderr, " required parameter is missing: -O is required \n");
            fprintf(stderr, "Run '%s --help' for help.\n", progname);
            return 1;
        }

        if(!options[NAME].doesOccur) /* -O we already have - don't report it. */
        {
            fprintf(stderr, " required parameter -p is missing \n");
            fprintf(stderr, "Run '%s --help' for help.\n", progname);
            return 1;
        }

        if(argc == 1) {
            fprintf(stderr,
                "No input files specified.\n"
                "Run '%s --help' for help.\n", progname);
            return 1;
        }
    }   /* end !needsHelp */

    if(argc<0 || needsHelp  ) {
        fprintf(stderr,
            "usage: %s [-options] [-] [packageFile] \n"
            "\tProduce packaged ICU data from the given list(s) of files.\n"
            "\t'-' by itself means to read from stdin.\n"
            "\tpackageFile is a text file containing the list of files to package.\n",
            progname);

        fprintf(stderr, "\n options:\n");
        for(i=0;i<(sizeof(options)/sizeof(options[0]));i++) {
            fprintf(stderr, "%-5s -%c %s%-10s  %s\n",
                (i<1?"[REQ]":""),
                options[i].shortName,
                options[i].longName ? "or --" : "     ",
                options[i].longName ? options[i].longName : "",
                options_help[i]);
        }

        fprintf(stderr, "modes: (-m option)\n");
        for(i=0;i<(sizeof(modes)/sizeof(modes[0]));i++) {
            fprintf(stderr, "   %-9s ", modes[i].name);
            if (modes[i].alt_name) {
                fprintf(stderr, "/ %-9s", modes[i].alt_name);
            } else {
                fprintf(stderr, "           ");
            }
            fprintf(stderr, "  %s\n", modes[i].desc);
        }
        return 1;
    }

    /* OK, fill in the options struct */
    uprv_memset(&o, 0, sizeof(o));

    o.mode      = options[MODE].value;
    o.version   = options[REVISION].doesOccur ? options[REVISION].value : 0;
    o.makeArgs  = options[MAKEARG].value;

    o.fcn = NULL;

    for(i=0;i<sizeof(modes)/sizeof(modes[0]);i++) {
        if(!uprv_strcmp(modes[i].name, o.mode)) {
            o.fcn = modes[i].fcn;
            break;
        } else if (modes[i].alt_name && !uprv_strcmp(modes[i].alt_name, o.mode)) {
            o.mode = modes[i].name;
            o.fcn = modes[i].fcn;
            break;
        }
    }

    if(o.fcn == NULL) {
        fprintf(stderr, "Error: invalid mode '%s' specified. Run '%s --help' to list valid modes.\n", o.mode, progname);
        return 1;
    }

    o.shortName = options[NAME].value;
    {
        int32_t len = (int32_t)uprv_strlen(o.shortName);
        char *csname, *cp;
        const char *sp;

        cp = csname = (char *) uprv_malloc((len + 1 + 1) * sizeof(*o.cShortName));
        if (*(sp = o.shortName)) {
            *cp++ = isalpha(*sp) ? * sp : '_';
            for (++sp; *sp; ++sp) {
                *cp++ = isalnum(*sp) ? *sp : '_';
            }
        }
        *cp = 0;

        o.cShortName = csname;
    }

    if(options[LIBNAME].doesOccur) { /* get libname from shortname, or explicit -L parameter */
      o.libName = options[LIBNAME].value;
    } else {
      o.libName = o.shortName;
    }

    if(options[QUIET].doesOccur) {
      o.quiet = TRUE;
    } else {
      o.quiet = FALSE;
    }

    o.verbose   = options[VERBOSE].doesOccur;
#ifdef U_MAKE_IS_NMAKE /* format is R:pathtoICU or D:pathtoICU */
    {
        char *pathstuff = (char *)options[BLDOPT].value;
        if(options[1].value[uprv_strlen(options[BLDOPT].value)-1] == '\\') {
            pathstuff[uprv_strlen(options[BLDOPT].value)-1] = '\0';
        }
        if(*pathstuff == PKGDATA_DERIVED_PATH || *pathstuff == 'R' || *pathstuff == 'D') {
            o.options = pathstuff;
            pathstuff++;
            if(*pathstuff == ':') {
                *pathstuff = '\0';
                pathstuff++;
            }
            else {
                fprintf(stderr, "Error: invalid windows build mode, should be R (release) or D (debug).\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Error: invalid windows build mode, should be R (release) or D (debug).\n");
            return 1;
        }
        o.icuroot = pathstuff;
        if (o.verbose) {
            fprintf(stdout, "# ICUROOT is %s\n", o.icuroot);
        }
    }
#else /* on UNIX, we'll just include the file... */
    o.options   = options[BLDOPT].value;
#endif
    if(options[COPYRIGHT].doesOccur) {
        o.comment = U_COPYRIGHT_STRING;
    } else if (options[COMMENT].doesOccur) {
        o.comment = options[COMMENT].value;
    }

    if( options[DESTDIR].doesOccur ) {
        o.targetDir = options[DESTDIR].value;
    } else {
        o.targetDir = ".";  /* cwd */
    }

    o.clean     = options[CLEAN].doesOccur;
    o.nooutput  = options[NOOUTPUT].doesOccur;
    o.rebuild   = options[REBUILD].doesOccur;

    if( options[TEMPDIR].doesOccur ) {
        o.tmpDir    = options[TEMPDIR].value;
    } else {
        o.tmpDir    = o.targetDir;
    }

    if( options[INSTALL].doesOccur ) {
        o.install  = options[INSTALL].value;
    }

    if( options[SOURCEDIR].doesOccur ) {
        o.srcDir   = options[SOURCEDIR].value;
    } else {
        o.srcDir   = ".";
    }

    if( options[ENTRYPOINT].doesOccur ) {
        o.entryName = options[ENTRYPOINT].value;
    } else {
        o.entryName = o.cShortName;
    }

    /* OK options are set up. Now the file lists. */
    tail = NULL;
    for( i=1; i<argc; i++) {
        if ( !uprv_strcmp(argv[i] , "-") ) {
            /* stdin */
            if( o.hadStdin == TRUE ) {
                fprintf(stderr, "Error: can't specify '-' twice!\n"
                    "Run '%s --help' for help.\n", progname);
                return 1;
            }
            o.hadStdin = TRUE;
        }

        o.fileListFiles = pkg_appendToList(o.fileListFiles, &tail, uprv_strdup(argv[i]));
    }

    /* load the files */
    loadLists(&o, &status);
    if( U_FAILURE(status) ) {
        fprintf(stderr, "error loading input file lists: %s\n", u_errorName(status));
        return 2;
    }

    o.makeFile = "DO_NOT_USE";


    if(o.nooutput == TRUE) {
        return 0; /* nothing to do. */
    }

    result = pkg_executeOptions(&o);

    return result;
}

#ifdef U_WINDOWS
#define LINK_CMD "link.exe /nologo /release /out:"
#define LINK_FLAGS "/DLL /NOENTRY /MANIFEST:NO  /base:0x4ad00000 /implib:"
#define LIB_CMD "LIB.exe /nologo /out:"
#define LIB_FILE "icudt.lib"
#define LIB_EXT ".lib"
#define DLL_EXT ".dll"
#else
#define LN_CMD "ln -s"
#define RM_CMD "rm -f"
#endif
#define ICUDATA_RES_FILE "icudata.res"

#define MODE_COMMON 'c'
#define MODE_STATIC 's'
#define MODE_DLL    'd'
#define MODE_FILES  'f'

static int32_t pkg_executeOptions(UPKGOptions *o) {
    int32_t result = 0;
    const char mode = o->mode[0];
    char datFileName[256] = "";
    char datFileNamePath[2048] = "";
    char cmd[1024];

    if (mode == MODE_FILES) {
        // TODO: Copy files over
    } else /* if (mode == MODE_COMMON || mode == MODE_STATIC || mode == MODE_DLL) */ {
        if (o->tmpDir != NULL) {
            uprv_strcpy(datFileNamePath, o->tmpDir);
            uprv_strcat(datFileNamePath, U_FILE_SEP_STRING);
        }

        uprv_strcpy(datFileName, o->shortName);
        uprv_strcat(datFileName, ".dat");

        uprv_strcat(datFileNamePath, datFileName);

        result = writePackageDatFile(datFileNamePath, o->comment, o->srcDir, o->fileListFiles->str, NULL, U_IS_BIG_ENDIAN ? 'b' : 'l');
        if (result != 0) {
            fprintf(stderr,"Error writing package dat file.\n");
            return result;
        }
        if (mode == MODE_COMMON) {
            char targetFileNamePath[2048] = "";

            if (o->targetDir != NULL) {
                uprv_strcpy(targetFileNamePath, o->targetDir);
                uprv_strcat(targetFileNamePath, U_FILE_SEP_STRING);
                uprv_strcat(targetFileNamePath, datFileName);

                /* Move the dat file created to the target directory. */
                rename(datFileNamePath, targetFileNamePath);

                return result;
            }
        } else /* if (mode[0] == MODE_STATIC || mode[0] == MODE_DLL) */ {
            char gencFilePath[512];

            if (pkg_readInFlags(o->options) < 0) {
                fprintf(stderr,"Unable to open or read \"%s\" option file.\n", o->options);
                return -1;
            }

            if (pkgDataFlags[GENCCODE_ASSEMBLY_TYPE][0] != 0) {
                const char* genccodeAssembly = pkgDataFlags[GENCCODE_ASSEMBLY_TYPE];

                /* Offset genccodeAssembly by 3 because "-a " */
                if (checkAssemblyHeaderName(genccodeAssembly+3)) {
                    /* TODO: tmpDir might be NULL */
                    writeAssemblyCode(datFileNamePath, o->tmpDir, o->entryName, NULL, gencFilePath);
                } else {
                    fprintf(stderr,"Assembly type \"%s\" is unknown.\n", genccodeAssembly);
                    return -1;
                }
            } else {
#if defined(U_WINDOWS) || defined(U_LINUX)
                writeObjectCode(datFileNamePath, o->tmpDir, o->entryName, NULL, NULL, gencFilePath);
#else
                fprintf(stderr,"Assembly type needs to be specified in config file: %s\n", o->options);
                return -1;
#endif
            }

#ifdef U_WINDOWS
            if (mode == MODE_STATIC) {
                char staticLibFilePath[512] = "";

                if (o->tmpDir != NULL || o->targetDir != NULL) {
                    uprv_strcpy(staticLibFilePath, o->tmpDir != NULL ? o->tmpDir : o->targetDir);
                    uprv_strcat(staticLibFilePath, U_FILE_SEP_STRING);
                }

                uprv_strcat(staticLibFilePath, o->entryName);
                uprv_strcat(staticLibFilePath, LIB_EXT);

                sprintf(cmd, "%s\"%s\" \"%s\"",
                        LIB_CMD,
                        staticLibFilePath,
                        gencFilePath);
            } else if (mode == MODE_DLL) {
                char dllFilePath[512] = "";
                char libFilePath[512] = "";
                char resFilePath[512] = "";

                if (o->tmpDir != NULL || o->targetDir != NULL) {
#ifdef CYGWINMSVC
                    uprv_strcpy(dllFilePath, o->targetDir != NULL ? o->targetDir : o->tmpDir);
#else
                    if (o->srcDir != NULL) {
                        uprv_strcpy(dllFilePath, o->srcDir);
                    } else {
                        uprv_strcpy(dllFilePath, o->tmpDir != NULL ? o->tmpDir : o->targetDir);
                    }
#endif
                    uprv_strcat(dllFilePath, U_FILE_SEP_STRING);
                    uprv_strcpy(libFilePath, dllFilePath);

                    uprv_strcpy(resFilePath, o->tmpDir != NULL ? o->tmpDir : o->targetDir);
                    uprv_strcat(resFilePath, U_FILE_SEP_STRING);
                }

                uprv_strcat(dllFilePath, o->entryName);
                uprv_strcat(dllFilePath, DLL_EXT);
                uprv_strcat(libFilePath, LIB_FILE);
                uprv_strcat(resFilePath, ICUDATA_RES_FILE);

                if (!T_FileStream_file_exists(resFilePath)) {
                    uprv_memset(resFilePath, 0, sizeof(resFilePath));
                }

                sprintf(cmd, "%s\"%s\" %s\"%s\" \"%s\" \"%s\"",
                        LINK_CMD,
                        dllFilePath,
                        LINK_FLAGS,
                        libFilePath,
                        gencFilePath,
                        resFilePath
                        );
            }
#else
#ifdef U_CYGWIN
            char libFileCygwin[512];
            sprintf(libFileCygwin, "cyg%s",
                    o->libName);
#endif
            char libFile[512] = "";
            char targetDir[512] = "";
            char tempObjectFile[512] = "";

            if (o->targetDir != NULL) {
                uprv_strcpy(targetDir, o->targetDir);
                uprv_strcat(targetDir, U_FILE_SEP_STRING);
            }

            uprv_strcpy(libFile, pkgDataFlags[LIBPREFIX]);
            uprv_strcat(libFile, o->libName);

            /* Remove the ending .s and replace it with .o for the new object file. */
            for(int32_t i = 0; i < sizeof(tempObjectFile); i++) {
                tempObjectFile[i] = gencFilePath[i];
                if (i > 0 && gencFilePath[i] == 0 && gencFilePath[i-1] == 's') {
                    tempObjectFile[i-1] = 'o';
                    break;
                }
            }

            /* Generate the object file. */
            sprintf(cmd, "%s %s -o %s %s",
                    pkgDataFlags[COMPILER],
                    pkgDataFlags[LIBFLAGS],
                    tempObjectFile,
                    gencFilePath);

            result = system(cmd);
            if (result != 0) {
                return result;
            }

            if (mode == MODE_STATIC) {
                sprintf(cmd, "%s %s %s%s.%s %s",
                        pkgDataFlags[AR],
                        pkgDataFlags[ARFLAGS],
                        targetDir,
                        libFile,
                        pkgDataFlags[A_EXT],
                        tempObjectFile);

            } else /* if (mode == MODE_DLL) */ {
                UBool reverseExt = FALSE;
                char version_major[10] = "";
                char libFileVersion[512] = "";
                char libFileVersionTmp[512] = "";
                char libFileMajor[512] = "";

                /* Certain platforms have different library extension ordering. (e.g. libicudata.##.so vs libicudata.so.##) */
                if (pkgDataFlags[LIB_EXT_ORDER][uprv_strlen(pkgDataFlags[LIB_EXT_ORDER])-1] == pkgDataFlags[SO_EXT][uprv_strlen(pkgDataFlags[SO_EXT])-1]) {
                    reverseExt = TRUE;
                }

                /* Get the version major number. */
                if (o->version != NULL) {
                    for (int32_t i = 0;i < sizeof(version_major);i++) {
                        if (o->version[i] == '.') {
                            version_major[i] = 0;
                            break;
                        }
                        version_major[i] = o->version[i];
                    }
                }

#ifdef U_CYGWIN
                sprintf(libFileCygwin, "cyg%s%s.%s",
                        o->libName,
                        version_major,
                        pkgDataFlags[SO_EXT]);

                sprintf(pkgDataFlags[SO_EXT], "%s.%s",
                        pkgDataFlags[SO_EXT],
                        pkgDataFlags[A_EXT]);
#else
                sprintf(libFileVersionTmp, "%s%s%s.%s",
                        libFile,
                        pkgDataFlags[LIB_EXT_ORDER][0] == '.' ? "." : "",
                        reverseExt ? o->version : pkgDataFlags[SOBJ_EXT],
                        reverseExt ? pkgDataFlags[SOBJ_EXT] : o->version);
#endif
                sprintf(libFileMajor, "%s%s%s.%s",
                        libFile,
                        pkgDataFlags[LIB_EXT_ORDER][0] == '.' ? "." : "",
                        reverseExt ? version_major : pkgDataFlags[SO_EXT],
                        reverseExt ? pkgDataFlags[SO_EXT] : version_major);

#ifdef U_CYGWIN
                uprv_strcpy(libFileVersionTmp, libFileMajor);
#endif

                pkg_checkFlag(o);

                /* Generate the library file. */
#ifdef U_CYGWIN
                sprintf(cmd, "%s %s%s%s -o %s%s %s %s%s %s %s",
#else
                sprintf(cmd, "%s %s -o %s%s %s %s%s %s %s",
#endif
                        pkgDataFlags[GENLIB],
                        pkgDataFlags[LDICUDTFLAGS],
                        targetDir,
                        libFileVersionTmp,
#ifdef U_CYGWIN
                        targetDir, libFileCygwin,
#endif
                        tempObjectFile,
                        pkgDataFlags[LD_SONAME],
                        pkgDataFlags[LD_SONAME][0] == 0 ? "" : libFileMajor,
                        pkgDataFlags[RPATH_FLAGS],
                        pkgDataFlags[BIR_FLAGS]);

                result = system(cmd);
                if (result != 0) {
                    return result;
                }

                /* Certain platforms uses archive library. (e.g. AIX) */
                if (uprv_strcmp(pkgDataFlags[SOBJ_EXT], pkgDataFlags[SO_EXT]) != 0 && uprv_strcmp(pkgDataFlags[A_EXT], pkgDataFlags[SO_EXT]) == 0) {
                    sprintf(libFileVersion, "%s%s%s.%s",
                            libFile,
                            pkgDataFlags[LIB_EXT_ORDER][0] == '.' ? "." : "",
                            reverseExt ? o->version : pkgDataFlags[SO_EXT],
                            reverseExt ? pkgDataFlags[SO_EXT] : o->version);

                    sprintf(cmd, "%s %s %s%s %s%s",
                            pkgDataFlags[AR],
                            pkgDataFlags[ARFLAGS],
                            targetDir,
                            libFileVersion,
                            targetDir,
                            libFileVersionTmp);

                    result = system(cmd);
                    if (result != 0) {
                        return result;
                    }

                    /* Remove unneeded library file. */
                    sprintf(cmd, "%s %s%s",
                            RM_CMD,
                            targetDir,
                            libFileVersionTmp);

                    result = system(cmd);
                    if (result != 0) {
                        return result;
                    }

                } else {
                    uprv_strcpy(libFileVersion, libFileVersionTmp);
                }

                /* Create symbolic links for the final library file. */
#ifndef U_CYGWIN
                sprintf(cmd, "%s %s%s && %s %s%s %s%s",
                        RM_CMD,
                        targetDir, libFileMajor,
                        LN_CMD,
                        targetDir, libFileVersion,
                        targetDir, libFileMajor);
                result = system(cmd);
                if (result != 0) {
                    return result;
                }
#endif
                sprintf(cmd, "%s %s%s.%s && %s %s%s %s%s.%s",
                        RM_CMD,
                        targetDir, libFile, pkgDataFlags[SO_EXT],
                        LN_CMD,
                        targetDir, libFileVersion,
                        targetDir, libFile, pkgDataFlags[SO_EXT]);
            }
#endif
        }

        result = system(cmd);
    }
    return result;
}

/*
 * Get the position after the '=' character.
 */
static int32_t flagOffset(const char *buffer, int32_t bufferSize) {
    int32_t offset = 0;

    for (offset = 0; offset < bufferSize;offset++) {
        if (buffer[offset] == '=') {
            offset++;
            break;
        }
    }

    if (offset == bufferSize || (offset - 1) == bufferSize) {
        offset = 0;
    }

    return offset;
}
/*
 * Extract the setting after the '=' and store it in flag excluding the newline character.
 */
static void extractFlag(char* buffer, int32_t bufferSize, char* flag) {
    char *pBuffer;
    int32_t offset;
    UBool bufferWritten = FALSE;

    if (buffer[0] != 0) {
        /* Get the offset (i.e. position after the '=') */
        offset = flagOffset(buffer, bufferSize);
        pBuffer = buffer+offset;
        for(int32_t i = 0;;i++) {
            if (pBuffer[i+1] == 0) {
                /* Indicates a new line character. End here. */
                flag[i] = 0;
                break;
            }
            flag[i] = pBuffer[i];
            if (i == 0) {
                bufferWritten = TRUE;
            }
        }
    }

    if (!bufferWritten) {
        flag[0] = 0;
    }
}

static void pkg_checkFlag(UPKGOptions *o) {
    char *flag;
    int32_t length;
    char tmpbuffer[512];
#ifdef U_AIX
    const char MAP_FILE_EXT[] = ".map";
    FileStream *f = NULL;
    char mapFile[512] = "";
    int32_t start = -1;
    int32_t count = 0;

    flag = pkgDataFlags[BIR_FLAGS];
    length = uprv_strlen(pkgDataFlags[BIR_FLAGS]);

    for (int32_t i = 0; i < length; i++) {
        if (flag[i] == MAP_FILE_EXT[count]) {
            if (count == 0) {
                start = i;
            }
            count++;
        } else {
            count = 0;
        }

        if (count == uprv_strlen(MAP_FILE_EXT)) {
            break;
        }
    }

    if (start >= 0) {
        int32_t index = 0;
        for (int32_t i = 0;;i++) {
            if (i == start) {
                for (int32_t n = 0;;n++) {
                    if (o->shortName[n] == 0) {
                        break;
                    }
                    tmpbuffer[index++] = o->shortName[n];
                }
            }

            tmpbuffer[index++] = flag[i];

            if (flag[i] == 0) {
                break;
            }
        }

        uprv_memset(flag, 0, length);
        uprv_strcpy(flag, tmpbuffer);

        uprv_strcpy(mapFile, o->shortName);
        uprv_strcat(mapFile, MAP_FILE_EXT);

        f = T_FileStream_open(mapFile, "w");
        if (f == NULL) {
            fprintf(stderr,"Unable to create map file: %s.\n", mapFile);
            return;
        }

        sprintf(tmpbuffer, "%s_dat ", o->entryName);

        T_FileStream_writeLine(f, tmpbuffer);

        T_FileStream_close(f);
    }
#elif defined(U_CYGWIN)
    flag = pkgDataFlags[GENLIB];
    length = uprv_strlen(pkgDataFlags[GENLIB]);

    int32_t position = length - 1;

    for(;position >= 0;position--) {
        if (flag[position] == '=') {
            break;
        }
    }

    uprv_memset(flag + position, 0, length - position);
#endif
}

#define MAX_BUFFER_SIZE 2048
/*
 * Reads in the given fileName and stores the data in pkgDataFlags.
 */
static int32_t pkg_readInFlags(const char *fileName) {
    char buffer[MAX_BUFFER_SIZE];
    char *pBuffer;
    int32_t result = 0;

#ifdef U_WINDOWS
    /* Zero out the flags since it is not being used. */
    for (int32_t i = 0; i < PKGDATA_FLAGS_SIZE; i++) {
        pkgDataFlags[i][0] = 0;
    }

#else
    FileStream *f = T_FileStream_open(fileName, "r");
    if (f == NULL) {
        return -1;
    }

    for (int32_t i = 0; i < PKGDATA_FLAGS_SIZE; i++) {
        if (T_FileStream_readLine(f, buffer, MAX_BUFFER_SIZE) == NULL) {
            result = -1;
            break;
        }

        extractFlag(buffer, MAX_BUFFER_SIZE, pkgDataFlags[i]);
    }

    T_FileStream_close(f);
#endif

    return result;
}

#if 0
{
    rc = system(cmd);

    if(rc < 0) {
        fprintf(stderr, "# Failed, rc=%d\n", rc);
    }

    return rc < 128 ? rc : (rc >> 8);
}
#endif


static void loadLists(UPKGOptions *o, UErrorCode *status)
{
    CharList   *l, *tail = NULL, *tail2 = NULL;
    FileStream *in;
    char        line[16384];
    char       *linePtr, *lineNext;
    const uint32_t   lineMax = 16300;
    char        tmp[1024];
    char       *s;
    int32_t     ln=0; /* line number */

    for(l = o->fileListFiles; l; l = l->next) {
        if(o->verbose) {
            fprintf(stdout, "# Reading %s..\n", l->str);
        }
        /* TODO: stdin */
        in = T_FileStream_open(l->str, "r"); /* open files list */

        if(!in) {
            fprintf(stderr, "Error opening <%s>.\n", l->str);
            *status = U_FILE_ACCESS_ERROR;
            return;
        }

        while(T_FileStream_readLine(in, line, sizeof(line))!=NULL) { /* for each line */
            ln++;
            if(uprv_strlen(line)>lineMax) {
                fprintf(stderr, "%s:%d - line too long (over %d chars)\n", l->str, (int)ln, (int)lineMax);
                exit(1);
            }
            /* remove spaces at the beginning */
            linePtr = line;
            while(isspace(*linePtr)) {
                linePtr++;
            }
            s=linePtr;
            /* remove trailing newline characters */
            while(*s!=0) {
                if(*s=='\r' || *s=='\n') {
                    *s=0;
                    break;
                }
                ++s;
            }
            if((*linePtr == 0) || (*linePtr == '#')) {
                continue; /* comment or empty line */
            }

            /* Now, process the line */
            lineNext = NULL;

            while(linePtr && *linePtr) { /* process space-separated items */
                while(*linePtr == ' ') {
                    linePtr++;
                }
                /* Find the next quote */
                if(linePtr[0] == '"')
                {
                    lineNext = uprv_strchr(linePtr+1, '"');
                    if(lineNext == NULL) {
                        fprintf(stderr, "%s:%d - missing trailing double quote (\")\n",
                            l->str, (int)ln);
                        exit(1);
                    } else {
                        lineNext++;
                        if(*lineNext) {
                            if(*lineNext != ' ') {
                                fprintf(stderr, "%s:%d - malformed quoted line at position %d, expected ' ' got '%c'\n",
                                    l->str, (int)ln, (int)(lineNext-line), (*lineNext)?*lineNext:'0');
                                exit(1);
                            }
                            *lineNext = 0;
                            lineNext++;
                        }
                    }
                } else {
                    lineNext = uprv_strchr(linePtr, ' ');
                    if(lineNext) {
                        *lineNext = 0; /* terminate at space */
                        lineNext++;
                    }
                }

                /* add the file */
                s = (char*)getLongPathname(linePtr);

                /* normal mode.. o->files is just the bare list without package names */
                o->files = pkg_appendToList(o->files, &tail, uprv_strdup(linePtr));
                if(uprv_pathIsAbsolute(s)) {
                    fprintf(stderr, "pkgdata: Error: absolute path encountered. Old style paths are not supported. Use relative paths such as 'fur.res' or 'translit%cfur.res'.\n\tBad path: '%s'\n", U_FILE_SEP_CHAR, s);
                    exit(U_ILLEGAL_ARGUMENT_ERROR);
                }
                uprv_strcpy(tmp, o->srcDir);
                uprv_strcat(tmp, o->srcDir[uprv_strlen(o->srcDir)-1]==U_FILE_SEP_CHAR?"":U_FILE_SEP_STRING);
                uprv_strcat(tmp, s);
                o->filePaths = pkg_appendToList(o->filePaths, &tail2, uprv_strdup(tmp));
                linePtr = lineNext;
            } /* for each entry on line */
        } /* for each line */
        T_FileStream_close(in);
    } /* for each file list file */
}

/* Try calling icu-config directly to get information */
static void fillInMakefileFromICUConfig(UOption *option)
{
#if U_HAVE_POPEN
    FILE *p;
    size_t n;
    static char buf[512] = "";
    static const char cmd[] = "icu-config --incfile";

    if(options[5].doesOccur)
    {
        /* informational */
        fprintf(stderr, "%s: No -O option found, trying '%s'.\n", progname, cmd);
    }

    p = popen(cmd, "r");

    if(p == NULL)
    {
        fprintf(stderr, "%s: icu-config: No icu-config found. (fix PATH or use -O option)\n", progname);
        return;
    }

    n = fread(buf, 1, 511, p);

    pclose(p);

    if(n<=0)
    {
        fprintf(stderr,"%s: icu-config: Could not read from icu-config. (fix PATH or use -O option)\n", progname);
        return;
    }

    if(buf[strlen(buf)-1]=='\n')
    {
        buf[strlen(buf)-1]=0;
    }

    if(buf[0] == 0)
    {
        fprintf(stderr, "%s: icu-config: invalid response from icu-config (fix PATH or use -O option)\n", progname);
        return;
    }

    if(options[5].doesOccur)
    {
        /* informational */
        fprintf(stderr, "%s: icu-config: using '-O %s'\n", progname, buf);
    }
    option->value = buf;
    option->doesOccur = TRUE;
#else  /* ! U_HAVE_POPEN */

#ifdef U_WINDOWS
    char pathbuffer[_MAX_PATH] = {0};
    char *fullEXEpath = NULL;
    char *pathstuff = NULL;

    if (strchr(progname, U_FILE_SEP_CHAR) != NULL || strchr(progname, U_FILE_ALT_SEP_CHAR) != NULL) {
        /* pkgdata was executed with relative path */
        fullEXEpath = _fullpath(pathbuffer, progname, sizeof(pathbuffer));
        pathstuff = (char *)options[1].value;

        if (fullEXEpath) {
            pathstuff = strrchr(fullEXEpath, U_FILE_SEP_CHAR);
            if (pathstuff) {
                pathstuff[1] = 0;
                uprv_memmove(fullEXEpath + 2, fullEXEpath, uprv_strlen(fullEXEpath)+1);
                fullEXEpath[0] = PKGDATA_DERIVED_PATH;
                fullEXEpath[1] = ':';
                option->value = uprv_strdup(fullEXEpath);
                option->doesOccur = TRUE;
            }
        }
    }
    else {
        /* pkgdata was executed from the path */
        /* Search for file in PATH environment variable: */
        _searchenv("pkgdata.exe", "PATH", pathbuffer );
        if( *pathbuffer != '\0' ) {
            fullEXEpath = pathbuffer;
            pathstuff = strrchr(pathbuffer, U_FILE_SEP_CHAR);
            if (pathstuff) {
                pathstuff[1] = 0;
                uprv_memmove(fullEXEpath + 2, fullEXEpath, uprv_strlen(fullEXEpath)+1);
                fullEXEpath[0] = PKGDATA_DERIVED_PATH;
                fullEXEpath[1] = ':';
                option->value = uprv_strdup(fullEXEpath);
                option->doesOccur = TRUE;
            }
        }
    }
    /* else can't determine the path */
#endif

    /* no popen available */
    /* Put other OS specific ways to search for the Makefile.inc type
       information or else fail.. */

#endif
}
