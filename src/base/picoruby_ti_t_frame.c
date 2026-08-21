#include "picoruby_ti_t_frame.h"
#include "picoruby_ti_arena.h"
#include "picoruby_ti_builtin_database.h"
#include "picoruby_ti_t.h"
#include <stddef.h>
#include <string.h>

typedef enum {
  TI_T_FRAME_VALUE,
  TI_T_FRAME_METHOD,
  TI_T_FRAME_INSTANCE_VARIABLE,
} TiTFrameEntryKind;

static TiTFrameEntry *t_frame;

int
ti_initialize_t_frame(void) {
  t_frame = ti_allocate_from_arena(sizeof(TiTFrameEntry) * TI_T_FRAME_CAPACITY);

  if (!t_frame)
    return 0;

  memset(t_frame, 0, sizeof(TiTFrameEntry) * TI_T_FRAME_CAPACITY);

  return 1;
}

static TiTFrameEntry *
find_t_frame_entry(
  uint8_t object_class_id,
  uint16_t name_id,
  TiTFrameEntryKind entry_kind
) {

  unsigned int index =
    (name_id + object_class_id + (unsigned int)entry_kind) %
    TI_T_FRAME_CAPACITY;

  for (int checked_slot_count = 0; checked_slot_count < TI_T_FRAME_CAPACITY;
       checked_slot_count++) {

    TiTFrameEntry *entry = &t_frame[index];

    if (
      entry->name_id == 0 ||
      (entry->name_id == name_id &&
       entry->object_class_id == object_class_id &&
       entry->entry_kind == entry_kind)
    ) {

      return entry;
    }

    index = (index + 1U) % TI_T_FRAME_CAPACITY;
  }

  return NULL;
}

static int
set_t_frame_entry_t(
  uint8_t object_class_id,
  uint16_t name_id,
  TiTFrameEntryKind entry_kind,
  uint16_t t_node_index
) {

  if (name_id == 0 || t_node_index == 0)
    return 1;

  TiTFrameEntry *entry =
    find_t_frame_entry(object_class_id, name_id, entry_kind);

  if (!entry)
    return 0;

  if (entry->name_id == 0) {
    entry->name_id = name_id;
    entry->t_node_index = t_node_index;
    entry->object_class_id = object_class_id;
    entry->entry_kind = (uint8_t)entry_kind;

    return 1;
  }

  uint16_t union_t_node_index =
    ti_make_union(entry->t_node_index, t_node_index);

  if (union_t_node_index == 0)
    return 0;

  entry->t_node_index = union_t_node_index;

  return 1;
}

static uint16_t
get_t_frame_entry_t(
  uint8_t object_class_id,
  uint16_t name_id,
  TiTFrameEntryKind entry_kind
) {

  if (name_id == 0)
    return 0;

  TiTFrameEntry *entry =
    find_t_frame_entry(object_class_id, name_id, entry_kind);

  if (!entry || entry->name_id != name_id)
    return 0;

  return entry->t_node_index;
}

int
ti_set_value_t(uint16_t name_id, uint16_t t_node_index) {
  return set_t_frame_entry_t(
    TI_CLASS_NONE,
    name_id,
    TI_T_FRAME_VALUE,
    t_node_index
  );
}

uint16_t
ti_get_value_t(uint16_t name_id) {
  return get_t_frame_entry_t(TI_CLASS_NONE, name_id, TI_T_FRAME_VALUE);
}

int
ti_set_method_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  return set_t_frame_entry_t(
    object_class_id,
    name_id,
    TI_T_FRAME_METHOD,
    t_node_index
  );
}

uint16_t
ti_get_method_t(uint8_t object_class_id, uint16_t name_id) {
  return get_t_frame_entry_t(object_class_id, name_id, TI_T_FRAME_METHOD);
}

int
ti_set_instance_variable_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  return set_t_frame_entry_t(
    object_class_id,
    name_id,
    TI_T_FRAME_INSTANCE_VARIABLE,
    t_node_index
  );
}

uint16_t
ti_get_instance_variable_t(uint8_t object_class_id, uint16_t name_id) {
  return get_t_frame_entry_t(
    object_class_id,
    name_id,
    TI_T_FRAME_INSTANCE_VARIABLE
  );
}

int
ti_find_instance_variable_and_advance_slot(
  uint8_t object_class_id,
  int *search_slot_index,
  uint16_t *name_id,
  uint16_t *t_node_index
) {

  if (
    !search_slot_index || !name_id || !t_node_index ||
    *search_slot_index < 0
  ) {

    return 0;
  }

  for (
    int slot_index = *search_slot_index;
    slot_index < TI_T_FRAME_CAPACITY;
    slot_index++
  ) {

    const TiTFrameEntry *entry = &t_frame[slot_index];

    if (
      entry->name_id == 0 ||
      entry->entry_kind != TI_T_FRAME_INSTANCE_VARIABLE ||
      entry->object_class_id != object_class_id
    ) {

      continue;
    }

    *name_id = entry->name_id;
    *t_node_index = entry->t_node_index;
    *search_slot_index = slot_index + 1;

    return 1;
  }

  return 0;
}
