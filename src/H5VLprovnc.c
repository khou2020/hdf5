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
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

typedef struct H5VL_provenance_t {
    hid_t  under_vol_id;        /* ID for underlying VOL connector */
    void   *under_object;       /* Info object for underlying VOL connector */
    H5I_type_t my_type;         //obj type, dataset, datatype, etc.,
    prov_helper_t* prov_helper; //pointer shared among all layers, one per process.
    void* generic_prov_info;    //TODO: start to use this.
    //should be casted to layer-specific type before use,
    //such as file_prov_info, dataset_prov_info
} H5VL_provenance_t;

/* The pass through VOL wrapper context */
typedef struct H5VL_provenance_wrap_ctx_t {
    hid_t under_vol_id;         /* VOL ID for under VOL */
    void *under_wrap_ctx;       /* Object wrapping context for under VOL */
    unsigned long root_file_no;
    hid_t dtype_id; //only used by datatype.
    file_prov_info_t* root_file_info;
    prov_helper_t* prov_helper; //shared pointer
} H5VL_provenance_wrap_ctx_t;

//======================================= statistics =======================================
//typedef struct H5VL_prov_t {
//    void   *under_object;
//    char* func_name;
//    int func_cnt;//stats
//} H5VL_prov_t;

struct H5VL_prov_datatype_info_t {
    prov_helper_t* prov_helper; //pointer shared among all layers, one per process.
    haddr_t native_addr;
    char* dtype_name;
    hid_t dtype_id;
    int ref_cnt;
    int datatype_commit_cnt;
    int datatype_get_cnt;
    file_prov_info_t* root_file_info;
    datatype_prov_info_t* next;
};

typedef struct H5VL_prov_dataset_info_t dataset_prov_info_t;
typedef struct H5VL_prov_group_info_t group_prov_info_t;

struct H5VL_prov_file_info_t {//assigned when a file is closed, serves to store stats (copied from shared_file_info)
    prov_helper_t* prov_helper; //pointer shared among all layers, one per process.
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
};
//grp only has links.

struct H5VL_prov_dataset_info_t {
    prov_helper_t* prov_helper; //pointer shared among all layers, one per process.
    haddr_t native_addr;
    char* dset_name;
    char* file_name;//parent structure
    H5T_class_t dt_class;//data type class
    H5S_class_t ds_class;//data space class
    H5D_layout_t layout;
    unsigned int dimension_cnt;
    hsize_t dimensions[H5S_MAX_RANK];
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
};

void dataset_get_wrapper(void *dset, hid_t driver_id, H5VL_dataset_get_t get_type, hid_t dxpl_id, void **req, ...);

struct H5VL_prov_group_info_t {
    prov_helper_t* prov_helper; //pointer shared among all layers, one per process.
    int func_cnt;//stats
    file_prov_info_t* root_file_info;//a group is a direct or indirect children of a file.
    haddr_t native_addr;
//    int group_get_cnt;
//    int group_specific_cnt;
    int ref_cnt;//multiple opens on a group
    group_prov_info_t* next;
};

typedef struct H5VL_prov_link_info_t {
    int link_get_cnt;
    int link_specific_cnt;
} link_prov_info_t;



prov_helper_t* PROV_HELPER;

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
    hid_t under_vol_id, prov_helper_t* helper);
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
    0,                                            /* capability flags */
    H5VL_provenance_init,                         /* initialize   */
    H5VL_provenance_term,                         /* terminate    */
    {                                           /* info_cls */
        sizeof(H5VL_provenance_info_t),           /* info size    */
        H5VL_provenance_info_copy,                /* info copy    */
        H5VL_provenance_info_cmp,                 /* info compare */
        H5VL_provenance_info_free,                /* info free    */
        H5VL_provenance_info_to_str,              /* info to str  */
        H5VL_provenance_str_to_info,              /* str to info  */
    },
    {                                           /* wrap_cls */
        H5VL_provenance_get_object,               /* get_object   */
        H5VL_provenance_get_wrap_ctx,             /* get_wrap_ctx */
        H5VL_provenance_wrap_object,              /* wrap_object  */
        H5VL_provenance_free_wrap_ctx,            /* free_wrap_ctx */
    },
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

/* Local routine prototypes */
datatype_prov_info_t *new_datatype_info(void);
dataset_prov_info_t *new_dataset_info(void);
group_prov_info_t *new_group_info(file_prov_info_t* root_file, haddr_t addr);
file_prov_info_t *new_file_info(const char* fname);
void dtype_info_free(datatype_prov_info_t* info);
void file_info_free(file_prov_info_t* info);
void group_info_free(group_prov_info_t* info);
void dataset_info_free(dataset_prov_info_t* info);
void dataset_stats_prov_write(const dataset_prov_info_t* ds_info);
void file_stats_prov_write(const file_prov_info_t* file_info);
void datatype_stats_prov_write(const datatype_prov_info_t* dt_info);
void group_stats_prov_write(const group_prov_info_t* grp_info);
void prov_helper_teardown(prov_helper_t* helper);
void file_ds_created(file_prov_info_t* info);
void file_ds_accessed(file_prov_info_t* info);
datatype_prov_info_t* add_dtype_node(prov_helper_t* helper, haddr_t native_addr);
int rm_dtype_node(prov_helper_t* helper, haddr_t native_addr);
file_prov_info_t* add_file_node(prov_helper_t* helper, const char* file_name, unsigned long file_no);
group_prov_info_t* add_grp_node(file_prov_info_t* root_file, haddr_t native_addr);
int rm_grp_node(file_prov_info_t* root_file, haddr_t native_addr);
int rm_file_node(prov_helper_t* helper, char* file_name);
file_prov_info_t* _search_home_file(unsigned long obj_file_no);
dataset_prov_info_t* add_dataset_node(unsigned long obj_file_no, H5VL_provenance_t* dset, haddr_t native_addr,
        file_prov_info_t* file_info_in, const char* ds_name, hid_t dxpl_id, void** req);
int rm_dataset_node(file_prov_info_t* file_info, haddr_t native_addr);
void ptr_cnt_increment(prov_helper_t* helper);
void ptr_cnt_decrement(prov_helper_t* helper);
void get_time_str(char *str_out);
unsigned long get_time_usec(void);
dataset_prov_info_t* new_ds_prov_info(void* under_object, hid_t vol_id, haddr_t native_addr,
        file_prov_info_t* file_info, const char* ds_name, hid_t dxpl_id, void **req);
void _new_loc_pram(H5I_type_t type, H5VL_loc_params_t *lparam);
herr_t get_native_info(void* obj, hid_t vol_id, hid_t dxpl_id, void **req, ...);
void get_native_file_no(unsigned long* file_num_out, const H5VL_provenance_t* file_obj);
herr_t provenance_file_setup(const char* str_in, char* file_path_out, Prov_level* level_out, char* format_out);
H5VL_provenance_t* _fake_obj_new(file_prov_info_t* root_file, hid_t under_vol_id);
void _fake_obj_free(H5VL_provenance_t* obj);
H5VL_provenance_t* _obj_wrap_under(void* under, H5VL_provenance_t* upper_o,
        const char *name, H5I_type_t type, hid_t dxpl_id, void** req);
H5VL_provenance_t* _file_open_common(void* under, hid_t vol_id, const char* name);

datatype_prov_info_t* new_datatype_info(void){
    datatype_prov_info_t* info = (datatype_prov_info_t *)calloc(1, sizeof(datatype_prov_info_t));
    info->prov_helper = PROV_HELPER;
    return info;
}

dataset_prov_info_t * new_dataset_info(void){
    dataset_prov_info_t* info = (dataset_prov_info_t *)calloc(1, sizeof(dataset_prov_info_t));
    info->prov_helper = PROV_HELPER;
    return info;
}

group_prov_info_t * new_group_info(file_prov_info_t* root_file, haddr_t addr){
    group_prov_info_t* info = (group_prov_info_t *)calloc(1, sizeof(group_prov_info_t));
    info->root_file_info = root_file;
    info->prov_helper = PROV_HELPER;
    info->native_addr = addr;
    info->ref_cnt = 0;
    return info;
}

file_prov_info_t* new_file_info(const char* fname) {
    file_prov_info_t* info = (file_prov_info_t *)calloc(1, sizeof(file_prov_info_t));
    info->file_name = strdup(fname);
    info->prov_helper = PROV_HELPER;
    return info;
}

void dtype_info_free(datatype_prov_info_t* info){
    if(!info->dtype_name)
        free(info->dtype_name);
    free(info);
}

void file_info_free(file_prov_info_t* info){
    if(info->file_name)
        free(info->file_name);
    free(info);
}

void group_info_free(group_prov_info_t* info){
    free(info);
}

void dataset_info_free(dataset_prov_info_t* info){
    if(!info)
        return;

    if(info->dset_name)
        free(info->dset_name);

    free(info);
}

void dataset_stats_prov_write(const dataset_prov_info_t* ds_info){
    if(!ds_info){
        printf("dataset_stats_prov_write(): ds_info is NULL.\n");
        return;
    }
    printf("Dataset name = %s,\ndata type class = %d, data space class = %d, data space size = %llu, data type size =%zu.\n",
            ds_info->dset_name, ds_info->dt_class, ds_info->ds_class,  (unsigned long long)ds_info->dset_space_size, ds_info->dset_type_size);
    printf("Dataset is %u dimensions.\n", ds_info->dimension_cnt);
    printf("Dataset is read %d time, %llu bytes in total, costs %llu us.\n", ds_info->dataset_read_cnt, ds_info->total_bytes_read, ds_info->total_read_time);
    printf("Dataset is written %d time, %llu bytes in total, costs %llu us.\n", ds_info->dataset_write_cnt, ds_info->total_bytes_written, ds_info->total_write_time);
}

//not file_prov_info_t!
void file_stats_prov_write(const file_prov_info_t* file_info) {
    if(!file_info){
        printf("file_stats_prov_write(): ds_info is NULL.\n");
        return;
    }

    printf("H5 file closed, %d datasets are created, %d datasets are accessed.\n", file_info->ds_created, file_info->ds_accessed);

}

void datatype_stats_prov_write(const datatype_prov_info_t* dt_info) {
    if(!dt_info){
        printf("datatype_stats_prov_write(): ds_info is NULL.\n");
        return;
    }
    printf("Datatype name = %s, commited %d times, datatype get is called %d times.\n", dt_info->dtype_name, dt_info->datatype_commit_cnt, dt_info->datatype_get_cnt);
}

void group_stats_prov_write(const group_prov_info_t* grp_info) {
    if(!grp_info){
        printf("group_stats_prov_write(): grp_info is NULL.\n");
        return;
    }
    printf("group_stats_prov_write() is yet to be implemented.\n");
}

prov_helper_t* prov_helper_init( char* file_path, Prov_level prov_level, char* prov_line_format){
    prov_helper_t* new_helper = (prov_helper_t *)calloc(1, sizeof(prov_helper_t));

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

    pthread_threadid_np(NULL, &(new_helper->tid));//OSX only

    getlogin_r(new_helper->user_name, 32);

    if(new_helper->prov_level == File_only || new_helper->prov_level == File_and_print){
        new_helper->prov_file_handle = fopen(new_helper->prov_file_path, "a");
    }

    return new_helper;
}

//problematic!!! May cause weird memory corruption issues.
void prov_helper_teardown(prov_helper_t* helper){
    if(helper){// not null
        if(helper->prov_level == File_only || helper->prov_level ==File_and_print){//no file
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

datatype_prov_info_t* add_dtype_node(prov_helper_t* helper, haddr_t native_addr){
    datatype_prov_info_t* new_node;
    assert(helper);

    if(!helper->opened_dtypes){//empty linked list, no opened type.
        helper->opened_dtypes_cnt = 0;
        new_node = (datatype_prov_info_t *)calloc(1, sizeof(datatype_prov_info_t));
        new_node->ref_cnt = 1;
        new_node->native_addr = native_addr;
        helper->opened_dtypes = new_node;
        helper->opened_dtypes_cnt++;
    } else {
        datatype_prov_info_t* cur = helper->opened_dtypes;
        datatype_prov_info_t* last = cur;

        while (cur) {
            if (cur->native_addr == native_addr) {//attempt to open an already opened type.
                cur->ref_cnt++;
                return cur;
            }
            last = cur;
            cur = cur->next;
        }

        new_node = (datatype_prov_info_t *)calloc(1, sizeof(datatype_prov_info_t));
        new_node->ref_cnt = 1;
        new_node->native_addr = native_addr;
        last->next = new_node;
        helper->opened_dtypes_cnt++;
    }

    return new_node;
}

int rm_dtype_node(prov_helper_t* helper, haddr_t native_addr){
    datatype_prov_info_t* cur;
    datatype_prov_info_t* last;
    int index;

    assert(helper);

    if(!helper->opened_dtypes){//no node found
        return helper->opened_dtypes_cnt;//no fopen files.
    }

    cur = helper->opened_dtypes;
    last = cur;
    index = 0;
    while(cur){
        if(cur->native_addr == native_addr){//node found
            cur->ref_cnt--;
            if(cur->ref_cnt > 0){//not to remove
                return helper->opened_dtypes_cnt;
            }
            else { //cur->next->ref_cnt == 0, remove file node & maybe print file stats (heppens when close a file)
                if(0 == index){//first node is the target
                    helper->opened_dtypes = helper->opened_dtypes->next;
                    dtype_info_free(cur);
                    helper->opened_dtypes_cnt--;
                    return helper->opened_dtypes_cnt;
                }

                last->next = cur->next;

                dtype_info_free(cur);
                helper->opened_dtypes_cnt--;
                if(helper->opened_dtypes_cnt == 0){
                    helper->opened_dtypes = NULL;
                }

                return helper->opened_dtypes_cnt;
            }
        }
        last = cur;
        cur = cur->next;
        index++;
    }
    //node not found.
    return helper->opened_dtypes_cnt;
}

file_prov_info_t* add_file_node(prov_helper_t* helper, const char* file_name, unsigned long file_no){
    file_prov_info_t* new_node;

    assert(helper);

    if(!file_name){//fake upper_o
        return NULL;
    }

    if(!helper->opened_files){//empty linked list, no opened file.
        helper->opened_files_cnt = 0;
        new_node = (file_prov_info_t *)calloc(1, sizeof(file_prov_info_t));
        new_node->file_name = strdup(file_name);
        new_node->ref_cnt = 1;
        new_node->file_no = file_no;
        helper->opened_files = new_node;
        helper->opened_files_cnt++;
    } else {
        file_prov_info_t* cur;
        file_prov_info_t* last;

        cur = helper->opened_files;
        last = cur;
        while (cur) {
            if (!strcmp(cur->file_name, file_name)) {//attempt to open an already opened file.
                if(cur->file_no == 0)
                    cur->file_no = file_no;

                cur->ref_cnt++;

                return cur;
            }
            last = cur;
            cur = cur->next;
        }

        new_node = (file_prov_info_t *)calloc(1, sizeof(file_prov_info_t));
        new_node->file_name = strdup(file_name);
        new_node->ref_cnt = 1;
        new_node->file_no = file_no;
        last->next = new_node;
        helper->opened_files_cnt++;
    }

    return new_node;
}

group_prov_info_t* add_grp_node(file_prov_info_t* root_file, haddr_t native_addr){
    group_prov_info_t* new_grp;

    assert(root_file);

    root_file->ref_cnt++;

    if(!root_file->opened_grps){
        root_file->opened_grps_cnt = 1;
        new_grp = new_group_info(root_file, native_addr);
        new_grp->ref_cnt = 1;
        root_file->opened_grps = new_grp;

        return new_grp;
    } else {
        group_prov_info_t* cur;
        group_prov_info_t* last;

        cur = root_file->opened_grps;
        last = cur;
        while(cur){
            if(native_addr == cur->native_addr){
                cur->ref_cnt++;

                return cur;
            }
            last = cur;
            cur = cur->next;
        }

        new_grp = new_group_info(root_file, native_addr);
        new_grp->ref_cnt = 1;
        last->next = new_grp;
        root_file->opened_grps_cnt++;
    }

    return new_grp;
}

int rm_grp_node(file_prov_info_t* root_file, haddr_t native_addr){
    group_prov_info_t* cur;
    group_prov_info_t* last;
    int index;

    assert(root_file);

    root_file->ref_cnt--;

    if(!root_file->opened_grps){
        return root_file->opened_grps_cnt;
    }

    if( root_file->opened_grps_cnt == 0){
        return 0;
    }

    cur = root_file->opened_grps;
    last = cur;
    index = 0;
    while(cur){
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
int rm_file_node(prov_helper_t* helper, char* file_name){
    file_prov_info_t* cur;
    file_prov_info_t* last;
    int index;

    assert(helper);
    assert(file_name);

    if(!helper->opened_files){//no node found
        return helper->opened_files_cnt;//no fopen files.
    }

    cur = helper->opened_files;
    last = cur;
    index = 0;
    while(cur){
        if(!strcmp(cur->file_name, file_name)){//node found
            cur->ref_cnt--;
            if(cur->ref_cnt > 0){//not to remove
                return helper->opened_files_cnt;
            }
            else { //cur->next->ref_cnt == 0, remove file node & maybe print file stats (heppens when close a file)
                if(0 == index){//first node is the target
                    helper->opened_files = helper->opened_files->next;
                    file_info_free(cur);
                    helper->opened_files_cnt--;
                    return helper->opened_files_cnt;
                }

                last->next = cur->next;

                file_info_free(cur);
                helper->opened_files_cnt--;
                if(helper->opened_files_cnt == 0){
                    helper->opened_files = NULL;
                }

                return helper->opened_files_cnt;
            }
        }
        last = cur;
        cur = cur->next;
        index++;
    }

    //node not found.
    return helper->opened_files_cnt;
}

file_prov_info_t* _search_home_file(unsigned long obj_file_no){
    file_prov_info_t* cur;

    if(PROV_HELPER->opened_files_cnt < 1)
        return NULL;

    cur = PROV_HELPER->opened_files;
    while (cur) {
        if (cur->file_no == obj_file_no) {//file found
            cur->ref_cnt++;
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

dataset_prov_info_t* add_dataset_node(unsigned long obj_file_no, H5VL_provenance_t* dset, haddr_t native_addr,
        file_prov_info_t* file_info_in, const char* ds_name, hid_t dxpl_id, void** req){
    file_prov_info_t* file_info;
    dataset_prov_info_t* new_node;

    assert(dset);
    assert(dset->under_object);
    assert(file_info_in);

    if(obj_file_no != file_info_in->file_no){//creating a dataset from an external place
        file_prov_info_t* external_home_file;

        external_home_file = _search_home_file(obj_file_no);
        if(external_home_file){//use extern home
            file_info = external_home_file;
        }else{//extern home not exist, fake one
            file_prov_info_t* new_file;

            new_file = new_file_info("dummy");
            new_file->file_no = obj_file_no;
            file_info = new_file;
        }
    }else{//local
        file_info = file_info_in;
    }

    if(!file_info->opened_datasets){//empty linked list, no opened file.
        new_node = new_ds_prov_info(dset->under_object, dset->under_vol_id,
                native_addr, file_info, ds_name, dxpl_id, req);
        file_info->opened_datasets_cnt = 0;
        new_node->ref_cnt = 1;
        file_info->opened_datasets = new_node;
        file_info->opened_datasets_cnt++;

        return new_node;
    } else {
        dataset_prov_info_t* cur;
        dataset_prov_info_t* last;
        int i;

        cur = file_info->opened_datasets;
        last = cur;
        i = 0;
        while (cur) {
            if (cur->native_addr == native_addr) {
                //attempt to open an already opened file.

                cur->ref_cnt++;
                return cur;
            }
            i++;
            last = cur;
            cur = cur->next;
        }

        new_node = new_ds_prov_info(dset->under_object, dset->under_vol_id, native_addr, file_info, ds_name, dxpl_id, req);
        last->next = new_node;
        file_info->opened_datasets_cnt++;

        return new_node;
    }
}

//need a dumy node to make it simpler
int rm_dataset_node(file_prov_info_t* file_info, haddr_t native_addr){
    dataset_prov_info_t* ds_cur;
    dataset_prov_info_t* last;
    int index;

    assert(file_info);

    if(!file_info->opened_datasets){//no node found
        return file_info->opened_datasets_cnt;//no fopen files.
    }

    ds_cur = file_info->opened_datasets;
    last = ds_cur;
    index = 0;
    while(ds_cur){
        if(ds_cur->native_addr == native_addr){//node found
            ds_cur->ref_cnt--;
            if(ds_cur->ref_cnt > 0){
                return ds_cur->ref_cnt;
            }
            else{//cur->next->ref_cnt == 0, remove file node & maybe print file stats (heppens when close a file)

                //special case: first node is the target, ==cur
                if(0 == index){
                    file_info->opened_datasets = file_info->opened_datasets->next;
                    dataset_info_free(ds_cur);
                    file_info->opened_datasets_cnt--;
                    return 0;//ref_cnt
                }
                last->next = ds_cur->next;

                dataset_info_free(ds_cur);
                file_info->opened_datasets_cnt--;

                if(file_info->opened_datasets_cnt == 0){
                    file_info->opened_datasets = NULL;
                } else {
                }
                return 0;//file_info->opened_datasets_cnt;
            }
        }
        last = ds_cur;
        ds_cur = ds_cur->next;
        index++;
    }

    //node not found.
    return -1;
}

void ptr_cnt_increment(prov_helper_t* helper){
    assert(helper);

    //mutex lock

    if(helper){
        (helper->ptr_cnt)++;
    }

    //mutex unlock
}

void ptr_cnt_decrement(prov_helper_t* helper){
    assert(helper);

    //mutex lock

    helper->ptr_cnt--;

    //mutex unlock

    if(helper->ptr_cnt == 0){
        // do nothing for now.
        //prov_helper_teardown(helper);loggin is not decided yet.
    }
}


void get_time_str(char *str_out){
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    *str_out = '\0';
    sprintf(str_out, "%d/%d/%d %d:%d:%d", timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

unsigned long get_time_usec(void) {
    struct timeval tp;

    gettimeofday(&tp, NULL);
    return (unsigned long)((1000000 * tp.tv_sec) + tp.tv_usec);
}

dataset_prov_info_t* new_ds_prov_info(void* under_object, hid_t vol_id, haddr_t native_addr,
        file_prov_info_t* file_info, const char* ds_name, hid_t dxpl_id, void **req){
    hid_t dcpl_id = -1;
    hid_t dt_id = -1;
    hid_t ds_id = -1;
    dataset_prov_info_t* ds_info;

    assert(under_object);
    assert(file_info);

    ds_info = (dataset_prov_info_t *)calloc(1, sizeof(dataset_prov_info_t));
    ds_info->prov_helper = PROV_HELPER;
    ds_info->root_file_info = file_info;
    ds_info->native_addr = native_addr;
    ds_info->next = NULL;
    dataset_get_wrapper(under_object, vol_id, H5VL_DATASET_GET_DCPL, dxpl_id, req, &dcpl_id);
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
        ds_info->dimension_cnt = (unsigned)H5Sget_simple_extent_ndims(ds_id);
        H5Sget_simple_extent_dims(ds_id, ds_info->dimensions, NULL);
        ds_info->dset_space_size = (hsize_t)H5Sget_simple_extent_npoints(ds_id);
    }
    ds_info->layout = H5Pget_layout(dcpl_id);

    return ds_info;
}

void _new_loc_pram(H5I_type_t type, H5VL_loc_params_t *lparam){
    assert(lparam);

    lparam->type = H5VL_OBJECT_BY_SELF;
    lparam->obj_type = type;
    return;
}

herr_t get_native_info(void* obj, hid_t vol_id, hid_t dxpl_id, void **req, ...){
    herr_t r = -1;
    va_list args;
    int got_info;

    va_start(args, req);//add an new arg after req
    got_info = H5VLobject_optional(obj, vol_id, dxpl_id, req, args);

    if(got_info < 0)
        return -1;

    return r;
}

void get_native_file_no(unsigned long* file_num_out, const H5VL_provenance_t* file_obj){
    H5VL_loc_params_t p;
    H5O_info_t oinfo;
    void* root_grp;

    _new_loc_pram(H5I_FILE, &p);
    root_grp = H5VLgroup_open(file_obj->under_object, &p, file_obj->under_vol_id,
            "/", H5P_DEFAULT, H5P_DEFAULT, NULL);
    p.obj_type = H5I_GROUP;
    get_native_info(root_grp, file_obj->under_vol_id,
            H5P_DEFAULT, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);

    *file_num_out = oinfo.fileno;
    H5VLgroup_close(root_grp, file_obj->under_vol_id, H5P_DEFAULT, NULL);//file_obj->under_object
}




void dataset_get_wrapper(void *dset, hid_t driver_id, H5VL_dataset_get_t get_type,
        hid_t dxpl_id, void **req, ...){
    va_list args;
    va_start(args, req);
    H5VLdataset_get(dset, driver_id, get_type, dxpl_id, req, args);
}

int prov_write(prov_helper_t* helper_in, const char* msg, unsigned long duration){
    const char* base = "H5VL_provenance_";
    size_t base_len;
    size_t msg_len;
    size_t trim_offset;
    char time[64];
    char pline[512];

    assert(helper_in);

    get_time_str(time);

    /* Trimming long VOL function names */
    base_len = strlen(base);
    msg_len = strlen(msg);
    trim_offset = 0;
    if(msg_len > base_len) {//strlen(H5VL_provenance_) == 16.
        size_t i = 0;

        for(; i < base_len; i++)
            if(base[i] != msg[i])
                break;

        if(i == base_len) {// do trimming
            trim_offset = base_len;
        }
    }

    sprintf(pline, "[%s][User:%s][PID:%d][TID:%llu][Func:%s][%luus]\n", time, helper_in->user_name, helper_in->pid, helper_in->tid, msg + trim_offset, duration);

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

        case Level3:
        case Level4:
        case Disabled:
        case Default:
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
H5VL_provenance_new_obj(void *under_obj, hid_t under_vol_id, prov_helper_t* helper)
{
    H5VL_provenance_t *new_obj;

    assert(under_obj);
    assert(helper);

    new_obj = (H5VL_provenance_t *)calloc(1, sizeof(H5VL_provenance_t));
    new_obj->under_object = under_obj;
    new_obj->under_vol_id = under_vol_id;
    new_obj->prov_helper = helper;
    ptr_cnt_increment(new_obj->prov_helper);
    H5Iinc_ref(new_obj->under_vol_id);

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
    ptr_cnt_decrement(PROV_HELPER);
    H5Idec_ref(obj->under_vol_id);

    free(obj);//changed value of PROV_HELPER->opened_files

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
    if(*cmp_value != 0)
        return 0;

    /* Compare under VOL connector info objects */
    H5VLcmp_connector_info(cmp_value, info1->under_vol_id, info1->under_vol_info, info2->under_vol_info);
    if(*cmp_value != 0)
        return 0;

    *cmp_value = strcmp(info1->prov_file_path, info2->prov_file_path);
    if(*cmp_value != 0)
        return 0;

    *cmp_value = strcmp(info1->prov_line_format, info2->prov_line_format);
    if(*cmp_value != 0)
        return 0;

    *cmp_value = (int)info1->prov_level - (int)info2->prov_level;
    if(*cmp_value != 0)
        return 0;

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

herr_t provenance_file_setup(const char* str_in, char* file_path_out, Prov_level* level_out, char* format_out){
    //acceptable format: path=$path_str;level=$level_int;format=$format_str
    char tmp_str[100] = {'\0'};
    char* toklist[4] = {NULL};
    int i;
    char *p;

    memcpy(tmp_str, str_in, strlen(str_in)+1);

    i = 0;
    p = strtok(tmp_str, ";");
    while(p != NULL) {
        toklist[i] = strdup(p);
        p = strtok(NULL, ";");
        i++;
    }

    sscanf(toklist[1], "path=%s", file_path_out);
    sscanf(toklist[2], "level=%d", (int *)level_out);
    sscanf(toklist[3], "format=%s", format_out);

    for(i = 0; i<=3; i++)
        if(toklist[i])
            free(toklist[i]);

    return 0;
}


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
static herr_t
H5VL_provenance_str_to_info(const char *str, void **_info)
{
    H5VL_provenance_info_t *info;
    unsigned under_vol_value;
    const char *under_vol_info_start, *under_vol_info_end;
    hid_t under_vol_id;
    void *under_vol_info = NULL;
    char *under_vol_info_str = NULL;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL INFO String To Info\n");
#endif

    /* Retrieve the underlying VOL connector value and info */
    sscanf(str, "under_vol=%u;", &under_vol_value);
    under_vol_id = H5VLregister_connector_by_value((H5VL_class_value_t)under_vol_value, H5P_DEFAULT);
    under_vol_info_start = strchr(str, '{');
    under_vol_info_end = strrchr(str, '}');
    assert(under_vol_info_end > under_vol_info_start);

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

    info->prov_file_path = (char *)calloc(64, sizeof(char));
    info->prov_line_format = (char *)calloc(64, sizeof(char));

    if(provenance_file_setup(under_vol_info_end, info->prov_file_path, &(info->prov_level), info->prov_line_format) != 0){
        free(info->prov_file_path);
        free(info->prov_line_format);
        info->prov_line_format = NULL;
        info->prov_file_path = NULL;
        info->prov_level = File_only;
    }

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
    void* ret;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL Get object\n");
#endif

    if(!o){
        printf("H5VLget_object() get a NULL o as a parameter.");
    }

    if(!(o->under_object)){
        printf("H5VLget_object() get a NULL o->under_object as a parameter.");
    }

    ret = H5VLget_object(o->under_object, o->under_vol_id);

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

    assert(o->my_type != 0);

    /* Allocate new VOL object wrapping context for the pass through connector */
    new_wrap_ctx = (H5VL_provenance_wrap_ctx_t *)calloc(1, sizeof(H5VL_provenance_wrap_ctx_t));
    switch(o->my_type){
        case H5I_DATASET:
            new_wrap_ctx->root_file_info = ((dataset_prov_info_t*) (o->generic_prov_info))->root_file_info;
            break;

        case H5I_GROUP:
            new_wrap_ctx->root_file_info = ((group_prov_info_t*)(o->generic_prov_info))->root_file_info;
            break;

        case H5I_FILE:
            new_wrap_ctx->root_file_info = (file_prov_info_t*)(o->generic_prov_info);
            break;

        case H5I_DATATYPE:
            //need to feed type_id here
            new_wrap_ctx->root_file_info = ((datatype_prov_info_t*)(o->generic_prov_info))->root_file_info;
            break;

        case H5I_ATTR:
        case H5I_UNINIT:
        case H5I_BADID:
        case H5I_DATASPACE:
        case H5I_VFL:
        case H5I_VOL:
        case H5I_GENPROP_CLS:
        case H5I_GENPROP_LST:
        case H5I_ERROR_CLASS:
        case H5I_ERROR_MSG:
        case H5I_ERROR_STACK:
        case H5I_NTYPES:
        default:
            printf("%s:%d: unexpected type: my_type = %d\n", __func__, __LINE__, (int)o->my_type);
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

    return 0;
} /* end H5VL_provenance_get_wrap_ctx() */

//This function makes up a fake upper layer obj used as a parameter in _obj_wrap_under(..., H5VL_provenance_t* upper_o,... ),
//Use this in H5VL_provenance_wrap_object() ONLY!!!
H5VL_provenance_t* _fake_obj_new(file_prov_info_t* root_file, hid_t under_vol_id){
    H5VL_provenance_t* obj;
    H5I_type_t fake_upper_o_type;

    obj = (H5VL_provenance_t *)calloc(1, sizeof(H5VL_provenance_t));
    fake_upper_o_type = H5I_FILE;  // FILE should work fine as a parent obj for all.
    obj->under_vol_id = under_vol_id;
    obj->my_type = fake_upper_o_type;
    obj->prov_helper = PROV_HELPER;
    switch(fake_upper_o_type){
        case H5I_DATASET: {
            dataset_prov_info_t* fake_info;

            fake_info = (dataset_prov_info_t *)calloc(1, sizeof(dataset_prov_info_t));
            fake_info->root_file_info = root_file;
            obj->generic_prov_info = (void*)fake_info;
            break;
        }

        case H5I_GROUP: {
            group_prov_info_t* fake_info;

            fake_info = (group_prov_info_t *)calloc(1, sizeof(group_prov_info_t));
            fake_info->root_file_info = root_file;
            obj->generic_prov_info = (void*)fake_info;
            break;
        }

        case H5I_FILE:
            obj->generic_prov_info = (void*)root_file;
            break;

        case H5I_DATATYPE:
        case H5I_ATTR:
        case H5I_UNINIT:
        case H5I_BADID:
        case H5I_DATASPACE:
        case H5I_VFL:
        case H5I_VOL:
        case H5I_GENPROP_CLS:
        case H5I_GENPROP_LST:
        case H5I_ERROR_CLASS:
        case H5I_ERROR_MSG:
        case H5I_ERROR_STACK:
        case H5I_NTYPES:
        default:
            printf("%s:%d: Unexpected type for fake_obj! fake_upper_o_type = %d\n",
                    __func__, __LINE__, fake_upper_o_type);
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
static void *
H5VL_provenance_wrap_object(void *under_under_in, H5I_type_t obj_type, void *_wrap_ctx_in)
{
    /* Generic object wrapping, make ctx based on types */
    H5VL_provenance_wrap_ctx_t *wrap_ctx = (H5VL_provenance_wrap_ctx_t *)_wrap_ctx_in;
    void *under;
    H5VL_provenance_t* new_obj;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL WRAP Object\n");
#endif

    /* Wrap the object with the underlying VOL */
    under = H5VLwrap_object(under_under_in, obj_type, wrap_ctx->under_vol_id, wrap_ctx->under_wrap_ctx);

    if(under) {
        H5VL_provenance_t* fake_upper_o;

        fake_upper_o = _fake_obj_new(wrap_ctx->root_file_info, wrap_ctx->under_vol_id);

        new_obj = _obj_wrap_under(under, fake_upper_o, NULL, obj_type, H5P_DEFAULT, NULL);

        _fake_obj_free(fake_upper_o);
    }
    else
        new_obj = NULL;

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

    ret_value = H5VLattr_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

    /* Release our wrapper, if underlying attribute was closed */
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
//    file_prov_info_t* root_file_info;
    void *under;

//    if(o->my_type == H5I_FILE){
//        root_file_info = (file_prov_info_t*)o->generic_prov_info;
//    }else if(o->my_type == H5I_GROUP){
//        root_file_info = ((group_prov_info_t*)o->generic_prov_info)->root_file_info;
//    }else{//error
//        return NULL;
//    }

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Create\n");
#endif

    under = H5VLdataset_create(o->under_object, loc_params, o->under_vol_id, ds_name, dcpl_id,  dapl_id, dxpl_id, req);

    if(under)
        dset = _obj_wrap_under(under, o, ds_name, H5I_DATASET, dxpl_id, req);
    else
        dset = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

    return (void *)dset;
} /* end H5VL_provenance_dataset_create() */

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
    H5I_type_t upper_o_type;

    upper_o_type = upper_o->my_type;
    if (under) {
        H5VL_loc_params_t p;
        H5O_info_t oinfo;
        haddr_t native_addr;
        unsigned long file_no;

        if (target_obj_name != NULL && 0 == strcmp(target_obj_name, ".")) {   //open same dataset.  // "."  case
            //open from types
            switch(upper_o_type){
                case H5I_DATASET:
                    root_file_info = ((dataset_prov_info_t*) (upper_o->generic_prov_info))->root_file_info;
                    break;

                case H5I_GROUP:
                    root_file_info = ((group_prov_info_t*)(upper_o->generic_prov_info))->root_file_info;
                    break;

                case H5I_FILE:
                case H5I_DATATYPE:
                case H5I_ATTR:
                case H5I_UNINIT:
                case H5I_BADID:
                case H5I_DATASPACE:
                case H5I_VFL:
                case H5I_VOL:
                case H5I_GENPROP_CLS:
                case H5I_GENPROP_LST:
                case H5I_ERROR_CLASS:
                case H5I_ERROR_MSG:
                case H5I_ERROR_STACK:
                case H5I_NTYPES:
                default:
                    break;
            }
        } else {// non-. cases, parent obj's type: where it's opened, file or group.
            if(upper_o_type == H5I_FILE){//
                 root_file_info = (file_prov_info_t*)upper_o->generic_prov_info;
             }else if(upper_o_type == H5I_GROUP){
                 root_file_info = ((group_prov_info_t*)upper_o->generic_prov_info)->root_file_info;
             }else{//error
                 return NULL;
             }
        }

        obj = H5VL_provenance_new_obj(under, upper_o->under_vol_id, upper_o->prov_helper);

        /* Check for async request */
        if (req && *req)
            *req = H5VL_provenance_new_obj(*req, upper_o->under_vol_id, upper_o->prov_helper);

        //obj types
        _new_loc_pram(target_obj_type, &p);
        get_native_info(under, upper_o->under_vol_id,
                dxpl_id, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);
        native_addr = oinfo.addr;
        file_no = oinfo.fileno;

        switch (target_obj_type) {
            case H5I_DATASET:
                obj->generic_prov_info = add_dataset_node(file_no, obj, native_addr, root_file_info, target_obj_name, dxpl_id, req);
                obj->my_type = H5I_DATASET;

                file_ds_accessed(root_file_info);
                break;

            case H5I_GROUP:
                obj->generic_prov_info = add_grp_node(root_file_info, native_addr);
                obj->my_type = H5I_GROUP;
                break;

            case H5I_FILE: //newly added. if target_obj_name == NULL: it's a fake upper_o
                obj->generic_prov_info = add_file_node(PROV_HELPER, target_obj_name, file_no);
                obj->my_type = H5I_FILE;
                break;

            case H5I_DATATYPE: //need a linked list for opened data types.
                obj->generic_prov_info = add_dtype_node(PROV_HELPER, native_addr);//and remove in datatype_close().
                obj->my_type = H5I_DATATYPE;
                break;

            case H5I_ATTR:
            case H5I_UNINIT:
            case H5I_BADID:
            case H5I_DATASPACE:
            case H5I_VFL:
            case H5I_VOL:
            case H5I_GENPROP_CLS:
            case H5I_GENPROP_LST:
            case H5I_ERROR_CLASS:
            case H5I_ERROR_MSG:
            case H5I_ERROR_STACK:
            case H5I_NTYPES:
            default:
                break;
        }
    } /* end if */
    else
        obj = NULL;

    return obj;
}


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
static void *
H5VL_provenance_dataset_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *ds_name, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    unsigned long start = get_time_usec();
    void *under;
    H5VL_provenance_t *dset;
    H5VL_provenance_t *o = (H5VL_provenance_t *)obj;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Open\n");
#endif

    under = H5VLdataset_open(o->under_object, loc_params, o->under_vol_id, ds_name, dapl_id, dxpl_id, req);

    if(under)
       dset = _obj_wrap_under(under, o, ds_name, H5I_DATASET, dxpl_id, req);
    else
        dset = NULL;

    if(dset)
        prov_write(dset->prov_helper, __func__, get_time_usec() - start);

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
    unsigned long start;
    unsigned long time;
    H5VL_provenance_t *o = (H5VL_provenance_t *)dset;
    hsize_t r_size;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Read\n");
#endif

    start = get_time_usec();
    ret_value = H5VLdataset_read(o->under_object, o->under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);
    time = get_time_usec() - start;

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(H5S_ALL == mem_space_id)
        r_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * ((dataset_prov_info_t*)o->generic_prov_info)->dset_space_size;
    else
        r_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * (hsize_t)H5Sget_select_npoints(mem_space_id);

    ((dataset_prov_info_t*)(o->generic_prov_info))->total_bytes_read += r_size;
    ((dataset_prov_info_t*)(o->generic_prov_info))->dataset_read_cnt++;
    ((dataset_prov_info_t*)(o->generic_prov_info))->total_read_time += time;

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
    H5VL_provenance_t *o = (H5VL_provenance_t *)dset;
    unsigned long start;
    unsigned long time;
    hsize_t w_size;
    herr_t ret_value;

    assert(dset);

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Write\n");
#endif

    start = get_time_usec();
    ret_value = H5VLdataset_write(o->under_object, o->under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);
    time = get_time_usec() - start;

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    if(H5S_ALL == mem_space_id)
        w_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * ((dataset_prov_info_t*)o->generic_prov_info)->dset_space_size;
    else
        w_size = ((dataset_prov_info_t*)o->generic_prov_info)->dset_type_size * (hsize_t)H5Sget_select_npoints(mem_space_id);

    prov_write(o->prov_helper, __func__, time);

    ((dataset_prov_info_t*)o->generic_prov_info)->total_bytes_written += w_size;
    ((dataset_prov_info_t*)(o->generic_prov_info))->dataset_write_cnt++;
    ((dataset_prov_info_t*)(o->generic_prov_info))->total_write_time += time;

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
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATASET Close\n");
#endif

    ret_value = H5VLdataset_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying dataset was closed */
    if(ret_value >= 0){
        dataset_prov_info_t* dset_info;
        file_prov_info_t* file_info;

        dset_info = (dataset_prov_info_t*)o->generic_prov_info;
        file_info = dset_info->root_file_info;

        dataset_stats_prov_write(dset_info);//output stats

        prov_write(o->prov_helper, __func__, get_time_usec() - start);
        rm_dataset_node(file_info, dset_info->native_addr);

        H5VL_provenance_free_obj(o);
    }

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
        dt = _obj_wrap_under(under, o, name, H5I_DATATYPE, dxpl_id, req);

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
        dt = _obj_wrap_under(under, o, name, H5I_DATATYPE, dxpl_id, req);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
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

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL DATATYPE Close\n");
#endif

    ret_value = H5VLdatatype_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying datatype was closed */
    if(ret_value >= 0){
        datatype_prov_info_t* info;

        info = (datatype_prov_info_t*)(o->generic_prov_info);

        datatype_stats_prov_write(info);
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

        rm_dtype_node(PROV_HELPER, info->native_addr);

        H5VL_provenance_free_obj(o);
    }

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
    unsigned long start;
    H5VL_provenance_info_t *info;
    H5VL_provenance_t *file;
    hid_t under_fapl_id;
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PROVNC VOL FILE Create\n");
#endif


    /* Get copy of our VOL info from FAPL */
    H5Pget_vol_info(fapl_id, (void **)&info);

    /* Copy the FAPL */
    under_fapl_id = H5Pcopy(fapl_id);
//H5Pget_driver(fapl_id) == src/H5FDmpio.h:H5FD_MPIO
//d_info = H5Pget_driver_info(fapl_id);
//d_info is a void* points to a H5FD_MPIO, which contains a MPI_COMM, and can be used later for parallel info collection.
//put the mpi_comm in file_shared


    /* Set the VOL ID and info for the underlying FAPL */
    H5Pset_vol(under_fapl_id, info->under_vol_id, info->under_vol_info);

    start = get_time_usec();
    /* Open the file with the underlying VOL connector */
    under = H5VLfile_create(name, flags, fcpl_id, under_fapl_id, dxpl_id, req);

    if(!PROV_HELPER)
        PROV_HELPER = prov_helper_init(info->prov_file_path, info->prov_level, info->prov_line_format);

    if(under) {
        unsigned long file_no = 0;

        file = H5VL_provenance_new_obj(under, info->under_vol_id, PROV_HELPER);

        file->my_type = H5I_FILE;
        get_native_file_no(&file_no, file);
        file->generic_prov_info = add_file_node(PROV_HELPER, name, file_no);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, info->under_vol_id, PROV_HELPER);
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
} /* end H5VL_provenance_file_create() */

H5VL_provenance_t* _file_open_common(void* under, hid_t vol_id, const char* name){
    H5VL_provenance_t *file;
    unsigned long file_no = 0;

    file = H5VL_provenance_new_obj(under, vol_id, PROV_HELPER);
    file->my_type = H5I_FILE;
    get_native_file_no(&file_no, file);
    file->generic_prov_info = add_file_node(PROV_HELPER, name, file_no);

    return file;
}


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
static void *
H5VL_provenance_file_open(const char *name, unsigned flags, hid_t fapl_id,
    hid_t dxpl_id, void **req)
{
    unsigned long start;
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

    start = get_time_usec();
    /* Open the file with the underlying VOL connector */
    under = H5VLfile_open(name, flags, under_fapl_id, dxpl_id, req);

    if(!PROV_HELPER)
        PROV_HELPER = prov_helper_init(info->prov_file_path, info->prov_level, info->prov_line_format);

    //setup global
    if(under) {
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

                    *ret = _file_open_common(*ret, under_vol_id, file_name);
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

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL FILE Close\n");
#endif

    if(o){
        file_stats_prov_write((file_prov_info_t*)(o->generic_prov_info));

        prov_write(o->prov_helper, __func__, get_time_usec() - start);
    }

    ret_value = H5VLfile_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying file was closed */
    if(ret_value >= 0){
        if(!o->generic_prov_info){//from fake upper_o, no need to remove linkedlist
printf("%s:%d: generic_prov_info is NULL: created from a fake upper_o. \n", __func__, __LINE__);
        }else{
            char* fname = ((file_prov_info_t*)(o->generic_prov_info))->file_name;

            if(o->generic_prov_info)
                rm_file_node(PROV_HELPER, fname);

            H5VL_provenance_free_obj(o);
        }
    }

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
    void *under;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Create\n");
#endif

    if(o->my_type == H5I_FILE){
        root_file_info = (file_prov_info_t*)o->generic_prov_info;
    }else if(o->my_type == H5I_GROUP){
        root_file_info = ((group_prov_info_t*)o->generic_prov_info)->root_file_info;
    }else{//error
        return NULL;
    }
    assert(root_file_info);

    under = H5VLgroup_create(o->under_object, loc_params, o->under_vol_id, name, gcpl_id,  gapl_id, dxpl_id, req);
    if(under) {
        H5VL_loc_params_t p;
        H5O_info_t oinfo;
        haddr_t native_addr;

        group = H5VL_provenance_new_obj(under, o->under_vol_id, o->prov_helper);

        _new_loc_pram(H5I_GROUP, &p);
        get_native_info(under, o->under_vol_id,
                dxpl_id, NULL, H5VL_NATIVE_OBJECT_GET_INFO, &p, &oinfo, H5O_INFO_BASIC);
        native_addr = oinfo.addr;

        group->generic_prov_info = add_grp_node(root_file_info, native_addr);
        group->my_type = H5I_GROUP;

        /* Check for async request */
        if(req && *req)
            *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);
    } /* end if */
    else
        group = NULL;

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

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

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL GROUP Open\n");
#endif

    if (o->my_type == H5I_FILE) {
        file_info = (file_prov_info_t*) o->generic_prov_info;
    } else if (o->my_type == H5I_GROUP) {
        file_info = ((group_prov_info_t*) o->generic_prov_info)->root_file_info;
    } else {    //error
        return NULL;
    }
    assert(file_info);

    under = H5VLgroup_open(o->under_object, loc_params, o->under_vol_id, name, gapl_id, dxpl_id, req);
    if(under) {
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
    unsigned long start = get_time_usec();
    H5VL_provenance_t *o = (H5VL_provenance_t *)grp;
    herr_t ret_value;

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL H5Gclose\n");
#endif

    ret_value = H5VLgroup_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

    /* Release our wrapper, if underlying file was closed */
    printf("%s:%d\n", __func__, __LINE__);
    if(ret_value >= 0){
        group_prov_info_t* grp_info;
        file_prov_info_t* root_file;

        grp_info = (group_prov_info_t*)o->generic_prov_info;
        root_file = grp_info->root_file_info;

        rm_grp_node(root_file, grp_info->native_addr);

        prov_write(o->prov_helper, __func__, get_time_usec() - start);
        group_stats_prov_write(grp_info);

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

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Open\n");
#endif

    under = H5VLobject_open(o->under_object, loc_params, o->under_vol_id,
            obj_to_open_type, dxpl_id, req);

    if(under) {
        const char* obj_name = NULL;

        if(loc_params->type == H5VL_OBJECT_BY_NAME)
            obj_name = loc_params->loc_data.loc_by_name.name;

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

#ifdef ENABLE_PROVNC_LOGGING
    printf("------- PASS THROUGH VOL OBJECT Specific\n");
#endif

    ret_value = H5VLobject_specific(o->under_object, loc_params, o->under_vol_id, specific_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_provenance_new_obj(*req, o->under_vol_id, o->prov_helper);

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

    ret_value = H5VLrequest_free(o->under_object, o->under_vol_id);

    if(o)
        prov_write(o->prov_helper, __func__, get_time_usec() - start);

    if(ret_value >= 0)
        H5VL_provenance_free_obj(o);

    return ret_value;
} /* end H5VL_provenance_request_free() */

