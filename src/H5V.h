#pragma once
#include "mpi.h"
#include "H5public.h"

// Timer IDs
// NOTE: macro dependency can't be too long to overflow the stack



#define EVAL_TIMER_H5MM_malloc 0
#define EVAL_TIMER_read (EVAL_TIMER_H5MM_malloc + 1)
#define EVAL_TIMER_pread (EVAL_TIMER_read + 1)
#define EVAL_TIMER_write (EVAL_TIMER_pread + 1)
#define EVAL_TIMER_pwrite (EVAL_TIMER_write + 1)
#define EVAL_TIMER_MPI_Allgather (EVAL_TIMER_pwrite + 1)
#define EVAL_TIMER_MPI_Allgatherv (EVAL_TIMER_MPI_Allgather + 1)
#define EVAL_TIMER_MPI_Allreduce (EVAL_TIMER_MPI_Allgatherv + 1)
#define EVAL_TIMER_MPI_Bcast (EVAL_TIMER_MPI_Allreduce + 1)
#define EVAL_TIMER_MPI_Gather (EVAL_TIMER_MPI_Bcast + 1)
#define EVAL_TIMER_MPI_Gatherv (EVAL_TIMER_MPI_Gather + 1)
#define EVAL_TIMER_MPI_Send (EVAL_TIMER_MPI_Gatherv + 1)
#define EVAL_TIMER_MPI_Isend (EVAL_TIMER_MPI_Send + 1)
#define EVAL_TIMER_MPI_Recv (EVAL_TIMER_MPI_Isend + 1)
#define EVAL_TIMER_MPI_Imrecv (EVAL_TIMER_MPI_Recv + 1)
#define EVAL_TIMER_MPI_Mprobe (EVAL_TIMER_MPI_Imrecv + 1)
#define EVAL_TIMER_MPI_File_read_at (EVAL_TIMER_MPI_Mprobe + 1)
#define EVAL_TIMER_MPI_File_read_at_all (EVAL_TIMER_MPI_File_read_at + 1)
#define EVAL_TIMER_MPI_File_write_at (EVAL_TIMER_MPI_File_read_at_all + 1)
#define EVAL_TIMER_MPI_File_write_at_all (EVAL_TIMER_MPI_File_write_at + 1)
#define EVAL_TIMER_MPI_File_set_view (EVAL_TIMER_MPI_File_write_at_all + 1)
#define EVAL_TIMER_MPI_Waitall (EVAL_TIMER_MPI_File_set_view + 1)
#define EVAL_TIMER_H5Fcreate 22 // Must be manually set to (EVAL_TIMER_MPI_Waitall + 1). Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5Fopen (EVAL_TIMER_H5Fcreate + 1)
#define EVAL_TIMER_H5Fclose (EVAL_TIMER_H5Fopen + 1)
#define EVAL_TIMER_H5Gcreate (EVAL_TIMER_H5Fclose + 1)
#define EVAL_TIMER_H5Gopen (EVAL_TIMER_H5Gcreate + 1)
#define EVAL_TIMER_H5Gclose (EVAL_TIMER_H5Gopen + 1)
#define EVAL_TIMER_H5Dcreate (EVAL_TIMER_H5Gclose + 1)
#define EVAL_TIMER_H5D__create_named (EVAL_TIMER_H5Dcreate + 1)
#define EVAL_TIMER_H5L_link_object_dataset (EVAL_TIMER_H5D__create_named + 1)
#define EVAL_TIMER_H5L__create_real_dataset (EVAL_TIMER_H5L_link_object_dataset + 1)
#define EVAL_TIMER_H5L__link_cb_dataset (EVAL_TIMER_H5L__create_real_dataset + 1)
#define EVAL_TIMER_H5O_obj_create_dataset (EVAL_TIMER_H5L__link_cb_dataset + 1)
#define EVAL_TIMER_H5O__dset_create (EVAL_TIMER_H5O_obj_create_dataset + 1)
#define EVAL_TIMER_H5D__create (EVAL_TIMER_H5O__dset_create + 1)
#define EVAL_TIMER_H5D__create_metadata (EVAL_TIMER_H5D__create + 1)
#define EVAL_TIMER_H5D__create_property (EVAL_TIMER_H5D__create_metadata + 1)
#define EVAL_TIMER_H5D__chunk_construct (EVAL_TIMER_H5D__create_property + 1)
#define EVAL_TIMER_H5D__update_oh_info (EVAL_TIMER_H5D__chunk_construct + 1)
#define EVAL_TIMER_H5D__layout_oh_create (EVAL_TIMER_H5D__update_oh_info + 1)
#define EVAL_TIMER_H5D__alloc_storage (EVAL_TIMER_H5D__layout_oh_create + 1)
#define EVAL_TIMER_H5D__init_storage (EVAL_TIMER_H5D__alloc_storage + 1)
#define EVAL_TIMER_H5D__chunk_allocate (EVAL_TIMER_H5D__init_storage + 1)
#define EVAL_TIMER_H5D__chunk_collective_fill (EVAL_TIMER_H5D__chunk_allocate + 1)
#define EVAL_TIMER_H5G_obj_insert_dataset (EVAL_TIMER_H5D__chunk_collective_fill + 1)
#define EVAL_TIMER_H5Dopen 46 // Must be manually set to (EVAL_TIMER_H5G_obj_insert_dataset + 1). Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5Dclose (EVAL_TIMER_H5Dopen + 1)
#define EVAL_TIMER_H5Acreate (EVAL_TIMER_H5Dclose + 1)
#define EVAL_TIMER_H5Aopen (EVAL_TIMER_H5Acreate + 1)
#define EVAL_TIMER_H5Aclose (EVAL_TIMER_H5Aopen + 1)
#define EVAL_TIMER_H5Dwrite 51 // Must be manually set to (EVAL_TIMER_H5Aclose + 1). Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5D__write (EVAL_TIMER_H5Dwrite + 1)
#define EVAL_TIMER_H5D__chunk_io_init_w (EVAL_TIMER_H5D__write + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_w (EVAL_TIMER_H5D__chunk_io_init_w + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_Reset_w (EVAL_TIMER_H5D__ioinfo_adjust_w + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_Get_comm_w (EVAL_TIMER_H5D__ioinfo_adjust_Reset_w + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_Chk_coll_w (EVAL_TIMER_H5D__ioinfo_adjust_Get_comm_w + 1)
#define EVAL_TIMER_H5D__chunk_collective_write (EVAL_TIMER_H5D__ioinfo_adjust_Chk_coll_w + 1)
#define EVAL_TIMER_H5D__chunk_collective_io_w (EVAL_TIMER_H5D__chunk_collective_write + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_w (EVAL_TIMER_H5D__chunk_collective_io_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Init_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_w + 1)
#define EVAL_TIMER_H5D__construct_filtered_io_info_list_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Init_w + 1)
#define EVAL_TIMER_H5D__chunk_redistribute_shared_chunks (EVAL_TIMER_H5D__construct_filtered_io_info_list_w + 1)
#define EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Chunk_assignment (EVAL_TIMER_H5D__chunk_redistribute_shared_chunks + 1)
#define EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Data_exchange (EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Chunk_assignment + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_w 66 // Must be manually set to (EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Data_exchange + 1). Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5F_block_read_fcoll_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_w (EVAL_TIMER_H5F_block_read_fcoll_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Self_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Waitall_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Self_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Unpack_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Waitall_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Unpack_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Gather_chunk_size_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Chunk_Alloc_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Gather_chunk_size_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Gather_num_entry_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Chunk_Alloc_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Type_Create_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Gather_num_entry_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_collective_io_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Type_Create_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Update_Index_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_collective_io_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Finalize_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Update_Index_w + 1)
#define EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Finalize_w + 1)
#define EVAL_TIMER_H5D__contig_collective_write (EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_w + 1)
#define EVAL_TIMER_H5D__inter_collective_io_w (EVAL_TIMER_H5D__contig_collective_write + 1)
#define EVAL_TIMER_H5D__inter_collective_io_collective_io_w (EVAL_TIMER_H5D__inter_collective_io_w + 1)
#define EVAL_TIMER_H5D__final_collective_io_w (EVAL_TIMER_H5D__inter_collective_io_collective_io_w + 1)
#define EVAL_TIMER_H5Dread 85 // Must be manually set to (EVAL_TIMER_H5D__final_collective_io_w + 1). Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5D__read (EVAL_TIMER_H5Dread + 1)
#define EVAL_TIMER_H5D__read_check_arg (EVAL_TIMER_H5D__read + 1)
#define EVAL_TIMER_H5D__chunk_io_init_r (EVAL_TIMER_H5D__read_check_arg + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_r (EVAL_TIMER_H5D__chunk_io_init_r + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_Reset_r (EVAL_TIMER_H5D__ioinfo_adjust_r + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_Get_comm_r (EVAL_TIMER_H5D__ioinfo_adjust_Reset_r + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_Chk_coll_r (EVAL_TIMER_H5D__ioinfo_adjust_Get_comm_r + 1)
#define EVAL_TIMER_H5D__chunk_collective_read (EVAL_TIMER_H5D__ioinfo_adjust_Chk_coll_r + 1)
#define EVAL_TIMER_H5D__chunk_collective_io_r (EVAL_TIMER_H5D__chunk_collective_read + 1)
#define EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_r (EVAL_TIMER_H5D__chunk_collective_io_r + 1)
#define EVAL_TIMER_H5D__construct_filtered_io_info_list_r (EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_r + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_r (EVAL_TIMER_H5D__construct_filtered_io_info_list_r + 1)
#define EVAL_TIMER_H5F_block_read_fcoll_r (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_r + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_r (EVAL_TIMER_H5F_block_read_fcoll_r + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Self_r (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_r + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_r (EVAL_TIMER_H5D__chunk_collective_io_r + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Init_r (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_r + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Finalize_r (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Init_r + 1)
#define EVAL_TIMER_H5D__chunk_read 104 // Must be manually set to (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Finalize_r + 1). Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5D__chunk_lookup_r (EVAL_TIMER_H5D__chunk_read + 1)
#define EVAL_TIMER_H5D__chunk_lock_r (EVAL_TIMER_H5D__chunk_lookup_r + 1)
#define EVAL_TIMER_H5F_block_read_lock_r (EVAL_TIMER_H5D__chunk_lock_r + 1)
#define EVAL_TIMER_H5D__chunk_lock_filter_r (EVAL_TIMER_H5F_block_read_lock_r + 1)
#define EVAL_TIMER_H5D__select_read (EVAL_TIMER_H5D__chunk_lock_filter_r + 1)
#define EVAL_TIMER_H5D__chunk_unlock_r (EVAL_TIMER_H5D__select_read + 1)
#define EVAL_TIMER_H5D__set_extent (EVAL_TIMER_H5D__chunk_unlock_r + 1)
#define EVAL_TIMER_H5D__chunk_prune_by_extent (EVAL_TIMER_H5D__set_extent + 1)
#define EVAL_TIMER_H5Ovisit (EVAL_TIMER_H5D__chunk_prune_by_extent + 1)
#define EVAL_TIMER_H5Ovisit2 (EVAL_TIMER_H5Ovisit + 1)
#define EVAL_TIMER_H5Z_filter_deflate_comp (EVAL_TIMER_H5Ovisit2 + 1)
#define EVAL_TIMER_H5Z_filter_deflate_decomp (EVAL_TIMER_H5Z_filter_deflate_comp + 1)

#define EVAL_NTIMER (EVAL_TIMER_H5Z_filter_deflate_decomp + 1)
#define EVAL_NMPI (EVAL_TIMER_MPI_Waitall + 1)

// For future expansion
// Due to implementation decision by HDF5, these function are only called on read or write
// We leave the timer instance for the other operation in case things change in newer version
#define EVAL_TIMER_DUMMY 150 // Must be manually set to anything > EVAL_NTIMER. Cannot use macro expansion due to stack overflow in compiler
#define EVAL_TIMER_H5D__chunk_lock_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__chunk_unlock_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5F_block_read_lock_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__chunk_lock_filter_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__final_collective_io_r EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__inter_collective_io_r EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__inter_collective_io_collective_io_r EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L_link_object_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L__create_real_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L__link_cb_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5O_obj_create_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L_link_object_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L__create_real_other EVAL_TIMER_DUMMY


void eval_add_time(int id, double t);
void eval_add_size(int id, int count, MPI_Datatype type);
void eval_add_size2(int id, size_t size);
int HDF_MPI_EVAL_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm );
void H5V_ShowHints(MPI_Info *mpiHints);
double HDF_EVAL_wtime();