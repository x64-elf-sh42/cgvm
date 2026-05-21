#include "io.h"

#include <stdlib.h>

io_port_t io_in_ports[IO_PORT_MAX] = {0};
io_port_t io_out_ports[IO_PORT_MAX] = {0};

bool io_port_create(uint8_t port_index, uint8_t op_size, IO_DIRECTION direction, io_handler_t handler) {
  io_port_t *io_list = nullptr;
  switch(direction) {
    case IO_DIRECTION_IN:
      io_list = &io_in_ports[0];
      break;
    case IO_DIRECTION_OUT:
      io_list = &io_out_ports[0];
      break;
    default:
      return false;
  }

  io_list[port_index].buffer.v = calloc(IO_MAX_BACKLOG, op_size);
  io_list[port_index].op_size = op_size;
  io_list[port_index].handler = handler;
  io_list[port_index].pending_events=false;
    
  return true;
}
void io_port_destroy(uint8_t port_index, IO_DIRECTION direction) {
  io_port_t *io_list = nullptr;
  switch(direction) {
    case IO_DIRECTION_IN:
      io_list = &io_in_ports[0];
      break;
    case IO_DIRECTION_OUT:
      io_list = &io_out_ports[0];
      break;
    default:
      return;
  }

  if(io_list[port_index].buffer.v) {
    free(io_list[port_index].buffer.v);
  }
  io_list[port_index].handler = nullptr;
  io_list[port_index].op_size = 0;
  io_list[port_index].pending_events=false;
  return;  
}

io_space_t io_space_get(void) {
  return (io_space_t){&io_in_ports[0], &io_out_ports[0]};
}
