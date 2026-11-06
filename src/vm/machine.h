#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "cpu.h"
#include "insn.h"
#include "op.h"
#include "mem.h"
#include "prog.h"

typedef enum { CPU_BUDGET, CPU_YIELD, CPU_HALT, CPU_FAULT, CPU_WAIT } cpu_status_t;
typedef enum { FAULT_NONE, FAULT_FETCH, FAULT_OPCODE, FAULT_DIV_ZERO,
               FAULT_PC_WRITE, FAULT_DEVICE, FAULT_BUS, FAULT_CONVERSION } cpu_fault_t;
typedef struct { cpu_status_t status; size_t executed; } cpu_result_t;
typedef struct { uint8_t red, green, blue; } machine_pixel_t;
#define VIDEO_BASE 0x01000000u
#define CLOCK_BASE 0x02002000u
#define INPUT_BASE 0x02001000u
/* Directional slots retain IDs 0–3; physical WASD keys are unassigned. */
enum { KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_Z, KEY_X, KEY_M, KEY_TAB, KEY_T, KEY_F, INPUT_KEY_COUNT };
#define INPUT_HELD 0u
#define INPUT_PRESSED 0x20u
#define INPUT_RELEASED 0x40u
#define INPUT_ACK 0x60u
#define GPU_BASE 0x02000000u
#define GUEST_HZ 6000000u
#define FRAME_TICKS 100000u
#define VIDEO_WIDTH 1024u
#define VIDEO_HEIGHT 768u
#define VIDEO_PIXELS (VIDEO_WIDTH * VIDEO_HEIGHT)
enum { GPU_WIDTH=0, GPU_HEIGHT=4, GPU_STRIDE=8, GPU_FORMAT=12,
       GPU_STATUS=16, GPU_COMMAND=20, GPU_COLOR=24, GPU_X=28, GPU_Y=32,
       GPU_FRAME=36, GPU_X2=40, GPU_Y2=44, GPU_X3=48, GPU_Y3=52, GPU_Q1=56, GPU_Q2=60, GPU_Q3=64, GPU_DEPTH=68 };
enum { GPU_PRESENT=1, GPU_CLEAR=2, GPU_PIXEL=3, GPU_LINE=4, GPU_TRIANGLE=5 };
typedef struct machine {
  reg_t regs[MAX_REGS];
  fp_reg_t fpregs[MAX_FP_REGS];
  cpu_flags_t flags;
  uint8_t data[DATA_SIZE];
  insn_t code[CODE_SIZE];
  size_t code_len;
  cpu_status_t state;
  cpu_fault_t fault;
  uint32_t fault_address;
  uint64_t ticks, frames;
  bool pending, frame_event, gpu_error;
  bool keys[INPUT_KEY_COUNT], pressed[INPUT_KEY_COUNT], released[INPUT_KEY_COUNT];
  uint32_t color, pixel_x, pixel_y, pixel_x2, pixel_y2, pixel_x3, pixel_y3, q1, q2, q3, depth_enabled;
  double depth[VIDEO_PIXELS];
  machine_pixel_t video[VIDEO_PIXELS], submitted[VIDEO_PIXELS], published[VIDEO_PIXELS];
  uint32_t fault_pc;
  /* Transitional graphics attachment; owned by the caller. */
  machine_pixel_t *fb, *fb2;
  size_t width, height;
} machine_t;
void machine_reset(machine_t *m);
bool machine_load(machine_t *m, const prog_t *p);
cpu_result_t cpu_step(machine_t *m, size_t budget);

bool bus_read(machine_t *m, uint32_t address, unsigned width, uint32_t *value);
bool bus_write(machine_t *m, uint32_t address, unsigned width, uint32_t value);
void machine_advance(machine_t *m, uint64_t ticks);
cpu_result_t machine_step(machine_t *m, size_t budget);

void machine_key(machine_t *m, unsigned key, bool down);
void machine_release_keys(machine_t *m);
