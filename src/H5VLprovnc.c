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

/*
 * Purpose:     This is a "pass through" VOL connector, which forwards each
 *              VOL callback to an underlying connector.
 *
 *              It is designed as an example VOL connector for developers to
 *              use when creating new connectors, especially connectors that
 *              are outside of the HDF5 library.  As such, it should _NOT_
 *              include _any_ private HDF5 header files.  This connector should
 *              therefore only make public HDF5 API calls and use standard C /
 *              POSIX calls.
 */


/* Header files needed */
/* (Public HDF5 and standard C / POSIX only) */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

#include "hdf5.h"
#include "H5VLprovnc.h"

/**********/
/* Macros */
/**********/

/* Whether to display log messge when callback is invoked */
/* (Uncomment to enable) */
/* #define ENABLE_PROVNC_LOGGING */

/* Hack for missing va_copy() in old Visual Studio editions
 * (from H5win2_defs.h - used on VS2012 and earlier)
 */
#if defined(_WIN32) && defined(_MSC_VER) && (_MSC_VER < 1800)
#define va_copy(D,S)      ((D) = (S))
#endif

/************/
/* Typedefs */
/************/


typedef struct dataset_node ds_node;
typedef struct dataset_node{
    ds_node* next;
}ds_node;

typedef struct file_node file_node;
typedef struct file_node {
    file_node* next;
}file_node;

typedef struct H5VL_provenance_t {
    hid_t  under_vol_id;        /* ID for underlying VOL connector */
    void   *under_object;       /* Info object for underlying VOL connector */
    H5I_type_t my_type;         //obj type, dataset, datatype, etc.,
    prov_helper* prov_helper; //pointer shared among all layers, one per process.
    //H5VL_provenance_info_t* shared_file_info; //TODO: stop use this.
    void* generic_prov_info;    //TODO: start to use this.
    //should be casted to layer-specific type before use,
    //such as file_prov_info, dataset_prov_info
} H5VL_provenance_t;

/* The pass through VOL wrapper context */
typedef struct H5VL_provenance_wrap_ctx_t {
    hid_t under_vol_id;         /* VOL ID for under VOL */
    void *under_wrap_ctx;       /* Object wrapping context for under VOL */
    unsigned long root_file_no;
    file_prov_info_t* root_file_info;
    prov_helper* prov_helper; //shared pointer
} H5VL_provenance_wrap_ctx_t;

//======================================= statistics =======================================
//typedef struct H5VL_prov_t {
//    void   *under_object;
//    char* func_name;
//    int func_cnt;//stats
//} H5VL_prov_t;

typedef struct H5VL_prov_datatype_info_t {
    prov_helper* prov_helper; //pointer shared among all layers, one per process.
    char* dtype_name;
    int datatype_commit_cnt;
    int datatype_get_cnt;

} datatype_prov_info_t;

typedef struct H5VL_prov_dataset_info_t dataset_prov_info_t;
typedef struct H5VL_prov_group_info_t group_prov_info_t;

typedef struct H5VL_prov_file_info_t {//assigned when a file is closed, serves to store stats (copied from shared_file_info)
    prov_helper* prov_helper; //pointer shared among all layers, one per process.
    char* file_name;
    int ref_cnt;
    unsigned long file_no;
    void* mpi_comm;
    group_prov_info_t* opened_grps;
    dataset_prov_info_t* opened_datasets;
    int opened_datasets_cnt;
    int opened_grps_cnt;
    int grp_created;
    int grp_accessed;
    int ds_created;
    int ds_accessed;//ds opened
    file_prov_info_t* next;
}file_prov_info_t;
//grp only has links.

typedef struct H5VL_prov_dataset_info_t {
    prov_helper* prov_helper; //pointer shared among all layers, one per process.
    haddr_t native_addr;
    char* dset_name;
    char* file_name;//parent structure
    H5T_class_t dt_class;//data type class
    H5S_class_t ds_class;//data space class
    H5D_layout_t layout;
    unsigned int dimension_cnt;
    unsigned int dimensions[H5S_MAX_RANK];
    size_t dset_type_size;
    hsize_t dset_space_size;//unsigned long long
    hsize_t total_bytes_read;
    hsize_t total_bytes_written;
    hsize_t total_read_time;
    hsize_t total_write_time;
    int dataset_read_cnt;
    int dataset_write_cnt;
    //H5VL_provenance_info_t* shared_file_info;
    int access_cnt;
    int ref_cnt;
    file_prov_info_t* root_file_info;
    dataset_prov_info_t* next;
} dataset_prov_info_t;

void dataset_get_wrapper(void *dset, hid_t driver_id, H5VL_dataset_get_t get_type, hid_t dxpl_id, void **req, ...);
dataset_prov_info_t* new_ds_prov_info(void* under_obj, hid_t vol_id, haddr_t native_addr, file_prov_info_t* file_info, char* ds_name, hid_t dxpl_id, void **req);//fill the data structure


typedef struct H5VL_prov_group_info_t {
    prov_helper* prov_helper; //pointer shared among all layers, one per process.
    int func_cnt;//stats
    file_prov_info_t* root_file_info;//a group is a direct or indirect children of a file.
    haddr_t native_addr;
    //    int group_get_cnt;
//    int group_specific_cnt;
    int ref_cnt;//multiple opens on a group
    group_prov_info_t* next;
} group_prov_info_t;

typedef struct H5VL_prov_link_info_t {
    int link_get_cnt;
    int link_specific_cnt;
} link_prov_info_t;



prov_helper* PROV_HELPER;

//======================================= statistics =======================================

/********************* */
/* Function prototypes */
/********************* */

/* Helper routines */
static herr_t H5VL_provenance_file_specific_reissue(void *obj, hid_t connector_id,
    H5VL_file_specific_t specific_type, hid_t dxpl_id, void **req, ...);
static herr_t H5VL_provenance_request_specific_reissue(void *obj, hid_t connector_id,
    H5VL_request_specific_t specific_type, ...);
static H5VL_provenance_t *H5VL_provenance_new_obj(void *under_obj,
    hid_t under_vol_id, prov_helper* helper);
static herr_t H5VL_provenance_free_obj(H5VL_provenance_t *obj);

/* "Management" callbacks */
static herr_t H5VL_provenance_init(hid_t vipl_id);
static herr_t H5VL_provenance_term(void);
static void *H5VL_provenance_info_copy(const void *info);
static herr_t H5VL_provenance_info_cmp(int *cmp_value, const void *info1, const void *info2);
static herr_t H5VL_provenance_info_free(void *info);
static herr_t H5VL_provenance_info_to_str(const void *info, char **str);
static herr_t H5VL_provenance_str_to_info(const char *str, void **info);
static void *H5VL_provenance_get_object(const void *obj);
static herr_t H5VL_provenance_get_wrap_ctx(const void *obj, void **wrap_ctx);
static herr_t H5VL_provenance_free_wrap_ctx(void *obj);
static void *H5VL_provenance_wrap_object(void *under_under_in, H5I_type_t obj_type, void *wrap_ctx);

/* Attribute callbacks */
static void *H5VL_provenance_attr_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req);
static void *H5VL_provenance_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t aapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_attr_write(void *attr, hid_t mem_type_id, const void *buf, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_attr_get(void *obj, H5VL_attr_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_attr_specific(void *obj, const H5VL_loc_params_t *loc_params, H5VL_attr_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_attr_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_attr_close(void *attr, hid_t dxpl_id, void **req);

/* Dataset callbacks */
static void *H5VL_provenance_dataset_create(void *obj, const H5VL_loc_params_t *loc_params, const char *ds_name, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req);
static void *H5VL_provenance_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *ds_name, hid_t dapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id,
                                    hid_t file_space_id, hid_t plist_id, void *buf, void **req);
static herr_t H5VL_provenance_dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, const void *buf, void **req);
static herr_t H5VL_provenance_dataset_get(void *dset, H5VL_dataset_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_dataset_specific(void *obj, H5VL_dataset_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_dataset_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_dataset_close(void *dset, hid_t dxpl_id, void **req);

/* Datatype callbacks */
static void *H5VL_provenance_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void **req);
static void *H5VL_provenance_datatype_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t tapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_datatype_get(void *dt, H5VL_datatype_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_datatype_specific(void *obj, H5VL_datatype_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_datatype_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_datatype_close(void *dt, hid_t dxpl_id, void **req);

/* File callbacks */
static void *H5VL_provenance_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id, void **req);
static void *H5VL_provenance_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_file_get(void *file, H5VL_file_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_file_specific(void *file, H5VL_file_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_file_optional(void *file, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_file_close(void *file, hid_t dxpl_id, void **req);

/* Group callbacks */
static void *H5VL_provenance_group_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req);
static void *H5VL_provenance_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t gapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_group_get(void *obj, H5VL_group_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_group_specific(void *obj, H5VL_group_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_group_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_group_close(void *grp, hid_t dxpl_id, void **req);

/* Link callbacks */
static herr_t H5VL_provenance_link_create(H5VL_link_create_type_t create_type, void *obj, const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_link_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_link_specific(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_link_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);

/* Object callbacks */
static void *H5VL_provenance_object_open(void *obj, const H5VL_loc_params_t *loc_params, H5I_type_t *obj_to_open_type, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name, void *dst_obj, const H5VL_loc_params_t *dst_loc_params, const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_provenance_object_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_object_specific(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_provenance_object_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);

/* Async request callbacks */
static herr_t H5VL_provenance_request_wait(void *req, uint64_t timeout, H5ES_status_t *status);
static herr_t H5VL_provenance_request_notify(void *obj, H5VL_request_notify_t cb, void *ctx);
static herr_t H5VL_provenance_request_cancel(void *req);
static herr_t H5VL_provenance_request_specific(void *req, H5VL_request_specific_t specific_type, va_list arguments);
static herr_t H5VL_provenance_request_optional(void *req, va_list arguments);
static herr_t H5VL_provenance_request_free(void *req);

/*******************/
/* Local variables */
/*******************/

/* Pass through VOL connector class struct */
static const H5VL_class_t H5VL_provenance_cls = {
    H5VL_PROVNC_VERSION,                          /* version      */
    (H5VL_class_value_t)H5VL_PROVNC_VALUE,        /* value        */
    H5VL_PROVNC_NAME,                             /* name         */
    0,                                              /* capability flags */
    H5VL_provenance_init,                         /* initialize   */
    H5VL_provenance_term,                         /* terminate    */
    sizeof(H5VL_provenance_info_t),               /* info size    */
    H5VL_provenance_info_copy,                    /* info copy    */
    H5VL_provenance_info_cmp,                     /* info compare */
    H5VL_provenance_info_free,                    /* info free    */
    H5VL_provenance_info_to_str,                  /* info to str  */
    H5VL_provenance_str_to_info,                  /* str to info  */
    H5VL_provenance_get_object,                   /* get_object   */
    H5VL_provenance_get_wrap_ctx,                 /* get_wrap_ctx */
    H5VL_provenance_wrap_object,                  /* wrap_object  */
    H5VL_provenance_free_wrap_ctx,                /* free_wrap_ctx */
    {                                           /* attribute_cls */
        H5VL_provenance_attr_create,                       /* create */
        H5VL_provenance_attr_open,                         /* open */
        H5VL_provenance_attr_read,                         /* read */
        H5VL_provenance_attr_write,                        /* write */
        H5VL_provenance_attr_get,                          /* get */
        H5VL_provenance_attr_specific,                     /* specific */
        H5VL_provenance_attr_optional,                     /* optional */
        H5VL_provenance_attr_close                         /* close */
    },
    {                                           /* dataset_cls */
        H5VL_provenance_dataset_create,                    /* create */
        H5VL_provenance_dataset_open,                      /* open */
        H5VL_provenance_dataset_read,                      /* read */
        H5VL_provenance_dataset_write,                     /* write */
        H5VL_provenance_dataset_get,                       /* get */
        H5VL_provenance_dataset_specific,                  /* specific */
        H5VL_provenance_dataset_optional,                  /* optional */
        H5VL_provenance_dataset_close                      /* close */
    },
    {                                               /* datatype_cls */
        H5VL_provenance_datatype_commit,                   /* commit */
        H5VL_provenance_datatype_open,                     /* open */
        H5VL_provenance_datatype_get,                      /* get_size */
        H5VL_provenance_datatype_specific,                 /* specific */
        H5VL_provenance_datatype_optional,                 /* optional */
        H5VL_provenance_datatype_close                     /* close */
    },
    {                                           /* file_cls */
        H5VL_provenance_file_create,                       /* create */
        H5VL_provenance_file_open,                         /* open */
        H5VL_provenance_file_get,                          /* get */
        H5VL_provenance_file_specific,                     /* specific */
        H5VL_provenance_file_optional,                     /* optional */
        H5VL_provenance_file_close                         /* close */
    },
    {                                           /* group_cls */
        H5VL_provenance_group_create,                      /* create */
        H5VL_provenance_group_open,                        /* open */
        H5VL_provenance_group_get,                         /* get */
        H5VL_provenance_group_specific,                    /* specific */
        H5VL_provenance_group_optional,                    /* optional */
        H5VL_provenance_group_close                        /* close */
    },
    {                                           /* link_cls */
        H5VL_provenance_link_create,                       /* create */
        H5VL_provenance_link_copy,                         /* copy */
        H5VL_provenance_link_move,                         /* move */
        H5VL_provenance_link_get,                          /* get */
        H5VL_provenance_link_specific,                     /* specific */
        H5VL_provenance_link_optional,                     /* optional */
    },
    {                                           /* object_cls */
        H5VL_provenance_object_open,                       /* open */
        H5VL_provenance_object_copy,                       /* copy */
        H5VL_provenance_object_get,                        /* get */
        H5VL_provenance_object_specific,                   /* specific */
        H5VL_provenance_object_optional,                   /* optional */
    },
    {                                           /* request_cls */
        H5VL_provenance_request_wait,                      /* wait */
        H5VL_provenance_request_notify,                    /* notify */
        H5VL_provenance_request_cancel,                    /* cancel */
        H5VL_provenance_request_specific,                  /* specific */
        H5VL_provenance_request_optional,                  /* optional */
        H5VL_provenance_request_free                       /* free */
    },
    NULL                                        /* optional */
};

/* The connector identification number, initialized at runtime */
static hid_t prov_connector_id_global = H5I_INVALID_HID;

H5VL_provenance_t* _obj_wrap_under(void* under, H5VL_provenance_t* upper_o,
        const char *name, H5I_type_t type, hid_t dxpl_id, void** req);

datatype_prov_info_t* new_datatype_info(void){
    datatype_prov_info_t* info = calloc(1, sizeof(datatype_prov_info_t));
    info->prov_helper = PROV_HELPER;
    return info;
}

dataset_prov_info_t * new_dataset_info(void){
    dataset_prov_info_t* info = calloc(1, sizeof(dataset_prov_info_t));
    info->prov_helper = PROV_HELPER;
    return info;
}

group_prov_info_t * new_group_info(file_prov_info_t* root_file, haddr_t addr){
    group_prov_info_t* info = calloc(1, sizeof(group_prov_info_t));
    info->root_file_info = root_file;
    info->prov_helper = PROV_HELPER;
    info->native_addr = addr;
    info->ref_cnt = 0;
    return info;
}

file_prov_info_t* new_file_info(char* fname) {
    file_prov_info_t* info = calloc(1, sizeof(file_prov_info_t));
    info->file_name = strdup(fname);
    info->prov_helper = PROV_HELPER;
    return info;
}

void file_info_free(file_prov_info_t* info){
    if(info->file_name){
        //printf("%s:%d, info->file_name = %p, = %s\n", __func__, __LINE__, info->file_name, info->file_name);
        free(info->file_name);
        //printf("%s:%d, After free: info->file_name = %p, = %s\n", __func__, __LINE__, info->file_name, info->file_name);
    }
    free(info);
    //printf("%s:%d, after info is freed: info = %p\n", __func__, __LINE__, info);
}

void group_info_free(group_prov_info_t* info){
    free(info);
}

void dataset_info_free(dataset_prov_info_t* info){
    if(!info)
        return;

//    if(info->file_name)
//        free(info->file_name);

    if(info->dset_name)
        free(info->dset_name);

    free(info);
}

void dataset_stats_prov_write(dataset_prov_info_t* ds_info){
    if(!ds_info){
        printf("dataset_stats_prov_write(): ds_info is NULL.\n");
        return;
    }
    printf("Dataset name = %s,\ndata type class = %d, data space class = %d, data space size = %llu, data type size =%llu.\n",
            ds_info->dset_name, ds_info->dt_class, ds_info->ds_class,  ds_info->dset_space_size, ds_info->dset_type_size);
    printf("Dataset is %d dimensions.\n", ds_info->dimension_cnt);
    printf("Dataset is read %d time, %llu bytes in total, costs %llu us.\n", ds_info->dataset_read_cnt, ds_info->total_bytes_read, ds_info->total_read_time);
    printf("Dataset is written %d time, %llu bytes in total, costs %llu us.\n", ds_info->dataset_write_cnt, ds_info->total_bytes_written, ds_info->total_write_time);
}

//not file_prov_info_t!
void file_stats_prov_write(file_prov_info_t* file_info) {
    if(!file_info){
        printf("file_stats_prov_write(): ds_info is NULL.\n");
        return;
    }

    printf("H5 file closed, %d datasets are created, %d datasets are accessed.\n", file_info->ds_created, file_info->ds_accessed);

}

void datatype_stats_prov_write(datatype_prov_info_t* dt_info) {
    if(!dt_info){
        printf("datatype_stats_prov_write(): ds_info is NULL.\n");
        return;
    }
    printf("Datatype name = %s, commited %d times, datatype get is called %d times.\n", dt_info->dtype_name, dt_info->datatype_commit_cnt, dt_info->datatype_get_cnt);
}

void group_stats_prov_write(group_prov_info_t* grp_info) {
    if(!grp_info){
        printf("group_stats_prov_write(): grp_info is NULL.\n");
        return;
    }
    printf("group_stats_prov_write() is yet to be implemented.\n");
}

prov_helper* prov_helper_init( char* file_path, Prov_level prov_level, char* prov_line_format){
    prov_helper* new_helper = calloc(1, sizeof(prov_helper));
    //printf("%s:%d, file_path = %s\n", __func__, __LINE__, file_path);
    if(prov_level >= 2) {//write to file
        if(!file_path){
            printf("prov_helper_init() failed, provenance file path is not set.\n");
            return NULL;
        }
    }

    new_helper->prov_file_path = strdup(file_path);

    new_helper->prov_line_format = strdup(prov_line_format);

    new_helper->prov_level = prov_level;

    new_helper->pid = getpid();

    new_helper->opened_files = NULL;
    new_helper->opened_files_cnt = 0;

    //pid_t tid = gettid(); Linux

    pthread_threadid_np(NULL, &(new_helper->tid));//OSX only

    getlogin_r(new_helper->user_name, 32);

    if(new_helper->prov_level == File_only | new_helper->prov_level == File_and_print){
        new_helper->prov_file_handle = fopen(new_helper->prov_file_path, "a");
    }

    return new_helper;
}

//problematic!!! May cause weird memory corruption issues.
void prov_helper_teardown(prov_helper* helper){
    if(helper){// not null
        if(helper->prov_level == File_only | helper->prov_level ==File_and_print){//no file
            fflush(helper->prov_file_handle);
            fclose(helper->prov_file_handle);
        }
        if(helper->prov_file_path)
            free(helper->prov_file_path);
        if(helper->prov_line_format)
            free(helper->prov_line_format);

        free(helper);
    }
}

void file_ds_created(file_prov_info_t* info){
    assert(info);
    if(info)
        info->ds_created++;
}

//counting how many times datasets are opened in a file.
//Called by a DS
void file_ds_accessed(file_prov_info_t* info){
    assert(info);
    if(info)
        info->ds_accessed++;
}

file_prov_info_t* add_file_node(prov_helper* helper, char* file_name, unsigned long file_no){
    assert(helper);
    //assert(file_name);
    if(!file_name){//fake upper_o
        return NULL;
    }
    printf("%s:%d, helper->opened_files_cnt = %d\n", __func__, __LINE__, helper->opened_files_cnt);

//    if(helper->opened_files_cnt == 0){
//        helper->opened_files->next = new_node;
//
//    } else {

    printf("%s:%d, helper = %p, helper->opened_files = %p\n", __func__, __LINE__, helper, helper->opened_files);

    if(!helper->opened_files){//empty linked list, no opened file.
        helper->opened_files_cnt = 0;
        file_prov_info_t* new_node = calloc(1, sizeof(file_prov_info_t));
        new_node->file_name = strdup(file_name);
        new_node->ref_cnt = 1;
        new_node->file_no = file_no;
        helper->opened_files = new_node;
        helper->opened_files_cnt++;
        printf("%s:%d, new node added as the first node. name = %s, pointer = %p\n", __func__, __LINE__, file_name, new_node);
        return new_node;
    } else {
        file_prov_info_t* cur = helper->opened_files;
        file_prov_info_t* last = cur;
        printf("%s:%d, helper->opened_files = %p, cur = %p\n", __func__, __LINE__, helper->opened_files, cur);
        while (cur) {
            printf("%s:%d, helper->opened_files = %p, cur = %p\n", __func__, __LINE__, helper->opened_files, cur);
            if (!strcmp(cur->file_name, file_name)) {//attempt to open an already opened file.
                if(cur->file_no == 0){
                    cur->file_no = file_no;
                }
                cur->ref_cnt++;
                printf("%s:%d, attempt to open an already opened file. name = %s, pointer = %p, now ref_cnt = %d\n", __func__, __LINE__, file_name, cur, cur->ref_cnt);
                return cur;
            }
            last = cur;
            cur = cur->next;
        }
        file_prov_info_t* new_node = calloc(1, sizeof(file_prov_info_t));
        new_node->file_name = strdup(file_name);
        new_node->ref_cnt = 1;
        new_node->file_no = file_no;
        last->next = new_node;
        printf("%s:%d, new node added as the last node. name = %s, pointer = %p\n", __func__, __LINE__, file_name, new_node);
        helper->opened_files_cnt++;
        return new_node;
    }
}

group_prov_info_t* add_grp_node(file_prov_info_t* root_file, haddr_t native_addr){
    assert(root_file);
    printf("%s:%d: native_addr = %d\n", __func__, __LINE__, native_addr);

    root_file->ref_cnt++;

    if(!root_file->opened_grps){

        root_file->opened_grps_cnt = 1;
        group_prov_info_t* new_grp = new_group_info(root_file, native_addr);
        new_grp->ref_cnt = 1;
        root_file->opened_grps = new_grp;
        printf("%s:%d, opened_grps was empty, now = %p\n", __func__, __LINE__, root_file->opened_grps);
        printf("%s:%d, new_grp = %p\n", __func__, __LINE__, new_grp);
        return new_grp;
    } else {
        group_prov_info_t* cur = root_file->opened_grps;
        group_prov_info_t* last = cur;
        printf("%s:%d, opened_grps = %p\n", __func__, __LINE__, cur);
        while(cur){
            if(native_addr == cur->native_addr){
                cur->ref_cnt++;
                printf("%s:%d, return cur = %p\n", __func__, __LINE__, cur);
                return cur;
            }
            last = cur;
            cur = cur->next;
        }
        group_prov_info_t* new_grp = new_group_info(root_file, native_addr);
        new_grp->ref_cnt = 1;
        last->next = new_grp;
        root_file->opened_grps_cnt++;
        printf("%s:%d, new_grp = %p\n", __func__, __LINE__, new_grp);
        return new_grp;
    }
}

int rm_grp_node(file_prov_info_t* root_file, haddr_t native_addr){
    assert(root_file);

    root_file->ref_cnt--;

    if(!root_file->opened_grps){
        printf("%s:%d: opened_grps is empty, target native_addr = %d\n", __func__, __LINE__, native_addr);
        return root_file->opened_grps_cnt;
    }

    group_prov_info_t* cur = root_file->opened_grps;
    group_prov_info_t* last = cur;
    printf("%s:%d: root_file = %p, file_name = %p, opened_grps = %p, opened_grps_cnt = %d\n",
            __func__, __LINE__, root_file, root_file->file_name, cur, root_file->opened_grps_cnt);
    if( root_file->opened_grps_cnt == 0){
        printf("%s:%d\n", __func__, __LINE__);
        printf("%s:%d: Empty group linkedlist: root_file->opened_grps = %p, "
                "root_file->opened_grps_cnt = %d, target native_addr = %d\n",
                __func__, __LINE__, cur, root_file->opened_grps_cnt, native_addr);
        return 0;
    }

    int index = 0;
    printf("%s:%d: non-empty group linkedlist: root_file->opened_grps = %p, "
            "root_file->opened_grps_cnt = %d, target native_addr = %d\n",
            __func__, __LINE__, cur, root_file->opened_grps_cnt, native_addr);
    while(cur){
        printf("%s:%d while ...\n", __func__, __LINE__);
//        printf("%s:%d: cur = %p, target native_addr = %d\n",
//                __func__, __LINE__, cur, native_addr);
        if(native_addr == cur->native_addr){
            cur->ref_cnt--;
            if(cur->ref_cnt > 0)
                return root_file->opened_grps_cnt;
            else{//remove node
                if(0 == index){
                    root_file->opened_grps = root_file->opened_grps->next;
                    group_info_free(cur);
                    root_file->opened_grps_cnt--;
                    return root_file->opened_grps_cnt;
                }
                last->next = cur->next;
                group_info_free(cur);
                root_file->opened_grps_cnt--;
                return root_file->opened_grps_cnt;
            }
        }
        last = cur;
        cur = cur->next;
        index++;
    }

    return root_file->opened_grps_cnt;
}
//need a dumy node to make it simpler
int rm_file_node(prov_helper* helper, char* file_name){
    assert(helper);
    assert(file_name);
    printf("%s:%d, helper->opened_files = %p, opened_files_cnt = %d, target filename = %p\n",
            __func__, __LINE__, helper->opened_files,helper->opened_files_cnt, file_name);
    if(!helper->opened_files){//no node found
        //helper->opened_files_cnt = 0;
        return helper->opened_files_cnt;//no fopen files.
    }
    file_prov_info_t* cur = helper->opened_files;
    file_prov_info_t* last = cur;
    int index = 0;
    printf("%s:%d\n", __func__, __LINE__);
    printf("%s:%d: helper->opened_files->file_name = [%s]\n",
            __func__, __LINE__, helper->opened_files->file_name);
    while(cur){
        if(!strcmp(cur->file_name, file_name)){//node found
            cur->ref_cnt--;
            if(cur->ref_cnt > 0){//not to remove
                printf("%s:%d, helper->opened_files = %p, file_cnt = %d, cur->ref_cnt = %d\n",
                        __func__, __LINE__, helper->opened_files, helper->opened_files_cnt, cur->ref_cnt);
                return helper->opened_files_cnt;
            }
            else { //cur->next->ref_cnt == 0, remove file node & maybe print file stats (heppens when close a file)
                //file_prov_info_t* t = cur; //to remove
                printf("%s:%d, cur->ref_cnt = %d, helper->opened_files = %p, "
                        "cur = %p, cur->next = %p, last->next = %p\n",
                        __func__, __LINE__, cur->ref_cnt, helper->opened_files, cur, cur->next, last->next);

                if(0 == index){//first node is the target
                    printf("%s:%d:helper->opened_files = %p, opened_files->next = %p, next to free: cur = %p\n", __func__, __LINE__, helper->opened_files, helper->opened_files->next, cur);
                    helper->opened_files = helper->opened_files->next;
                    file_info_free(cur);
                    printf("%s:%d:after free cur: helper->opened_files = %p,  cur = %p\n", __func__, __LINE__, helper->opened_files, cur);
                    helper->opened_files_cnt--;
                    printf("%s:%d, first node is the target, after unlink node: current helper->opened_files = %p, file_cnt = %d\n",
                            __func__, __LINE__, helper->opened_files, helper->opened_files_cnt);
                    return helper->opened_files_cnt;
                }

                last->next = cur->next;

                file_info_free(cur);
                helper->opened_files_cnt--;
                printf("%s:%d, helper->opened_files_cnt = %d\n",
                        __func__, __LINE__, helper->opened_files_cnt);
                if(helper->opened_files_cnt == 0){
                    helper->opened_files = NULL;
                }

                printf("%s:%d, helper->opened_files = %p, file_cnt = %d\n",
                        __func__, __LINE__, helper->opened_files, helper->opened_files_cnt);
                return helper->opened_files_cnt;
            }
        }
        last = cur;
        cur = cur->next;
        index++;
    }
    //node not found.
    printf("%s:%d, helper->opened_files = %p, file_cnt = %d\n",
            __func__, __LINE__, helper->opened_files, helper->opened_files_cnt);
    return helper->opened_files_cnt;
}

file_prov_info_t* _search_home_file(unsigned long obj_file_no){
    if(PROV_HELPER->opened_files_cnt < 1)
        return NULL;

    file_prov_info_t* cur = PROV_HELPER->opened_files;
    file_prov_info_t* last = cur;
    while (cur) {
        printf("%s:%d, cur->file_no %d\n", __func__, __LINE__, cur->file_no);
        if (cur->file_no == obj_file_no) {//file found
            cur->ref_cnt++;
            printf("%s:%d, home_file now ref_cnt = %d\n", __func__, __LINE__, cur->ref_cnt);
            return cur;
        }

        last = cur;
        cur = cur->next;
    }

    return NULL;
}
dataset_prov_info_t* add_dataset_node(unsigned long obj_file_no, H5VL_provenance_t* dset, haddr_t native_addr,
        file_prov_info_t* file_info_in, char* ds_name, hid_t dxpl_id, void** req){
    assert(dset);
    assert(dset->under_object);
    assert(file_info_in);
    file_prov_info_t* file_info;
    if(obj_file_no != file_info_in->file_no){//creating a dataset from an external place
        file_prov_info_t* external_home_file = _search_home_file(obj_file_no);
        if(external_home_file){//use extern home
            file_info = external_home_file;
        }else{//extern home not exist, fake one
            file_prov_info_t* new_file = new_file_info("dummy");
            new_file->file_no = obj_file_no;
            file_info = new_file;
        }
    }else{//local
        file_info = file_info_in;
    }

    dataset_prov_info_t* new_node;
//    new_ds_prov_info(dset, native_addr, file_info, ds_name, dapl_id, dxpl_id, req);
    printf("%s:%d, file_info->opened_datasets_cnt = %d, file_info->file_name = [%s], native_addr = [%d], ds_name = [%s]\n",
            __func__, __LINE__, file_info->opened_datasets_cnt, file_info->file_name, native_addr, ds_name);
    printf("%s:%d, file_info->opened_datasets = %p, \n",
            __func__, __LINE__, file_info->opened_datasets);//, file_info->opened_datasets->dset_name
    if(!file_info->opened_datasets){//empty linked list, no opened file.
        printf("%s:%d\n", __func__, __LINE__);
        new_node = new_ds_prov_info(dset->under_object, dset->under_vol_id,
                native_addr, file_info, ds_name, dxpl_id, req);
        file_info->opened_datasets_cnt = 0;
        new_node->ref_cnt = 1;
        file_info->opened_datasets = new_node;
        file_info->opened_datasets_cnt++;
        printf("%s:%d, new node added as the first node. name = [%s], returned new_node = %p, new_node->next = %p, file_info->opened_datasets_cnt = %d\n",
                __func__, __LINE__, new_node->dset_name, new_node, new_node->next, file_info->opened_datasets_cnt);

        return new_node;
    } else {
        printf("%s:%d\n", __func__, __LINE__);
        dataset_prov_info_t* cur = file_info->opened_datasets;
        dataset_prov_info_t* last = cur;
//        if(cur){
//            printf("%s:%d, head node file_info->opened_datasets_cnt = %d, cur = %p, cur->ref_cnt = %d, cur->next = %p\n",
//                    __func__, __LINE__, file_info->opened_datasets_cnt, cur, cur->ref_cnt, cur->next);
//            printf("%s:%d: cur->dset_name = [%s]\n",
//                    __func__, __LINE__,  cur->dset_name);
//        }
        int i = 0;

        while (cur) {
//            printf("%s:%d, file_info->opened_datasets = %p, node No.%d, cur = %p, opened_datasets_cnt = %d\n",
//                    __func__, __LINE__, file_info->opened_datasets, i, cur, file_info->opened_datasets_cnt);
//            printf("%s:%d: cur->dset_name = %p\n",
//                    __func__, __LINE__, cur->dset_name);
            printf("%s:%d,: cur->native_addr = [%p], native_addr = %d\n",
                    __func__, __LINE__, cur->native_addr, native_addr);
            if (cur->native_addr == native_addr) {
                //attempt to open an already opened file.

                cur->ref_cnt++;
                printf("%s:%d, attempted to open an already opened dataset. name = [%s], pointer = %p, now ref_cnt = %d\n",
                                        __func__, __LINE__, cur->dset_name, cur, cur->ref_cnt);
                return cur;
            }
            i++;
            last = cur;
            cur = cur->next;
        }
        printf("%s:%d, ds_name = [%s]\n", __func__, __LINE__, ds_name);
        new_node = new_ds_prov_info(dset->under_object, dset->under_vol_id, native_addr, file_info, ds_name, dxpl_id, req);
        last->next = new_node;
        file_info->opened_datasets_cnt++;
        printf("%s:%d, new node added to the tail. name = [%s], pointer = %p, ref_cnt = %d, file_info->opened_datasets_cnt = %d\n",
                __func__, __LINE__, new_node->dset_name, new_node, new_node->ref_cnt, file_info->opened_datasets_cnt);

        return new_node;
    }
}

//need a dumy node to make it simpler
int rm_dataset_node(file_prov_info_t* file_info, haddr_t native_addr){
    assert(file_info);
    printf("%s:%d, before removal: file_info = %p, opened_ds_cnt = %d\n", __func__, __LINE__, file_info, file_info->opened_datasets_cnt);

//    printf("%s:%d, before removal: "
//            "file_info->opened_datasets = %p, file_info->opened_datasets_cnt = %d, "
//            "\n",
//            __func__, __LINE__,
//            file_info->opened_datasets, file_info->opened_datasets_cnt
//            );
    if(!file_info->opened_datasets){//no node found
        //file_info->opened_datasets_cnt = 0;
        return file_info->opened_datasets_cnt;//no fopen files.
    }

    dataset_prov_info_t* ds_cur = file_info->opened_datasets;

    dataset_prov_info_t* last = ds_cur;
    int index = 0;
    while(ds_cur){
        printf("%s:%d\n, opened_datasets_cnt = %d, index = %d\n", __func__, __LINE__, file_info->opened_datasets_cnt, index);
//        if(file_info->opened_datasets_cnt > 0){
//
//        }
        printf("%s:%d ds_cur = [%p]\n", __func__, __LINE__, ds_cur);
        printf("%s:%d: while(): cur = %p, cur->next = %p, "
                "cur->ref_cnt = %d, index = %d\n",
                __func__, __LINE__,
                ds_cur,
                ds_cur->next,
                ds_cur->ref_cnt, index);//strcmp(cur->dset_name, dset_name)
        //printf("%s:%d, ds_name = [%s]\n", __func__, __LINE__, ds_cur->dset_name);
        if(ds_cur->native_addr == native_addr){//node found
            printf("%s:%d:  found match: cur->dset_name = [%s], cur->ref_cnt = %d\n",
                    __func__, __LINE__, ds_cur->dset_name, ds_cur->ref_cnt);
            ds_cur->ref_cnt--;
            if(ds_cur->ref_cnt > 0){
                printf("%s:%d:  still have refs: cur->dset_name = [%s], cur->ref_cnt = %d\n",
                        __func__, __LINE__, ds_cur->dset_name, ds_cur->ref_cnt);
                return ds_cur->ref_cnt;
            }
            else{//cur->next->ref_cnt == 0, remove file node & maybe print file stats (heppens when close a file)
                //file_prov_info_t* t = cur; //to remove
                printf("%s:%d, REMOVE NODE: cur->ref_cnt = %d, file_info->opened_datasets = %p, cur = %p, cur->next = %p, last->next = %p\n", __func__, __LINE__, ds_cur->ref_cnt, file_info->opened_datasets, ds_cur, ds_cur->next, last->next);

                //special case: first node is the target, ==cur
                if(0 == index){
                    printf("%s:%d, ds_cnt = %d\n", __func__, __LINE__, file_info->opened_datasets_cnt);
                    file_info->opened_datasets = file_info->opened_datasets->next;
                    printf("%s:%d\n", __func__, __LINE__);
                    dataset_info_free(ds_cur);
                    printf("%s:%d\n", __func__, __LINE__);
                    file_info->opened_datasets_cnt--;
                    printf("%s:%d\n", __func__, __LINE__);
                    printf("%s:%d, After removing the head node: file_info = %p, file_info->file_name = [%p], file_info->opened_datasets = %p\n",
                            __func__, __LINE__, file_info, file_info->file_name, file_info->opened_datasets);
                    //return file_info->opened_datasets_cnt;
                    return 0;//ref_cnt
                }
                printf("%s:%d\n", __func__, __LINE__);
                last->next = ds_cur->next;

                dataset_info_free(ds_cur);
                file_info->opened_datasets_cnt--;
                printf("%s:%d, file_info->opened_datasets_cnt = %d\n",
                        __func__, __LINE__, file_info->opened_datasets_cnt);

                if(file_info->opened_datasets_cnt == 0){
                    file_info->opened_datasets = NULL;
                } else {
                    printf("%s:%d\n", __func__, __LINE__);
                    printf("%s:%d, file_info->opened_datasets = %p,"
                            " file_info->opened_datasets->dset_name = [%s],"
                            " file_info->opened_datasets->file_name = [%s]\n"
                            ,
                            __func__, __LINE__, file_info->opened_datasets
                             ,file_info->opened_datasets->dset_name
                             ,file_info->opened_datasets->file_name
                            );
                    //printf("file_info->opened_datasets->file_name = [%s]\n", file_info->opened_datasets->file_name);
                }
                printf("%s:%d\n", __func__, __LINE__);
                printf("%s:%d, after removal: file_info->opened_datasets = %p, file_info->opened_datasets_cnt = %d\n", __func__, __LINE__, file_info->opened_datasets, file_info->opened_datasets_cnt);
                return 0;//file_info->opened_datasets_cnt;
            }
        }
        last = ds_cur;
        ds_cur = ds_cur->next;
        index++;
    }
    printf("%s:%d\n", __func__, __LINE__);
    //node not found.
    printf("%s:%d, after removal: file_info->opened_datasets = %p, file_info->opened_datasets_cnt = %d\n", __func__, __LINE__, file_info->opened_datasets, file_info->opened_datasets_cnt);
    return -1;
}

void ptr_cnt_increment(prov_helper* helper){
    assert(helper);
    //printf("%s:%d, helper = %p\n", __func__, __LINE__, helper);
    //printf("%s:%d, &helper->ptr_cnt = %p\n", __func__, __LINE__, &(helper->ptr_cnt));
    //mutex lock
    if(helper){


        //printf("%s:%d, helper->ptr_cnt = %d\n", __func__, __LINE__, helper->ptr_cnt);
        (helper->ptr_cnt)++;//memory corruption ?
    }
    //printf("%s:%d\n", __func__, __LINE__);
    //mutex unlock
}

void ptr_cnt_decrement(prov_helper* helper){
    assert(helper);
    //assert(helper->ptr_cnt);
    //mutex lock
    //printf("%s:%d, helper->ptr_cnt = %d\n", __func__, __LINE__, helper->ptr_cnt);
    helper->ptr_cnt--;
    //mutex unlock
    if(helper->ptr_cnt == 0);// do nothing for now.
        //prov_helper_teardown(helper);loggin is not decided yet.
}


void get_time_str(char *str_out){
    *str_out = '\0';
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    sprintf(str_out, "%d/%d/%d %d:%d:%d", timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

unsigned long get_time_usec() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return 1000000 * tp.tv_sec + tp.tv_usec;
}

dataset_prov_info_t* new_ds_prov_info(void* under_object, hid_t vol_id, haddr_t native_addr,
        file_prov_info_t* file_info, char* ds_name, hid_t dxpl_id, void **req){
    assert(under_object);
    assert(file_info);
    hid_t dcpl_id = -1;
    hid_t dt_id = -1;
    hid_t ds_id = -1;

    dataset_prov_info_t* ds_info = calloc(1, sizeof(dataset_prov_info_t));
    ds_info->prov_helper = PROV_HELPER;
    ds_info->root_file_info = file_info;
    ds_info->native_addr = native_addr;
    ds_info->next = NULL;
    dataset_get_wrapper(under_object, vol_id, H5VL_DATASET_GET_DCPL, dxpl_id, req, &dcpl_id);
//    printf("file_info->file_name = [%s], ->ref_cnt = %d, p_helper = %p, PROV_HELPER->opened_files->file_name = %s, file_cnt = %d\n",
//            file_info->file_name, file_info->ref_cnt, file_info->prov_helper, PROV_HELPER->opened_files->file_name, PROV_HELPER->opened_files_cnt);
    if(file_info)
        ds_info->file_name = strdup(file_info->file_name);

    if(ds_name == NULL)
        ds_info->dset_name = strdup("NULL");
    else
        ds_info->dset_name = strdup(ds_name);

    dataset_get_wrapper(under_object, vol_id, H5VL_DATASET_GET_TYPE, dxpl_id, req, &dt_id);
    ds_info->dt_class = H5Tget_class(dt_id);
    ds_info->dset_type_size = H5Tget_size(dt_id);

    dataset_get_wrapper(under_object, vol_id, H5VL_DATASET_GET_SPACE, dxpl_id, req, &ds_id);

    ds_info->ds_class = H5Sget_simple_extent_type(ds_id);

    if (ds_info->ds_class == H5S_SIMPLE) {
        ds_info->dimension_cnt = H5Sget_simple_extent_ndims(ds_id);
        H5Sget_simple_extent_dims(ds_id, ds_info->dimensions, NULL);
        ds_info->dset_space_size = H5Sget_simple_extent_npoints(ds_id);
    }
    ds_info->layout = H5Pget_layout(dcpl_id);

    return ds_info;
}

H5VL_loc_params_t _new_loc_pram(H5I_type_t type){
    H5VL_loc_params_t loc_params_new;
    loc_params_new.type = H5VL_OBJECT_BY_SELF;
    loc_params_new.obj_type = type;
    return loc_params_new;
}

herr_t get_native_info(void* obj, hid_t vol_id, hid_t dxpl_id, void **req, ...){
    herr_t r = -1;
    va_list args;

    va_start(args, req);//add an new arg after req
    int got_info = H5VLobject_optional(obj, vol_id, dxpl_id, req, args);

    if(got_info < 0){
        printf("%s:%d: H5VL_object_optional() failed to get obj into. ret = %d \n", __func__, __LINE__, got_info);
        return -1;
    }
    return r;
}


void get_native_file_no(unsigned long* file_num_out, H5VL_provenance_t* file_obj){

    H5VL_loc_params_t p = _new_loc_pram(H5I_FILE);
    H5O_info_t oinfo;
    void* root_grp = H5VLgroup_open(file_obj->under_object, &p, file_obj->under_vol_id,
            "/", H5P_DEFAULT, H5P_DEFAULT, NULL);
    printf("%s:%d: root_grp = %p\n", __func__, __LINE__, root_grp);
    p.obj_type = H5I_GROUP;
    get_native_info(root_grp, file_obj->under_vol_id,
            H5P_DEFAULT, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);

    *file_num_out = oinfo.fileno;
    printf("%s:%d: file_no = %lu\n", __func__, __LINE__, oinfo.fileno);
    H5VLgroup_close(root_grp, file_obj->under_vol_id, H5P_DEFAULT, NULL);//file_obj->under_object
}




void dataset_get_wrapper(void *dset, hid_t driver_id, H5VL_dataset_get_t get_type,
        hid_t dxpl_id, void **req, ...){
    va_list args;
    va_start(args, req);
    H5VLdataset_get(dset, driver_id, get_type, dxpl_id, req, args);
}

int prov_write(prov_helper* helper_in, const char* msg, unsigned long duration){
    assert(helper_in);
    char time[64];
    get_time_str(time);
    char pline[512];

    /* Trimming long VOL function names */
    char* base = "H5VL_provenance_";
    int base_len = strlen(base);
    int msg_len = strlen(msg);
    int trim_offset = 0;
    if(msg_len > base_len) {//strlen(H5VL_provenance_) == 16.

        int i = 0;
        for(; i < base_len; i++){
            if(base[i] != msg[i])
                break;
        }
        if(i == base_len) {// do trimming
            trim_offset = base_len;
        }
    }

    sprintf(pline, "[%s][User:%s][PID:%d][TID:%llu][Func:%s][%luus]\n", time, helper_in->user_name, helper_in->pid, helper_in->tid, msg + base_len, duration);

    switch(helper_in->prov_level){
        case File_only:
            fputs(pline, helper_in->prov_file_handle);
            break;

        case File_and_print:
            fputs(pline, helper_in->prov_file_handle);
            printf("%s", pline);
            break;

        case Print_only:
            printf("%s", pline);
            break;

        default:
            break;
    }

    if(helper_in->prov_level == (File_only | File_and_print)){
        fputs(pline, helper_in->prov_file_handle);
    }

    return 0;
}

/*-------------------------------------------------------------------------
 * Function:    H5VL__provenance_new_obj
 *
 * Purpose:     Create a new pass through object for an underlying object
 *
 * Return:      Success:    Pointer to the new pass through object
 *              Failure:    NULL
 *
 * Programmer:  Quincey Koziol
 *              Monday, December 3, 2018
 *
 *-------------------------------------------------------------------------
 */
static H5VL_provenance_t *
H5VL_provenance_new_obj(void *under_obj, hid_t under_vol_id, prov_helper* helper)
{
    assert(under_obj);
    assert(helper);
    H5VL_provenance_t *new_obj;
    printf("%s:%d\n", __func__, __LINE__);
    new_obj = (H5VL_provenance_t *)calloc(1, sizeof(H5VL_provenance_t));
    new_obj->under_object = under_obj;
    new_obj->under_vol_id = under_vol_id;
    //printf("%s:%d\n", __func__, __LINE__);
    new_obj->prov_helper = helper;
    //new_obj->
    //printf("%s:%d: helper = %p\n", __func__, __LINE__, helper);
    //printf("%s:%d: &helper->ptr_cnt = %p\n", __func__, __LINE__, &(helper->ptr_cnt));
    ptr_cnt_increment(new_obj->prov_helper);
    //printf("%s:%d\n", __func__, __LINE__);
    //new_obj->shared_file_info = calloc(1, sizeof(H5VL_provenance_info_t)); // done by file_open/create
    printf("%s:%d\n", __func__, __LINE__);
    H5Iinc_ref(new_obj->under_vol_id);
    printf("%s:%d\n", __func__, __LINE__);

    return new_obj;
} /* end H5VL__provenance_new_obj() */


/*-------------------------------------------------------------------------
 * Function:    H5VL__provenance_free_obj
 *
 * Purpose:     Release a pass through object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 * Programmer:  Quincey Koziol
 *              Monday, December 3, 2018
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_free_obj(H5VL_provenance_t *obj)
{
//    printf("%s:%d, helper->opened_files = %p, opened_filed_cnt = %d\n",
//            __func__, __LINE__, PROV_HELPER->opened_files, PROV_HELPER->opened_files_cnt);
    int n = PROV_HELPER->opened_files_cnt;
//    if(n > 0){
//        file_prov_info_t* cur = PROV_HELPER->opened_files;
//        int i = 0;
//        while(cur){
//            if(cur->file_name){
//                printf("%s:%d: file No.%d, cur = %p, file_name = %s, ds_cnt = %d \n",
//                    __func__, __LINE__, i, cur, cur->file_name, cur->opened_datasets_cnt);
//                if(cur->opened_datasets_cnt > 0){
//                    printf("%s:%d: file No.%d, cur->opened_datasets = %p, ds_name = %p\n",
//                        __func__, __LINE__, i, cur->opened_datasets);
//                }
//            }else{//null
//                printf("%s:%d: file No.%d, cur = %p, file_name = %p, ds_cnt = %d \n",
//                                    __func__, __LINE__, i, cur->file_name, cur->opened_datasets_cnt);
//            }
//            cur = cur->next;
//            i++;
//        }
//    }
//    printf("%s:%d \n", __func__, __LINE__);
    ptr_cnt_decrement(PROV_HELPER);
//    printf("%s:%d \n", __func__, __LINE__);
    //obj->prov_helper = NULL;
    //obj->shared_file_info = NULL;//skip this field so it can be used later.
    H5Idec_ref(obj->under_vol_id);

    //printf("%s:%d \n", __func__, __LINE__);

    //printf("%s:%d, helper->opened_files = %p\n", __func__, __LINE__, PROV_HELPER->opened_files);
    //obj->prov_helper = NULL;

//    printf("%s:%d: obj = %p \n", __func__, __LINE__, obj);

    free(obj);//changed value of PROV_HELPER->opened_files
//    printf("%s:%d \n", __func__, __LINE__);
//    printf("%s:%d, after free(obj), obj = %p, helper->opened_files = %p\n",
//            __func__, __LINE__, obj, PROV_HELPER->opened_files);
    return 0;
} /* end H5VL__provenance_free_obj() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_register
 *
 * Purpose:     Register the pass-through VOL connector and retrieve an ID
 *              for it.
 *
 * Return:      Success:    The ID for the pass-through VOL connector
 *              Failure:    -1
 *
 * Programmer:  Quincey Koziol
 *              Wednesday, November 28, 2018
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_provenance_register(void)
{
    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Singleton register the pass-through VOL connector ID */
    if(H5I_VOL != H5Iget_type(prov_connector_id_global))
        prov_connector_id_global = H5VLregister_connector(&H5VL_provenance_cls, H5P_DEFAULT);

    return prov_connector_id_global;
} /* end H5VL_provenance_register() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_init
 *
 * Purpose:     Initialize this VOL connector, performing any necessary
 *              operations for the connector that will apply to all containers
 *              accessed with the connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_init(hid_t vipl_id)
{

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INIT\n");
#endif

    /* Shut compiler up about unused parameter */
    vipl_id = vipl_id;

    return 0;
} /* end H5VL_provenance_init() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_term
 *
 * Purpose:     Terminate this VOL connector, performing any necessary
 *              operations for the connector that release connector-wide
 *              resources (usually created / initialized with the 'init'
 *              callback).
 *
 * Return:      Success:    0
 *              Failure:    (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_term(void)
{

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL TERM\n");
#endif

    /* Reset VOL ID */
    prov_connector_id_global = H5I_INVALID_HID;

    return 0;
} /* end H5VL_provenance_term() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_info_copy
 *
 * Purpose:     Duplicate the connector's info object.
 *
 * Returns:     Success:    New connector info object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_provenance_info_copy(const void *_info)
{
    const H5VL_provenance_info_t *info = (const H5VL_provenance_info_t *)_info;
    H5VL_provenance_info_t *new_info;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INFO Copy\n");
#endif

    /* Allocate new VOL info struct for the pass through connector */
    new_info = (H5VL_provenance_info_t *)calloc(1, sizeof(H5VL_provenance_info_t));

    /* Increment reference count on underlying VOL ID, and copy the VOL info */
    new_info->under_vol_id = info->under_vol_id;

    if(info->prov_file_path)
        new_info->prov_file_path = strdup(info->prov_file_path);
    if(info->prov_line_format)
        new_info->prov_line_format = strdup(info->prov_line_format);

    new_info->prov_level = info->prov_level;

    H5Iinc_ref(new_info->under_vol_id);
    if(info->under_vol_info)
        H5VLcopy_connector_info(new_info->under_vol_id, &(new_info->under_vol_info), info->under_vol_info);

    return new_info;
} /* end H5VL_provenance_info_copy() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_info_cmp
 *
 * Purpose:     Compare two of the connector's info objects, setting *cmp_value,
 *              following the same rules as strcmp().
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_info_cmp(int *cmp_value, const void *_info1, const void *_info2)
{
    const H5VL_provenance_info_t *info1 = (const H5VL_provenance_info_t *)_info1;
    const H5VL_provenance_info_t *info2 = (const H5VL_provenance_info_t *)_info2;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INFO Compare\n");
#endif

    /* Sanity checks */
    assert(info1);
    assert(info2);

    /* Initialize comparison value */
    *cmp_value = 0;
    
    /* Compare under VOL connector classes */
    H5VLcmp_connector_cls(cmp_value, info1->under_vol_id, info2->under_vol_id);
    if(*cmp_value != 0){
        return 0;
    }

    /* Compare under VOL connector info objects */
    H5VLcmp_connector_info(cmp_value, info1->under_vol_id, info1->under_vol_info, info2->under_vol_info);
    if(*cmp_value != 0){
        return 0;
    }

    *cmp_value = strcmp(info1->prov_file_path, info2->prov_file_path);
    if(*cmp_value != 0){
        return 0;
    }

    *cmp_value = strcmp(info1->prov_line_format, info2->prov_line_format);
    if(*cmp_value != 0){
        return 0;
    }
    *cmp_value = info1->prov_level - info2->prov_level;
    if(*cmp_value != 0){
        return 0;
    }

    return 0;
} /* end H5VL_provenance_info_cmp() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_info_free
 *
 * Purpose:     Release an info object for the connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_info_free(void *_info)
{
    H5VL_provenance_info_t *info = (H5VL_provenance_info_t *)_info;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INFO Free\n");
#endif

    /* Release underlying VOL ID and info */
    if(info->under_vol_info)
        H5VLfree_connector_info(info->under_vol_id, info->under_vol_info);

    H5Idec_ref(info->under_vol_id);
    //shared_file_info_free(info);
//    if(info->prov_file_path)
//        free(info->prov_file_path);
//
//    if(info->prov_line_format)
//        free(info->prov_line_format);
    /* Free pass through info object itself */
    //free(info);

    return 0;
} /* end H5VL_provenance_info_free() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_info_to_str
 *
 * Purpose:     Serialize an info object for this connector into a string
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_info_to_str(const void *_info, char **str)
{
    const H5VL_provenance_info_t *info = (const H5VL_provenance_info_t *)_info;
    H5VL_class_value_t under_value = (H5VL_class_value_t)-1;
    char *under_vol_string = NULL;
    size_t under_vol_str_len = 0;
    size_t path_len = 0;
    size_t format_len = 0;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INFO To String\n");
#endif

    /* Get value and string for underlying VOL connector */
    H5VLget_value(info->under_vol_id, &under_value);
    H5VLconnector_info_to_str(info->under_vol_info, info->under_vol_id, &under_vol_string);

    /* Determine length of underlying VOL info string */
    if(under_vol_string)
        under_vol_str_len = strlen(under_vol_string);


    if(info->prov_file_path)
        path_len = strlen(info->prov_file_path);

    if(info->prov_line_format)
        format_len = strlen(info->prov_line_format);

    /* Allocate space for our info */
    *str = (char *)H5allocate_memory(64 + under_vol_str_len + path_len + format_len, (hbool_t)0);
    assert(*str);

    /* Encode our info
     * Normally we'd use snprintf() here for a little extra safety, but that
     * call had problems on Windows until recently. So, to be as platform-independent
     * as we can, we're using sprintf() instead.
     */
    sprintf(*str, "under_vol=%u;under_info={%s};path=%s;level=%d;format=%s",
            (unsigned)under_value, (under_vol_string ? under_vol_string : ""), info->prov_file_path, info->prov_level, info->prov_line_format);

    return 0;
} /* end H5VL_provenance_info_to_str() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_str_to_info
 *
 * Purpose:     Deserialize a string into an info object for this connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
herr_t provenance_file_setup(char* str_in, char* file_path_out, int* level_out, char* format_out){
    //acceptable format: path=$path_str;level=$level_int;format=$format_str
    char tmp_str[100] = {'\0'};
    memcpy(tmp_str, str_in, strlen(str_in)+1);
    //int ret = sscanf(tmp_str, "};path=%s level=%d format=%s", file_path_out, level_out, format_out);
    char* toklist[4] = {NULL};
    int i = 0;
        char *p;
        p = strtok(tmp_str, ";");
        while(p != NULL) {
            printf("p=[%s], i=%d\n", p, i);
            toklist[i] = strdup(p);
            //printf("toklist[%d] = %s\n", i, toklist[i]);
            p = strtok(NULL, ";");
            i++;
        }

        sscanf(toklist[1], "path=%s", file_path_out);

        sscanf(toklist[2], "level=%d", level_out);

        sscanf(toklist[3], "format=%s", format_out);

        for(int i = 0; i<=3; i++){
            if(toklist[i]){
                free(toklist[i]);
            }
        }

    //printf("Parsing result: sample_str = %s,\n path = %s,\n level = %d,\n format =%s.\n", str_in, file_path_out, *level_out, format_out);

    return 0;
}

static herr_t
H5VL_provenance_str_to_info(const char *str, void **_info)
{
    H5VL_provenance_info_t *info;
    unsigned under_vol_value;
    const char *under_vol_info_start, *under_vol_info_end;
    hid_t under_vol_id;
    void *under_vol_info = NULL;

    char* prov_file_path;//assign from parsed str
    Prov_level level;
    char* prov_format;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INFO String To Info\n");
#endif

    /* Retrieve the underlying VOL connector value and info */
    sscanf(str, "under_vol=%u;", &under_vol_value);
    under_vol_id = H5VLregister_connector_by_value((H5VL_class_value_t)under_vol_value, H5P_DEFAULT);
    under_vol_info_start = strchr(str, '{');
    under_vol_info_end = strrchr(str, '}');
    assert(under_vol_info_end > under_vol_info_start);
    char *under_vol_info_str = NULL;
    if(under_vol_info_end != (under_vol_info_start + 1)) {
        under_vol_info_str = (char *)malloc((size_t)(under_vol_info_end - under_vol_info_start));
        memcpy(under_vol_info_str, under_vol_info_start + 1, (size_t)((under_vol_info_end - under_vol_info_start) - 1));
        *(under_vol_info_str + (under_vol_info_end - under_vol_info_start)) = '\0';

        H5VLconnector_str_to_info(under_vol_info_str, under_vol_id, &under_vol_info);//generate under_vol_info obj.

    } /* end else */

    /* Allocate new pass-through VOL connector info and set its fields */
    info = (H5VL_provenance_info_t *)calloc(1, sizeof(H5VL_provenance_info_t));
    info->under_vol_id = under_vol_id;
    info->under_vol_info = under_vol_info;

    info->prov_file_path = calloc(64, sizeof(char));
    info->prov_line_format = calloc(64, sizeof(char));

    if(provenance_file_setup(under_vol_info_end, info->prov_file_path, &(info->prov_level), info->prov_line_format) != 0){
        free(info->prov_file_path);
        free(info->prov_line_format);
        info->prov_line_format = NULL;
        info->prov_file_path = NULL;
        info->prov_level = -1;
    }
    //info->prov_level = tmp;
    //printf("After provenance_file_setup(): sample_str = %s,\n path = %s,\n level = %d,\n format =%s.\n", under_vol_info_end, info->prov_file_path, info->prov_level, info->prov_line_format);
    /* Set return value */
    *_info = info;

    if(under_vol_info_str)
        free(under_vol_info_str);
    return 0;
} /* end H5VL_provenance_str_to_info() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_get_object
 *
 * Purpose:     Retrieve the 'data' for a VOL object.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_provenance_get_object(const void *obj)
{
    const H5VL_provenance_t *o = (const H5VL_provenance_t *)obj;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL Get object\n");
#endif

    if(!o){
        printf("H5VLget_object() get a NULL o as a parameter.");
    }

    if(!(o->under_object)){
        printf("H5VLget_object() get a NULL o->under_object as a parameter.");
    }

    void* ret = H5VLget_object(o->under_object, o->under_vol_id);

    return ret;

} /* end H5VL_provenance_get_object() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_get_wrap_ctx
 *
 * Purpose:     Retrieve a "wrapper context" for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_get_wrap_ctx(const void *obj, void **wrap_ctx)
{
    const H5VL_provenance_t *o = (const H5VL_provenance_t *)obj;
    H5VL_provenance_wrap_ctx_t *new_wrap_ctx;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL WRAP CTX Get\n");
#endif
    printf("%s:%d: o->my_type = %d (1 for file, 2 for group, 5 for dataset)\n", __func__, __LINE__, o->my_type);
    /* Allocate new VOL object wrapping context for the pass through connector */
    new_wrap_ctx = (H5VL_provenance_wrap_ctx_t *)calloc(1, sizeof(H5VL_provenance_wrap_ctx_t));
    switch(o->my_type){
    case H5I_DATASET: {
        new_wrap_ctx->root_file_info = ((dataset_prov_info_t*) (o->generic_prov_info))->root_file_info;
        break;
    }
    case H5I_GROUP: {
        new_wrap_ctx->root_file_info = ((group_prov_info_t*)(o->generic_prov_info))->root_file_info;
        //printf("%s:%d: root_file_name = %s\n", __func__, __LINE__, new_wrap_ctx->root_file_info->file_name);
        break;
    }
    case H5I_FILE: {
        new_wrap_ctx->root_file_info = (file_prov_info_t*)(o->generic_prov_info);
        printf("%s:%d: file_name = %s\n", __func__, __LINE__, new_wrap_ctx->root_file_info->file_name);
        break;
    }
    default:
        printf("%s:%d: unexpected type: my_type = %d, o = %p, o->generic_prov_info = %p \n",
                __func__, __LINE__, o->my_type, o->generic_prov_info);
        break;
    }
    if(new_wrap_ctx->root_file_info){
//        printf("%s:%d: file->rec_cnt = %d, file_name = %s\n",
//                __func__, __LINE__, new_wrap_ctx->root_file_info->ref_cnt,
//                new_wrap_ctx->root_file_info->file_name);
    }else{
        printf("%s:%d: new_wrap_ctx->root_file_info is NULL. \n", __func__, __LINE__);
    }

    /* Increment reference count on underlying VOL ID, and copy the VOL info */
    new_wrap_ctx->under_vol_id = o->under_vol_id;
    H5Iinc_ref(new_wrap_ctx->under_vol_id);
    H5VLget_wrap_ctx(o->under_object, o->under_vol_id, &new_wrap_ctx->under_wrap_ctx);

    /* Set wrap context to return */
    *wrap_ctx = new_wrap_ctx;
    printf("%s:%d: o->my_type = %d\n", __func__, __LINE__, o->my_type);
    return 0;
} /* end H5VL_provenance_get_wrap_ctx() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_wrap_object
 *
 * Purpose:     Use a "wrapper context" to wrap a data object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */

//This function makes up a fake upper layer obj used as a parameter in _obj_wrap_under(..., H5VL_provenance_t* upper_o,... ),
//Use this in H5VL_provenance_wrap_object() ONLY!!!
H5VL_provenance_t* _fake_obj_new(file_prov_info_t* root_file, hid_t under_vol_id){
    H5VL_provenance_t* obj = calloc(1, sizeof(H5VL_provenance_t));

    H5I_type_t fake_upper_o_type = H5I_FILE;  // FILE should work fine as a parent obj for all.
    obj->under_vol_id = under_vol_id;
    obj->my_type = fake_upper_o_type;
    obj->prov_helper = PROV_HELPER;
    switch(fake_upper_o_type){
        case H5I_DATASET: {
            dataset_prov_info_t* fake_info = calloc(1, sizeof(dataset_prov_info_t));
            fake_info->root_file_info = root_file;
            obj->generic_prov_info = (void*)fake_info;
            break;
        }

        case H5I_GROUP: {
            group_prov_info_t* fake_info = calloc(1, sizeof(group_prov_info_t));
            fake_info->root_file_info = root_file;
            obj->generic_prov_info = (void*)fake_info;
            break;
        }

        case H5I_FILE: {
            obj->generic_prov_info = (void*)root_file;
            break;
        }

        default:
            printf("%s:%d: Unexpected type for fake_obj! fake_upper_o_type = %d\n", fake_upper_o_type, __func__, __LINE__);
            break;
    }

    return obj;
}

void _fake_obj_free(H5VL_provenance_t* obj){
    assert(obj);
    if(obj->my_type != H5I_FILE )
        free(obj->generic_prov_info);
    free(obj);
}

static void *
H5VL_provenance_wrap_object(void *under_under_in, H5I_type_t obj_type, void *_wrap_ctx_in)
{
    /* Generic object wrapping, make ctx based on types */
    H5VL_provenance_wrap_ctx_t *wrap_ctx = (H5VL_provenance_wrap_ctx_t *)_wrap_ctx_in;
    //H5VL_provenance_t *o = H5VL_provenance_new_obj(under_under, wrap_ctx->under_vol_id, PROV_HELPER);
    printf("%s:%d\n", __func__, __LINE__);
    void *under;
    H5VL_provenance_t* new_obj;
    unsigned long file_no = 0;
    //get_native_file_no(&file_no, );

//    H5VL_provenance_t *o = (H5VL_provenance_t*)obj_upper;
//    prov_helper* ph = o->prov_helper;// wrong casting, still null.
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL WRAP Object\n");
#endif

    //printf("%s:%d: obj = %p, ph = %p\n", __func__, __LINE__, obj_upper, PROV_HELPER);
    /* Wrap the object with the underlying VOL */
    //What's wrong: obj_type is a random number, trap_ctx is a bad pointer

    under = H5VLwrap_object(under_under_in, obj_type, wrap_ctx->under_vol_id, wrap_ctx->under_wrap_ctx);
    printf("%s:%d, obj_type = %d, under = %p\n", __func__, __LINE__, obj_type, under);
    if(!under){
        printf("%s:%d, Failed: H5VLwrap_object() == NULL. obj_type = %d\n", __func__, __LINE__, obj_type);
    }
    H5VL_provenance_t* fake_upper_o = _fake_obj_new(wrap_ctx->root_file_info, wrap_ctx->under_vol_id);
    if(under){
        new_obj = _obj_wrap_under(under, fake_upper_o, NULL, obj_type, H5P_DEFAULT, NULL);

        printf("%s:%d: _obj_wrap_under(obj_type): obj_type = %d \n", __func__, __LINE__, obj_type);
    }
    else
        new_obj = NULL;

    //H5VL_provenance_free_obj(o);
    _fake_obj_free(fake_upper_o);
    return (void*)new_obj;
} /* end H5VL_provenance_wrap_object() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_provenance_free_wrap_ctx
 *
 * Purpose:     Release a "wrapper context" for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_free_wrap_ctx(void *_wrap_ctx)
{
    H5VL_provenance_wrap_ctx_t *wrap_ctx = (H5VL_provenance_wrap_ctx_t *)_wrap_ctx;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL WRAP CTX Free\n");
#endif

    /* Release underlying VOL ID and wrap context */
    if(wrap_ctx->under_wrap_ctx)
        H5VLfree_wrap_ctx(wrap_ctx->under_wrap_ctx, wrap_ctx->under_vol_id);
    H5Idec_ref(wrap_ctx->under_vol_id);

    /* Free pass through wrap context object itself */
    free(wrap_ctx);

    return 0;
} /* end H5VL_provenance_free_wrap_ctx() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_create
 *
 * Purpose:     Creates an attribute on an object.
 *
 * Return:      Success:    Pointer to attribute object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_attr_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *attr;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    void *under;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Create\n");
#endif

    under = H5VLattr_create(o->under_object, loc_params, o->under_vol_id, name, acpl_id, aapl_id, dxpl_id, req);
    if(under) {
        attr = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    } /* end if */
    else
        attr = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return (void*)attr;
} /* end H5VL_provenance_attr_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_open
 *
 * Purpose:     Opens an attribute on an object.
 *
 * Return:      Success:    Pointer to attribute object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_attr_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t aapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *attr;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    void *under;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Open\n");
#endif

    under = H5VLattr_open(o->under_object, loc_params, o->under_vol_id, name, aapl_id, dxpl_id, req);
    if(under) {
        attr = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    } /* end if */
    else
        attr = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return (void *)attr;
} /* end H5VL_provenance_attr_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_read
 *
 * Purpose:     Reads data from attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_attr_read(void *attr, hid_t mem_type_id, void *buf,
    hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)attr;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Read\n");
#endif

    ret_value = H5VLattr_read(o->under_object, o->under_vol_id, mem_type_id, buf, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_attr_read() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_write
 *
 * Purpose:     Writes data to attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_attr_write(void *attr, hid_t mem_type_id, const void *buf,
    hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)attr;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Write\n");
#endif

    ret_value = H5VLattr_write(o->under_object, o->under_vol_id, mem_type_id, buf, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_attr_write() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_get
 *
 * Purpose:     Gets information about an attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_attr_get(void *obj, H5VL_attr_get_t get_type, hid_t dxpl_id,
    void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Get\n");
#endif

    ret_value = H5VLattr_get(o->under_object, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_attr_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_specific
 *
 * Purpose:     Specific operation on attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_attr_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Specific\n");
#endif
    printf("%s:%d\n", __func__, __LINE__);
    ret_value = H5VLattr_specific(o->under_object, loc_params, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_attr_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_optional
 *
 * Purpose:     Perform a connector-specific operation on an attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_attr_optional(void *obj, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Optional\n");
#endif

    ret_value = H5VLattr_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_attr_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_attr_close
 *
 * Purpose:     Closes an attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1, attr not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)attr;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL ATTRIBUTE Close\n");
#endif
    printf("%s:%d\n", __func__, __LINE__);
    ret_value = H5VLattr_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying attribute was closed */
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    if(ret_value >= 0)
        H5VL_provenance_free_obj(o);


    return ret_value;
} /* end H5VL_provenance_attr_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_create
 *
 * Purpose:     Creates a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_dataset_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *ds_name, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *dset;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    file_prov_info_t* root_file_info;
    void *under;

    printf("%s:%d: ds_name = [%s]\n", __func__, __LINE__, ds_name);

    if(!ds_name){
        printf("%s:%d: o = [%p], my_type = %d\n", __func__, __LINE__, o, o->my_type);
    }

    if(o->my_type == H5I_FILE){
        printf("%s:%d: obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, o->my_type);
        root_file_info = (file_prov_info_t*)o->generic_prov_info;
        printf("%s:%d: root_file_info = %p, file_name = %s\n", __func__, __LINE__, root_file_info, root_file_info->file_name);
    }else if(o->my_type == H5I_GROUP){
        printf("%s:%d: obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, o->my_type);
        root_file_info = ((group_prov_info_t*)o->generic_prov_info)->root_file_info;
        printf("%s:%d: root_file_info = %p, file_name = %s\n", __func__, __LINE__, root_file_info, root_file_info->file_name);
    }else{//error
        printf("%s:%d: obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, o->my_type);
        return NULL;
    }

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Create\n");
#endif

    under = H5VLdataset_create(o->under_object, loc_params, o->under_vol_id, ds_name, dcpl_id,  dapl_id, dxpl_id, req);
    //if(!under)
        //printf("%s:%d: under is null.\n", __func__, __LINE__);

    //printf("%s:%d\n", __func__, __LINE__);

    if(under) {
        dset = _obj_wrap_under(under, o, ds_name, H5I_DATASET, dxpl_id, req);
//        dset = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);
//        //printf("%s:%d\n", __func__, __LINE__);
//        /* Check for async request */
//        if(req && *req)
//            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
//        printf("%s:%d\n", __func__, __LINE__);
//
//
//        H5VL_loc_params_t p = _new_loc_pram(H5I_DATASET);
//        H5O_info_t oinfo;
//        get_native_info(under, o->under_vol_id,
//                dxpl_id, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);
//        haddr_t native_addr = oinfo.addr;
//        //haddr_t native_addr = -1;
//        //printf("%s:%d\n", __func__, __LINE__);
//        dset->generic_prov_info = add_dataset_node(dset, native_addr, root_file_info, ds_name, dxpl_id, req);
//        dset->my_type = H5I_DATASET;
//        //new_ds_prov_info(dset, native_addr, file_info, ds_name, dapl_id, dxpl_id, req);
//
//        file_ds_created(root_file_info);
    } /* end if */
    else
        dset = NULL;
    //printf("================================%s:%d: dset = %p\n", __func__, __LINE__, dset);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

    //printf("%s:%d: Return dset = %p\n", __func__, __LINE__, dset);
    return (void *)dset;
} /* end H5VL_provenance_dataset_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_open
 *
 * Purpose:     Opens a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
//use ctx to store shared info, including prov_helper.
//_prov_dataset_ctx_setup(void *new_under, void* obj_upper){
//    H5VL_provenance_t *dset;
//    H5VL_provenance_t *o = (H5VL_provenance_t *)obj_upper;
//    hid_t dcpl_id = -1;
//    hid_t dt_id = -1;
//    hid_t ds_id = -1;
//    hid_t dxpl_id = H5P_DEFAULT;
//
//    dset = H5VL_provenance_new_obj(new_under, o->under_vol_id, o->prov_helper);
//
//    dset->generic_prov_info = new_dataset_info();
////    printf("\n%s:%d:o->shared_file_info = %p, dset = %p, o->under_vol_id = %llx, o->prov_helper = %p\n",
////            __func__, __LINE__, o->shared_file_info, dset, o->under_vol_id, o->prov_helper);
////    dset->shared_file_info = o->shared_file_info;
//    //file_ds_accessed(o->shared_file_info);
//
//    /* Check for async request */
//    void* req = NULL;
////    if (req && *req)
////        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
//
//    dataset_get_wrapper(dset->under_object, dset->under_vol_id, H5VL_DATASET_GET_DCPL, dxpl_id, req, &dcpl_id);
//    dataset_get_wrapper(dset->under_object, dset->under_vol_id, H5VL_DATASET_GET_TYPE, dxpl_id, req, &dt_id);
//    dataset_get_wrapper(dset->under_object, dset->under_vol_id, H5VL_DATASET_GET_SPACE, dxpl_id, req, &ds_id);
//
//    //NOTE: prov_info here is a shared pointer, the structure is generated at top level, higher than file level
////    ((dataset_prov_info_t*) dset->prov_info)->dset_name = strdup(name);
////    ((dataset_prov_info_t*) dset->prov_info)->dt_class = H5Tget_class(dt_id);
////    ((dataset_prov_info_t*) dset->prov_info)->dset_type_size = H5Tget_size(dt_id);
////    ((dataset_prov_info_t*) dset->prov_info)->layout = H5Pget_layout(dcpl_id);
////    ((dataset_prov_info_t*) dset->prov_info)->ds_class = H5Sget_simple_extent_type(ds_id);
//
//    // This part  belongs to a per-dataset structure, such as dataset_info_t, not to prov_helper
//    if (((dataset_prov_info_t*) dset->generic_prov_info)->ds_class == H5S_SIMPLE) {
//        ((dataset_prov_info_t*) dset->generic_prov_info)->dimension_cnt = H5Sget_simple_extent_ndims(ds_id);
//        H5Sget_simple_extent_dims(ds_id, ((dataset_prov_info_t*) dset->generic_prov_info)->dimensions, NULL);
//        ((dataset_prov_info_t*) dset->generic_prov_info)->dset_space_size = H5Sget_simple_extent_npoints(ds_id);
//    }
//
//
//}

/* under: obj need to be wrapped
 * upper_o: holder or upper layer object. Mostly used to pass root_file_info, vol_id, etc,.
 *      - it's a fake obj if called by H5VL_provenance_wrap_object().
 * target_obj_type:
 *      - for H5VL_provenance_wrap_object(obj_type): the obj should be wrapped into this type
 *      - for H5VL_provenance_object_open(): it's the obj need to be opened as this type
 *
 */
H5VL_provenance_t* _obj_wrap_under(void* under, H5VL_provenance_t* upper_o,
        const char *target_obj_name, H5I_type_t target_obj_type, hid_t dxpl_id, void** req){
    H5VL_provenance_t *obj;
    file_prov_info_t* root_file_info = NULL;
    printf("%s:%d: target_obj_type = %d, upper_o->my_type = %d, target_obj_name = %p\n", __func__, __LINE__, target_obj_type, upper_o->my_type, target_obj_name);

    H5I_type_t upper_o_type = upper_o->my_type;
    if (under) {

        if (target_obj_name != NULL && 0 == strcmp(target_obj_name, ".")) {   //open same dataset.  // "."  case
            //open from types
            switch(upper_o_type){
                case H5I_DATASET: {
                    root_file_info = ((dataset_prov_info_t*) (upper_o->generic_prov_info))->root_file_info;
                    break;
                }
                case H5I_GROUP: {
                    root_file_info = ((group_prov_info_t*)(upper_o->generic_prov_info))->root_file_info;
                    break;
                }
                default:
                    break;
            }
        } else {// non-. cases, parent obj's type: where it's opened, file or group.
            if(upper_o_type == H5I_FILE){//
                 printf("%s:%d: obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, upper_o_type);
                 root_file_info = (file_prov_info_t*)upper_o->generic_prov_info;
                 printf("%s:%d: root_file_info = %p, file_name = %s\n", __func__, __LINE__, root_file_info, root_file_info->file_name);
             }else if(upper_o_type == H5I_GROUP){
                 printf("%s:%d: obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, upper_o_type);
                 root_file_info = ((group_prov_info_t*)upper_o->generic_prov_info)->root_file_info;
                 printf("%s:%d: root_file_info = %p, file_name = %s\n", __func__, __LINE__, root_file_info, root_file_info->file_name);
             }else{//error
                 printf("%s:%d: ERROR: obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, upper_o_type);
                 return NULL;
             }
        }

        printf("%s:%d: target_obj_name = %s\n", __func__, __LINE__, target_obj_name);

        obj = H5VL_provenance_new_obj(under, upper_o->under_vol_id, upper_o->prov_helper);

        printf("%s:%d: target_obj_name = %s\n", __func__, __LINE__, target_obj_name);
        /* Check for async request */
        if (req && *req)
            *req = H5VL_provenance_new_obj(*req, upper_o->under_vol_id, upper_o->prov_helper);
        printf("%s:%d: target_obj_name = %s\n", __func__, __LINE__, target_obj_name);
        //obj types
        H5VL_loc_params_t p = _new_loc_pram(target_obj_type);
        H5O_info_t oinfo;
        get_native_info(under, upper_o->under_vol_id,
                dxpl_id, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);
        haddr_t native_addr = oinfo.addr;
        unsigned long file_no = oinfo.fileno;

        printf("%s:%d: oinfo.native_addr = %d, file_no = %d\n", __func__, __LINE__, oinfo.addr, oinfo.fileno);

        switch (target_obj_type) {
        case H5I_DATASET: {
            obj->generic_prov_info = add_dataset_node(file_no, obj, native_addr, root_file_info, target_obj_name, dxpl_id, req);

            obj->my_type = H5I_DATASET;

            file_ds_accessed(root_file_info);
            break;
        }
        case H5I_GROUP: {
            obj->generic_prov_info = add_grp_node(root_file_info, native_addr);
            obj->my_type = H5I_GROUP;
            printf("%s:%d: ========== obj->generic_prov_info = %p, grp_info->native_addr = %d \n", __func__, __LINE__,
                    obj->generic_prov_info, ((group_prov_info_t*) (obj->generic_prov_info))->native_addr);
            break;
        }
        case H5I_FILE: {//newly added. if target_obj_name == NULL: it's a fake upper_o
            obj->generic_prov_info = add_file_node(PROV_HELPER, target_obj_name, file_no);
            break;
        }
        default:
            printf("%s:%d: Unexpected target_obj_type = %d\n", __func__, __LINE__, target_obj_type);
            break;
        }


    } /* end if */
    else
        obj = NULL;
    return obj;
}

static void *
H5VL_provenance_dataset_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *ds_name, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    void *under;
    H5VL_provenance_t *dset;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    file_prov_info_t* root_file_info = NULL;


    printf("%s:%d, obj = %p, o->generic_prov_info = %p\n", __func__, __LINE__, o, o->generic_prov_info);


#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Open\n");
#endif

    hid_t dcpl_id = -1;
    hid_t dt_id = -1;
    hid_t ds_id = -1;
    printf("%s:%d\n", __func__, __LINE__);
    //printf("\n%s:%d:o->shared_file_info = %p, dset = %p, o->under_vol_id = %llx, o->prov_helper = %p\n", __func__, __LINE__, o->shared_file_info, dset, o->under_vol_id, o->prov_helper);
    under = H5VLdataset_open(o->under_object, loc_params, o->under_vol_id, ds_name, dapl_id, dxpl_id, req);
    printf("%s:%d: under = %p\n", __func__, __LINE__, under);
    //assert(under);



    //TODO: opening ds with different names (from groups)

//    _ds_obj_open() :---
   dset = _obj_wrap_under(under, o, ds_name, H5I_DATASET, dxpl_id, req);

    printf("%s:%d\n", __func__, __LINE__);
    if(under){
        if(dset)
            prov_write(dset->prov_helper, __func__, get_time_usec() - start);
    }
    return (void *)dset;


} /* end H5VL_provenance_dataset_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_read
 *
 * Purpose:     Reads data elements from a dataset into a buffer.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, hid_t plist_id, void *buf, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Read\n");
#endif

    ret_value = H5VLdataset_read(o->under_object, o->under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    hid_t dset_type;
    hid_t dset_space;
    hsize_t r_size = 0;
    unsigned long time = get_time_usec() - start;
    if(H5S_ALL == mem_space_id){
        r_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * ((dataset_prov_info_t*)o->generic_prov_info)->dset_space_size;;
    }else{
        r_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * H5Sget_select_npoints(mem_space_id);
    }

    ((dataset_prov_info_t*)(o->generic_prov_info))->total_bytes_read += r_size;
    ((dataset_prov_info_t*)(o->generic_prov_info))->dataset_read_cnt++;
    ((dataset_prov_info_t*)(o->generic_prov_info))->total_read_time += time;

    printf("read size = %llu\n", r_size);

    if(o)
        prov_write(o->prov_helper, __func__, time);
    return ret_value;
} /* end H5VL_provenance_dataset_read() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_write
 *
 * Purpose:     Writes data elements from a buffer into a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, hid_t plist_id, const void *buf, void **req)
{
    assert(dset);
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Write\n");
#endif

    ret_value = H5VLdataset_write(o->under_object, o->under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    hid_t dset_type;
    hid_t dset_space;
    hsize_t w_size = 0;

    if(H5S_ALL == mem_space_id){
        w_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * ((dataset_prov_info_t*)o->generic_prov_info)->dset_space_size;;
    }else{
        w_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * H5Sget_select_npoints(mem_space_id);
    }
    unsigned long time = get_time_usec() - start;
    prov_write(o->prov_helper, __func__, time);
    ((dataset_prov_info_t*)o->generic_prov_info)->total_bytes_written += w_size;
    ((dataset_prov_info_t*)(o->generic_prov_info))->dataset_write_cnt++;
    ((dataset_prov_info_t*)(o->generic_prov_info))->total_write_time += time;
    printf("write size = %llu\n", w_size);

    return ret_value;
} /* end H5VL_provenance_dataset_write() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_get
 *
 * Purpose:     Gets information about a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_dataset_get(void *dset, H5VL_dataset_get_t get_type,
    hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Get\n");
#endif

    ret_value = H5VLdataset_get(o->under_object, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_dataset_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_specific
 *
 * Purpose:     Specific operation on a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_dataset_specific(void *obj, H5VL_dataset_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL H5Dspecific\n");
#endif

    ret_value = H5VLdataset_specific(o->under_object, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    if(specific_type == H5VL_DATASET_SET_EXTENT){//user changes the dimensions
        //TODO: update dimensions data for stats
    }

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_dataset_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_optional
 *
 * Purpose:     Perform a connector-specific operation on a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_dataset_optional(void *obj, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Optional\n");
#endif

    ret_value = H5VLdataset_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_dataset_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_dataset_close
 *
 * Purpose:     Closes a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1, dataset not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)dset;
    printf("%s:%d: dset obj = %p, obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, dset, o->my_type);
    printf("%s:%d: obj->generic_prov_info = %p\n", __func__, __LINE__, o->generic_prov_info);
    herr_t ret_value;
    dataset_prov_info_t* dset_info = (dataset_prov_info_t*)o->generic_prov_info;

    file_prov_info_t* file_info = dset_info->root_file_info;
    char* ds_name = dset_info->dset_name;
    printf("%s:%d: dset_info = %p, ds_name = [%s]\n",  __func__, __LINE__, dset_info, ds_name);
//    printf("%s:%d: file_info = %p, dset_info = %p,\n"
//            " file_info->opened_datasets = %p, \n"
//            " file_info->opened_datasets_cnt = %d,\n"
//            " dset_info->ref_cnt = %d,\n"
//            " \n,"
//            " dset_info->file_name = %p\n",
//            __func__, __LINE__, file_info, dset_info,
//            file_info->opened_datasets,
//            file_info->opened_datasets_cnt,
//            dset_info->ref_cnt,
//
//            dset_info->file_name);
//    printf("%s:%d: file_info->file_name = [%s], dset_info->file_name = %s,  dset_info->ref_cnt =%d\n",
//            __func__, __LINE__,
//            file_info->file_name,
//            dset_info->file_name,
//
//            dset_info->ref_cnt);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Close\n");
#endif


    ret_value = H5VLdataset_close(o->under_object, o->under_vol_id, dxpl_id, req);
    printf("%s:%d: file_info = %p, dset_info->root_file_info = %p, ret = %d\n",
            __func__, __LINE__, file_info, dset_info->root_file_info, ret_value);
    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying dataset was closed */
    //printf("%s:%d: file_info = %p, dset_info->root_file_info = %p\n", __func__, __LINE__, file_info, dset_info->root_file_info);
    if(ret_value >= 0){
        if(o){
            //printf("%s:%d: file_info = %p, dset_info->root_file_info = %p\n", __func__, __LINE__, file_info, dset_info->root_file_info);

            dataset_stats_prov_write(dset_info);//output stats
            //printf("%s:%d: file_info = %p, dset_info->root_file_info = %p\n", __func__, __LINE__, file_info, dset_info->root_file_info);

            prov_write(o->prov_helper, __func__, get_time_usec() - start);
            printf("%s:%d: file_info = %p, dset_info = %p, dset_info->ds_name = %s\n", __func__, __LINE__, file_info, dset_info, dset_info->dset_name);
            int ref_cnt = rm_dataset_node(file_info, dset_info->native_addr);
            printf("%s:%d: After rm_dataset_node: ref_cnt = %d\n",  __func__, __LINE__, ref_cnt);
            H5VL_provenance_free_obj(o);
        }


        //printf("\n%s:%d: check: o->shared_file_info = %p\n", __func__, __LINE__, o->shared_file_info);
    }
    //printf("\n%s:%d: after: o->shared_file_info = %p\n", __func__, __LINE__, o->shared_file_info);
    return ret_value;
} /* end H5VL_provenance_dataset_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_datatype_commit
 *
 * Purpose:     Commits a datatype inside a container.
 *
 * Return:      Success:    Pointer to datatype object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id,
    hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *dt;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Commit\n");
#endif

    under = H5VLdatatype_commit(o->under_object, loc_params, o->under_vol_id, name, type_id, lcpl_id, tcpl_id, tapl_id, dxpl_id, req);
    if(under) {
        dt = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);
        dt->generic_prov_info = new_datatype_info();

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    } /* end if */
    else
        dt = NULL;

    if(dt)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return (void *)dt;
} /* end H5VL_provenance_datatype_commit() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_datatype_open
 *
 * Purpose:     Opens a named datatype inside a container.
 *
 * Return:      Success:    Pointer to datatype object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_datatype_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t tapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *dt;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Open\n");
#endif

    under = H5VLdatatype_open(o->under_object, loc_params, o->under_vol_id, name, tapl_id, dxpl_id, req);
    if(under) {
        dt = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

        dt->generic_prov_info = new_datatype_info();
    } /* end if */
    else
        dt = NULL;

    if(dt)
        prov_write(dt->prov_helper, __func__, get_time_usec() - start);
    return (void *)dt;
} /* end H5VL_provenance_datatype_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_datatype_get
 *
 * Purpose:     Get information about a datatype
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_datatype_get(void *dt, H5VL_datatype_get_t get_type,
    hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)dt;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Get\n");
#endif

    ret_value = H5VLdatatype_get(o->under_object, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_datatype_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_datatype_specific
 *
 * Purpose:     Specific operations for datatypes
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_datatype_specific(void *obj, H5VL_datatype_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Specific\n");
#endif

    ret_value = H5VLdatatype_specific(o->under_object, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_datatype_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_datatype_optional
 *
 * Purpose:     Perform a connector-specific operation on a datatype
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_datatype_optional(void *obj, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Optional\n");
#endif

    ret_value = H5VLdatatype_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_datatype_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_datatype_close
 *
 * Purpose:     Closes a datatype.
 *
 * Return:      Success:    0
 *              Failure:    -1, datatype not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_datatype_close(void *dt, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)dt;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Close\n");
#endif

    assert(o->under_object);
    printf("%s:%d\n", __func__, __LINE__);
    ret_value = H5VLdatatype_close(o->under_object, o->under_vol_id, dxpl_id, req);
    printf("%s:%d\n", __func__, __LINE__);
    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    printf("%s:%d\n", __func__, __LINE__);
    /* Release our wrapper, if underlying datatype was closed */
    printf("%s:%d\n", __func__, __LINE__);
    if(o){
        printf("%s:%d\n", __func__, __LINE__);
        datatype_stats_prov_write(o->generic_prov_info);
        printf("%s:%d\n", __func__, __LINE__);
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    }
    printf("%s:%d\n", __func__, __LINE__);
    if(ret_value >= 0)
        H5VL_provenance_free_obj(o);

    printf("%s:%d\n", __func__, __LINE__);
    return ret_value;
} /* end H5VL_provenance_datatype_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_create
 *
 * Purpose:     Creates a container using this connector
 *
 * Return:      Success:    Pointer to a file object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_file_create(const char *name, unsigned flags, hid_t fcpl_id,
    hid_t fapl_id, hid_t dxpl_id, void **req)
{

    H5VL_provenance_info_t *info;
    H5VL_provenance_t *file;
    hid_t under_fapl_id;
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PROVNC VOL FILE Create\n");
#endif


    printf("%s:%d\n", __func__, __LINE__);
    /* Get copy of our VOL info from FAPL */

    H5Pget_vol_info(fapl_id, (void **)&info);

    printf("Verifying info content: prov_file_path = [%s], prov_level = [%d], format = [%s]\n", info->prov_file_path, info->prov_level, info->prov_line_format);

    /* Copy the FAPL */
    under_fapl_id = H5Pcopy(fapl_id);
//H5Pget_driver(fapl_id) == src/H5FDmpio.h:H5FD_MPIO
//d_info = H5Pget_driver_info(fapl_id);
//d_info is a void* points to a H5FD_MPIO, which contains a MPI_COMM, and can be used later for parallel info collection.
//put the mpi_comm in file_shared


    /* Set the VOL ID and info for the underlying FAPL */
    H5Pset_vol(under_fapl_id, info->under_vol_id, info->under_vol_info);

    unsigned long start = get_time_usec();
    printf("%s:%d\n", __func__, __LINE__);
    /* Open the file with the underlying VOL connector */
    under = H5VLfile_create(name, flags, fcpl_id, under_fapl_id, dxpl_id, req);
    printf("%s:%d:under = %p\n", __func__, __LINE__, under);
    if(!PROV_HELPER)
        PROV_HELPER = prov_helper_init(info->prov_file_path, info->prov_level, info->prov_line_format);
    unsigned long file_no = 0;
    if(under) {

        printf("%s:%d\n", __func__, __LINE__);
        file = H5VL_provenance_new_obj(under, info->under_vol_id, PROV_HELPER);

        printf("%s:%d, before add new node: PROV_HELPER->opened_files = %p, PROV_HELPER->opened_files_cnt = %d\n",
                __func__, __LINE__, PROV_HELPER->opened_files, PROV_HELPER->opened_files_cnt);
        file->my_type = H5I_FILE;
        get_native_file_no(&file_no, file);
        file->generic_prov_info = add_file_node(PROV_HELPER, name, file_no);
        printf("%s:%d, file->generic_prov_info = %p, file_no = %d.\n", __func__, __LINE__, file->generic_prov_info, file_no);
        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, info->under_vol_id, PROV_HELPER);
    } /* end if */
    else
        file = NULL;
    if(file)
        prov_write(file->prov_helper, __func__, get_time_usec() - start);
    /* Close underlying FAPL */
    printf("%s:%d\n", __func__, __LINE__);
    H5Pclose(under_fapl_id);
    printf("%s:%d\n", __func__, __LINE__);
    /* Release copy of our VOL info */
    H5VL_provenance_info_free(info);
    printf("%s:%d\n", __func__, __LINE__);
    return (void *)file;
} /* end H5VL_provenance_file_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_open
 *
 * Purpose:     Opens a container created with this connector
 *
 * Return:      Success:    Pointer to a file object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */

void* _file_open_common(void* under, hid_t vol_id, char* name){
    H5VL_provenance_t *file;
    file = H5VL_provenance_new_obj(under, vol_id, PROV_HELPER);
    unsigned long file_no = 0;
    file->my_type = H5I_FILE;
    get_native_file_no(&file_no, file);

    file->generic_prov_info = add_file_node(PROV_HELPER, name, file_no);

    return (void*)file;
}
static void *
H5VL_provenance_file_open(const char *name, unsigned flags, hid_t fapl_id,
    hid_t dxpl_id, void **req)
{
    printf("%s:%d\n", __func__, __LINE__);
    H5VL_provenance_info_t *info;
    H5VL_provenance_t *file;
    hid_t under_fapl_id;
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL FILE Open\n");
#endif

    /* Get copy of our VOL info from FAPL */
    H5Pget_vol_info(fapl_id, (void **)&info);

    /* Copy the FAPL */
    under_fapl_id = H5Pcopy(fapl_id);

    /* Set the VOL ID and info for the underlying FAPL */
    H5Pset_vol(under_fapl_id, info->under_vol_id, info->under_vol_info);

    unsigned long start = get_time_usec();
    /* Open the file with the underlying VOL connector */
    under = H5VLfile_open(name, flags, under_fapl_id, dxpl_id, req);
    //printf("%s:%d\n", __func__, __LINE__);
    if(!PROV_HELPER)
        PROV_HELPER = prov_helper_init(info->prov_file_path, info->prov_level, info->prov_line_format);
    //setup global
    //printf("%s:%d\n", __func__, __LINE__);
    if(under) {
        unsigned long file_no = 0;

        //get_native_file_no(&file_no, file, dxpl_id);

        file = _file_open_common(under, info->under_vol_id, name);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, info->under_vol_id, file->prov_helper);
    } /* end if */
    else
        file = NULL;
    if(file)
        prov_write(file->prov_helper, __func__, get_time_usec() - start);
    /* Close underlying FAPL */
    H5Pclose(under_fapl_id);

    /* Release copy of our VOL info */
    H5VL_provenance_info_free(info);

    return (void *)file;
} /* end H5VL_provenance_file_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_get
 *
 * Purpose:     Get info about a file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_file_get(void *file, H5VL_file_get_t get_type, hid_t dxpl_id,
    void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)file;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL FILE Get\n");
#endif

    ret_value = H5VLfile_get(o->under_object, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_file_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_specific_reissue
 *
 * Purpose:     Re-wrap vararg arguments into a va_list and reissue the
 *              file specific callback to the underlying VOL connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_file_specific_reissue(void *obj, hid_t connector_id,
    H5VL_file_specific_t specific_type, hid_t dxpl_id, void **req, ...)
{
    va_list arguments;
    herr_t ret_value;
    va_start(arguments, req);
    ret_value = H5VLfile_specific(obj, connector_id, specific_type, dxpl_id, req, arguments);
    va_end(arguments);

    return ret_value;
} /* end H5VL_provenance_file_specific_reissue() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_specific
 *
 * Purpose:     Specific operation on file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_file_specific(void *file, H5VL_file_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)file;
    hid_t under_vol_id = -1;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL FILE Specific\n");
#endif

    /* Unpack arguments to get at the child file pointer when mounting a file */
    if(specific_type == H5VL_FILE_MOUNT) {
        H5I_type_t loc_type;
        const char *name;
        H5VL_provenance_t *child_file;
        hid_t plist_id;

        /* Retrieve parameters for 'mount' operation, so we can unwrap the child file */
        loc_type = (H5I_type_t)va_arg(arguments, int); /* enum work-around */
        name = va_arg(arguments, const char *);
        child_file = (H5VL_provenance_t *)va_arg(arguments, void *);
        plist_id = va_arg(arguments, hid_t);

        /* Keep the correct underlying VOL ID for possible async request token */
        under_vol_id = o->under_vol_id;

        /* Re-issue 'file specific' call, using the unwrapped pieces */
        ret_value = H5VL_provenance_file_specific_reissue(o->under_object, o->under_vol_id, specific_type, dxpl_id, req, (int)loc_type, name, child_file->under_object, plist_id);
    } /* end if */
    else if(specific_type == H5VL_FILE_IS_ACCESSIBLE) {
        H5VL_provenance_info_t *info;
        hid_t fapl_id, under_fapl_id;
        const char *name;
        htri_t *ret;

        /* Get the arguments for the 'is accessible' check */
        fapl_id = va_arg(arguments, hid_t);
        name    = va_arg(arguments, const char *);
        ret     = va_arg(arguments, htri_t *);

        /* Get copy of our VOL info from FAPL */
        H5Pget_vol_info(fapl_id, (void **)&info);

        /* Copy the FAPL */
        under_fapl_id = H5Pcopy(fapl_id);

        /* Set the VOL ID and info for the underlying FAPL */
        H5Pset_vol(under_fapl_id, info->under_vol_id, info->under_vol_info);

        /* Keep the correct underlying VOL ID for possible async request token */
        under_vol_id = info->under_vol_id;

        /* Re-issue 'file specific' call */
        ret_value = H5VL_provenance_file_specific_reissue(NULL, info->under_vol_id, specific_type, dxpl_id, req, under_fapl_id, name, ret);

        /* Close underlying FAPL */
        H5Pclose(under_fapl_id);

        /* Release copy of our VOL info */
        H5VL_provenance_info_free(info);
    } /* end else-if */
    else {
        va_list my_arguments;

        /* Make a copy of the argument list for later, if reopening */
        if(specific_type == H5VL_FILE_REOPEN)
            va_copy(my_arguments, arguments);

        /* Keep the correct underlying VOL ID for possible async request token */
        under_vol_id = o->under_vol_id;

        ret_value = H5VLfile_specific(o->under_object, o->under_vol_id, specific_type, dxpl_id, req, arguments);

        /* Wrap file struct pointer, if we reopened one */
        if(specific_type == H5VL_FILE_REOPEN) {
            if(ret_value >= 0) {
                void      **ret = va_arg(my_arguments, void **);

                if(ret && *ret){
                    char* file_name = ((file_prov_info_t*)(o->generic_prov_info))->file_name;
                    printf("%s:%d, file_name = [%s]\n", __func__, __LINE__, file_name);
                    //unsigned long file_no = 0;
                    //get_native_file_no(&file_no, *ret, dxpl_id);
                    *ret = _file_open_common(*ret, under_vol_id, file_name);
//                    H5VL_provenance_t *file;
//                    file = H5VL_provenance_new_obj(*ret, o->under_vol_id, o->prov_helper);
//                    char* name = ((file_prov_info_t*)(o->generic_prov_info))->file_name;
//                    file->generic_prov_info = add_file_node(PROV_HELPER, name);
//                    file->my_type = H5I_FILE;

                }

            } /* end if */

            /* Finish use of copied vararg list */
            va_end(my_arguments);
        } /* end if */
    } /* end else */

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_file_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_optional
 *
 * Purpose:     Perform a connector-specific operation on a file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_file_optional(void *file, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)file;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL File Optional\n");
#endif

    ret_value = H5VLfile_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_file_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_file_close
 *
 * Purpose:     Closes a file.
 *
 * Return:      Success:    0
 *              Failure:    -1, file not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_file_close(void *file, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)file;
    herr_t ret_value;
    printf("%s:%d, o = %p, o->generic_prov_info = %p, type = %d\n", __func__, __LINE__, o, o->generic_prov_info, o->my_type);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL FILE Close\n");
#endif
    //printf("%s:%d\n", __func__, __LINE__);

    if(o){
        file_stats_prov_write((file_prov_info_t*)(o->generic_prov_info));

        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    }

    //printf("%s:%d\n", __func__, __LINE__);
    ret_value = H5VLfile_close(o->under_object, o->under_vol_id, dxpl_id, req);
    printf("%s:%d: ret_value = %d\n", __func__, __LINE__, ret_value);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    //printf("%s:%d\n", __func__, __LINE__);
    /* Release our wrapper, if underlying file was closed */

    printf("%s:%d, o = %p, o->generic_prov_info = %p\n", __func__, __LINE__, o, o->generic_prov_info);
//    printf("%s:%d, fname = %p\n", __func__, __LINE__, fname);
    if(ret_value >= 0){
        if(!o->generic_prov_info){//from fake upper_o, no need to remove linkedlist
            printf("%s:%d: generic_prov_info is NULL: created from a fake upper_o. \n", __func__, __LINE__);
        }else{
            char* fname = ((file_prov_info_t*)(o->generic_prov_info))->file_name;
            printf("%s:%d, close file: fname = %s\n", __func__, __LINE__, fname);
            if(o->generic_prov_info)
                rm_file_node(PROV_HELPER, fname);

            H5VL_provenance_free_obj(o);

        }


        //((file_prov_info_t*)(o->generic_prov_info))->file_name
        //prov_helper_teardown(o->prov_helper);

        //file_info_free(o->generic_prov_info); // this is freed in rm_file_node().
        //printf("%s:%d, helper->opened_files = %p\n", __func__, __LINE__, PROV_HELPER->opened_files);




        //printf("%s:%d, helper->opened_files = %p\n", __func__, __LINE__, PROV_HELPER->opened_files);
    }
    //printf("%s:%d\n", __func__, __LINE__);
    //printf("%s:%d, file_close completed, helper->opened_files_cnt = %d\n", __func__, __LINE__, PROV_HELPER->opened_files_cnt);
    return ret_value;
} /* end H5VL_provenance_file_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_group_create
 *
 * Purpose:     Creates a group inside a container
 *
 * Return:      Success:    Pointer to a group object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_group_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *group;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    file_prov_info_t* root_file_info = NULL;
    printf("%s:%d: group name = %s\n", __func__, __LINE__, name);
    if(o->my_type == H5I_FILE){
        printf("%s:%d: obj type = %d. Creat group in a file. (1 for file, 2 for group)\n", __func__, __LINE__, o->my_type);
        root_file_info = (file_prov_info_t*)o->generic_prov_info;
        printf("%s:%d: root_file_info = %p, file_name = [%p][%s]\n", __func__, __LINE__, root_file_info, root_file_info->file_name, root_file_info->file_name);
    }else if(o->my_type == H5I_GROUP){
        printf("%s:%d: obj type = %d. Creat group in a group. (1 for file, 2 for group)\n", __func__, __LINE__, o->my_type);
        root_file_info = ((group_prov_info_t*)o->generic_prov_info)->root_file_info;
        printf("%s:%d: root_file_info = %p, file_name = [%p][%s]\n", __func__, __LINE__, root_file_info, root_file_info->file_name, root_file_info->file_name);
    }else{//error
        printf("%s:%d: Unexpected obj type = %d. (1 for file, 2 for group)\n", __func__, __LINE__, o->my_type);
        return NULL;
    }
    assert(root_file_info);
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Create\n");
#endif

    under = H5VLgroup_create(o->under_object, loc_params, o->under_vol_id, name, gcpl_id,  gapl_id, dxpl_id, req);
    if(under) {
        group = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);

        H5VL_loc_params_t p = _new_loc_pram(H5I_GROUP);

        H5O_info_t oinfo;
        get_native_info(under, o->under_vol_id,
                dxpl_id, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);
        haddr_t native_addr = oinfo.addr;

        group->generic_prov_info = add_grp_node(root_file_info, native_addr);
        printf("%s:%d: ========== o = %p, grp_info = %p, native_addr = %d\n", __func__, __LINE__, o, group->generic_prov_info, native_addr);
        group->my_type = H5I_GROUP;

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    } /* end if */
    else
        group = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

    printf("%s:%d, (void*)new_grp = %p\n", __func__, __LINE__, group);
    return (void *)group;
} /* end H5VL_provenance_group_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_group_open
 *
 * Purpose:     Opens a group inside a container
 *
 * Return:      Success:    Pointer to a group object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_group_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *group;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    void *under;
    file_prov_info_t* file_info;

    if (o->my_type == H5I_FILE) {
        file_info = (file_prov_info_t*) o->generic_prov_info;

    } else if (o->my_type == H5I_GROUP) {
        file_info = ((group_prov_info_t*) o->generic_prov_info)->root_file_info;
    } else {    //error
        return NULL;
    }
    assert(file_info);

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Open\n");
#endif

    under = H5VLgroup_open(o->under_object, loc_params, o->under_vol_id, name, gapl_id, dxpl_id, req);
    if(under) {
//        group = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);
//
//        H5VL_loc_params_t p = _new_loc_pram(H5I_GROUP);
//        H5O_info_t oinfo;
//        get_native_info(under, o->under_vol_id,
//                dxpl_id, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);
//        haddr_t native_addr = oinfo.addr;
//        group->generic_prov_info = add_grp_node(file_info, native_addr);
//        printf("%s:%d: ========== o = %p, grp_info = %p\n", __func__, __LINE__, o, group->generic_prov_info);
//        group->my_type = H5I_GROUP;
        printf("%s:%d\n", __func__, __LINE__);
        group = _obj_wrap_under(under, o, name, H5I_GROUP, dxpl_id, req);
        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    } /* end if */
    else
        group = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return (void *)group;
} /* end H5VL_provenance_group_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_group_get
 *
 * Purpose:     Get info about a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_group_get(void *obj, H5VL_group_get_t get_type, hid_t dxpl_id,
    void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Get\n");
#endif

    ret_value = H5VLgroup_get(o->under_object, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_group_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_group_specific
 *
 * Purpose:     Specific operation on a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_group_specific(void *obj, H5VL_group_specific_t specific_type,
    hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Specific\n");
#endif

    ret_value = H5VLgroup_specific(o->under_object, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_group_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_group_optional
 *
 * Purpose:     Perform a connector-specific operation on a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_group_optional(void *obj, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Optional\n");
#endif

    ret_value = H5VLgroup_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_group_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_group_close
 *
 * Purpose:     Closes a group.
 *
 * Return:      Success:    0
 *              Failure:    -1, group not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_group_close(void *grp, hid_t dxpl_id, void **req)
{
    printf("%s:%d: (void*)grp = %p\n", __func__, __LINE__, grp);
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)grp;
    herr_t ret_value;
    ret_value = H5VLgroup_close(o->under_object, o->under_vol_id, dxpl_id, req);

    group_prov_info_t* grp_info = (group_prov_info_t*)o->generic_prov_info;

    printf("%s:%d: ========== o = %p, grp_info = %p\n", __func__, __LINE__, o, grp_info);

    file_prov_info_t* root_file = grp_info->root_file_info;

    printf("%s:%d: ========== root_file = %p\n", __func__, __LINE__, root_file);
    printf("%s:%d: ========== root_file->file_name = %p\n", __func__, __LINE__, root_file->file_name);


#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL H5Gclose\n");
#endif



    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying file was closed */
    printf("%s:%d\n", __func__, __LINE__);
    if(ret_value >= 0){
        printf("%s:%d\n", __func__, __LINE__);
        rm_grp_node(root_file, grp_info->native_addr);
        printf("%s:%d\n", __func__, __LINE__);
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
        group_stats_prov_write(o->generic_prov_info);
        H5VL_provenance_free_obj(o);

    }


    return ret_value;
} /* end H5VL_provenance_group_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_link_create
 *
 * Purpose:     Creates a hard / soft / UD / external link.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_link_create(H5VL_link_create_type_t create_type, void *obj, const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    hid_t under_vol_id = -1;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL LINK Create\n");
#endif

    /* Try to retrieve the "under" VOL id */
    if(o)
        under_vol_id = o->under_vol_id;

    /* Fix up the link target object for hard link creation */
    if(H5VL_LINK_CREATE_HARD == create_type) {
        void         *cur_obj;

        /* Retrieve the object for the link target */
        H5Pget(lcpl_id, H5VL_PROP_LINK_TARGET, &cur_obj);

        /* If it's a non-NULL pointer, find the 'under object' and re-set the property */
        if(cur_obj) {
            /* Check if we still need the "under" VOL ID */
            if(under_vol_id < 0)
                under_vol_id = ((H5VL_provenance_t *)cur_obj)->under_vol_id;

            /* Set the object for the link target */
            H5Pset(lcpl_id, H5VL_PROP_LINK_TARGET, &(((H5VL_provenance_t *)cur_obj)->under_object));
        } /* end if */
    } /* end if */

    ret_value = H5VLlink_create(create_type, (o ? o->under_object : NULL), loc_params, under_vol_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_link_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_link_copy
 *
 * Purpose:     Renames an object within an HDF5 container and copies it to a new
 *              group.  The original name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1,
    void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id,
    hid_t lapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o_src = (H5VL_provenance_t *)src_obj;
    H5VL_provenance_t *o_dst = (H5VL_provenance_t *)dst_obj;
    hid_t under_vol_id = -1;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL LINK Copy\n");
#endif

    /* Retrieve the "under" VOL id */
    if(o_src)
        under_vol_id = o_src->under_vol_id;
    else if(o_dst)
        under_vol_id = o_dst->under_vol_id;
    assert(under_vol_id > 0);

    ret_value = H5VLlink_copy((o_src ? o_src->under_object : NULL), loc_params1, (o_dst ? o_dst->under_object : NULL), loc_params2, under_vol_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, under_vol_id, o_dst->prov_helper);
            
    if(o_dst)
        prov_write(o_dst->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_link_copy() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_link_move
 *
 * Purpose:     Moves a link within an HDF5 file to a new group.  The original
 *              name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1,
    void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id,
    hid_t lapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o_src = (H5VL_provenance_t *)src_obj;
    H5VL_provenance_t *o_dst = (H5VL_provenance_t *)dst_obj;
    hid_t under_vol_id = -1;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL LINK Move\n");
#endif

    /* Retrieve the "under" VOL id */
    if(o_src)
        under_vol_id = o_src->under_vol_id;
    else if(o_dst)
        under_vol_id = o_dst->under_vol_id;
    assert(under_vol_id > 0);

    ret_value = H5VLlink_move((o_src ? o_src->under_object : NULL), loc_params1, (o_dst ? o_dst->under_object : NULL), loc_params2, under_vol_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, under_vol_id, o_dst->prov_helper);

    if(o_dst)
        prov_write(o_dst->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_link_move() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_link_get
 *
 * Purpose:     Get info about a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_link_get(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_link_get_t get_type, hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL LINK Get\n");
#endif

    ret_value = H5VLlink_get(o->under_object, loc_params, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
            
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_link_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_link_specific
 *
 * Purpose:     Specific operation on a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_link_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;
    printf("%s:%d\n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL LINK Specific\n");
#endif

    ret_value = H5VLlink_specific(o->under_object, loc_params, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_link_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_link_optional
 *
 * Purpose:     Perform a connector-specific operation on a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_provenance_link_optional(void *obj, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL LINK Optional\n");
#endif
    printf("%s:%d\n", __func__, __LINE__);
    ret_value = H5VLlink_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_link_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_object_open
 *
 * Purpose:     Opens an object inside a container.
 *
 * Return:      Success:    Pointer to object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_provenance_object_open(void *obj, const H5VL_loc_params_t *loc_params,
    H5I_type_t *obj_to_open_type, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *new_obj;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    void *under;
    char* obj_name = NULL;
    printf("%s:%d: \n", __func__, __LINE__);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Open\n");
#endif
    printf("%s:%d:loc_params->type = %d, obj_type = %d, o->my_type = %d, obj_to_open_type = %d \n",
            __func__, __LINE__, loc_params->type, loc_params->obj_type, o->my_type, *obj_to_open_type);

    if(loc_params->type == H5VL_OBJECT_BY_NAME){
       printf("%s:%d: H5VL_OBJECT_BY_NAME: obj_name = [%s]\n",
               __func__, __LINE__, loc_params->loc_data.loc_by_name.name);
    }
    if(loc_params->type == H5VL_OBJECT_BY_ADDR){
       printf("%s:%d: H5VL_OBJECT_BY_ADDR: addr = [%d]\n",
               __func__, __LINE__, loc_params->loc_data.loc_by_addr.addr);
    }
    if(loc_params->type == H5VL_OBJECT_BY_REF){
       printf("%s:%d: H5VL_OBJECT_BY_REF\n", __func__, __LINE__);
    }
    if(loc_params->type == H5VL_OBJECT_BY_IDX){
       printf("%s:%d: H5VL_OBJECT_BY_IDX\n", __func__, __LINE__);
    }
    if(loc_params->type == H5VL_OBJECT_BY_SELF){
       printf("%s:%d: H5VL_OBJECT_BY_SELF\n", __func__, __LINE__);
    }
    under = H5VLobject_open(o->under_object, loc_params, o->under_vol_id,
            obj_to_open_type, dxpl_id, req);

    printf("%s:%d:loc_params->type = %d, obj_type = %d, o->my_type = %d, obj_to_open_type = %d \n",
            __func__, __LINE__, loc_params->type, loc_params->obj_type, o->my_type, *obj_to_open_type);

    if(under) {
        if(loc_params->type == H5VL_OBJECT_BY_NAME){
            obj_name = loc_params->loc_data.loc_by_name.name;
            printf("%s:%d: obj_name = [%s]\n", __func__, __LINE__, obj_name);
        }

        printf("%s:%d:loc_params->type = %d, obj_type = %d, o->my_type = %d, obj_to_open_type = %d \n",
                __func__, __LINE__, loc_params->type, loc_params->obj_type, o->my_type, *obj_to_open_type);

        new_obj = _obj_wrap_under(under, o, obj_name, *obj_to_open_type, dxpl_id, req);

    } /* end if */
    else
        new_obj = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return (void *)new_obj;
} /* end H5VL_provenance_object_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_object_copy
 *
 * Purpose:     Copies an object inside a container.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params,
    const char *src_name, void *dst_obj, const H5VL_loc_params_t *dst_loc_params,
    const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id,
    void **req)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o_src = (H5VL_provenance_t *)src_obj;
    H5VL_provenance_t *o_dst = (H5VL_provenance_t *)dst_obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Copy\n");
#endif

    ret_value = H5VLobject_copy(o_src->under_object, src_loc_params, src_name, o_dst->under_object, dst_loc_params, dst_name, o_src->under_vol_id, ocpypl_id, lcpl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o_src->under_vol_id, o_dst->prov_helper);

    if(o_dst)
        prov_write(o_dst->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_object_copy() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_object_get
 *
 * Purpose:     Get info about an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_object_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_get_t get_type, hid_t dxpl_id, void **req, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Get\n");
#endif

    ret_value = H5VLobject_get(o->under_object, loc_params, o->under_vol_id, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_object_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_object_specific
 *
 * Purpose:     Specific operation on an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_object_specific(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_object_specific_t specific_type, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;
    //printf("%s:%d: o->helper = %p\n", __func__, __LINE__, o->prov_helper);
    //printf("%s:%d: o->helper->ptr_cnt = %d\n", __func__, __LINE__, o->prov_helper->ptr_cnt);
#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Specific\n");
#endif
//o->under_object
    //printf("specific_type = %d\n", specific_type);
    ret_value = H5VLobject_specific(o->under_object, loc_params, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    //printf("%s:%d: o->helper = %p\n", __func__, __LINE__, o->prov_helper);
    //printf("%s:%d: o->helper->ptr_cnt = %d\n", __func__, __LINE__, o->prov_helper->ptr_cnt);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_object_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_object_optional
 *
 * Purpose:     Perform a connector-specific operation for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_object_optional(void *obj, hid_t dxpl_id, void **req,
    va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Optional\n");
#endif

    ret_value = H5VLobject_optional(o->under_object, o->under_vol_id, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_object_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_wait
 *
 * Purpose:     Wait (with a timeout) for an async operation to complete
 *
 * Note:        Releases the request if the operation has completed and the
 *              connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_wait(void *obj, uint64_t timeout,
    H5ES_status_t *status)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL REQUEST Wait\n");
#endif

    ret_value = H5VLrequest_wait(o->under_object, o->under_vol_id, timeout, status);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    if(ret_value >= 0 && *status != H5ES_STATUS_IN_PROGRESS)
        H5VL_provenance_free_obj(o);


    return ret_value;
} /* end H5VL_provenance_request_wait() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_notify
 *
 * Purpose:     Registers a user callback to be invoked when an asynchronous
 *              operation completes
 *
 * Note:        Releases the request, if connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_notify(void *obj, H5VL_request_notify_t cb, void *ctx)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL REQUEST Wait\n");
#endif

    ret_value = H5VLrequest_notify(o->under_object, o->under_vol_id, cb, ctx);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    if(ret_value >= 0)
        H5VL_provenance_free_obj(o);


    return ret_value;
} /* end H5VL_provenance_request_notify() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_cancel
 *
 * Purpose:     Cancels an asynchronous operation
 *
 * Note:        Releases the request, if connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_cancel(void *obj)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL REQUEST Cancel\n");
#endif

    ret_value = H5VLrequest_cancel(o->under_object, o->under_vol_id);
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

    if(ret_value >= 0)
        H5VL_provenance_free_obj(o);


    return ret_value;
} /* end H5VL_provenance_request_cancel() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_specific_reissue
 *
 * Purpose:     Re-wrap vararg arguments into a va_list and reissue the
 *              request specific callback to the underlying VOL connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_specific_reissue(void *obj, hid_t connector_id,
    H5VL_request_specific_t specific_type, ...)
{
    va_list arguments;
    herr_t ret_value;

    va_start(arguments, specific_type);
    ret_value = H5VLrequest_specific(obj, connector_id, specific_type, arguments);
    va_end(arguments);

    return ret_value;
} /* end H5VL_provenance_request_specific_reissue() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_specific
 *
 * Purpose:     Specific operation on a request
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_specific(void *obj, H5VL_request_specific_t specific_type,
    va_list arguments)
{

    herr_t ret_value = -1;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL REQUEST Specific\n");
#endif

    if(H5VL_REQUEST_WAITANY == specific_type ||
            H5VL_REQUEST_WAITSOME == specific_type ||
            H5VL_REQUEST_WAITALL == specific_type) {
        va_list tmp_arguments;
        size_t req_count;

        /* Sanity check */
        assert(obj == NULL);

        /* Get enough info to call the underlying connector */
        va_copy(tmp_arguments, arguments);
        req_count = va_arg(tmp_arguments, size_t);

        /* Can only use a request to invoke the underlying VOL connector when there's >0 requests */
        if(req_count > 0) {
            void **req_array;
            void **under_req_array;
            uint64_t timeout;
            H5VL_provenance_t *o;
            size_t u;               /* Local index variable */

            /* Get the request array */
            req_array = va_arg(tmp_arguments, void **);

            /* Get a request to use for determining the underlying VOL connector */
            o = (H5VL_provenance_t *)req_array[0];

            /* Create array of underlying VOL requests */
            under_req_array = (void **)malloc(req_count * sizeof(void **));
            for(u = 0; u < req_count; u++)
                under_req_array[u] = ((H5VL_provenance_t *)req_array[u])->under_object;

            /* Remove the timeout value from the vararg list (it's used in all the calls below) */
            timeout = va_arg(tmp_arguments, uint64_t);

            /* Release requests that have completed */
            if(H5VL_REQUEST_WAITANY == specific_type) {
                size_t *index;          /* Pointer to the index of completed request */
                H5ES_status_t *status;  /* Pointer to the request's status */

                /* Retrieve the remaining arguments */
                index = va_arg(tmp_arguments, size_t *);
                assert(*index <= req_count);
                status = va_arg(tmp_arguments, H5ES_status_t *);

                /* Reissue the WAITANY 'request specific' call */
                ret_value = H5VL_provenance_request_specific_reissue(o->under_object, o->under_vol_id, specific_type, req_count, under_req_array, timeout, index, status);

                /* Release the completed request, if it completed */
                if(ret_value >= 0 && *status != H5ES_STATUS_IN_PROGRESS) {
                    H5VL_provenance_t *tmp_o;

                    tmp_o = (H5VL_provenance_t *)req_array[*index];
                    H5VL_provenance_free_obj(tmp_o);
                } /* end if */
            } /* end if */
            else if(H5VL_REQUEST_WAITSOME == specific_type) {
                size_t *outcount;               /* # of completed requests */
                unsigned *array_of_indices;     /* Array of indices for completed requests */
                H5ES_status_t *array_of_statuses; /* Array of statuses for completed requests */

                /* Retrieve the remaining arguments */
                outcount = va_arg(tmp_arguments, size_t *);
                assert(*outcount <= req_count);
                array_of_indices = va_arg(tmp_arguments, unsigned *);
                array_of_statuses = va_arg(tmp_arguments, H5ES_status_t *);

                /* Reissue the WAITSOME 'request specific' call */
                ret_value = H5VL_provenance_request_specific_reissue(o->under_object, o->under_vol_id, specific_type, req_count, under_req_array, timeout, outcount, array_of_indices, array_of_statuses);

                /* If any requests completed, release them */
                if(ret_value >= 0 && *outcount > 0) {
                    unsigned *idx_array;    /* Array of indices of completed requests */

                    /* Retrieve the array of completed request indices */
                    idx_array = va_arg(tmp_arguments, unsigned *);

                    /* Release the completed requests */
                    for(u = 0; u < *outcount; u++) {
                        H5VL_provenance_t *tmp_o;

                        tmp_o = (H5VL_provenance_t *)req_array[idx_array[u]];
                        H5VL_provenance_free_obj(tmp_o);
                    } /* end for */
                } /* end if */
            } /* end else-if */
            else {      /* H5VL_REQUEST_WAITALL == specific_type */
                H5ES_status_t *array_of_statuses; /* Array of statuses for completed requests */

                /* Retrieve the remaining arguments */
                array_of_statuses = va_arg(tmp_arguments, H5ES_status_t *);

                /* Reissue the WAITALL 'request specific' call */
                ret_value = H5VL_provenance_request_specific_reissue(o->under_object, o->under_vol_id, specific_type, req_count, under_req_array, timeout, array_of_statuses);

                /* Release the completed requests */
                if(ret_value >= 0) {
                    for(u = 0; u < req_count; u++) {
                        if(array_of_statuses[u] != H5ES_STATUS_IN_PROGRESS) {
                            H5VL_provenance_t *tmp_o;

                            tmp_o = (H5VL_provenance_t *)req_array[u];
                            H5VL_provenance_free_obj(tmp_o);
                        } /* end if */
                    } /* end for */
                } /* end if */
            } /* end else */

            /* Release array of requests for underlying connector */
            free(under_req_array);
        } /* end if */

        /* Finish use of copied vararg list */
        va_end(tmp_arguments);
    } /* end if */
    else
        assert(0 && "Unknown 'specific' operation");

    return ret_value;
} /* end H5VL_provenance_request_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_optional
 *
 * Purpose:     Perform a connector-specific operation for a request
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_optional(void *obj, va_list arguments)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL REQUEST Optional\n");
#endif

    ret_value = H5VLrequest_optional(o->under_object, o->under_vol_id, arguments);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    return ret_value;
} /* end H5VL_provenance_request_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_provenance_request_free
 *
 * Purpose:     Releases a request, allowing the operation to complete without
 *              application tracking
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_provenance_request_free(void *obj)
{
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL REQUEST Free\n");
#endif
    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    ret_value = H5VLrequest_free(o->under_object, o->under_vol_id);

    if(ret_value >= 0)
        H5VL_provenance_free_obj(o);

    return ret_value;
} /* end H5VL_provenance_request_free() */

