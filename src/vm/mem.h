#pragma once


typedef union {
  uint8_t *b;
  uint16_t *w;
  uint32_t *d;
  uint64_t *q;
  void *v;
} mem_ptr_t;

#define MB_1 (1 * 1024 * 1024)
#define DATA_SIZE (MB_1 * 16)
#define CODE_SIZE (MB_1 * 16)


