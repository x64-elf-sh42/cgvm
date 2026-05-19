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
  
  // Arithmetic
  OP_ADD_RR,  // Add
  OP_SUB_RR,  // Sub
  OP_MUL_RR,  // Mul
  OP_DIV_RR,  // Div
  
  // branching & comparisson
  OP_CMP_RR,  // Cmp
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


  // End
  OP_MAX_OPERATION,
} op_t;

