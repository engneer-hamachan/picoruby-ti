#ifndef PICORUBY_TI_NAME_H
#define PICORUBY_TI_NAME_H

#include <prism.h>
#include <stdint.h>

#define TI_NAME_CAPACITY 512
#define TI_NAME_BYTE_CAPACITY 4096

typedef struct {
  uint16_t byte_offset;
  uint16_t byte_length;
} TiName;

int ti_initialize_names(void);
int ti_intern_constant(
  const pm_parser_t *parser,
  pm_constant_id_t constant_id,
  uint16_t *name_id
);
const TiName *ti_get_name(uint16_t name_id);
const uint8_t *ti_get_name_bytes(const TiName *name);

#endif
