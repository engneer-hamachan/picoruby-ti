#ifndef PICORUBY_TI_T_FRAME_H
#define PICORUBY_TI_T_FRAME_H

#include <stdint.h>

int ti_initialize_t_frame(void);
int ti_set_value_t(uint16_t name_id, uint16_t t_node_index);
uint16_t ti_get_value_t(uint16_t name_id);
int ti_set_method_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
);
uint16_t ti_get_method_t(uint8_t object_class_id, uint16_t name_id);
int ti_set_instance_variable_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
);
uint16_t ti_get_instance_variable_t(
  uint8_t object_class_id,
  uint16_t name_id
);
int ti_find_instance_variable_and_advance_slot(
  uint8_t object_class_id,
  int *search_slot_index,
  uint16_t *name_id,
  uint16_t *t_node_index
);

#endif
