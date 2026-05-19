#pragma once

#include <stdint.h>
#include "../gfx/gfx.h"

#include "op.h"
#include "insn.h"
#include "cpu.h"
#include "mem.h"
#include "prog.h"
struct vm {
  cpu_flags_t flags;
  reg_t regs[MAX_REGS];
  uint8_t data[DATA_SIZE];
  insn_t code[CODE_SIZE];
  pixel_t fb[FB_SIZE];
};
typedef struct vm vm_t;

void vm_reset();
void vm_load(const prog_t *p);
bool vm_run();
