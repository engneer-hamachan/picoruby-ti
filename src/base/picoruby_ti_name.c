#include "picoruby_ti_name.h"
#include "picoruby_ti_arena.h"
#include <stddef.h>
#include <string.h>

static TiName *s_names;
static uint8_t *s_name_bytes;
static int s_name_count;
static uint16_t s_name_byte_length;

int
ti_initialize_names(void) {
  s_names = ti_allocate_from_arena(sizeof(TiName) * TI_NAME_CAPACITY);
  s_name_bytes = ti_allocate_from_arena(TI_NAME_BYTE_CAPACITY);

  if (!s_names || !s_name_bytes)
    return 0;

  s_name_count = 0;
  s_name_byte_length = 0;

  return 1;
}

int
ti_intern_concatenated_bytes(
  const uint8_t *first_bytes,
  size_t first_byte_length,
  const uint8_t *second_bytes,
  size_t second_byte_length,
  uint16_t *name_id
) {

  if (!first_bytes || !name_id)
    return 0;

  size_t name_byte_length = first_byte_length + second_byte_length;

  if (name_byte_length > UINT16_MAX)
    return 0;

  for (int index = 0; index < s_name_count; index++) {
    const TiName *name = &s_names[index];

    if (name->byte_length != name_byte_length)
      continue;

    const uint8_t *name_bytes = s_name_bytes + name->byte_offset;

    if (memcmp(name_bytes, first_bytes, first_byte_length) != 0)
      continue;

    if (
      second_byte_length > 0 &&
      memcmp(
        name_bytes + first_byte_length,
        second_bytes,
        second_byte_length
      ) != 0
    ) {

      continue;
    }

    *name_id = (uint16_t)(index + 1);

    return 1;
  }

  if (
    s_name_count >= TI_NAME_CAPACITY ||
    name_byte_length > (size_t)(TI_NAME_BYTE_CAPACITY - s_name_byte_length)
  ) {

    return 0;
  }

  memcpy(s_name_bytes + s_name_byte_length, first_bytes, first_byte_length);

  if (second_byte_length > 0) {
    memcpy(
      s_name_bytes + s_name_byte_length + first_byte_length,
      second_bytes,
      second_byte_length
    );
  }

  TiName *name = &s_names[s_name_count++];
  name->byte_offset = s_name_byte_length;
  name->byte_length = (uint16_t)name_byte_length;

  s_name_byte_length = (uint16_t)(s_name_byte_length + name_byte_length);

  *name_id = (uint16_t)s_name_count;

  return 1;
}

int
ti_intern_constant(
  const pm_parser_t *parser,
  pm_constant_id_t constant_id,
  uint16_t *name_id
) {

  if (!parser || constant_id == 0)
    return 0;

  const pm_constant_t *constant =
    pm_constant_pool_id_to_constant(&parser->constant_pool, constant_id);

  if (!constant)
    return 0;

  return ti_intern_concatenated_bytes(
    constant->start,
    constant->length,
    NULL,
    0,
    name_id
  );
}

const TiName *
ti_get_name(uint16_t name_id) {
  if (name_id == 0 || name_id > s_name_count)
    return NULL;

  return &s_names[name_id - 1];
}

const uint8_t *
ti_get_name_bytes(const TiName *name) {
  if (!name || name->byte_offset >= s_name_byte_length)
    return NULL;

  return s_name_bytes + name->byte_offset;
}
