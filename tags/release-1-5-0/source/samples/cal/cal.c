/*
**********************************************************************
* Copyright (C) 1998-2000, International Business Machines Corporation 
* and others.  All Rights Reserved.
**********************************************************************
*
* File date.c
*
* Modification History:
*
*   Date        Name        Description
*   06/16/99    stephen     Creation.
*******************************************************************************
*/

#include <stdio.h>
#include <string.h>

#include "unicode/uloc.h"
#include "unicode/udat.h"
#include "unicode/ucal.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"

#include "uprint.h"

/* Protos */
static void usage();
static void version();
static void cal(int32_t month, int32_t year,
		UBool useLongNames, UErrorCode *status);
static void get_days(const UChar *days [], UBool useLongNames, 
		     int32_t fdow, UErrorCode *status);
static void get_months(const UChar *months [], UBool useLongNames,
		       UErrorCode *status);
static void indent(int32_t count, FILE *f);
static void print_days(const UChar *days [], FILE *f, UErrorCode *status);
static void  print_month(UCalendar *c, 
			 const UChar *days [], 
			 UBool useLongNames, int32_t fdow, 
			 UErrorCode *status);
static void  print_year(UCalendar *c, 
			const UChar *days [], const UChar *months [],
			UBool useLongNames, int32_t fdow, 
			UErrorCode *status);

/* The version of cal */
#define CAL_VERSION "1.0"

/* Number of days in a week */
#define DAY_COUNT 7

/* Number of months in a year (yes, 13) */
#define MONTH_COUNT 13

/* Separation between months in year view */
#define MARGIN_WIDTH 4

/* Size of stack buffers */
#define BUF_SIZE 64

/* Patterm string - "MMM yyyy" */
static const UChar sShortPat [] = { 0x004D, 0x004D, 0x004D, 0x0020, 
				    0x0079, 0x0079, 0x0079, 0x0079 };
/* Pattern string - "MMMM yyyy" */
static const UChar sLongPat [] = { 0x004D, 0x004D, 0x004D, 0x004D, 0x0020, 
				   0x0079, 0x0079, 0x0079, 0x0079 };


int
main(int argc,
     char **argv)
{
  int printUsage = 0;
  int printVersion = 0;
  int useLongNames = 0;
  int optind = 1;
  char *arg;
  int32_t month = -1, year = -1;
  UErrorCode status = U_ZERO_ERROR;


  /* parse the options */
  for(optind = 1; optind < argc; ++optind) {
    arg = argv[optind];
    
    /* version info */
    if(strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
      printVersion = 1;
    }
    /* usage info */
    else if(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      printUsage = 1;
    }
    /* use long day names */
    else if(strcmp(arg, "-l") == 0 || strcmp(arg, "--long") == 0) {
      useLongNames = 1;
    }
    /* POSIX.1 says all arguments after -- are not options */
    else if(strcmp(arg, "--") == 0) {
      /* skip the -- */
      ++optind;
      break;
    }
    /* unrecognized option */
    else if(strncmp(arg, "-", strlen("-")) == 0) {
      printf("cal: invalid option -- %s\n", arg+1);
      printUsage = 1;
    }
    /* done with options, display cal */
    else {
      break;
    }
  }

  /* Get the month and year to display, if specified */
  if(optind != argc) {

    /* Month and year specified */
    if(argc - optind == 2) {
      sscanf(argv[optind], "%d", &month);
      sscanf(argv[optind + 1], "%d", &year);

      /* Make sure the month value is legal */
      if(month < 0 || month > 12) {
	printf("cal: Bad value for month -- %d\n", month);

	/* Display usage */
	printUsage = 1;
      }
      
      /* Adjust because months are 0-based */
      --month;
    }
    /* Only year specified */
    else {
      sscanf(argv[optind], "%d", &year);
    }
  }

  /* print usage info */
  if(printUsage) {
    usage();
    return 0;
  }

  /* print version info */
  if(printVersion) {
    version();
    return 0;
  }

  /* print the cal */
  cal(month, year, useLongNames, &status);

  return (U_FAILURE(status) ? 1 : 0);
}

/* Usage information */
static void
usage()
{  
  puts("Usage: cal [OPTIONS] [[MONTH] YEAR]");
  puts("");
  puts("Options:");
  puts("  -h, --help        Print this message and exit.");
  puts("  -v, --version     Print the version number of cal and exit.");
  puts("  -l, --long        Use long names.");
  puts("");
  puts("Arguments (optional):");
  puts("  MONTH             An integer (1-12) indicating the month to display");
  puts("  YEAR              An integer indicating the year to display");
  puts("");
  puts("For an interesting calendar, look at October 1582");
}

/* Version information */
static void
version()
{
  printf("cal version %s (ICU version %s), created by Stephen F. Booth.\n", 
	 CAL_VERSION, U_ICU_VERSION); 
  puts("Copyright (C) 1998-2000 International Business Machines Corporation and others.");
  puts("All Rights Reserved.");
}

static void
cal(int32_t month,
    int32_t year,
    UBool useLongNames,
    UErrorCode *status)
{
  UCalendar *c;
  const UChar *days [DAY_COUNT];
  const UChar *months [MONTH_COUNT];
  int32_t fdow;

  if(U_FAILURE(*status)) return;

  /* Create a new calendar */
  c = ucal_open(0, -1, uloc_getDefault(), UCAL_TRADITIONAL, status);

  /* Determine if we are printing a calendar for one month or for a year */

  /* Print an entire year */
  if(month == -1 && year != -1) {

    /* Set the year */
    ucal_set(c, UCAL_YEAR, year);

    /* Determine the first day of the week */
    fdow = ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK);
    
    /* Set up the day names */
    get_days(days, useLongNames, fdow, status);

    /* Set up the month names */
    get_months(months, useLongNames, status);

    /* Print the calendar for the year */
    print_year(c, days, months, useLongNames, fdow, status);
  }

  /* Print only one month */
  else {
    
    /* Set the month and the year, if specified */
    if(month != -1)
      ucal_set(c, UCAL_MONTH, month);
    if(year != -1)
      ucal_set(c, UCAL_YEAR, year);
    
    /* Determine the first day of the week */
    fdow = ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK);

    /* Set up the day names */
    get_days(days, useLongNames, fdow, status);
    
    /* Print the calendar for the month */
    print_month(c, days, useLongNames, fdow, status);
  }
  
  /* Clean up */
  ucal_close(c);
}

/* Get the day names for the specified locale, in either long or short
   form.  Also, reorder the days so that they are in the proper order
   for the locale (not all locales begin weeks on Sunday; in France,
   weeks start on Monday) */
static void
get_days(const UChar *days [],
	 UBool useLongNames,
	 int32_t fdow,
	 UErrorCode *status)
{
  UResourceBundle *bundle;
  int32_t i, count;
  const char *key = (useLongNames ? "DayNames" : "DayAbbreviations");

  if(U_FAILURE(*status)) return;

  /* fdow is 1-based */
  --fdow;

  bundle = ures_open(0, 0, status);
  count = ures_countArrayItems(bundle, key, status);
  if(count != DAY_COUNT) goto finish;   /* sanity check */
  for(i = 0; i < count; ++i) {
    days[i] = ures_getArrayItem(bundle, key, ((i + fdow) % DAY_COUNT), status);
  }
  
 finish:
  ures_close(bundle);
}

/* Get the month names for the specified locale, in either long or
   short form. */
static void
get_months(const UChar *months [],
	   UBool useLongNames,
	   UErrorCode *status)
{
  UResourceBundle *bundle;
  int32_t i, count;
  const char *key = (useLongNames ? "MonthNames" : "MonthAbbreviations");

  if(U_FAILURE(*status)) return;

  bundle = ures_open(0, 0, status);
  count = ures_countArrayItems(bundle, key, status);
  if(count != MONTH_COUNT) goto finish;   /* sanity check */
  for(i = 0; i < count; ++i) {
    months[i] = ures_getArrayItem(bundle, key, i, status);
  }
  
 finish:
  ures_close(bundle);
}

/* Indent a certain number of spaces */
static void
indent(int32_t count,
       FILE *f)
{
  char c [BUF_SIZE];
  
  if(count < BUF_SIZE) {
    memset(c, (int)' ', count);
    fwrite(c, sizeof(char), count, f);
  }
  else {
    int32_t i;
    for(i = 0; i < count; ++i)
      putc(' ', f);
  }
}

/* Print the days */
static void
print_days(const UChar *days [],
	   FILE *f,
	   UErrorCode *status)
{
  int32_t i;

  if(U_FAILURE(*status)) return;

  /* Print the day names */
  for(i = 0; i < DAY_COUNT; ++i) {
    uprint(days[i], f, status);
    putc(' ', f);
  }
}

/* Print out a calendar for c's current month */
static void
print_month(UCalendar *c, 
	    const UChar *days [], 
	    UBool useLongNames,
	    int32_t fdow,
	    UErrorCode *status)
{
  int32_t width, pad, i, day;
  int32_t lens [DAY_COUNT];
  int32_t firstday, current;
  UNumberFormat *nfmt;
  UDateFormat *dfmt;
  UChar s [BUF_SIZE];
  const UChar *pat = (useLongNames ? sLongPat : sShortPat);
  int32_t len = (useLongNames ? 9 : 8);

  if(U_FAILURE(*status)) return;


  /* ========== Generate the header containing the month and year */

  /* Open a formatter with a month and year only pattern */
  dfmt = udat_openPattern(pat, len, 0, status);
  
  /* Format the date */
  udat_format(dfmt, ucal_getMillis(c, status), s, BUF_SIZE, 0, status);


  /* ========== Print the header */
  
  /* Calculate widths for justification */
  width = 6; /* 6 spaces, 1 between each day name */
  for(i = 0; i < DAY_COUNT; ++i) {
    lens[i] = u_strlen(days[i]);
    width += lens[i];
  }

  /* Print the header, centered among the day names */
  pad = width - u_strlen(s);
  indent(pad / 2, stdout);
  uprint(s, stdout, status);
  putc('\n', stdout);


  /* ========== Print the day names */

  print_days(days, stdout, status);
  putc('\n', stdout);


  /* ========== Print the calendar */

  /* Get the first of the month */
  ucal_set(c, UCAL_DATE, 1);
  firstday = ucal_get(c, UCAL_DAY_OF_WEEK, status);

  /* The day of the week for the first day of the month is based on
     1-based days of the week, which were also reordered when placed
     in the days array.  Account for this here by offsetting by the
     first day of the week for the locale, which is also 1-based. */
  firstday -= fdow;

  /* Open the formatter */
  nfmt = unum_open(UNUM_DECIMAL, 0, status);

  /* Indent the correct number of spaces for the first week */
  current = firstday;
  for(i = 0; i < current; ++i)
    indent(lens[i] + 1, stdout);

  /* Finally, print out the days */
  day = ucal_get(c, UCAL_DATE, status);
  do {

    /* Format the current day string */
    unum_format(nfmt, day, s, BUF_SIZE, 0, status);

    /* Calculate the justification and indent */
    pad = lens[current] - u_strlen(s);
    indent(pad, stdout);

    /* Print the day number out, followed by a space */
    uprint(s, stdout, status);
    putc(' ', stdout);

    /* Update the current day */
    ++current;
    current %= DAY_COUNT;

    /* If we're at day 0 (first day of the week), insert a newline */
    if(current == 0) {
      putc('\n', stdout);
    }

    /* Go to the next day */
    ucal_add(c, UCAL_DATE, 1, status);
    day = ucal_get(c, UCAL_DATE, status);

  } while(day != 1);
  
  /* Output a trailing newline */
  putc('\n', stdout);

  /* Clean up */
  unum_close(nfmt);
  udat_close(dfmt);
}

/* Print out a calendar for c's current year */
static void
print_year(UCalendar *c, 
	   const UChar *days [], 
	   const UChar *months [],
	   UBool useLongNames, 
	   int32_t fdow, 
	   UErrorCode *status)
{
  int32_t width, pad, i, j;
  int32_t lens [DAY_COUNT];
  UNumberFormat *nfmt;
  UDateFormat *dfmt;
  UChar s [BUF_SIZE];
  const UChar pat [] = { 0x0079, 0x0079, 0x0079, 0x0079 };
  int32_t len = 4;
  UCalendar  *left_cal, *right_cal;
  int32_t left_day, right_day;
  int32_t left_firstday, right_firstday, left_current, right_current;
  int32_t left_month, right_month;

  if(U_FAILURE(*status)) return;

  /* Alias */
  left_cal = c;

  /* ========== Generate the header containing the year (only) */

  /* Open a formatter with a month and year only pattern */
  dfmt = udat_openPattern(pat, len, 0, status);
  
  /* Format the date */
  udat_format(dfmt, ucal_getMillis(left_cal, status), s, BUF_SIZE, 0, status);

  
  /* ========== Print the header, centered */

  /* Calculate widths for justification */
  width = 6; /* 6 spaces, 1 between each day name */
  for(i = 0; i < DAY_COUNT; ++i) {
    lens[i] = u_strlen(days[i]);
    width += lens[i];
  }

  /* width is the width for 1 calendar; we are displaying in 2 cols
     with MARGIN_WIDTH spaces between months */

  /* Print the header, centered among the day names */
  pad = 2 * width + MARGIN_WIDTH - u_strlen(s);
  indent(pad / 2, stdout);
  uprint(s, stdout, status);
  putc('\n', stdout);
  putc('\n', stdout);

  /* Generate a copy of the calendar to use */
  right_cal = ucal_open(0, -1, uloc_getDefault(), UCAL_TRADITIONAL, status);
  ucal_setMillis(right_cal, ucal_getMillis(left_cal, status), status);

  /* Open the formatter */
  nfmt = unum_open(UNUM_DECIMAL, 0, status);

  /* ========== Calculate and display the months, two at a time */
  for(i = 0; i < MONTH_COUNT - 1; i += 2) {

    /* Print the month names for the two current months */
    pad = width - u_strlen(months[i]);
    indent(pad / 2, stdout);
    uprint(months[i], stdout, status);
    indent(pad / 2 + MARGIN_WIDTH, stdout);
    pad = width - u_strlen(months[i + 1]);
    indent(pad / 2, stdout);
    uprint(months[i + 1], stdout, status);
    putc('\n', stdout);
    
    /* Print the day names, twice  */
    print_days(days, stdout, status);
    indent(MARGIN_WIDTH, stdout);
    print_days(days, stdout, status);
    putc('\n', stdout);

    /* Setup the two calendars */
    ucal_set(left_cal, UCAL_MONTH, i);
    ucal_set(left_cal, UCAL_DATE, 1);
    ucal_set(right_cal, UCAL_MONTH, i + 1);
    ucal_set(right_cal, UCAL_DATE, 1);

    left_firstday = ucal_get(left_cal, UCAL_DAY_OF_WEEK, status);
    right_firstday = ucal_get(right_cal, UCAL_DAY_OF_WEEK, status);

    /* The day of the week for the first day of the month is based on
       1-based days of the week.  However, the days were reordered
       when placed in the days array.  Account for this here by
       offsetting by the first day of the week for the locale, which
       is also 1-based. */

    /* We need to mod by DAY_COUNT since fdow can be > firstday.  IE,
       if fdow = 2 = Monday (like in France) and the first day of the
       month is a 1 = Sunday, we want firstday to be 6, not -1 */
    left_firstday += (DAY_COUNT - fdow);
    left_firstday %= DAY_COUNT;

    right_firstday += (DAY_COUNT - fdow);
    right_firstday %= DAY_COUNT;
    
    left_current = left_firstday;
    right_current = right_firstday;

    left_day = ucal_get(left_cal, UCAL_DATE, status);
    right_day = ucal_get(right_cal, UCAL_DATE, status);

    left_month = ucal_get(left_cal, UCAL_MONTH, status);
    right_month = ucal_get(right_cal, UCAL_MONTH, status);

    /* Finally, print out the days */
    while(left_month == i || right_month == i + 1) {
      
      /* If the left month is finished printing, but the right month
         still has days to be printed, indent the width of the days
         strings and reset the left calendar's current day to 0 */
      if(left_month != i && right_month == i + 1) {
	indent(width + 1, stdout);
	left_current = 0;
      }
      
      while(left_month == i) {
	
	/* If the day is the first, indent the correct number of
           spaces for the first week */
	if(left_day == 1) {
	  for(j = 0; j < left_current; ++j)
	    indent(lens[j] + 1, stdout);
	}
	
	/* Format the current day string */
	unum_format(nfmt, left_day, s, BUF_SIZE, 0, status);
	
	/* Calculate the justification and indent */
	pad = lens[left_current] - u_strlen(s);
	indent(pad, stdout);
	
	/* Print the day number out, followed by a space */
	uprint(s, stdout, status);
	putc(' ', stdout);
	
	/* Update the current day */
	++left_current;
	left_current %= DAY_COUNT;
	
	/* Go to the next day */
	ucal_add(left_cal, UCAL_DATE, 1, status);
	left_day = ucal_get(left_cal, UCAL_DATE, status);

	/* Determine the month */
	left_month = ucal_get(left_cal, UCAL_MONTH, status);

	/* If we're at day 0 (first day of the week), break and go to
           the next month */
	if(left_current == 0) {
	  break;
	}
      };

      /* If the current day isn't 0, indent to make up for missing
         days at the end of the month */
      if(left_current != 0) {
	for(j = left_current; j < DAY_COUNT; ++j)
	  indent(lens[j] + 1, stdout);
      }

      /* Indent between the two months */
      indent(MARGIN_WIDTH, stdout);

      while(right_month == i + 1) {

	/* If the day is the first, indent the correct number of
           spaces for the first week */
	if(right_day == 1) {
	  for(j = 0; j < right_current; ++j)
	    indent(lens[j] + 1, stdout);
	}

	/* Format the current day string */
	unum_format(nfmt, right_day, s, BUF_SIZE, 0, status);
	
	/* Calculate the justification and indent */
	pad = lens[right_current] - u_strlen(s);
	indent(pad, stdout);
	
	/* Print the day number out, followed by a space */
	uprint(s, stdout, status);
	putc(' ', stdout);
	
	/* Update the current day */
	++right_current;
	right_current %= DAY_COUNT;
	
	/* Go to the next day */
	ucal_add(right_cal, UCAL_DATE, 1, status);
	right_day = ucal_get(right_cal, UCAL_DATE, status);

	/* Determine the month */
	right_month = ucal_get(right_cal, UCAL_MONTH, status);
	
	/* If we're at day 0 (first day of the week), break out */
	if(right_current == 0) {
	  break;
	}

      };

      /* Output a newline */
      putc('\n', stdout);
    }

    /* Output a trailing newline */
    putc('\n', stdout);
  }

  /* Clean up */
  udat_close(dfmt);
  unum_close(nfmt);
  ucal_close(right_cal);
}

