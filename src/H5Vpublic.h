#pragma once

#include "H5public.h"

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL void H5Venable(void);
H5_DLL void H5Vdisable(void);
H5_DLL void H5Vprint(void);
H5_DLL void H5Vreset(void);

#ifdef __cplusplus
}
#endif