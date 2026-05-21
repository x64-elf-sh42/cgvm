#pragma once

#include <stdint.h>
#include "../gfx/gfx.h"

#include "op.h"
#include "insn.h"
#include "cpu.h"
#include "mem.h"
#include "prog.h"
#include <stdatomic.h>
#include <pthread.h>
#include "gpu.h"
#include "io.h"
#include "../util.h"

typedef void *(*system_thread)(void*);
struct gpu;

struct vm {
  cpu_flags_t flags;
  reg_t regs[MAX_REGS];
  fp_reg_t fpregs[MAX_FP_REGS];
  uint8_t data[DATA_SIZE];
  insn_t code[CODE_SIZE];
  struct gpu *gpu;
  io_space_t io_space;
  pthread_t sys_thread;
  pthread_attr_t sys_tattr;
  system_thread run_system;
  size_t time_slice_length;
  atomic_bool running; // should be atomic
  bool program_loaded;
  bool reset;
};
typedef struct vm vm_t;

void vm_reset();
void vm_load(const prog_t *p);
bool vm_run();
