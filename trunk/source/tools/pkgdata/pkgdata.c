/******************************************************************************
*
*   Copyright (C) 2000, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
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

#include <stdio.h>
#include <stdlib.h>
#include "unicode/utypes.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "toolutil.h"
#include "unewdata.h"
#include "uoptions.h"
#include "pkgtypes.h"
#include "makefile.h"

#ifdef UDATA_SO_SUFFIX
extern void pkg_mode_dll(UPKGOptions* o, FileStream *stream, UErrorCode *status);
#endif 
extern void pkg_mode_common(UPKGOptions* o, FileStream *stream, UErrorCode *status);

static void loadLists(UPKGOptions *o, UErrorCode *status);


/* This sets the modes that are available */
static struct
{
  const char *name;
  UPKGMODE   *fcn;
  const char *desc;
} modes[] = 
{
#ifdef UDATA_SO_SUFFIX
  { "dll",    pkg_mode_dll,    "Generates one shared library, <package>" UDATA_SO_SUFFIX },
#endif
  { "common", pkg_mode_common, "Generates one common data file, <package>.dat" }
};

static UOption options[]={
    UOPTION_DEF( "name",    'p', UOPT_REQUIRES_ARG),       
    UOPTION_DEF( "bldopt",  'O', UOPT_REQUIRES_ARG),
    UOPTION_DEF( "mode",    'm', UOPT_REQUIRES_ARG),
    UOPTION_HELP_H,                                   /* -h */
    UOPTION_HELP_QUESTION_MARK,                       /* -? */
    UOPTION_VERBOSE,                                  /* -v */
    UOPTION_COPYRIGHT,                                /* -c */
    UOPTION_DEF( "comment", 'C', UOPT_REQUIRES_ARG),  
    UOPTION_DESTDIR,                                  /* -d */
    UOPTION_DEF( "clean",   'k', UOPT_NO_ARG),    
    UOPTION_DEF( "nooutput",'n', UOPT_NO_ARG),
    UOPTION_DEF( "rebuild", 'F', UOPT_NO_ARG),
    UOPTION_DEF( "tempdir", 'T', UOPT_REQUIRES_ARG),
    UOPTION_DEF( "install", 'I', UOPT_REQUIRES_ARG),
    UOPTION_DEF( "srcdir",  's', UOPT_REQUIRES_ARG)
};

const char options_help[][80]={
  "Set the data name",
  "Specify options for the builder",
  "Specify the mode of building (see below)",
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
  "Specify a custom source directory"
};


int
main(int argc, const char *argv[]) {
  FileStream  *in, *out;
  UPKGOptions  o;
  CharList    *tail;
  const char  *progname;
  bool_t       needsHelp = FALSE;
  UErrorCode   status = U_ZERO_ERROR;
  char         tmp[1024];
  int32_t i;

  progname = argv[0];

  /* read command line options */
  argc=u_parseArgs(argc, argv, sizeof(options)/sizeof(options[0]), options);
  
  /* error handling, printing usage message */
  /* I've decided to simply print an error and quit. This tool has too 
     many options to just display them all of the time. */

  if(options[3].doesOccur || options[4].doesOccur) {
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
    
    if(! (options[0].doesOccur && options[1].doesOccur &&
          options[2].doesOccur) ) {
      fprintf(stderr, " required parameters are missing: -p AND -O AND -m \n");
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
            "usage: %s [-options] [-] [ filename1 filename2 ... ] \n"
            "\tProduce packaged ICU data from the given list(s) of files.\n"
            "\t'-' by itself means to read from stdin.\n",
            progname);
    
    fprintf(stderr, "\n options:\n");
    for(i=0;i<(sizeof(options)/sizeof(options[0]));i++) {
      fprintf(stderr, "%-5s -%c or --%-10s  %s\n",
              (i<3?"[REQ]":""),
              options[i].shortName,
              options[i].longName,
              options_help[i]);
    }

    fprintf(stderr, "modes: (-m option)\n");
    for(i=0;i<(sizeof(modes)/sizeof(modes[0]));i++) {
      fprintf(stderr, "   %-10s %s\n", modes[i].name, modes[i].desc);
    }
    return 1;
  }

  /* OK, fill in the options struct */
  uprv_memset(&o, 0, sizeof(o));

  o.mode      = options[2].value;

  o.fcn = NULL;

  for(i=0;i<sizeof(modes)/sizeof(modes[0]);i++) {
    if(!uprv_strcmp(modes[i].name, o.mode)) {
      o.fcn = modes[i].fcn;
      break;
    }
  }

  if(o.fcn == NULL) {
    fprintf(stderr, "Error: invalid mode '%s' specified. Run '%s --help' to list valid modes.\n", o.mode, progname);
    return 1;
  }

  o.shortName = options[0].value;
  o.options   = options[1].value;
  o.verbose   = options[5].doesOccur;
  if(options[6].doesOccur) {
    o.comment = U_COPYRIGHT_STRING;
  } else if (options[7].doesOccur) {
    o.comment = options[7].value;
  }

  if( options[8].doesOccur ) {
    o.targetDir = options[8].value;
  } else {
    o.targetDir = "";  /* cwd */
  }

  o.clean     = options[9].doesOccur;
  o.nooutput  = options[10].doesOccur;
  o.rebuild   = options[11].doesOccur;
  
  if( options[12].doesOccur ) {
    o.tmpDir    = options[12].value;
  } else {
    o.tmpDir    = o.targetDir;
  }

  if( options[13].doesOccur ) {
    o.install  = options[13].value;
  }

  if( options[14].doesOccur ) {
    o.srcDir   = options[12].value;
  } else {
    o.srcDir   = ".";
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

  /* Makefile pathname */
  uprv_strcpy(tmp, o.tmpDir);
  uprv_strcat(tmp, U_FILE_SEP_STRING);
  uprv_strcat(tmp, o.shortName);
  uprv_strcat(tmp, "_");
  uprv_strcat(tmp, o.mode);
  uprv_strcat(tmp, ".mak");  /* MAY NEED TO CHANGE PER PLATFORM */

  o.makeFile = uprv_strdup(tmp);

  out = T_FileStream_open(o.makeFile, "w");

  pkg_mak_writeHeader(out, &o); /* need to take status */

  o.fcn(&o, out, &status);

  pkg_mak_writeFooter(out, &o);

  T_FileStream_close(out);

  if(U_FAILURE(status)) {
    fprintf(stderr, "Error creating makefile [%s]: %s\n", o.mode, 
            u_errorName(status));
    return 1;
  }

  if(o.nooutput == TRUE) {
    return 0; /* nothing to do. */
  }
  
#ifdef WIN32
# error how do you execute things on windows?
#else
  /* POSIX - execute makefile */
  {
    char cmd[1024];
    char pwd[1024];
    char *make;
    int rc;
    
    make = getenv("MAKE");

    if(!make || !make[0]) {
      make = "gmake";
    }

    getcwd(pwd, 1024);
    sprintf(cmd, "%s %s%s -f %s %s %s %s",
            make,
            o.install ? "INSTALLTO=" : "",
            o.install ? o.install    : "",
            o.makeFile,
            o.clean   ? "clean"      : "",
            o.rebuild ? "rebuild"    : "",
            o.install ? "install"    : "");
    if(o.verbose) {
      puts(cmd);
    }

    rc = system(cmd);
    
    if(rc < 0) {
      fprintf(stderr, "# Failed, rc=%d\n", rc);
    }
    return rc;
  }
#endif

  return 0;
}

static void loadLists(UPKGOptions *o, UErrorCode *status)
{
  CharList   *l, *tail = NULL, *tail2 = NULL;
  FileStream *in; 
  char        line[2048];
  char       *s;
  
  for(l = o->fileListFiles; l; l = l->next) {
    if(o->verbose) {
      fprintf(stderr, "# Reading %s..\n", l->str);
    }
    
    /* TODO: stdin */
    in = T_FileStream_open(l->str, "r");
    
    if(!in) {
      fprintf(stderr, "Error opening <%s>.\n", l->str);
      *status = U_FILE_ACCESS_ERROR;
      return;
    }
    
    while(T_FileStream_readLine(in, line, sizeof(line))!=NULL) {
      /* remove trailing newline characters */
      s=line;
      while(*s!=0) {
        if(*s=='\r' || *s=='\n') {
          *s=0;
          break;
        }
        ++s;
      }
      if((*line == 0) || (*line == '#')) {
        continue; /* comment or empty line */
      }
      
      /* add the file */
      s = (char*)getLongPathname(line);

      o->files = pkg_appendToList(o->files, &tail, uprv_strdup(findBasename(s)));
      o->filePaths = pkg_appendToList(o->filePaths, &tail2, uprv_strdup(s));
    }
    
    T_FileStream_close(in);
  }
}


