/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2002-2003, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "uperf.h"

static const char delim = '/';
static int32_t execCount = 0;
UPerfTest* UPerfTest::gTest = NULL;
static const int MAXLINES       = 40000;
//static const char *currDir = ".";
                
enum
{
    HELP1,
    HELP2,
    VERBOSE,
    SOURCEDIR,
    ENCODING,
    USELEN,
    FILE_NAME,
    PASSES,
    ITERATIONS,
    TIME,
    LINE_MODE,
    BULK_MODE,
    LOCALE
};


static UOption options[]={
                      UOPTION_HELP_H,
                      UOPTION_HELP_QUESTION_MARK,
                      UOPTION_VERBOSE,
                      UOPTION_SOURCEDIR,
                      UOPTION_ENCODING,
                      UOPTION_DEF( "uselen",        'u', UOPT_NO_ARG),
                      UOPTION_DEF( "file-name",     'f', UOPT_REQUIRES_ARG),
                      UOPTION_DEF( "passes",        'p', UOPT_REQUIRES_ARG),
                      UOPTION_DEF( "iterations",    'i', UOPT_REQUIRES_ARG),                    
                      UOPTION_DEF( "time",          't', UOPT_REQUIRES_ARG),
                      UOPTION_DEF( "line-mode",     'l', UOPT_NO_ARG),
                      UOPTION_DEF( "bulk-mode",     'b', UOPT_NO_ARG),
                      UOPTION_DEF( "locale",        'L', UOPT_REQUIRES_ARG)
                  };

UPerfTest::UPerfTest(int32_t argc, const char* argv[], UErrorCode& status){
    
    _argc = argc;
    _argv = argv;
    ucharBuf = NULL;
    encoding = "";
    uselen = FALSE;
    fileName = NULL;
    sourceDir = ".";
    lines = NULL;
    numLines = 0;
    line_mode = TRUE;
    buffer = NULL;
    bufferLen = 0;
    verbose = FALSE;
    bulk_mode = FALSE;
    passes = iterations = time = 0;
    locale = NULL;
    
    //initialize the argument list
    U_MAIN_INIT_ARGS(argc, argv);
    //parse the arguments
    _remainingArgc = u_parseArgs(argc, (char**)argv, (int32_t)(sizeof(options)/sizeof(options[0])), options);

    // Now setup the arguments
    if(argc==1 || options[HELP1].doesOccur || options[HELP2].doesOccur) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if(options[VERBOSE].doesOccur) {
        verbose = TRUE;
    }

    if(options[SOURCEDIR].doesOccur) {
        sourceDir = options[SOURCEDIR].value;
    }

    if(options[ENCODING].doesOccur) {
        encoding = options[ENCODING].value;
    }

    if(options[USELEN].doesOccur) {
        uselen = TRUE;
    }

    if(options[FILE_NAME].doesOccur){
        fileName = options[FILE_NAME].value;
    }

    if(options[PASSES].doesOccur) {
        passes = atoi(options[PASSES].value);
    }
    if(options[ITERATIONS].doesOccur) {
        iterations = atoi(options[ITERATIONS].value);
    }
 
    if(options[TIME].doesOccur) {
        time = atoi(options[TIME].value);
    }
    
    if(options[LINE_MODE].doesOccur) {
        line_mode = TRUE;
        bulk_mode = FALSE;
    }

    if(options[BULK_MODE].doesOccur) {
        bulk_mode = TRUE;
        line_mode = FALSE;
    }
    
    if(options[LOCALE].doesOccur) {
      locale = options[LOCALE].value;
    }

    if(time > 0 && iterations >0){
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int32_t len = 0;
    resolvedFileName = NULL;
    if(fileName!=NULL){
        //pre-flight
        ucbuf_resolveFileName(sourceDir, fileName,resolvedFileName,&len, &status);
        resolvedFileName = (char*) uprv_malloc(len);
        if(fileName==NULL){
            status= U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if(status == U_BUFFER_OVERFLOW_ERROR){
            status = U_ZERO_ERROR;
        }
        ucbuf_resolveFileName(sourceDir, fileName, resolvedFileName, &len, &status);
        ucharBuf = ucbuf_open(resolvedFileName,&encoding,TRUE,FALSE,&status);

        if(U_FAILURE(status)){
            printf("Could not open the input file %s. Error: %s\n", fileName, u_errorName(status));
            return;
        }
    }
}

ULine* UPerfTest::getLines(UErrorCode& status){
    lines     = new ULine[MAXLINES];
    int maxLines = MAXLINES;
    numLines=0;
    const UChar* line=NULL;
    int32_t len =0;
    for (;;) {
            line = ucbuf_readline(ucharBuf,&len,&status);
            if(line == NULL || U_FAILURE(status)){
                break;
            }
            lines[numLines].name  = new UChar[len];
            lines[numLines].len   = len;
            memcpy(lines[numLines].name, line, len * U_SIZEOF_UCHAR);
            
            numLines++;
            len = 0;
            if (numLines >= maxLines) {
                maxLines += MAXLINES;
                ULine *newLines = new ULine[maxLines];
                if(newLines == NULL) {
                    fprintf(stderr, "Out of memory reading line %d.\n", numLines);
                    status= U_MEMORY_ALLOCATION_ERROR;
                    delete lines;
                    return NULL;
                }

                memcpy(newLines, lines, numLines*sizeof(ULine));
                delete lines;
                lines = newLines;
            }
    }
    return lines;
}
const UChar* UPerfTest::getBuffer(int32_t& len, UErrorCode& status){
    len = ucbuf_size(ucharBuf);
    buffer =  (UChar*) uprv_malloc(U_SIZEOF_UCHAR * (len+1));
    u_strncpy(buffer,ucbuf_getBuffer(ucharBuf,&bufferLen,&status),len);
    buffer[len]=0;
    len = bufferLen;
    return buffer;
}
UBool UPerfTest::run(){
    if(_remainingArgc==1){
        // Testing all methods
        return runTest();
    }
    UBool res=FALSE;
    // Test only the specified fucntion
    for (int i = 1; i < _remainingArgc; ++i) {
        if (_argv[i][0] != '-') {
            char* name = (char*) _argv[i];
            if(verbose==TRUE){
                //fprintf(stdout, "\n=== Handling test: %s: ===\n", name);
                //fprintf(stdout, "\n%s:\n", name);
            }
            char* parameter = strchr( name, '@' );
            if (parameter) {
                *parameter = 0;
                parameter += 1;
            }
            execCount = 0;
            res = runTest( name, parameter );
            if (!res || (execCount <= 0)) {
                fprintf(stdout, "\n---ERROR: Test doesn't exist: %s!\n", name);
                return FALSE;
            }
        }
    }
    return res;
}
UBool UPerfTest::runTest(char* name, char* par ){
    UBool rval;
    char* pos = NULL;

    if (name)
        pos = strchr( name, delim ); // check if name contains path (by looking for '/')
    if (pos) {
        path = pos+1;   // store subpath for calling subtest
        *pos = 0;       // split into two strings
    }else{
        path = NULL;
    }

    if (!name || (name[0] == 0) || (strcmp(name, "*") == 0)) {
        rval = runTestLoop( NULL, NULL );

    }else if (strcmp( name, "LIST" ) == 0) {
        this->usage();
        rval = TRUE;

    }else{
        rval = runTestLoop( name, par );
    }

    if (pos)
        *pos = delim;  // restore original value at pos
    return rval;
}


void UPerfTest::setPath( char* pathVal )
{
    this->path = pathVal;
}

// call individual tests, to be overriden to call implementations
UPerfFunction* UPerfTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* par )
{
    // to be overriden by a method like:
    /*
    switch (index) {
        case 0: name = "First Test"; if (exec) FirstTest( par ); break;
        case 1: name = "Second Test"; if (exec) SecondTest( par ); break;
        default: name = ""; break;
    }
    */
    fprintf(stderr,"*** runIndexedTest needs to be overriden! ***");
    name = ""; exec = exec; index = index; par = par;
    return NULL;
}


UBool UPerfTest::runTestLoop( char* testname, char* par )
{
    int32_t    index = 0;
    const char*   name;
    UBool  run_this_test;
    UBool  rval = FALSE;
    UErrorCode status = U_ZERO_ERROR;
    UPerfTest* saveTest = gTest;
    gTest = this;
    int32_t loops = 0;
    double t=0;
    int32_t n = 1;
    do {
        this->runIndexedTest( index, FALSE, name );
        if (!name || (name[0] == 0))
            break;
        if (!testname) {
            run_this_test = TRUE;
        }else{
            run_this_test = (UBool) (strcmp( name, testname ) == 0);
        }
        if (run_this_test) {
            UPerfFunction* testFunction = this->runIndexedTest( index, TRUE, name, par );
            execCount++;
            rval=TRUE;
            if(testFunction==NULL){
                fprintf(stderr,"%s function returned NULL", name);
                return FALSE;
            }
            if (testFunction->getOperationsPerIteration() < 1) {
                fprintf(stderr, "%s returned an illegal operations/iteration()\n", name);
                return FALSE;
            }
            if(iterations == 0) {
              n = time;
              // Run for specified duration in seconds
              if(verbose==TRUE){
                  fprintf(stdout,"= %s calibrating %i seconds \n" ,name, n);
              }

              //n *=  1000; // s => ms
              //System.out.println("# " + meth.getName() + " " + n + " sec");                            
              int32_t failsafe = 1; // last resort for very fast methods
              t = 0;
              while (t < (int)(n * 0.9)) { // 90% is close enough
                  if (loops == 0 || t == 0) {
                      loops = failsafe;
                      failsafe *= 10;
                  } else {
                      //System.out.println("# " + meth.getName() + " x " + loops + " = " + t);                            
                      loops = (int)((double)n / t * loops + 0.5);
                      if (loops == 0) {
                          fprintf(stderr,"Unable to converge on desired duration");
                          return FALSE;
                      }
                  }
                  //System.out.println("# " + meth.getName() + " x " + loops);
                        t = testFunction->time(loops,&status);
						if(U_FAILURE(status)){
							printf("Performance test failed with error: %s \n", u_errorName(status));
							break;
						}
              }
            } else {
              loops = iterations;
            }

            for(int32_t ps =0; ps < passes; ps++){
              long events = -1;
              fprintf(stdout,"= %s begin " ,name);
              if(verbose==TRUE){
                  if(iterations > 0) {
                    fprintf(stdout, "%i\n", loops);
                  } else {
                    fprintf(stdout, "%i\n", n);
                  }
              } else {
                fprintf(stdout, "\n");
              }
              t = testFunction->time(loops, &status);
						if(U_FAILURE(status)){
							printf("Performance test failed with error: %s \n", u_errorName(status));
							break;
						}
              events = testFunction->getEventsPerIteration();
              //print info only in verbose mode
              if(verbose==TRUE){
/*
                  if(events == -1){
                      fprintf(stdout,"= %s end %f %i %i\n",name , t , loops, testFunction->getOperationsPerIteration());
                  }else{
                      fprintf(stdout,"= %s end %f %i %i %i\n",name , t , loops, testFunction->getOperationsPerIteration(), events);
                  }
*/
                  if(events == -1){
                      fprintf(stdout,"= %s end: %f loops: %i operations: %li \n",name , t , loops, testFunction->getOperationsPerIteration());
                  }else{
                      fprintf(stdout,"= %s end: %f loops: %i operations: %li events: %li\n",name , t , loops, testFunction->getOperationsPerIteration(), events);
                  }
              }else{
/*
                   if(events == -1){
                      fprintf(stdout,"= %f %i %i \n", t , loops, testFunction->getOperationsPerIteration());
                  }else{
                      fprintf(stdout,"= %f %i %i %i\n", t , loops, testFunction->getOperationsPerIteration(), events);
                  }                   
*/
                  if(events == -1){
                      fprintf(stdout,"= %s end %f %i %li\n",name , t , loops, testFunction->getOperationsPerIteration());
                  }else{
                      fprintf(stdout,"= %s end %f %i %li %li\n",name , t , loops, testFunction->getOperationsPerIteration(), events);
                  }
              }
            }
            delete testFunction;
        }
        index++;
    }while(name);

    gTest = saveTest;
    return rval;
}

/**
* Print a usage message for this test class.
*/
void UPerfTest::usage( void )
{
    UBool save_verbose = verbose;
    verbose = TRUE;
    fprintf(stdout,"Test names:\n");
    fprintf(stdout,"-----------\n");

    int32_t index = 0;
    const char* name = NULL;
    do{
        this->runIndexedTest( index, FALSE, name );
        if (!name) break;
        fprintf(stdout,name);
        fprintf(stdout,"\n");
        index++;
    }while (name && (name[0] != 0));
    verbose = save_verbose;
}




void UPerfTest::setCaller( UPerfTest* callingTest )
{
    caller = callingTest;
    if (caller) {
        verbose = caller->verbose;
    }
}

UBool UPerfTest::callTest( UPerfTest& testToBeCalled, char* par )
{
    execCount--; // correct a previously assumed test-exec, as this only calls a subtest
    testToBeCalled.setCaller( this );
    return testToBeCalled.runTest( path, par );
}

UPerfTest::~UPerfTest(){
    if(lines!=NULL){
        delete[] lines;
    }
    if(buffer!=NULL){
        uprv_free(buffer);
    }
    ucbuf_close(ucharBuf);
}


