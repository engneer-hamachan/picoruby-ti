#ifndef PICORUBY_TI_TYPE_H
#define PICORUBY_TI_TYPE_H

#include <stddef.h>
#include <stdint.h>

#define TI_TYPE_STRING_CAPACITY 96

int ti_type_to_string(
  uint16_t t_node_index,
  char *buffer,
  size_t capacity
);

#endif
