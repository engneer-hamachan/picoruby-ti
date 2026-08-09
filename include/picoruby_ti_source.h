#ifndef PICORUBY_TI_SOURCE_H
#define PICORUBY_TI_SOURCE_H

typedef struct {
  const char *source;
  int source_byte_length;
} TiSource;

typedef struct {
  const TiSource *items;
  int count;
} TiSourceList;

#endif
