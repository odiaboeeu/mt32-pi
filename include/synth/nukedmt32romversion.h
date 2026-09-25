//
// nukedmt32romversion.h
//

#ifndef _nukedmt32romversion_h
#define _nukedmt32romversion_h

#include "utility.h"

#define ENUM_NUKEDMT32ROMVERSION(ENUM) \
    ENUM(V1_04, 1.04)                  \
    ENUM(V1_05, 1.05)                  \
    ENUM(V1_06, 1.06)                  \
    ENUM(V1_07, 1.07)                  \
    ENUM(V2_04, 2.04)                  \
    ENUM(V2_06, 2.06)                  \
    ENUM(V2_07, 2.07)

CONFIG_ENUM(
    TNukedMT32ROMVersion,
    ENUM_NUKEDMT32ROMVERSION
);

#endif
