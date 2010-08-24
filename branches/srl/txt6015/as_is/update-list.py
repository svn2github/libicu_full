## Copyright (C)  2010, International Business Machines Corporation and Others. All Rights Reserved. 
##
## Usage: Run this to regenerate as_is/binfiles.txt and as_is/txtfiles.txt based on svn props
## Depends on pysvn

import os
import pysvn
import sys

debug = False

# change to parent dir of as_is
os.chdir('..')

print "# Building lists in " + os.getcwd() + " under as_is/*files.txt"

## Set up override lists

# from unpax-icu.sh
binary_suffixes='brk BRK bin BIN res RES cnv CNV dat DAT icu ICU spp SPP xml XML nrm NRM'
source_suffixes='sed c cc cpp h rc sh pl py txt java ucm rtf xml Makefile in mak mk m4 spec README'

suffixtypemap = {}

for suf in binary_suffixes.split():
    suffixtypemap[suf]='application/octet-stream'

for suf in source_suffixes.split():
    suffixtypemap[suf]='text/plain'

## Get the SVN client and the list
client = pysvn.Client()
entries = client.list('.',recurse=True)

## Open the files for writing
binFile = open('as_is/binfiles.txt', 'w')
txtFile = open('as_is/txtfiles.txt', 'w')

for entry in entries:
    if entry[0].has_props and entry[0].kind == pysvn.node_kind.file:
        svnfile = '.'+entry[0].path

        #if debug:  print 'svnfile: ' + svnfile
        
        pg = client.propget( 'svn:mime-type', svnfile )

        type = ""

        if(len(pg)>0):
            for k in pg:
                type = pg[k]


        # look for source
        if len(type) == 0:
            extn = svnfile.split('.')[-1]
            if extn in suffixtypemap.keys():
                type = suffixtypemap[extn]
                #if debug: print 'Auto ' + extn + ' = ' + type
            else:
                if debug: print 'Assuming text: ' + svnfile
                type = 'text/unknown' 

        #if debug: print '#'+svnfile + '\t' + type

        # US-ASCII, *.*: convert
        # [not specified] *.*: no convert
        # [not specified] text/sources:  convert
        # [not specified] non-text: no convert

        charset = 'US-ASCII'

        if type.startswith('text/'):
            parts = type.split(';')
            if(len(parts)>1):
                if(parts[1].startswith('charset=')):
                    charset = parts[1].split('=')[1]
            if charset is 'US-ASCII':
                if debug: print svnfile
                print >>txtFile, svnfile
            else:
                if debug: print '# No Cvt: '+svnfile
                print >>binFile, svnfile
        else:
            if debug: print '# NotTxt: '+svnfile
            print >>binFile, svnfile

print "# Done."
