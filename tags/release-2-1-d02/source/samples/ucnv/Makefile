# Copyright (c) 2000 IBM, Inc. and others
# conversion sample code $Revision: 1.4 $


# Usage:
#  - configure and build ICU [see the docs] .. use "--prefix=" something
#     (I  used --prefix=/home/srl/III )
# 
#  - do 'make install' of icu 
#
#  - change the following line to point to the $(prefix) that 
#    was used (will look for $(prefix)/lib/icu/Makefile.inc )
#      OR 
#   set the variable ICU_PREFIX to point at $(prefix)
#  
#  - do 'make' in this directory

ICU_DEFAULT_PREFIX=/home/srl/II

ifeq ($(strip $(ICU_PREFIX)),)
  ICU_INC=$(ICU_DEFAULT_PREFIX)/lib/icu/Makefile.inc
else
  ICU_INC=$(ICU_PREFIX)/lib/icu/Makefile.inc
endif
ICUPATH=

include $(ICU_INC)

# Name of your target
TARGET=convsamp

# All object files (C or C++)
OBJECTS=convsamp.o flagcb.o

CLEANFILES=*~ $(TARGET).out

DEPS=$(OBJECTS:.o=.d)

# turn on super warnings
#CPPFLAGS += -Wall


all: $(TARGET)

.PHONY: all clean distclean check report

distclean clean:
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)
	-$(RMV) $(OBJECTS) $(TARGET)
	-$(RMV) $(DEPS)

# Can change this to LINK.c if it is a C only program
# Can add more libraries here. 
$(TARGET): $(OBJECTS)
	$(LINK.cc) -o $@ $^ $(ICULIBS)

# Make check: simply runs the sample, logged to a file
check: $(TARGET)
	$(TARGET) | tee $(TARGET).out

# Make report: creates a 'report file' with both source and sample run
report: $(TARGET).report

$(TARGET).report: check $(TARGET).cpp
	more $(TARGET).cpp $(TARGET).out > $@

$(ICU_INC):
	@echo "Please read the directions at the top of this file (Makefile)"
	@echo "Can't open $(ICU_INC)"
	@false

ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
