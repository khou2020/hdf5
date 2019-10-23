#include <stdlib.h>
#include "mpi.h"
#include "H5V.h"

static double eval_tlocal[EVAL_NTIMER];
static double eval_clocal[EVAL_NTIMER];


const char * const eval_tname[] = { 
                                    "hdf5_eval_H5Fcreate",
                                    "hdf5_eval_H5Fopen",
                                    "hdf5_eval_H5Fclose",
                                    "hdf5_eval_H5Gcreate",
                                    "hdf5_eval_H5Gopen",
                                    "hdf5_eval_H5Gclose",
                                    "hdf5_eval_H5Dcreate",
                                    "    hdf5_eval_H5D__create_named",
                                    "        hdf5_eval_H5L_link_object_dataset",
                                    "            hdf5_eval_H5L__create_real_dataset",
                                    "                hdf5_eval_H5L__link_cb_dataset",
                                    "                    hdf5_eval_H5O_obj_create_dataset",
                                    "                        hdf5_eval_H5O__dset_create",
                                    "                            hdf5_eval_H5D__create",
                                    "                    hdf5_eval_H5G_obj_insert_dataset",
                                    "hdf5_eval_H5Dopen",
                                    "hdf5_eval_H5Dclose",
                                    "hdf5_eval_H5Acreate",
                                    "hdf5_eval_H5Aopen",
                                    "hdf5_eval_H5Aclose",
                                    "hdf5_eval_H5Dwrite", 
                                    "    hdf5_eval_H5D__write", 
                                    "        hdf5_eval_H5D__chunk_io_init_w", 
                                    "        hdf5_eval_H5D__ioinfo_adjust_w",
                                    "        hdf5_eval_H5D__chunk_collective_write", 
                                    "             hdf5_eval_H5D__chunk_collective_io_w", 
                                    "                 hdf5_eval_H5D__link_chunk_filtered_collective_io_w", 
                                    "                     hdf5_eval_H5D__construct_filtered_io_info_list_w",
                                    "                         hdf5_eval_H5D__chunk_redistribute_shared_chunks", 
                                    "                             hdf5_eval_H5D__chunk_redistribute_shared_chunks::Chunk_assignmentw", 
                                    "                             hdf5_eval_H5D__chunk_redistribute_shared_chunks:Data_exchange", 
                                    "                     hdf5_eval_H5D__filtered_collective_chunk_entry_io_w",
                                    "                         hdf5_eval_H5F_block_read@H5D__filtered_collective_chunk_entry_io_w(READ_BACK_PARTIAL_CHUNK)",
                                    "                         hdf5_eval_H5D__filtered_collective_chunk_entry_io::Unfilter_w(DECOMPRESS_PARTIAL_CHUNK)",
                                    "                         hdf5_eval_H5D__filtered_collective_chunk_entry_io::Self_w",
                                    "                         hdf5_eval_H5D__filtered_collective_chunk_entry_io::Unpack_w",
                                    "                         hdf5_eval_H5D__filtered_collective_chunk_entry_io::Filter_w(COMRPESSION)",
                                    "                     hdf5_eval_H5D__link_chunk_filtered_collective_io::Chunk_Alloc_w",
                                    "                     hdf5_eval_H5D__link_chunk_filtered_collective_io::Type_Create_w",
                                    "                     hdf5_eval_H5D__final_collective_io_w(WRITE_TO_FILE)",
                                    "                     hdf5_eval_H5D__link_chunk_filtered_collective_io::Update_Index_w",
                                    "                 hdf5_eval_H5D__multi_chunk_filtered_collective_io_w",
                                    "hdf5_eval_H5Dread",
                                    "    hdf5_eval_H5D__read",
                                    "        hdf5_eval_H5D__read::check_arg",
                                    "        hdf5_eval_H5D__chunk_io_init_r", 
                                    "        hdf5_eval_H5D__ioinfo_adjust_r",
                                    "        hdf5_eval_H5D__chunk_collective_read(COLLECTIVE_READ)", 
                                    "             hdf5_eval_H5D__chunk_collective_io_r", 
                                    "                 hdf5_eval_H5D__multi_chunk_filtered_collective_io_r",
                                    "                     hdf5_eval_H5D__construct_filtered_io_info_list_r",      
                                    "                     hdf5_eval_H5D__filtered_collective_chunk_entry_io_r",  
                                    "                         hdf5_eval_H5F_block_read@H5D__filtered_collective_chunk_entry_io_r(READ_RAW_DATA)",
                                    "                         hdf5_eval_H5D__filtered_collective_chunk_entry_io::Unfilter_r(DECOMPRESSION)",
                                    "                         hdf5_eval_H5D__filtered_collective_chunk_entry_io::Self_r",
                                    "                 hdf5_eval_H5D__link_chunk_filtered_collective_io_r", 
                                    "        hdf5_eval_H5D__chunk_read(INDEPENDENT_READ)",
                                    "            hdf5_eval_H5D__chunk_lookup_r",
                                    "            hdf5_eval_H5D__chunk_lock_r",
                                    "                hdf5_eval_H5F_block_read@H5D__chunk_lock_r(READ_RAW_DATA)",
                                    "                hdf5_eval_H5D__chunk_lock::Filter_r(DECOMPRESSION)",
                                    "            hdf5_eval_H5D__select_read",
                                    "            hdf5_eval_H5D__chunk_unlock_r",
                                    "hdf5_eval_H5Ovisit",
                                    "hdf5_eval_H5Ovisit2",
                                    "hdf5_eval_H5Z_filter_deflate_comp",
                                    "hdf5_eval_H5Z_filter_deflate_decomp",
                                    "hdf5_eval_MPI_Allgather",
                                    "hdf5_eval_MPI_Allgatherv",
                                    "hdf5_eval_MPI_Allreduce",
                                    "hdf5_eval_MPI_Bcast",
                                    "hdf5_eval_MPI_Gather",
                                    "hdf5_eval_MPI_Gatherv",
                                    "hdf5_eval_MPI_Send",
                                    "hdf5_eval_MPI_Isend",
                                    "hdf5_eval_MPI_Recv",
                                    "hdf5_eval_MPI_Imrecv",
                                    "hdf5_eval_MPI_Mprobe",
                                    "hdf5_eval_MPI_File_read_at",
                                    "hdf5_eval_MPI_File_read_at_all",
                                    "hdf5_eval_MPI_File_write_at",
                                    "hdf5_eval_MPI_File_write_at_all",
                                    "hdf5_eval_MPI_File_set_view",
                                    };


static eval_need_finalize = 0;
static eval_fcnt = 0;
static eval_enable = 0;
void eval_init_mpi(){
    int flag;
    MPI_Initialized(&flag);
    if (!flag){
        MPI_Init(NULL, NULL);
        eval_need_finalize = 1;
    }
    eval_fcnt++;
}
void eval_free_mpi(){
    eval_fcnt--;
    if (eval_fcnt == 0){
        if (eval_need_finalize){
            MPI_Finalize();
            eval_need_finalize = 0;
        }
    }
}
void eval_add_time(int id, double t){
    if (eval_enable && (id > 100)){
        return;
    }
    eval_tlocal[id] += t;
    eval_clocal[id]++;
}

void H5Venable(){
    eval_enable = 1;
}
void H5Vdisable(){
    eval_enable = 0;
}

// Note: This only work if everyone calls H5Fclose
void H5Vprint(){
    int i;
    int np, rank, flag;
    double tmax[EVAL_NTIMER], tmin[EVAL_NTIMER], tmean[EVAL_NTIMER], tvar[EVAL_NTIMER], tvar_local[EVAL_NTIMER];

    MPI_Initialized(&flag);
    if (!flag){
        MPI_Init(NULL, NULL);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &np);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Reduce(eval_tlocal, tmax, EVAL_NTIMER, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(eval_tlocal, tmin, EVAL_NTIMER, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Allreduce(eval_tlocal, tmean, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    for(i = 0; i < EVAL_NTIMER; i++){
        tmean[i] /= np;
        tvar_local[i] = (eval_tlocal[i] - tmean[i]) * (eval_tlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0){
        for(i = 0; i < EVAL_NTIMER; i++){
            printf("#+$: %s_time_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_time_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_time_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_time_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

    MPI_Reduce(eval_clocal, tmax, EVAL_NTIMER, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(eval_clocal, tmin, EVAL_NTIMER, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Allreduce(eval_clocal, tmean, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    for(i = 0; i < EVAL_NTIMER; i++){
        tmean[i] /= np;
        tvar_local[i] = (eval_clocal[i] - tmean[i]) * (eval_clocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0){
        for(i = 0; i < EVAL_NTIMER; i++){
            printf("#+$: %s_count_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_count_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_count_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_count_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

    if (!flag){
        MPI_Finalize();
    }
}
void H5Vreset(){
    int i;
    
    for(i = 0; i < EVAL_NTIMER; i++){
        eval_tlocal[i] = 0;
        eval_clocal[i] = 0;
    }
}

int HDF_MPI_EVAL_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm ){
    int ret;
    double t1, t2;

    t1 = MPI_Wtime();

    ret = MPI_Bcast(buffer, count, datatype, root, comm);
    
    t2 = MPI_Wtime();
    eval_add_time(EVAL_TIMER_MPI_Bcast, t2 - t1);

    return ret;
}