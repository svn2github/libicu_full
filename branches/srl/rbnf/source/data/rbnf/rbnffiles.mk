# *   Copyright (C) 1997-2008, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# A list of txt's to build
# Note: 
#
#   If you are thinking of modifying this file, READ THIS. 
#
# Instead of changing this file [unless you want to check it back in],
# you should consider creating a 'reslocal.mk' file in this same directory.
# Then, you can have your local changes remain even if you upgrade or
# reconfigure ICU.
#
# Example 'rbnflocal.mk' files:
#
#  * To add an additional locale to the list: 
#    _____________________________________________________
#    |  RBNF_SOURCE_LOCAL =   myLocale.txt ...
#
#  * To REPLACE the default list and only build with a few
#     locale:
#    _____________________________________________________
#    |  RBNF_SOURCE = ar.txt ar_AE.txt en.txt de.txt zh.txt
#
#


# This is the list of locales that are built, but not considered installed in ICU.
# These are usually aliased locales or the root locale.
RBNF_ALIAS_SOURCE = 


# Please try to keep this list in alphabetical order
RBNF_SOURCE = \
af.txt \
ca.txt \
cs.txt \
da.txt \
de.txt \
el.txt \
en.txt en_GB.txt \
eo.txt \
es.txt \
et.txt \
fa.txt fa_AF.txt \
fi.txt \
fo.txt \
fr.txt fr_BE.txt fr_CH.txt \
ga.txt \
he.txt \
hu.txt \
hy.txt \
it.txt \
is.txt \
ja.txt \
ka.txt \
kl.txt \
ko.txt \
lt.txt \
lv.txt \
mt.txt \
nah.txt \
nb.txt \
nl.txt \
nn.txt \
pl.txt \
pt.txt pt_PT.txt \
ro.txt \
ru.txt \
se.txt \
sk.txt \
sq.txt \
sv.txt \
th.txt \
tr.txt \
uk.txt \
vi.txt \
zh.txt zh_Hant.txt \
#

#sl.txt
#hr.txt
#bs.txt
#sr_Latn.txt

#sr.txt //cyrl
#mk.txt
#bg.txt
#be.txt
#be_tarask.txt

#lo.txt
