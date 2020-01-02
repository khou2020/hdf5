#pragma once
#include "mpi.h"
#include "H5public.h"

// NOTE: macro dependency can't be too long to overflow the stack




#define EVAL_TIMER_H5Dwrite_cb 0
#define EVAL_TIMER_H5Dwrite_rd (EVAL_TIMER_H5Dwrite_cb + 1)
#define EVAL_TIMER_H5Dwrite_decom (EVAL_TIMER_H5Dwrite_rd + 1)
#define EVAL_TIMER_H5Dwrite_com (EVAL_TIMER_H5Dwrite_decom + 1)
#define EVAL_TIMER_H5Dwrite_wr (EVAL_TIMER_H5Dwrite_com + 1)
#define EVAL_TIMER_H5Dread_cb (EVAL_TIMER_H5Dwrite_wr + 1)
#define EVAL_TIMER_H5Dread_rd (EVAL_TIMER_H5Dread_cb + 1)
#define EVAL_TIMER_H5Dread_decom (EVAL_TIMER_H5Dread_rd + 1)


#define EVAL_NTIMER (EVAL_TIMER_H5Dread_decom + 1)
#define EVAL_NMPI 0

void eval_add_time(int id, double t);
void eval_sub_time(int id, double t);
int HDF_MPI_EVAL_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm );
void H5V_ShowHints(MPI_Info *mpiHints);