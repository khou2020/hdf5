#include <stdlib.h>
#include "mpi.h"
#include "H5V.h"
#include "H5public.h"

static double eval_tlocal[EVAL_NTIMER];
static double eval_clocal[EVAL_NTIMER];

static double eval_maxlocal[EVAL_NMPI];
static double eval_minlocal[EVAL_NMPI];
static double eval_sumlocal[EVAL_NMPI];

const char * const eval_tname[] = { 
                                    "MPI_Allgather",
                                    "MPI_Allgatherv",
                                    "MPI_Allreduce",
                                    "MPI_Bcast",
                                    "MPI_Gather",
                                    "MPI_Gatherv",
                                    "MPI_Send",
                                    "MPI_Isend",
                                    "MPI_Recv",
                                    "MPI_Imrecv",
                                    "MPI_Mprobe",
                                    "MPI_File_read_at",
                                    "MPI_File_read_at_all",
                                    "MPI_File_write_at",
                                    "MPI_File_write_at_all",
                                    "MPI_File_set_view",
                                    "H5Fcreate",
                                    "H5Fopen",
                                    "H5Fclose",
                                    "H5Gcreate",
                                    "H5Gopen",
                                    "H5Gclose",
                                    "H5Dcreate",
                                    "    H5D__create_named",
                                    "        H5L_link_object_dataset",
                                    "            H5L__create_real_dataset",
                                    "                H5L__link_cb_dataset",
                                    "                    H5O_obj_create_dataset",
                                    "                        H5O__dset_create",
                                    "                            H5D__create",
                                    "                                H5D__create.metadata",
                                    "                                H5D__create.property",
                                    "                                H5D__chunk_construct",
                                    "                                H5D__update_oh_info",
                                    "                    H5G_obj_insert_dataset",
                                    "H5Dopen",
                                    "H5Dclose",
                                    "H5Acreate",
                                    "H5Aopen",
                                    "H5Aclose",
                                    "H5Dwrite", 
                                    "    H5D__write", 
                                    "        H5D__chunk_io_init_w", 
                                    "        H5D__ioinfo_adjust_w",
                                    "        H5D__chunk_collective_write", 
                                    "             H5D__chunk_collective_io_w", 
                                    "                 H5D__link_chunk_filtered_collective_io_w", 
                                    "                     H5D__construct_filtered_io_info_list_w",
                                    "                         H5D__chunk_redistribute_shared_chunks", 
                                    "                             H5D__chunk_redistribute_shared_chunks.Chunk_assignmentw", 
                                    "                             H5D__chunk_redistribute_shared_chunks:Data_exchange", 
                                    "                     H5D__filtered_collective_chunk_entry_io_w",
                                    "                         H5F_block_read@H5D__filtered_collective_chunk_entry_io_w(READ_BACK_PARTIAL_CHUNK)",
                                    "                         H5D__filtered_collective_chunk_entry_io.Unfilter_w(DECOMPRESS_PARTIAL_CHUNK)",
                                    "                         H5D__filtered_collective_chunk_entry_io.Self_w",
                                    "                         H5D__filtered_collective_chunk_entry_io.Unpack_w",
                                    "                         H5D__filtered_collective_chunk_entry_io.Filter_w(COMRPESSION)",
                                    "                     H5D__link_chunk_filtered_collective_io.Chunk_Alloc_w",
                                    "                     H5D__link_chunk_filtered_collective_io.Type_Create_w",
                                    "                     H5D__final_collective_io_w(WRITE_TO_FILE)",
                                    "                     H5D__link_chunk_filtered_collective_io.Update_Index_w",
                                    "                 H5D__multi_chunk_filtered_collective_io_w",
                                    "H5Dread",
                                    "    H5D__read",
                                    "        H5D__read.check_arg",
                                    "        H5D__chunk_io_init_r", 
                                    "        H5D__ioinfo_adjust_r",
                                    "        H5D__chunk_collective_read(COLLECTIVE_READ)", 
                                    "             H5D__chunk_collective_io_r", 
                                    "                 H5D__multi_chunk_filtered_collective_io_r",
                                    "                     H5D__construct_filtered_io_info_list_r",      
                                    "                     H5D__filtered_collective_chunk_entry_io_r",  
                                    "                         H5F_block_read@H5D__filtered_collective_chunk_entry_io_r(READ_RAW_DATA)",
                                    "                         H5D__filtered_collective_chunk_entry_io.Unfilter_r(DECOMPRESSION)",
                                    "                         H5D__filtered_collective_chunk_entry_io.Self_r",
                                    "                 H5D__link_chunk_filtered_collective_io_r", 
                                    "        H5D__chunk_read(INDEPENDENT_READ)",
                                    "            H5D__chunk_lookup_r",
                                    "            H5D__chunk_lock_r",
                                    "                H5F_block_read@H5D__chunk_lock_r(READ_RAW_DATA)",
                                    "                H5D__chunk_lock.Filter_r(DECOMPRESSION)",
                                    "            H5D__select_read",
                                    "            H5D__chunk_unlock_r",
                                    "H5Ovisit",
                                    "H5Ovisit2",
                                    "H5Z_filter_deflate_comp",
                                    "H5Z_filter_deflate_decomp",
                                    };


static eval_need_finalize = 0;
static eval_fcnt = 0;
static eval_enable = 0;

void eval_add_time(int id, double t){
    if (eval_enable && (id > EVAL_NTIMER)){
        return;
    }
    eval_tlocal[id] += t;
    eval_clocal[id]++;
}

void eval_add_size(int id, int count, MPI_Datatype type){
    int esize;
    double size;
    
    if (eval_enable && (id > EVAL_NMPI)){
        return;
    }
    
    esize = 0;
    MPI_Type_size(type, &esize);
    size = (double)(esize * count);

    eval_sumlocal[id] += size;
    if (eval_maxlocal[id] < size){
        eval_maxlocal[id] = size;
    }
    if ((eval_minlocal[id] > size) || (eval_minlocal[id] == 0)){
        eval_minlocal[id] = size;
    }
}

herr_t H5Venable(){
    eval_enable = 1;
    return 0;
}

herr_t H5Vdisable(){
    eval_enable = 0;
    return 0;
}

herr_t H5Vprint(){
    return H5Vprint2("hdf5_eval_");
}

herr_t H5Vprint2(char *prefix){
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
            printf("#+$: %s%s_time_mean: %lf\n", prefix, eval_tname[i], tmean[i]);
            printf("#+$: %s%s_time_max: %lf\n", prefix, eval_tname[i], tmax[i]);
            printf("#+$: %s%s_time_min: %lf\n", prefix, eval_tname[i], tmin[i]);
            printf("#+$: %s%s_time_var: %lf\n\n", prefix, eval_tname[i], tvar[i]);
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
            printf("#+$: %s%s_count_mean: %lf\n", prefix, eval_tname[i], tmean[i]);
            printf("#+$: %s%s_count_max: %lf\n", prefix, eval_tname[i], tmax[i]);
            printf("#+$: %s%s_count_min: %lf\n", prefix, eval_tname[i], tmin[i]);
            printf("#+$: %s%s_count_var: %lf\n\n", prefix, eval_tname[i], tvar[i]);
        }
    }

    MPI_Reduce(eval_sumlocal, tmax, EVAL_NMPI, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(eval_sumlocal, tmin, EVAL_NMPI, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Allreduce(eval_sumlocal, tmean, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    for(i = 0; i < EVAL_NMPI; i++){
        tmean[i] /= np;
        tvar_local[i] = (eval_sumlocal[i] - tmean[i]) * (eval_sumlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0){
        for(i = 0; i < EVAL_NMPI; i++){
            printf("#+$: %s%s_size_sum_mean: %lf\n", prefix, eval_tname[i], tmean[i]);
            printf("#+$: %s%s_size_sum_max: %lf\n", prefix, eval_tname[i], tmax[i]);
            printf("#+$: %s%s_size_sum_min: %lf\n", prefix, eval_tname[i], tmin[i]);
            printf("#+$: %s%s_size_sum_var: %lf\n\n", prefix, eval_tname[i], tvar[i]);
        }
    }

    MPI_Reduce(eval_maxlocal, tmax, EVAL_NMPI, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(eval_maxlocal, tmin, EVAL_NMPI, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Allreduce(eval_maxlocal, tmean, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    for(i = 0; i < EVAL_NMPI; i++){
        tmean[i] /= np;
        tvar_local[i] = (eval_maxlocal[i] - tmean[i]) * (eval_maxlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0){
        for(i = 0; i < EVAL_NMPI; i++){
            printf("#+$: %s%s_size_max_mean: %lf\n", prefix, eval_tname[i], tmean[i]);
            printf("#+$: %s%s_size_max_max: %lf\n", prefix, eval_tname[i], tmax[i]);
            printf("#+$: %s%s_size_max_min: %lf\n", prefix, eval_tname[i], tmin[i]);
            printf("#+$: %s%s_size_max_var: %lf\n\n", prefix, eval_tname[i], tvar[i]);
        }
    }

    MPI_Reduce(eval_minlocal, tmax, EVAL_NMPI, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(eval_minlocal, tmin, EVAL_NMPI, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Allreduce(eval_minlocal, tmean, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    for(i = 0; i < EVAL_NMPI; i++){
        tmean[i] /= np;
        tvar_local[i] = (eval_minlocal[i] - tmean[i]) * (eval_minlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0){
        for(i = 0; i < EVAL_NMPI; i++){
            printf("#+$: %s%s_size_min_mean: %lf\n", prefix, eval_tname[i], tmean[i]);
            printf("#+$: %s%s_size_min_max: %lf\n", prefix, eval_tname[i], tmax[i]);
            printf("#+$: %s%s_size_min_min: %lf\n", prefix, eval_tname[i], tmin[i]);
            printf("#+$: %s%s_size_min_var: %lf\n\n", prefix, eval_tname[i], tvar[i]);
        }
    }

    if (!flag){
        MPI_Finalize();
    }

    return 0;
}
herr_t H5Vreset(){
    int i;
    
    for(i = 0; i < EVAL_NTIMER; i++){
        eval_tlocal[i] = 0;
        eval_clocal[i] = 0;
    }

    for(i = 0; i < EVAL_NMPI; i++){
        eval_sumlocal[i] = 0;
        eval_maxlocal[i] = 0;
        eval_minlocal[i] = 0;
    }

    return 0;
}

int HDF_MPI_EVAL_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm ){
    int ret;
    double t1, t2;

    t1 = MPI_Wtime();

    ret = MPI_Bcast(buffer, count, datatype, root, comm);
    
    t2 = MPI_Wtime();
    eval_add_time(EVAL_TIMER_MPI_Bcast, t2 - t1);
    
    eval_add_size(EVAL_TIMER_MPI_Bcast, count, datatype);

    return ret;
}