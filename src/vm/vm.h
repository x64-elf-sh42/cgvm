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

typedef void *(*system_thread)(void*);
struct gpu;

struct vm {
  cpu_flags_t flags;
  reg_t regs[MAX_REGS];
  uint8_t data[DATA_SIZE];
  insn_t code[CODE_SIZE];
  struct gpu *gpu;

  pthread_t sys_thread;
  pthread_attr_t sys_tattr;
  system_thread run_system;
  atomic_bool running; // should be atomic
  bool program_loaded;
  bool reset;
};
typedef struct vm vm_t;

void vm_reset();
void vm_load(const prog_t *p);
bool vm_run();
