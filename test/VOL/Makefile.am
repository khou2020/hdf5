#
# Copyright by The HDF Group.
# All rights reserved.
#
# This file is part of HDF5.  The full HDF5 copyright notice, including
# terms governing use, modification, and redistribution, is contained in
# the COPYING file, which can be found at the root of the source code
# distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.
# If you do not have access to either file, you may request a copy from
# help@hdfgroup.org.
##
## Makefile.am
## Run automake to generate a Makefile.in from this file.
##
#
# HDF5 Library VOL Test Makefile(.in)
#

include $(top_srcdir)/config/commence.am

AM_CFLAGS += -I$(top_srcdir)/test -I$(top_srcdir)/src -I$(top_builddir)/test -I$(top_builddir)/src

if BUILD_PARALLEL_CONDITIONAL
   AM_CFLAGS += -I$(top_srcdir)/testpar -I$(top_srcdir)/testpar/VOL -I$(top_builddir)/testpar -I$(top_builddir)/testpar/VOL
endif

check_PROGRAMS = vol_test

vol_test_SOURCES=vol_test.c vol_file_test.c vol_group_test.c vol_dataset_test.c vol_attribute_test.c \
                 vol_datatype_test.c vol_link_test.c vol_object_test.c vol_misc_test.c

vol_test_LDADD=$(LIBH5TEST) $(LIBHDF5)

include $(top_srcdir)/config/conclude.am