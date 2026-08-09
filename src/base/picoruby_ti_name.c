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
ti_intern_constant(
  const pm_parser_t *parser,
  pm_constant_id_t constant_id,
  uint16_t *name_id
) {

  if (!parser || constant_id == 0 || !name_id)
    return 0;

  const pm_constant_t *constant =
    pm_constant_pool_id_to_constant(&parser->constant_pool, constant_id);

  if (!constant || constant->length > UINT16_MAX)
    return 0;

  for (int index = 0; index < s_name_count; index++) {
    const TiName *name = &s_names[index];

    if (
      name->byte_length == constant->length &&
      memcmp(
        s_name_bytes + name->byte_offset,
        constant->start,
        constant->length
      ) == 0
    ) {

      *name_id = (uint16_t)(index + 1);

      return 1;
    }
  }

  if (
    s_name_count >= TI_NAME_CAPACITY ||
    constant->length > (size_t)(TI_NAME_BYTE_CAPACITY - s_name_byte_length)
  ) {

    return 0;
  }

  memcpy(
    s_name_bytes + s_name_byte_length,
    constant->start,
    constant->length
  );

  TiName *name = &s_names[s_name_count++];
  name->byte_offset = s_name_byte_length;
  name->byte_length = (uint16_t)constant->length;

  s_name_byte_length =
    (uint16_t)(s_name_byte_length + constant->length);

  *name_id = (uint16_t)s_name_count;

  return 1;
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
