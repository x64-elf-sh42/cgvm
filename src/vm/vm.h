#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "insn.h"
#include "prog.h"
#include "cpu.h"
struct vm { struct gpu *gpu; };
typedef struct vm vm_t;
#include "gpu.h"
void vm_reset(void);
void vm_load(const prog_t *p);
bool vm_run(void);
