#!/usr/bin/perl -w
#  ********************************************************************
#  * COPYRIGHT:
#  * Copyright (c) 2005-2007, International Business Machines Corporation and
#  * others. All Rights Reserved.
#  ********************************************************************

use strict;

use lib '../perldriver';

use PerfFramework;


my $options = {
	       "title"=>"UnicodeSet span()/contains() performance",
	       "headers"=>"ICU",
	       "operationIs"=>"Unicode string",
	       "passes"=>"1",
	       "time"=>"2",
	       #"outputType"=>"HTML",
	       "dataDir"=>"/temp/udhr",
	       "outputDir"=>"../results"
	      };

# programs
# tests will be done for all the programs. Results will be stored and connected
my $p = "Release/unisetperf.exe -e gb18030";

my $tests = { 
	     "SpanUTF8  Bv",    ["$p SpanUTF8  --type Bv   -e UTF-8 "],
	     "SpanUTF8  BvF",   ["$p SpanUTF8  --type BvF  -e UTF-8 "],
	     "SpanUTF8  Bvp",   ["$p SpanUTF8  --type Bvp  -e UTF-8 "],
	     "SpanUTF8  BvpF",  ["$p SpanUTF8  --type BvpF -e UTF-8 "],
	     "SpanUTF8  L",     ["$p SpanUTF8  --type L    -e UTF-8 "],
	     "SpanUTF8  Bvl",   ["$p SpanUTF8  --type Bvl  -e UTF-8 "],
	    };

my $dataFiles = {
		 "",
		 [
		  "udhr_eng.txt",
          "udhr_deu.txt",
          "udhr_fra.txt",
          "udhr_rus.txt",
          "udhr_tha.txt",
          "udhr_jpn.txt",
          "udhr_cmn.txt",
          "udhr_jpn.html"
		 ]
		};

runTests($options, $tests, $dataFiles);
