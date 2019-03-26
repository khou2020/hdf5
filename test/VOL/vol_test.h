/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef VOL_TEST_H
#define VOL_TEST_H

#include "h5test.h"

/* The name of the file that all of the tests will operate on */
#define TEST_FILE_NAME "vol_test.h5"
extern char vol_test_filename[];

/* The names of a set of container groups which hold objects
 * created by each of the different types of tests.
 */
#define GROUP_TEST_GROUP_NAME         "group_tests"
#define ATTRIBUTE_TEST_GROUP_NAME     "attribute_tests"
#define DATASET_TEST_GROUP_NAME       "dataset_tests"
#define DATATYPE_TEST_GROUP_NAME      "datatype_tests"
#define LINK_TEST_GROUP_NAME          "link_tests"
#define OBJECT_TEST_GROUP_NAME        "object_tests"
#define MISCELLANEOUS_TEST_GROUP_NAME "miscellaneous_tests"

#define ARRAY_LENGTH(array) sizeof(array) / sizeof(array[0])

#define UNUSED(o) (void) (o);

#define VOL_TEST_FILENAME_MAX_LENGTH 1024

/* The maximum size of a dimension in an HDF5 dataspace as allowed
 * for this testing suite so as not to try to create too large
 * of a dataspace/datatype. */
#define MAX_DIM_SIZE 16

#define NO_LARGE_TESTS

/*
 * XXX: Set of compatibility macros that should be replaced once the
 * VOL connector feature support situation is resolved.
 */
#define GROUP_CREATION_IS_SUPPORTED

hid_t generate_random_datatype(H5T_class_t parent_class);
hid_t generate_random_dataspace(int rank, const hsize_t *max_dims);

#endif
