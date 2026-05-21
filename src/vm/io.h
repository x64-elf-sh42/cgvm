#pragma once

#include "mem.h"
#include <stdint.h>

#define IO_MAX_BACKLOG 32
#define IO_PORT_MAX 256

typedef void *(*io_handler_t)(void *);

struct io_port {
  mem_ptr_t buffer;
  io_handler_t handler;
  uint8_t op_size; // 1/2/4/8 bytes per I/O
  bool is_used;
  bool pending_events;
};
typedef struct io_port io_port_t;

typedef enum : bool {
  IO_DIRECTION_IN = 0,
  IO_DIRECTION_OUT = 1,
} IO_DIRECTION;

struct io_space {
  io_port_t *in_ports;
  io_port_t *out_ports;
};
typedef struct io_space io_space_t;

bool io_port_create(uint8_t port_index, uint8_t op_size, IO_DIRECTION direction, io_handler_t handler);
void io_port_destroy(uint8_t port_index, IO_DIRECTION direction);
io_space_t io_space_get(void);
