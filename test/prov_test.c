/*
 * prov_test.c
 *
 *  Created on: Jan 8, 2019
 *      Author: tonglin
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hdf5.h"
#include "h5test.h"
#include "H5VLprovnc.h"


int main(int argc, char* argv[]) {
    printf("HDF5 provenance VOL test start...\n");

    hid_t       file_id, dataset_id, dataspace_id;  /* identifiers */
     hsize_t     dims[2];
     herr_t      status;
     int         i, j, dset_data[4][6];

    printf("1\n");
    const char* file_name = "prov_test.h5";

    /* Create a new file using default properties. */
    hid_t fapl = h5_fileaccess();
    file_id = H5Fcreate(file_name, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);//H5P_DEFAULT
    printf("2\n");
    /* Create the data space for the dataset. */
    dims[0] = 4;
    dims[1] = 6;
    dataspace_id = H5Screate_simple(2, dims, NULL);
    printf("3\n");
    /* Create the dataset. */
    dataset_id = H5Dcreate2(file_id, "/dset", H5T_STD_I32BE, dataspace_id,
                           H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    printf("4\n");
    /* End access to the dataset and release resources used by it. */
    status = H5Dclose(dataset_id);
    printf("5\n");
    /* Terminate access to the data space. */
    status = H5Sclose(dataspace_id);
    printf("6\n");
    /* Close the file. */
    status = H5Fclose(file_id);

    printf("7\n");

    /* Initialize the dataset. */
    for (i = 0; i < 4; i++)
       for (j = 0; j < 6; j++)
          dset_data[i][j] = i * 6 + j + 1;

    /* Open an existing file. */
    file_id = H5Fopen(file_name, H5F_ACC_RDWR, fapl);
    printf("8\n");
    /* Open an existing dataset. */
    dataset_id = H5Dopen2(file_id, "/dset", H5P_DEFAULT);
    printf("9\n");
    /* Write the dataset. */
    status = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data);
    printf("10\n");
    status = H5Dread(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data);
    printf("11\n");
//    /* Close the dataset. */
    status = H5Dclose(dataset_id);
    printf("12\n");
    H5Pclose(fapl);
    printf("13\n");
    H5Fclose(file_id);

    printf("14\n");

    printf("HDF5 provenance VOL test done.\n");

    return 0;
}
