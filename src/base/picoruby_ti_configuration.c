#include "picoruby_ti_configuration.h"
#include "picoruby_ti_define_info.h"
#include "picoruby_ti_name.h"
#include "picoruby_ti_t.h"
#include "picoruby_ti_t_frame.h"
#include <stddef.h>

#define TI_INITIAL_ARENA_ALLOCATION_BYTE_SIZE \
  (TI_ARENA_ALIGNED_BYTE_SIZE(sizeof(TiName) * TI_NAME_CAPACITY) + \
   TI_ARENA_ALIGNED_BYTE_SIZE((size_t)TI_NAME_BYTE_CAPACITY) + \
   TI_ARENA_ALIGNED_BYTE_SIZE(sizeof(T) * TI_T_CAPACITY) + \
   TI_ARENA_ALIGNED_BYTE_SIZE(sizeof(TiTFrameEntry) * TI_T_FRAME_CAPACITY) + \
   TI_ARENA_ALIGNED_BYTE_SIZE(sizeof(TiDefineInfo) * TI_DEFINE_INFO_CAPACITY))

_Static_assert(
  TI_INITIAL_ARENA_ALLOCATION_BYTE_SIZE <= TI_ARENA_SIZE,
  "TI_ARENA_SIZE is smaller than the arena bytes that evaluation "
  "initialization allocates for TI_NAME_CAPACITY, TI_NAME_BYTE_CAPACITY, "
  "TI_T_CAPACITY, TI_T_FRAME_CAPACITY, TI_DEFINE_INFO_CAPACITY and "
  "TI_DEFINE_ARG_CAPACITY"
);
