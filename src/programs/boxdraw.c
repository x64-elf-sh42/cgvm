#include "boxdraw.h"
#include "emitters.h"

/* Guest register allocation: R9=x, R10=y, R12/13=maximum x/y.
 * GPU emitters use R0/R1; line drawing uses R5-R8. */
static insn_t code[MAX_CODE];
static size_t read_key(size_t pc, unsigned key, unsigned dst) {
  code[pc++]=ENC_RI(OP_LUI,0,INPUT_BASE>>12);
  code[pc++]=ADD_RI(0,key*4);
  code[pc++]=ENC_RR(OP_LOAD32,dst,0);
  return pc;
}
static size_t move_axis(size_t pc, unsigned position, unsigned limit,
                        unsigned negative, unsigned positive) {
  pc=read_key(pc,negative,2); pc=read_key(pc,positive,3);
  code[pc++]=CMP_RR(2,3);
  size_t idle=pc++; /* Both pressed or both released: no movement. */
  code[pc++]=CMP_RI(2,0);
  size_t forward=pc++;
  code[pc++]=CMP_RI(position,3);
  size_t lower=pc++;
  code[pc++]=SUB_RI(position,3);
  size_t done_negative=pc++;
  code[lower]=JL(pc);
  code[pc++]=MOV_RI(position,0);
  size_t done_clamp=pc++;
  code[forward]=ENC_I24(OP_JE,pc);
  code[pc++]=ADD_RI(position,3);
  code[pc++]=CMP_RR(position,limit);
  size_t within=pc++;
  code[pc++]=ENC_RR(OP_MOV_RR,position,limit);
  code[idle]=ENC_I24(OP_JE,pc);
  code[done_negative]=JMP(pc); code[done_clamp]=JMP(pc);
  code[within]=ENC_I24(OP_JLE,pc);
  return pc;
}
static size_t edge(size_t pc, bool vertical, unsigned offset) {
  code[pc++]=ENC_RR(OP_MOV_RR,5,9);
  code[pc++]=ENC_RR(OP_MOV_RR,6,10);
  code[pc++]=ADD_RI(vertical ? 5 : 6,offset);
  code[pc++]=MOV_RI(7,100);
  size_t loop=pc;
  pc=emit_gpu_pixel(code,pc,0x33ff44);
  code[pc++]=ADD_RI(vertical ? 6 : 5,1);
  code[pc++]=SUB_RI(7,1);
  code[pc++]=CMP_RI(7,0);
  code[pc++]=JG(loop);
  return pc;
}
prog_t boxdraw(void) {
  size_t pc=0;
  /* Query dimensions and derive inclusive top-left limits for a 100x100 box. */
  code[pc++]=ENC_RI(OP_LUI,0,GPU_BASE>>12);
  code[pc++]=ENC_RR(OP_LOAD32,12,0);
  code[pc++]=ADD_RI(0,4);
  code[pc++]=ENC_RR(OP_LOAD32,13,0);
  code[pc++]=SUB_RI(12,100); code[pc++]=SUB_RI(13,100);
  code[pc++]=MOV_RI(2,2);
  code[pc++]=ENC_RR(OP_MOV_RR,9,12); code[pc++]=ENC_RR(OP_DIV_RR,9,2);
  code[pc++]=ENC_RR(OP_MOV_RR,10,13); code[pc++]=ENC_RR(OP_DIV_RR,10,2);
  size_t draw=pc;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_CLEAR);
  pc=edge(pc,false,0); pc=edge(pc,false,99);
  pc=edge(pc,true,0); pc=edge(pc,true,99);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_PRESENT);
  code[pc++]=ENC_OP(OP_WAIT_FRAME);
  pc=move_axis(pc,9,12,KEY_LEFT,KEY_RIGHT);
  pc=move_axis(pc,10,13,KEY_UP,KEY_DOWN);
  code[pc++]=JMP(draw);
  return (prog_t){.code=code,.codelen=pc};
}
