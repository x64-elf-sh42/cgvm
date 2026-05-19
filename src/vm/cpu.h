#pragma once

#include <stdint.h>

#define MAX_REGS 32

typedef uint32_t reg_t;
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
  REG_R16 = 16,
  REG_R17 = 17,
  REG_R18 = 18,
  REG_R19 = 19,
  REG_R20 = 20,
  REG_R21 = 21,
  REG_R22 = 22,
  REG_R23 = 23,
  REG_R24 = 24,
  REG_R25 = 25,
  REG_R26 = 26,
  REG_R27 = 27,
  REG_R28 = 28,
  REG_R29 = 29,
  REG_R30 = 30,
  REG_R31 = 31 
} REG_NAME;


