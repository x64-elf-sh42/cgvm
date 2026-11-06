#include "emitters.h"
#include "../vm/machine.h"

size_t emit_hline(insn_t *code, size_t pc, uint32_t x, uint32_t y, uint32_t len, uint32_t color) {
  code[pc++] = MOV_RI(REG_R7, 1);
  code[pc++] = MOV_RI(REG_R8, len+x);
  code[pc++] = ADD_RR(REG_R8, REG_R9); // NASTY
  code[pc++] = MOV_RI(REG_R5, x);
  code[pc++] = ADD_RR(REG_R5, REG_R9); // YUCK
  code[pc++] = MOV_RI(REG_R6, y);

  size_t loop = pc;
  pc = emit_gpu_pixel(code, pc, color);


  code[pc++] = ADD_RR(REG_R5, REG_R7);
  code[pc++] = CMP_RR(REG_R5, REG_R8);
  code[pc++] = JL(loop);

  return pc;
}

size_t emit_vline(insn_t *code, size_t pc, uint32_t x, uint32_t y, uint32_t len, uint32_t color) {
  code[pc++] = MOV_RI(REG_R7, 1);
  code[pc++] = MOV_RI(REG_R8, len+y);
  code[pc++] = MOV_RI(REG_R5, x);
  code[pc++] = ADD_RR(REG_R5, REG_R9); // NO!
  code[pc++] = MOV_RI(REG_R6, y);

  size_t loop = pc;
  pc = emit_gpu_pixel(code, pc, color);
  code[pc++] = ADD_RR(REG_R6, REG_R7);
  code[pc++] = CMP_RR(REG_R6, REG_R8);
  code[pc++] = JL(loop);

  return pc;
}


size_t emit_gpu_write(insn_t *code, size_t pc, unsigned offset, unsigned value) {
  code[pc++]=ENC_RI(OP_LUI,0,GPU_BASE>>12);
  code[pc++]=ADD_RI(0,offset);
  code[pc++]=ENC_RI(OP_LUI,1,value>>12);
  code[pc++]=ADD_RI(1,value&4095);
  code[pc++]=ENC_RR(OP_STORE32,1,0);
  return pc;
}
size_t emit_gpu_pixel(insn_t *code, size_t pc, uint32_t color) {
  pc=emit_gpu_write(code,pc,GPU_COLOR,color);
  code[pc++]=ENC_RI(OP_LUI,0,GPU_BASE>>12);
  code[pc++]=ADD_RI(0,GPU_X);
  code[pc++]=ENC_RR(OP_STORE32,5,0);
  code[pc++]=ADD_RI(0,4);
  code[pc++]=ENC_RR(OP_STORE32,6,0);
  return emit_gpu_write(code,pc,GPU_COMMAND,GPU_PIXEL);
}
