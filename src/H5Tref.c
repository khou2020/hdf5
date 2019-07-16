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
 * Module Info: This module contains the functionality for reference
 *      datatypes in the H5T interface.
 */

#include "H5Tmodule.h"          /* This source code file is part of the H5T module */
#define H5F_FRIEND              /*suppress error about including H5Fpkg   */
#define H5R_FRIEND              /*suppress error about including H5Rpkg   */

#include "H5private.h"          /* Generic Functions    */
#include "H5CXprivate.h"        /* API Contexts         */
#include "H5Eprivate.h"         /* Error handling       */
#include "H5Iprivate.h"         /* IDs                  */
#include "H5MMprivate.h"        /* Memory management    */
#include "H5HGprivate.h"        /* Global Heaps         */
#include "H5Fpkg.h"             /* File                 */
#include "H5Rpkg.h"             /* References           */
#include "H5Tpkg.h"             /* Datatypes            */

/****************/
/* Local Macros */
/****************/

#define H5T_REF_MEM_SIZE            (H5R_REF_BUF_SIZE)
#define H5T_REF_OBJ_MEM_SIZE        (H5R_OBJ_REF_BUF_SIZE)
#define H5T_REF_DSETREG_MEM_SIZE    (H5R_DSET_REG_REF_BUF_SIZE)

/* Size of element on disk is 4 bytes for the size, plus the size
 * of an address in this file, plus 4 bytes for the size of a heap ID
 */
#define H5T_REF_DISK_SIZE(f)            ((2 * H5_SIZEOF_UINT32_T) + (size_t)H5F_SIZEOF_ADDR(f))
#define H5T_REF_OBJ_DISK_SIZE(f)        ((size_t)H5F_SIZEOF_ADDR(f))
#define H5T_REF_DSETREG_DISK_SIZE(f)    (H5_SIZEOF_UINT32_T + (size_t)H5F_SIZEOF_ADDR(f))

/******************/
/* Local Typedefs */
/******************/

/* For region compatibility support */
struct H5Tref_dsetreg {
    haddr_t obj_addr;   /* Object address */
    H5S_t *space;       /* Dataspace */
};

/********************/
/* Local Prototypes */
/********************/

static size_t H5T__ref_mem_getsize(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f);
static herr_t H5T__ref_mem_read(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f, void *dst_buf, size_t dst_size);
static herr_t H5T__ref_mem_write(H5F_t *src_f, const void *src_buf, size_t src_size, H5R_type_t src_type, H5F_t *dst_f, void *dst_buf, size_t dst_size, void *bg_buf);

static size_t H5T__ref_disk_getsize(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f);
static herr_t H5T__ref_disk_read(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f, void *dst_buf, size_t dst_size);
static herr_t H5T__ref_disk_write(H5F_t *src_f, const void *src_buf, size_t src_size, H5R_type_t src_type, H5F_t *dst_f, void *dst_buf, size_t dst_size, void *bg_buf);

/* For compatibility */
static size_t H5T__ref_obj_disk_getsize(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f);
static herr_t H5T__ref_obj_disk_read(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f, void *dst_buf, size_t dst_size);

static size_t H5T__ref_dsetreg_disk_getsize(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f);
static herr_t H5T__ref_dsetreg_disk_read(H5F_t *src_f, const void *src_buf, size_t src_size, H5F_t *dst_f, void *dst_buf, size_t dst_size);

/*******************/
/* Local Variables */
/*******************/

/*-------------------------------------------------------------------------
 * Function: H5T__ref_set_loc
 *
 * Purpose:	Sets the location of a reference datatype to be either on disk
 *          or in memory
 *
 * Return:
 *  One of two values on success:
 *      TRUE - If the location of any reference types changed
 *      FALSE - If the location of any reference types is the same
 *  Negative value is returned on failure
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5T__ref_set_loc(const H5T_t *dt, H5F_t *f, H5T_loc_t loc)
{
    htri_t ret_value = FALSE; /* Indicate success, but no location change */

    FUNC_ENTER_PACKAGE

    HDassert(dt);
    /* f is NULL when loc == H5T_LOC_MEMORY */
    HDassert(loc >= H5T_LOC_BADLOC && loc < H5T_LOC_MAXLOC);

    /* Only change the location if it's different */
    if(loc == dt->shared->u.atomic.u.r.loc && f == dt->shared->u.atomic.u.r.f)
        HGOTO_DONE(FALSE)

    switch(loc) {
        case H5T_LOC_MEMORY: /* Memory based reference datatype */
            HDassert(NULL == f);

            /* Mark this type as being stored in memory */
            dt->shared->u.atomic.u.r.loc = H5T_LOC_MEMORY;

            /* Reset file ID (since this reference is in memory) */
            dt->shared->u.atomic.u.r.f = f;     /* f is NULL */

            if(dt->shared->u.atomic.u.r.opaque) {
                /* Size in memory, disk size is different */
                dt->shared->size = H5T_REF_MEM_SIZE;

                /* Set up the function pointers to access the reference in memory */
                dt->shared->u.atomic.u.r.getsize = H5T__ref_mem_getsize;
                dt->shared->u.atomic.u.r.read = H5T__ref_mem_read;
                dt->shared->u.atomic.u.r.write = H5T__ref_mem_write;

            } else if(dt->shared->u.atomic.u.r.rtype == H5R_OBJECT1) {
                /* Size in memory, disk size is different */
                dt->shared->size = H5T_REF_OBJ_MEM_SIZE;

                /* Unused for now */
                dt->shared->u.atomic.u.r.getsize = NULL;
                dt->shared->u.atomic.u.r.read = NULL;
                dt->shared->u.atomic.u.r.write = NULL;

            } else if(dt->shared->u.atomic.u.r.rtype == H5R_DATASET_REGION1) {
                /* Size in memory, disk size is different */
                dt->shared->size = H5T_REF_DSETREG_MEM_SIZE;

                /* Unused for now */
                dt->shared->u.atomic.u.r.getsize = NULL;
                dt->shared->u.atomic.u.r.read = NULL;
                dt->shared->u.atomic.u.r.write = NULL;
            }
            break;

        case H5T_LOC_DISK: /* Disk based reference datatype */
            HDassert(f);

            /* Mark this type as being stored on disk */
            dt->shared->u.atomic.u.r.loc = H5T_LOC_DISK;

            /* Set file pointer (since this reference is on disk) */
            dt->shared->u.atomic.u.r.f = f;

            if(dt->shared->u.atomic.u.r.rtype == H5R_OBJECT1) {
                /* Size on disk, memory size is different */
                dt->shared->size = H5T_REF_OBJ_DISK_SIZE(f);

                /* Set up the function pointers to access the reference in memory */
                dt->shared->u.atomic.u.r.getsize = H5T__ref_obj_disk_getsize;
                dt->shared->u.atomic.u.r.read = H5T__ref_obj_disk_read;
                dt->shared->u.atomic.u.r.write = NULL;

            } else if(dt->shared->u.atomic.u.r.rtype == H5R_DATASET_REGION1) {
                /* Size on disk, memory size is different */
                dt->shared->size = H5T_REF_DSETREG_DISK_SIZE(f);

                /* Set up the function pointers to access the reference in memory */
                dt->shared->u.atomic.u.r.getsize = H5T__ref_dsetreg_disk_getsize;
                dt->shared->u.atomic.u.r.read = H5T__ref_dsetreg_disk_read;
                dt->shared->u.atomic.u.r.write = NULL;

            } else {
                /* Size on disk, memory size is different */
                dt->shared->size = H5T_REF_DISK_SIZE(f);

                /* Set up the function pointers to access the information on
                 * disk. Region and attribute references are stored identically
                 * on disk, so use the same functions.
                 */
                dt->shared->u.atomic.u.r.getsize = H5T__ref_disk_getsize;
                dt->shared->u.atomic.u.r.read = H5T__ref_disk_read;
                dt->shared->u.atomic.u.r.write = H5T__ref_disk_write;
            }
            break;

        case H5T_LOC_BADLOC:
        case H5T_LOC_MAXLOC:
        default:
            HGOTO_ERROR(H5E_DATATYPE, H5E_BADRANGE, FAIL, "invalid reference datatype location")
    } /* end switch */

    /* Indicate that the location changed */
    ret_value = TRUE;

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_set_loc() */


/*-------------------------------------------------------------------------
 * Function:	H5T__ref_mem_getsize
 *
 * Purpose:	Retrieves the size of a memory based reference.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static size_t
H5T__ref_mem_getsize(H5F_t H5_ATTR_UNUSED *src_f, const void *src_buf,
    size_t H5_ATTR_UNUSED src_size, H5F_t *dst_f)
{
    const struct href *src_ref = (const struct href *)src_buf;
    unsigned flags = 0;
    size_t ret_value = 0;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_buf);
    HDassert(src_size == H5T_REF_MEM_SIZE);

    /* Set external flag is referenced file is not destination file */
    flags |= (0 != HDstrcmp(H5F_OPEN_NAME(dst_f), H5R_REF_FILENAME(src_ref)))
        ? H5R_IS_EXTERNAL : 0;

    /* Force re-calculating encoding size if any flags are set */
    if(flags || !src_ref->encode_size) {
        /* Pass the correct encoding version for the selection depending on the
         * file libver bounds, this is later retrieved in H5S hyper encode */
        H5CX_set_libver_bounds(dst_f);

        /* Cache encoding size */
        if(H5R__encode(src_ref, NULL, (size_t *)&src_ref->encode_size, flags) < 0)
            HGOTO_ERROR(H5E_REFERENCE, H5E_CANTENCODE, 0, "unable to determine encoding size")
    }

    /* Get encoding size */
    ret_value = src_ref->encode_size;

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_mem_getsize() */


/*-------------------------------------------------------------------------
 * Function:	H5T__ref_mem_read
 *
 * Purpose:	"Reads" the memory based reference into a buffer
 *
 * Return:	Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5T__ref_mem_read(H5F_t H5_ATTR_UNUSED *src_f, const void *src_buf,
    size_t H5_ATTR_UNUSED src_size, H5F_t *dst_f, void *dst_buf,
    size_t dst_size)
{
    const struct href *src_ref = (const struct href *)src_buf;
    unsigned flags = 0;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_buf);
    HDassert(src_size == H5T_REF_MEM_SIZE);
    HDassert(dst_f);
    HDassert(dst_buf);
    HDassert(dst_size);

    /* Pass the correct encoding version for the selection depending on the
     * file libver bounds, this is later retrieved in H5S hyper encode */
    H5CX_set_libver_bounds(dst_f);

    /* Set external flag is referenced file is not destination file */
    if(0 != HDstrcmp(H5F_OPEN_NAME(dst_f), H5R_REF_FILENAME(src_ref)))
        flags |= H5R_IS_EXTERNAL;

    /* Encode reference */
    if(H5R__encode(src_ref, dst_buf, &dst_size, flags) < 0)
        HGOTO_ERROR(H5E_REFERENCE, H5E_CANTENCODE, FAIL, "Cannot encode reference")

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_mem_read() */


/*-------------------------------------------------------------------------
 * Function:	H5T__ref_mem_write
 *
 * Purpose:	"Writes" the memory reference from a buffer
 *
 * Return:	Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5T__ref_mem_write(H5F_t *src_f, const void *src_buf, size_t src_size,
    H5R_type_t src_type, H5F_t H5_ATTR_UNUSED *dst_f, void *dst_buf,
    size_t dst_size, void H5_ATTR_UNUSED *bg_buf)
{
    struct href *dst_ref = (struct href *)dst_buf;
    hid_t loc_id = H5I_INVALID_HID;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_f);
    HDassert(src_buf);
    HDassert(src_size);
    HDassert(dst_buf);
    HDassert(dst_size == H5T_REF_MEM_SIZE);

    /* Make sure reference buffer is correctly initialized */
    HDmemset(dst_buf, 0, dst_size);

    switch(src_type) {
        case H5R_OBJECT1: {
            const haddr_t *obj_addr_ptr = (const haddr_t *)src_buf;

            if(H5R__create_object(H5F_OPEN_NAME(src_f), *obj_addr_ptr, dst_ref) < 0)
                HGOTO_ERROR(H5E_REFERENCE, H5E_CANTCREATE, FAIL, "unable to create object reference")
        }
            break;
        case H5R_DATASET_REGION1: {
            const struct H5Tref_dsetreg *src_reg = (const struct H5Tref_dsetreg *)src_buf;

            if(H5R__create_region(H5F_OPEN_NAME(src_f), src_reg->obj_addr, src_reg->space, dst_ref) < 0)
                HGOTO_ERROR(H5E_REFERENCE, H5E_CANTCREATE, FAIL, "unable to create region reference")
            /* create_region creates its internal copy of the space */
            if(H5S_close(src_reg->space) < 0)
                HGOTO_ERROR(H5E_REFERENCE, H5E_CANTFREE, FAIL, "Cannot close dataspace")
        }
            break;
        case H5R_OBJECT2:
        case H5R_DATASET_REGION2:
        case H5R_ATTR:
            /* Decode reference */
            if(H5R__decode(H5F_OPEN_NAME(src_f), (const unsigned char *)src_buf, &src_size, dst_ref) < 0)
                HGOTO_ERROR(H5E_REFERENCE, H5E_CANTDECODE, FAIL, "Cannot decode reference")
            break;
        case H5R_BADTYPE:
        case H5R_MAXTYPE:
        default:
            HDassert("unknown reference type" && 0);
            HGOTO_ERROR(H5E_REFERENCE, H5E_UNSUPPORTED, FAIL, "internal error (unknown reference type)")
    }

    /* Set location ID if reference filename matches */
    if(0 == HDstrcmp(H5F_OPEN_NAME(src_f), H5R_REF_FILENAME(dst_ref))) {
        /* Retrieve loc ID */
        if((loc_id = H5F__get_file_id(src_f)) < 0)
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object")

        /* Attach loc ID to reference */
        if(H5R__set_loc_id(dst_ref, loc_id) < 0)
            HGOTO_ERROR(H5E_REFERENCE, H5E_CANTSET, FAIL, "unable to attach location id to reference")
    }

done:
    if((loc_id != H5I_INVALID_HID) && (H5I_dec_ref(loc_id) < 0))
        HDONE_ERROR(H5E_REFERENCE, H5E_CANTDEC, FAIL, "unable to decrement refcount on location id")
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_mem_write() */


/*-------------------------------------------------------------------------
 * Function:	H5T__ref_disk_getsize
 *
 * Purpose:	Retrieves the length of a disk based reference.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static size_t
H5T__ref_disk_getsize(H5F_t *src_f, const void *src_buf,
    size_t H5_ATTR_UNUSED src_size, H5F_t H5_ATTR_UNUSED *dst_f)
{
    const uint8_t *p = (const uint8_t *)src_buf;
    size_t ret_value = 0;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    HDassert(src_buf);
    HDassert(src_size == H5T_REF_DISK_SIZE(src_f));

    /* Retrieve encoded data size */
    UINT32DECODE(p, ret_value);

    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_disk_getsize() */


/*-------------------------------------------------------------------------
 * Function:	H5T__ref_disk_read
 *
 * Purpose:	Reads the disk based reference into a buffer
 *
 * Return:	Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5T__ref_disk_read(H5F_t *src_f, const void *src_buf, size_t src_size,
    H5F_t H5_ATTR_UNUSED *dst_f, void *dst_buf, size_t dst_size)
{
    const uint8_t *p = (const uint8_t *)src_buf;
    size_t buf_size_left = src_size;
    size_t expected_size = dst_size;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_f);
    HDassert(src_buf);
    HDassert(src_size == H5T_REF_DISK_SIZE(src_f));
    HDassert(dst_buf);
    HDassert(dst_size);

    /* Skip the length of the sequence */
    p += H5_SIZEOF_UINT32_T;
    HDassert(buf_size_left > H5_SIZEOF_UINT32_T);
    buf_size_left -= H5_SIZEOF_UINT32_T;

    /* Decode from heap */
    if(H5R__decode_heap(src_f, p, &buf_size_left, (unsigned char **)&dst_buf, &dst_size) < 0)
        HGOTO_ERROR(H5E_REFERENCE, H5E_CANTDECODE, FAIL, "Cannot decode reference from heap")

    if(dst_size != expected_size)
        HGOTO_ERROR(H5E_REFERENCE, H5E_CANTDECODE, FAIL, "Expected data size does not match")
done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_disk_read() */


/*-------------------------------------------------------------------------
 * Function:	H5T__ref_disk_write
 *
 * Purpose:	Writes the disk based reference from a buffer
 *
 * Return:	Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5T__ref_disk_write(H5F_t H5_ATTR_UNUSED *src_f, const void *src_buf,
    size_t src_size, H5R_type_t H5_ATTR_UNUSED src_type, H5F_t *dst_f,
    void *dst_buf, size_t dst_size, void *bg_buf)
{
    uint8_t *p = (uint8_t *)dst_buf;
    size_t buf_size_left = dst_size;
    uint8_t *p_bg = (uint8_t *)bg_buf;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_buf);
    HDassert(src_size);
    HDassert(dst_f);
    HDassert(dst_buf);
    HDassert(dst_size == H5T_REF_DISK_SIZE(dst_f));

    /* TODO Should get rid of bg stuff */
    if(p_bg) {
        size_t p_buf_size_left = dst_size;

        /* Skip the length of the reference */
        p_bg += H5_SIZEOF_UINT32_T;
        HDassert(p_buf_size_left > H5_SIZEOF_UINT32_T);
        p_buf_size_left -= H5_SIZEOF_UINT32_T;

        /* Free heap object for old data */
        if(H5R__free_heap(dst_f, p_bg, p_buf_size_left) < 0)
            HGOTO_ERROR(H5E_REFERENCE, H5E_CANTFREE, FAIL, "Cannot free reference from heap")
    } /* end if */

    /* Set the size */
    UINT32ENCODE(p, src_size);
    HDassert(buf_size_left > H5_SIZEOF_UINT32_T);
    buf_size_left -= H5_SIZEOF_UINT32_T;

    /* Encode to heap */
    if(H5R__encode_heap(dst_f, p, &buf_size_left, src_buf, src_size) < 0)
        HGOTO_ERROR(H5E_REFERENCE, H5E_CANTENCODE, FAIL, "Cannot encode reference to heap")

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_disk_write() */


/*-------------------------------------------------------------------------
 * Function:    H5T__ref_obj_disk_getsize
 *
 * Purpose: Retrieves the length of a disk based reference.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static size_t
H5T__ref_obj_disk_getsize(H5F_t H5_ATTR_UNUSED *src_f,
    const void H5_ATTR_UNUSED *src_buf, size_t H5_ATTR_UNUSED src_size,
    H5F_t H5_ATTR_UNUSED *dst_f)
{
    size_t ret_value = sizeof(haddr_t);

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    HDassert(src_buf);
    HDassert(src_size == H5T_REF_OBJ_DISK_SIZE(src_f));

    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_obj_disk_getsize() */


/*-------------------------------------------------------------------------
 * Function:    H5T__ref_obj_disk_read
 *
 * Purpose: Reads the disk based reference into a buffer
 *
 * Return:  Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5T__ref_obj_disk_read(H5F_t H5_ATTR_UNUSED *src_f, const void *src_buf,
    size_t src_size, H5F_t H5_ATTR_UNUSED *dst_f, void *dst_buf,
    size_t dst_size)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_f);
    HDassert(src_buf);
    HDassert(src_size == H5T_REF_OBJ_DISK_SIZE(src_f));
    HDassert(dst_buf);
    HDassert(dst_size == sizeof(haddr_t));

    /* Get object address */
    if(H5R__decode_obj_addr_compat((const unsigned char *)src_buf, &src_size, (haddr_t *)dst_buf) < 0)
        HGOTO_ERROR(H5E_REFERENCE, H5E_CANTDECODE, H5I_INVALID_HID, "unable to get object address")

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_obj_disk_read() */


/*-------------------------------------------------------------------------
 * Function:    H5T__ref_dsetreg_disk_getsize
 *
 * Purpose: Retrieves the length of a disk based reference.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static size_t
H5T__ref_dsetreg_disk_getsize(H5F_t H5_ATTR_UNUSED *f,
    const void H5_ATTR_UNUSED *buf, size_t H5_ATTR_UNUSED buf_size,
    H5F_t H5_ATTR_UNUSED *dst_f)
{
    size_t ret_value = sizeof(struct H5Tref_dsetreg);

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    HDassert(buf);
    HDassert(buf_size == H5T_REF_DSETREG_DISK_SIZE(f));

    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_dsetreg_disk_getsize() */


/*-------------------------------------------------------------------------
 * Function:    H5T__ref_dsetreg_disk_read
 *
 * Purpose: Reads the disk based reference into a buffer
 *
 * Return:  Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5T__ref_dsetreg_disk_read(H5F_t *src_f, const void *src_buf, size_t src_size,
    H5F_t H5_ATTR_UNUSED *dst_f, void *dst_buf, size_t H5_ATTR_UNUSED dst_size)
{
    struct H5Tref_dsetreg *dst_reg = (struct H5Tref_dsetreg *)dst_buf;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    HDassert(src_f);
    HDassert(src_buf);
    HDassert(src_size == H5T_REF_DSETREG_DISK_SIZE(src_f));
    HDassert(dst_buf);
    HDassert(dst_size == sizeof(struct H5Tref_dsetreg));

    /* Retrieve object address and space */
    if(H5R__decode_addr_region_compat(src_f, (const unsigned char *)src_buf, &src_size, &dst_reg->obj_addr, &dst_reg->space) < 0)
        HGOTO_ERROR(H5E_REFERENCE, H5E_CANTDECODE, FAIL, "unable to get object address")

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5T__ref_dsetreg_disk_read() */
