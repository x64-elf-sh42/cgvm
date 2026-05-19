#include "emitters.h"
#include "../cgvm.h"

size_t emit_cls(insn_t *code, size_t pc, uint32_t color) {
  (void)color;  
  code[pc++] = MOV_RI(REG_R11, 1024*768);
  // DO A LOOP TO CLEAR SCREEN
  
  return pc;  
}

size_t emit_hline(insn_t *code, size_t pc, uint32_t x, uint32_t y, uint32_t len, uint32_t color) {
  code[pc++] = MOV_RI(REG_R7, 1);
  code[pc++] = MOV_RI(REG_R8, len+x);
  code[pc++] = ADD_RR(REG_R8, REG_R9); // offset
  code[pc++] = MOV_RI(REG_R5, x);
  code[pc++] = ADD_RR(REG_R5, REG_R9); // offset
  code[pc++] = MOV_RI(REG_R6, y);

  size_t loop = pc;
  code[pc++] = PIXEL(color);


  code[pc++] = ADD_RR(REG_R5, REG_R7);
  code[pc++] = CMP_RR(REG_R5, REG_R8);
  code[pc++] = JL(loop);

  return pc;
}

size_t emit_vline(insn_t *code, size_t pc, uint32_t x, uint32_t y, uint32_t len, uint32_t color) {
  code[pc++] = MOV_RI(REG_R7, 1);
  code[pc++] = MOV_RI(REG_R8, len+y);
  code[pc++] = MOV_RI(REG_R5, x);
  code[pc++] = ADD_RR(REG_R5, REG_R9); // offset
  code[pc++] = MOV_RI(REG_R6, y);

  size_t loop = pc;
  code[pc++] = PIXEL(color);
  code[pc++] = ADD_RR(REG_R6, REG_R7);
  code[pc++] = CMP_RR(REG_R6, REG_R8);
  code[pc++] = JL(loop);

  return pc;
}

