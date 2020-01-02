#include <stdlib.h>
#include "mpi.h"
#include "H5V.h"

static double eval_tlocal[EVAL_NTIMER];
static double eval_clocal[EVAL_NTIMER];

static double eval_maxlocal[EVAL_NMPI];
static double eval_minlocal[EVAL_NMPI];
static double eval_sumlocal[EVAL_NMPI];








const char * const eval_tname[] = { 
                                    "hdf5_eval_H5Dwrite_cb",
                                    "hdf5_eval_H5Dwrite_rd",
                                    "hdf5_eval_H5Dwrite_decom",
                                    "hdf5_eval_H5Dwrite_com",
                                    "hdf5_eval_H5Dwrite_wr",
                                    "hdf5_eval_H5Dread_cb",
                                    "hdf5_eval_H5Dread_rd",
                                    "hdf5_eval_H5Dread_decom",
                                    };


static eval_need_finalize = 0;
static eval_fcnt = 0;
static eval_enable = 0;

void eval_add_time(int id, double t){
    if ((!eval_enable) || (id > EVAL_NTIMER)){
        return;
    }
    eval_tlocal[id] += t;
    eval_clocal[id]++;
}

void eval_sub_time(int id, double t){
    if ((!eval_enable) && (id > EVAL_NTIMER)){
        return;
    }
    eval_tlocal[id] -= t;
}

herr_t H5Venable(){
    eval_enable = 1;
    return 0;
}
herr_t H5Vdisable(){
    eval_enable = 0;
    return 0;
}

// Note: This only work if everyone calls H5Fclose
herr_t H5Vprint(){
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
            printf("#+$: %s_size_sum_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_size_sum_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_size_sum_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_size_sum_var: %lf\n\n", eval_tname[i], tvar[i]);
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
            printf("#+$: %s_size_max_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_size_max_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_size_max_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_size_max_var: %lf\n\n", eval_tname[i], tvar[i]);
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
            printf("#+$: %s_size_min_mean: %lf\n", eval_tname[i], tmean[i]);
            printf("#+$: %s_size_min_max: %lf\n", eval_tname[i], tmax[i]);
            printf("#+$: %s_size_min_min: %lf\n", eval_tname[i], tmin[i]);
            printf("#+$: %s_size_min_var: %lf\n\n", eval_tname[i], tvar[i]);
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

void H5V_ShowHints(MPI_Info *mpiHints) {
    char key[MPI_MAX_INFO_VAL];
    char value[MPI_MAX_INFO_VAL];
    int flag, i, nkeys;

    MPI_Info_get_nkeys(*mpiHints, &nkeys);

    for (i = 0; i < nkeys; i++) {
            MPI_Info_get_nthkey(*mpiHints, i, key);

            MPI_Info_get(*mpiHints, key, MPI_MAX_INFO_VAL - 1,
                                    value, &flag);
            printf("\t%s = %s\n", key, value);
    }
}