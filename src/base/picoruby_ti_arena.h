#ifndef PICORUBY_TI_ARENA_H
#define PICORUBY_TI_ARENA_H

#include "picoruby_ti_configuration.h"
#include <stddef.h>

void ti_reset_arena(void);
void *ti_allocate_from_arena(size_t size);
int ti_did_arena_overflow(void);

#endif
