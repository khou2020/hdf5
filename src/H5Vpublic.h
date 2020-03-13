#pragma once

#include "H5public.h"

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL herr_t H5Venable(void);  // Enable profiling
H5_DLL herr_t H5Vdisable(void); // Disable profiling
H5_DLL herr_t H5Vprint(void);   // Print result
H5_DLL herr_t H5Vreset(void);   // Reset everything to 0

#ifdef __cplusplus
}
#endif