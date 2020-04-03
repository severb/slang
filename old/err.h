#ifndef clox_err_h
#define clox_err_h

#include "str.h" // SLICE
#include "val.h" // ERR_PTR_TYPE, REF_FLAG, u_val_, SLICE_PTR_TYPE

#include <stdint.h> // uintptr_t

#define ERROR(err)                                                             \
  (u_val_((ERR_PTR_TYPE | REF_FLAG) +                                          \
          (uintptr_t)&u_val_((SLICE_PTR_TYPE | REF_FLAG) +                     \
                             (uintptr_t)&SLICE(err))))

#endif
