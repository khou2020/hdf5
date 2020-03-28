#include <stdlib.h>
#include "mpi.h"
#include "H5V.h"
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

static double eval_tlocal[EVAL_NTIMER];    // Time
static double eval_clocal[EVAL_NTIMER];    // Count (number of function calls)

static double eval_maxlocal[EVAL_NMPI];    // Max size
static double eval_minlocal[EVAL_NMPI];    // Min size
static double eval_sumlocal[EVAL_NMPI];    // Total size



// Timer name
const char * const eval_tname[] = { 
                                    "hdf5_eval_H5MM_malloc",
                                    "hdf5_eval_read",
                                    "hdf5_eval_pread",
                                    "hdf5_eval_write",
                                    "hdf5_eval_pwrite",
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
                                    "hdf5_eval_MPI_Waitall",
                                    "hdf5_eval_H5Fcreate",
                                    "hdf5_eval_H5Fopen",
                                    "hdf5_eval_H5Fclose",
                                    "hdf5_eval_H5Gcreate",
                                    "hdf5_eval_H5Gopen",
                                    "hdf5_eval_H5Gclose",
                                    "hdf5_eval_H5Dcreate",
                                    "    hdf5_eval_H5VL_dataset_create",
                                    "        hdf5_eval_H5VL__dataset_create",
                                    "            hdf5_eval_H5VL__native_dataset_create",
                                    "                hdf5_eval_H5D__create_named",
                                    "                    hdf5_eval_H5L_link_object_dataset",
                                    "                        hdf5_eval_H5L__create_real_dataset",
                                    "                            hdf5_eval_H5L__link_cb_dataset",
                                    "                                hdf5_eval_H5O_obj_create_dataset",
                                    "                                    hdf5_eval_H5O__dset_create",
                                    "                                        hdf5_eval_H5D__create",
                                    "                                            hdf5_eval_H5D__create.metadata",
                                    "                                            hdf5_eval_H5D__create.property",
                                    "                                            hdf5_eval_H5D__chunk_construct",
                                    "                                            hdf5_eval_H5D__update_oh_info",
                                    "                                                hdf5_eval_H5D__layout_oh_create",
                                    "                                                    hdf5_eval_H5D__alloc_storage",
                                    "                                                        hdf5_eval_H5D__init_storage",
                                    "                                                            hdf5_eval_H5D__chunk_allocate",
                                    "                                                                hdf5_eval_H5D__chunk_collective_fill",
                                    "                                hdf5_eval_H5G_obj_insert_dataset",
                                    "hdf5_eval_H5Dopen",
                                    "hdf5_eval_H5Dclose",
                                    "hdf5_eval_H5Acreate",
                                    "hdf5_eval_H5Aopen",
                                    "hdf5_eval_H5Aclose",
                                    "hdf5_eval_H5Dwrite", 
                                    "    hdf5_eval_H5VL_dataset_write", 
                                    "        hdf5_eval_H5VL__dataset_write", 
                                    "            hdf5_eval_H5VL__native_dataset_write",                                                                         
                                    "                hdf5_eval_H5D__write", 
                                    "                    hdf5_eval_H5D__chunk_io_init_w", 
                                    "                    hdf5_eval_H5D__ioinfo_adjust_w",
                                    "                        hdf5_eval_H5D__ioinfo_adjust.Reset_w",
                                    "                        hdf5_eval_H5D__ioinfo_adjust.Get_comm_w",
                                    "                        hdf5_eval_H5D__ioinfo_adjust.Check_coll_w",
                                    "                    hdf5_eval_H5D__chunk_collective_write", 
                                    "                         hdf5_eval_H5D__chunk_collective_io_w(WRITE_CHUNKED_DATASET)", 
                                    "                             hdf5_eval_H5D__link_chunk_filtered_collective_io_w(WRITE_CHUNK_COMPRESSED_DATASET)", 
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Init_w",
                                    "                                 hdf5_eval_H5D__construct_filtered_io_info_list_w",
                                    "                                     hdf5_eval_H5D__chunk_redistribute_shared_chunks", 
                                    "                                         hdf5_eval_H5D__chunk_redistribute_shared_chunks.Chunk_assignmentw", 
                                    "                                         hdf5_eval_H5D__chunk_redistribute_shared_chunks:Data_exchange", 
                                    "                                 hdf5_eval_H5D__filtered_collective_chunk_entry_io_w",
                                    "                                     hdf5_eval_H5F_shared_block_read@H5D__filtered_collective_chunk_entry_io_w(READ_BACK_PARTIAL_CHUNK)",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Unfilter_w(DECOMPRESS_PARTIAL_CHUNK)",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Self_w",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Waitall_w",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Unpack_w",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Filter_w(COMRPESSION)",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Gather_Size_w",                                    
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Chunk_Alloc_w",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Gather_Nchunk_w",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Type_Create_w",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Collective_IO_w(WRITE_TO_FILE)",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Update_Index_w",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Finalize_w",
                                    "                             hdf5_eval_H5D__multi_chunk_filtered_collective_io_w",
                                    "                             hdf5_eval_H5D__contig_collective_write(WRITE_CONTIGUOUS_DATASET)",
                                    "                                 hdf5_eval_H5D__inter_collective_io_w",
                                    "                                     hdf5_eval_H5D__inter_collective_io.Collective_IO_w(WRITE_TO_FILE)",
                                    "                             hdf5_eval_H5D__final_collective_io_w",
                                    "hdf5_eval_H5Dread",
                                    "    hdf5_eval_H5VL_dataset_read",
                                    "        hdf5_eval_H5VL__dataset_read",
                                    "            hdf5_eval_H5VL__native_dataset_read",
                                    "                hdf5_eval_H5D__read",
                                    "                    hdf5_eval_H5D__read.check_arg",
                                    "                    hdf5_eval_H5D__chunk_io_init_r", 
                                    "                    hdf5_eval_H5D__ioinfo_adjust_r",
                                    "                        hdf5_eval_H5D__ioinfo_adjust.Reset_r",
                                    "                        hdf5_eval_H5D__ioinfo_adjust.Get_comm_r",
                                    "                        hdf5_eval_H5D__ioinfo_adjust.Check_coll_r",
                                    "                    hdf5_eval_H5D__chunk_collective_read(COLLECTIVE_READ)", 
                                    "                         hdf5_eval_H5D__chunk_collective_io_r", 
                                    "                             hdf5_eval_H5D__multi_chunk_filtered_collective_io_r",
                                    "                                 hdf5_eval_H5D__construct_filtered_io_info_list_r",      
                                    "                                 hdf5_eval_H5D__filtered_collective_chunk_entry_io_r",  
                                    "                                     hdf5_eval_H5F_shared_block_read@H5D__filtered_collective_chunk_entry_io_r(READ_RAW_DATA)",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Unfilter_r(DECOMPRESSION)",
                                    "                                     hdf5_eval_H5D__filtered_collective_chunk_entry_io.Self_r",
                                    "                             hdf5_eval_H5D__link_chunk_filtered_collective_io_r", 
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Init_r",
                                    "                                 hdf5_eval_H5D__link_chunk_filtered_collective_io.Finalize_r",
                                    "                    hdf5_eval_H5D__chunk_read(INDEPENDENT_READ)",
                                    "                        hdf5_eval_H5D__chunk_lookup_r",
                                    "                        hdf5_eval_H5D__chunk_lock_r",
                                    "                            hdf5_eval_H5F_shared_block_read@H5D__chunk_lock_r(READ_RAW_DATA)",
                                    "                            hdf5_eval_H5D__chunk_lock.Filter_r(DECOMPRESSION)",
                                    "                        hdf5_eval_H5D__select_read",
                                    "                        hdf5_eval_H5D__chunk_unlock_r",
                                    "hdf5_eval_H5D__set_extent",
                                    "    hdf5_eval_H5D__chunk_prune_by_extent",
                                    "hdf5_eval_H5Ovisit",
                                    "hdf5_eval_H5Ovisit2",
                                    "hdf5_eval_H5Ovisit3",
                                    "hdf5_eval_H5Z_filter_deflate_comp",
                                    "hdf5_eval_H5Z_filter_deflate_decomp",
                                    };

static int eval_enable = 0;

// Report time
void eval_add_time(int id, double t){
    if ((!eval_enable) || (id >= EVAL_NTIMER) || (id < 0)){
        return;
    }
    eval_tlocal[id] += t;
    eval_clocal[id]++;
}

// Report size
void eval_add_size(int id, int count, MPI_Datatype type){
    int esize;
    double size;
    
    if ((!eval_enable) || (id >= EVAL_NMPI) || (id < 0)){
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

// Enable timer
herr_t H5Venable(){
    eval_enable = 1;
    return 0;
}
// Disable timer
herr_t H5Vdisable(){
    eval_enable = 0;
    return 0;
}

// Print current timer status
// Note: This is a collective function
herr_t H5Vprint(){
    int i;
    int np, rank, flag;
    double tmax[EVAL_NTIMER], tmin[EVAL_NTIMER], tmean[EVAL_NTIMER], tvar[EVAL_NTIMER], tvar_local[EVAL_NTIMER];

    // In case the application hasn't initialize MPI
#ifdef H5_HAVE_PARALLEL
    MPI_Initialized(&flag); 
    if (!flag){
        MPI_Init(NULL, NULL);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &np);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Reduce(eval_tlocal, tmax, EVAL_NTIMER, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD); // Max time across all process
    MPI_Reduce(eval_tlocal, tmin, EVAL_NTIMER, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD); // Min time across all process
    MPI_Allreduce(eval_tlocal, tmean, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD); // Total time across all process
    for(i = 0; i < EVAL_NTIMER; i++){   // Calculate mean and variance
        tmean[i] /= np;
        tvar_local[i] = (eval_tlocal[i] - tmean[i]) * (eval_tlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); // Time variance across all process
#else
    rank = 0;
    np = 1;
    memcpy(tmax, eval_tlocal, sizeof(double) * EVAL_NTIMER);
    memcpy(tmin, eval_tlocal, sizeof(double) * EVAL_NTIMER);
    memcpy(tmean, eval_tlocal, sizeof(double) * EVAL_NTIMER);
    memset(tvar, 0, sizeof(double) * EVAL_NTIMER);
#endif

    if (rank == 0){ // Rank 0 prints the result
        for(i = 0; i < EVAL_NTIMER; i++){
            printf("#+$: %s_time_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_time_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_time_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_time_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

#ifdef H5_HAVE_PARALLEL
    MPI_Reduce(eval_clocal, tmax, EVAL_NTIMER, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD); // Max count across all process
    MPI_Reduce(eval_clocal, tmin, EVAL_NTIMER, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD); // Min count across all process
    MPI_Allreduce(eval_clocal, tmean, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD); // Total count across all process
    for(i = 0; i < EVAL_NTIMER; i++){   // Calculate mean and variance
        tmean[i] /= np;
        tvar_local[i] = (eval_clocal[i] - tmean[i]) * (eval_clocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NTIMER, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); // Count variance across all process
#else
    memcpy(tmax, eval_clocal, sizeof(double) * EVAL_NTIMER);
    memcpy(tmin, eval_clocal, sizeof(double) * EVAL_NTIMER);
    memcpy(tmean, eval_clocal, sizeof(double) * EVAL_NTIMER);
    memset(tvar, 0, sizeof(double) * EVAL_NTIMER);
#endif

    if (rank == 0){ // Rank 0 prints the result
        for(i = 0; i < EVAL_NTIMER; i++){
            printf("#+$: %s_count_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_count_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_count_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_count_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

#ifdef H5_HAVE_PARALLEL
    MPI_Reduce(eval_sumlocal, tmax, EVAL_NMPI, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD); // Max total size across all process
    MPI_Reduce(eval_sumlocal, tmin, EVAL_NMPI, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD); // Min total size across all process
    MPI_Allreduce(eval_sumlocal, tmean, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD); // Total size across all process
    for(i = 0; i < EVAL_NMPI; i++){   // Calculate mean and variance
        tmean[i] /= np;
        tvar_local[i] = (eval_sumlocal[i] - tmean[i]) * (eval_sumlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); // Total size variance across all process
#else
    memcpy(tmax, eval_sumlocal, sizeof(double) * EVAL_NMPI);
    memcpy(tmin, eval_sumlocal, sizeof(double) * EVAL_NMPI);
    memcpy(tmean, eval_sumlocal, sizeof(double) * EVAL_NMPI);
    memset(tvar, 0, sizeof(double) * EVAL_NMPI);
#endif

    if (rank == 0){
        for(i = 0; i < EVAL_NMPI; i++){
            printf("#+$: %s_size_sum_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_size_sum_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_size_sum_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_size_sum_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

#ifdef H5_HAVE_PARALLEL
    MPI_Reduce(eval_maxlocal, tmax, EVAL_NMPI, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD); // Max size ever recorded across all process
    MPI_Reduce(eval_maxlocal, tmin, EVAL_NMPI, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD); // Min of the maximum size ever recorded across process
    MPI_Allreduce(eval_maxlocal, tmean, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD); // Sum of maximum size ever recorded across all process
    for(i = 0; i < EVAL_NMPI; i++){  // Calculate mean and variance
        tmean[i] /= np;
        tvar_local[i] = (eval_maxlocal[i] - tmean[i]) * (eval_maxlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); // Variance of the maximum size evern recorded across processes
#else
    memcpy(tmax, eval_maxlocal, sizeof(double) * EVAL_NMPI);
    memcpy(tmin, eval_maxlocal, sizeof(double) * EVAL_NMPI);
    memcpy(tmean, eval_maxlocal, sizeof(double) * EVAL_NMPI);
    memset(tvar, 0, sizeof(double) * EVAL_NMPI);
#endif

    if (rank == 0){
        for(i = 0; i < EVAL_NMPI; i++){
            printf("#+$: %s_size_max_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_size_max_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_size_max_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_size_max_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

#ifdef H5_HAVE_PARALLEL
    MPI_Reduce(eval_minlocal, tmax, EVAL_NMPI, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD); // Max of the minimum size ever recorded across process
    MPI_Reduce(eval_minlocal, tmin, EVAL_NMPI, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD); // Minimum size ever recorded across all process
    MPI_Allreduce(eval_minlocal, tmean, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD); // Sum of minimum size ever recorded across all process
    for(i = 0; i < EVAL_NMPI; i++){  // Calculate mean and variance
        tmean[i] /= np;
        tvar_local[i] = (eval_minlocal[i] - tmean[i]) * (eval_minlocal[i] - tmean[i]);
    }
    MPI_Reduce(tvar_local, tvar, EVAL_NMPI, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); // Variance of the minimum size evern recorded across processes
#else
    memcpy(tmax, eval_minlocal, sizeof(double) * EVAL_NMPI);
    memcpy(tmin, eval_minlocal, sizeof(double) * EVAL_NMPI);
    memcpy(tmean, eval_minlocal, sizeof(double) * EVAL_NMPI);
    memset(tvar, 0, sizeof(double) * EVAL_NMPI);
#endif

    if (rank == 0){
        for(i = 0; i < EVAL_NMPI; i++){
            printf("#+$: %s_size_min_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_size_min_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_size_min_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_size_min_var: %lf\n\n", eval_tname[i], tvar[i]);
        }
    }

    // If we initialized MPI, we finalize it
#ifdef H5_HAVE_PARALLEL
    if (!flag){
        MPI_Finalize();
    }
#endif

    return 0;
}
// Reset timer
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

// Wrapper to collect MPI_Bcast statistics
int HDF_MPI_EVAL_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm ){
    int ret;
    double t1, t2;

    t1 = HDF_EVAL_wtime();

    ret = MPI_Bcast(buffer, count, datatype, root, comm);
    
    t2 = HDF_EVAL_wtime();
    eval_add_time(EVAL_TIMER_MPI_Bcast, t2 - t1);
    
    eval_add_size(EVAL_TIMER_MPI_Bcast, count, datatype);

    return ret;
}

// Print MPI hint
void H5V_ShowHints(MPI_Info *mpiHints) {
    char key[MPI_MAX_INFO_VAL];
    char value[MPI_MAX_INFO_VAL];
    int flag, i, nkeys, rank; 

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0){
        MPI_Info_get_nkeys(*mpiHints, &nkeys);

        for (i = 0; i < nkeys; i++) {
            MPI_Info_get_nthkey(*mpiHints, i, key);

            MPI_Info_get(*mpiHints, key, MPI_MAX_INFO_VAL - 1,
                                    value, &flag);
            printf("\t%s = %s\n", key, value);
        }
    }
}

// Wrapper to collect POSIX write statistics
ssize_t HDF_EVAL_write(int fd, const void *buf, size_t count){
    ssize_t ret;
    double t1, t2;

    t1 = HDF_EVAL_wtime();
    ret = write(fd, buf, count);
    t2 = HDF_EVAL_wtime();

    eval_add_time(EVAL_TIMER_write, t2 - t1);
    eval_add_size(EVAL_TIMER_write, count, MPI_BYTE);

    return ret;
}

// Wrapper to collect POSIX pwrite statistics
ssize_t HDF_EVAL_pwrite(int fd, const void *buf, size_t count, off_t offset){
    ssize_t ret;
    double t1, t2;

    t1 = HDF_EVAL_wtime();
    ret = pwrite(fd, buf, count, offset);
    t2 = HDF_EVAL_wtime();

    eval_add_time(EVAL_TIMER_pwrite, t2 - t1);
    eval_add_size(EVAL_TIMER_pwrite, count, MPI_BYTE);

    return ret;
}

// Wrapper to collect POSIX read statistics
ssize_t HDF_EVAL_read(int fd, const void *buf, size_t count){
    ssize_t ret;
    double t1, t2;

    t1 = HDF_EVAL_wtime();
    ret = read(fd, buf, count);
    t2 = HDF_EVAL_wtime();

    eval_add_time(EVAL_TIMER_read, t2 - t1);
    eval_add_size(EVAL_TIMER_read, count, MPI_BYTE);

    return ret;
}

// Wrapper to collect POSIX pread statistics
ssize_t HDF_EVAL_pread(int fd, const void *buf, size_t count, off_t offset){
    ssize_t ret;
    double t1, t2;

    t1 = HDF_EVAL_wtime();
    ret = pread(fd, buf, count, offset);
    t2 = HDF_EVAL_wtime();

    eval_add_time(EVAL_TIMER_pread, t2 - t1);
    eval_add_size(EVAL_TIMER_pread, count, MPI_BYTE);
    
    return ret;
}

// Get wall time in sec
// We do not use MPI_Wtime cause we must cover posix driver
// MPI_Wtime also cause problem on sequential utilities such as h5dump
double HDF_EVAL_wtime(void){
    return ((double)clock()) / CLOCKS_PER_SEC;
}