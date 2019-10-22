#pragma once



// NOTE: macro dependency can't be too long to overflow the stack




#define EVAL_TIMER_H5Fcreate 0
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
#define EVAL_TIMER_H5G_obj_insert_dataset (EVAL_TIMER_H5O_obj_create_dataset + 1)
#define EVAL_TIMER_H5Dopen (EVAL_TIMER_H5G_obj_insert_dataset + 1)
#define EVAL_TIMER_H5Dclose (EVAL_TIMER_H5Dopen + 1)
#define EVAL_TIMER_H5Acreate (EVAL_TIMER_H5Dclose + 1)
#define EVAL_TIMER_H5Aopen (EVAL_TIMER_H5Acreate + 1)
#define EVAL_TIMER_H5Aclose (EVAL_TIMER_H5Aopen + 1)
#define EVAL_TIMER_H5Dwrite 18
#define EVAL_TIMER_H5D__write (EVAL_TIMER_H5Dwrite + 1)
#define EVAL_TIMER_H5D__chunk_io_init_w (EVAL_TIMER_H5D__write + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_w (EVAL_TIMER_H5D__chunk_io_init_w + 1)
#define EVAL_TIMER_H5D__chunk_collective_write (EVAL_TIMER_H5D__ioinfo_adjust_w + 1)
#define EVAL_TIMER_H5D__chunk_collective_io_w (EVAL_TIMER_H5D__chunk_collective_write + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_w (EVAL_TIMER_H5D__chunk_collective_io_w + 1)
#define EVAL_TIMER_H5D__construct_filtered_io_info_list_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_w + 1)
#define EVAL_TIMER_H5D__chunk_redistribute_shared_chunks (EVAL_TIMER_H5D__construct_filtered_io_info_list_w + 1)
#define EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Chunk_assignment (EVAL_TIMER_H5D__chunk_redistribute_shared_chunks + 1)
#define EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Data_exchange (EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Chunk_assignment + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_w (EVAL_TIMER_H5D__chunk_redistribute_shared_chunks_Data_exchange + 1)
#define EVAL_TIMER_H5F_block_read_fcoll_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_w (EVAL_TIMER_H5F_block_read_fcoll_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Self_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Unpack_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Self_w + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Unpack_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Chunk_Alloc_w (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Type_Create_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Chunk_Alloc_w + 1)
#define EVAL_TIMER_H5D__final_collective_io_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Type_Create_w + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Update_Index_w (EVAL_TIMER_H5D__final_collective_io_w + 1)
#define EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_w (EVAL_TIMER_H5D__link_chunk_filtered_collective_io_Update_Index_w + 1)
#define EVAL_TIMER_H5Dread 40
#define EVAL_TIMER_H5D__read (EVAL_TIMER_H5Dread + 1)
#define EVAL_TIMER_H5D__read_check_arg (EVAL_TIMER_H5D__read + 1)
#define EVAL_TIMER_H5D__chunk_io_init_r (EVAL_TIMER_H5D__read_check_arg + 1)
#define EVAL_TIMER_H5D__ioinfo_adjust_r (EVAL_TIMER_H5D__chunk_io_init_r + 1)
#define EVAL_TIMER_H5D__chunk_collective_read (EVAL_TIMER_H5D__ioinfo_adjust_r + 1)
#define EVAL_TIMER_H5D__chunk_collective_io_r (EVAL_TIMER_H5D__chunk_collective_read + 1)
#define EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_r (EVAL_TIMER_H5D__chunk_collective_io_r + 1)
#define EVAL_TIMER_H5D__construct_filtered_io_info_list_r (EVAL_TIMER_H5D__multi_chunk_filtered_collective_io_r + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_r (EVAL_TIMER_H5D__construct_filtered_io_info_list_r + 1)
#define EVAL_TIMER_H5F_block_read_fcoll_r (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_r + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_r (EVAL_TIMER_H5F_block_read_fcoll_r + 1)
#define EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Self_r (EVAL_TIMER_H5D__filtered_collective_chunk_entry_io_Filter_Reverse_r + 1)
#define EVAL_TIMER_H5D__link_chunk_filtered_collective_io_r (EVAL_TIMER_H5D__chunk_collective_io_r + 1)
#define EVAL_TIMER_H5D__chunk_read 54
#define EVAL_TIMER_H5D__chunk_lookup_r (EVAL_TIMER_H5D__chunk_read + 1)
#define EVAL_TIMER_H5D__chunk_lock_r (EVAL_TIMER_H5D__chunk_lookup_r + 1)
#define EVAL_TIMER_H5F_block_read_lock_r (EVAL_TIMER_H5D__chunk_lock_r + 1)
#define EVAL_TIMER_H5D__chunk_lock_filter_r (EVAL_TIMER_H5F_block_read_lock_r + 1)
#define EVAL_TIMER_H5D__select_read (EVAL_TIMER_H5D__chunk_lock_filter_r + 1)
#define EVAL_TIMER_H5D__chunk_unlock_r (EVAL_TIMER_H5D__select_read + 1)

#define EVAL_TIMER_H5Ovisit (EVAL_TIMER_H5D__chunk_unlock_r + 1)
#define EVAL_TIMER_H5Ovisit2 (EVAL_TIMER_H5Ovisit + 1)

#define EVAL_TIMER_H5Z_filter_deflate_comp (EVAL_TIMER_H5Ovisit2 + 1)
#define EVAL_TIMER_H5Z_filter_deflate_decomp (EVAL_TIMER_H5Z_filter_deflate_comp + 1)

#define EVAL_NTIMER (EVAL_TIMER_H5Z_filter_deflate_decomp + 1)



extern void eval_add_time(int id, double t);

#define EVAL_TIMER_DUMMY 100
#define EVAL_TIMER_H5D__chunk_lock_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__chunk_unlock_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5F_block_read_lock_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__chunk_lock_filter_w EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5D__final_collective_io_r EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L_link_object_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L__create_real_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L__link_cb_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5O_obj_create_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L_link_object_other EVAL_TIMER_DUMMY
#define EVAL_TIMER_H5L__create_real_other EVAL_TIMER_DUMMY