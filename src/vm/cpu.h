#pragma once

#include <stdint.h>

#define MAX_REGS 16
#define MAX_FP_REGS 16

typedef uint32_t reg_t;
typedef double fp_reg_t;
typedef uint32_t cpu_flags_t;

typedef enum : uint8_t {
  REG_BP = 0,  // R0
  REG_SP = 1,  // R1
  REG_DS = 2,  // R2
  REG_SS = 3,  // R3
  REG_PC = 4,  // R4
  REG_R5 = 5,
  REG_R6 = 6,
  REG_R7 = 7,
  REG_R8 = 8,
  REG_R9 = 9,
  REG_R10 = 10,
  REG_R11 = 11,
  REG_R12 = 12,
  REG_R13 = 13,
  REG_R14 = 14,
  REG_R15 = 15,
} REG_NAME;

typedef enum : uint8_t {
  FP_REG_DT=0,
} FP_REG_NAME;

