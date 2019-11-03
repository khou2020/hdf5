#pragma once

#include "H5public.h"

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL herr_t H5Venable(void);
H5_DLL herr_t H5Vdisable(void);
H5_DLL herr_t H5Vprint(void);
H5_DLL herr_t H5Vprint2(char*)
H5_DLL herr_t H5Vreset(void);

#ifdef __cplusplus
}
#endif