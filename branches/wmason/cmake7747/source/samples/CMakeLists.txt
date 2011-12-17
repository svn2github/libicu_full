# Individual sample targets will add themselves as dependencies
# to this one, so that building the samples target will build
# them all.
ADD_CUSTOM_TARGET(samples)

ADD_SUBDIRECTORY(break)
ADD_SUBDIRECTORY(cal)
ADD_SUBDIRECTORY(case)
ADD_SUBDIRECTORY(citer)
ADD_SUBDIRECTORY(coll)
ADD_SUBDIRECTORY(csdet)
ADD_SUBDIRECTORY(date)
ADD_SUBDIRECTORY(datecal)
ADD_SUBDIRECTORY(datefmt)

