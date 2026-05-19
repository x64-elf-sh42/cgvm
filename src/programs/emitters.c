#include "emitters.h"
#include "../cgvm.h"
size_t emit_line(insn_t *code, size_t pc, uint8_t y, uint32_t color) {
  code[pc++] = MOV_RI(REG_R7, 1);
  code[pc++] = MOV_RI(REG_R8, 900);
  code[pc++] = MOV_RI(REG_R5, 100);
  code[pc++] = MOV_RI(REG_R6, y);

  size_t loop = pc;
  code[pc++] = PIXEL(color);
  code[pc++] = ADD_RR(REG_R5, REG_R7);
  code[pc++] = CMP_RR(REG_R5, REG_R8);
  code[pc++] = JL(loop);

  return pc;
}

