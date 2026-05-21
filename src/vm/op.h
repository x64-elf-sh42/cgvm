#pragma once
#include <stdint.h>

typedef enum : uint8_t {
  OP_UNDEFINED = 0,

  // VM Exits
  OP_EXIT,
  
  // data movement
  OP_MOV_RR,  // Mov
  OP_MOV_RI,
  OP_MOV_RM,  // 20-bits mem ptr
  OP_MOV_MR,  // 20-bits mem ptr
  
  // Integer Arithmetic
  OP_ADD_RR,  // Add
  OP_ADD_RI,
  OP_SUB_RR,  // Sub
  OP_SUB_RI,
  OP_MUL_RR,  // Mul
  OP_DIV_RR,  // Div

  // Floating-point Arithmetic
  OP_FADD_RR,
  OP_FSUB_RR,
  OP_FMUL_RR,
  OP_FDIV_RR,

  
  // branching & comparisson
  OP_CMP_RR,  // Cmp
  OP_CMP_RI,
  OP_FCMP_RR,

  OP_JMP,   // Jmp -> [Reg]
  OP_JE,    // 
  OP_JNE,
  OP_JG,
  OP_JL,
  OP_JGE,
  OP_JLE,

  // Logic
  // ..

  // state & control (dseg,flags etc.)
  // ..

  // graphics
  OP_SET_PIXEL,
  OP_FB_CLEAR,
  OP_FB_SWAP,

  // End
  OP_MAX_OPERATION,
} op_t;

