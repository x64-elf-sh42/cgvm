#include "tetrahedron.h"
#include "emitters.h"
#include <assert.h>
/* Constant data is binary32 little-endian; projection scratch begins at 4096. */
#include "tetrahedron_data.h"
static insn_t code[4096];
static size_t pc;
static void e(insn_t ins) { assert(pc<4096); code[pc++]=ins; }
static void f(unsigned op,unsigned d,unsigned s) { e(ENC_RR(op,d,s)); }
static void loadf(unsigned d,unsigned address) { e(MOV_RI(0,address)); f(OP_FLOAD32,d,0); }
static void copy(unsigned d,unsigned s) { f(OP_FMOV_RR,d,s); }
static void key(unsigned k,unsigned d) {
  e(ENC_RI(OP_LUI,0,INPUT_BASE>>12)); e(ADD_RI(0,k*4)); f(OP_LOAD32,d,0);
}
static void control(unsigned reg,unsigned negative,unsigned positive,unsigned low,unsigned high,bool wrap) {
  key(negative,2); key(positive,3); e(CMP_RR(2,3)); size_t idle=pc++; e(CMP_RI(2,0)); size_t plus=pc++;
  e(CMP_RI(reg,low)); size_t bottom=pc++;
  e(SUB_RI(reg,1)); size_t minusdone=pc++;
  code[bottom]=ENC_I24(OP_JLE,pc); e(MOV_RI(reg,wrap?high:low)); size_t bottomdone=pc++;
  code[plus]=ENC_I24(OP_JE,pc); e(CMP_RI(reg,high)); size_t top=pc++;
  e(ADD_RI(reg,1)); size_t plusdone=pc++;
  code[top]=ENC_I24(OP_JGE,pc); e(MOV_RI(reg,wrap?low:high));
  code[idle]=ENC_I24(OP_JE,pc); code[minusdone]=JMP(pc); code[bottomdone]=JMP(pc); code[plusdone]=JMP(pc);
}
static void trig(unsigned index,unsigned sinreg,unsigned cosreg) {
  f(OP_MOV_RR,0,index); e(MOV_RI(1,4)); f(OP_MUL_RR,0,1);
  f(OP_FLOAD32,sinreg,0); e(ADD_RI(0,1024)); f(OP_FLOAD32,cosreg,0);
}
static void register_from_memory(unsigned offset,unsigned address) {
  e(MOV_RI(0,address)); f(OP_LOAD32,1,0);
  e(ENC_RI(OP_LUI,0,GPU_BASE>>12)); e(ADD_RI(0,offset)); f(OP_STORE32,1,0);
}
prog_t tetrahedron(void) {
  pc=0;
  /* R9 yaw index; R10 pitch index; R11 distance in hundredths. */
  e(MOV_RI(9,24)); e(MOV_RI(10,12)); e(MOV_RI(11,600));
  size_t draw=pc;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0); pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_CLEAR);
  trig(9,0,1); trig(10,2,3);
  f(OP_ITOF,4,11); loadf(15,2096); f(OP_FMUL_RR,4,15); /* distance * .01 */
  for(unsigned v=0;v<4;v++) {
    loadf(5,2048+v*12); loadf(6,2052+v*12); loadf(7,2056+v*12);
    /* yaw: x'=x*cos+z*sin; z'=z*cos-x*sin */
    copy(8,5); f(OP_FMUL_RR,8,1); copy(9,7); f(OP_FMUL_RR,9,0); f(OP_FADD_RR,8,9);
    copy(10,7); f(OP_FMUL_RR,10,1); copy(9,5); f(OP_FMUL_RR,9,0); f(OP_FSUB_RR,10,9);
    /* pitch: y'=y*cos-z'*sin; depth=z'*cos+y*sin+distance */
    copy(11,6); f(OP_FMUL_RR,11,3); copy(9,10); f(OP_FMUL_RR,9,2); f(OP_FSUB_RR,11,9);
    f(OP_FMUL_RR,10,3); copy(9,6); f(OP_FMUL_RR,9,2); f(OP_FADD_RR,10,9); f(OP_FADD_RR,10,4);
    loadf(15,2100); f(OP_FMUL_RR,8,15); f(OP_FDIV_RR,8,10);
    f(OP_FMUL_RR,11,15); f(OP_FDIV_RR,11,10);
    loadf(15,2104); f(OP_FADD_RR,8,15);
    loadf(15,2108); f(OP_FSUB_RR,15,11);
    f(OP_FTOI,5,8); f(OP_FTOI,6,15);
    e(MOV_RI(0,4096+v*8)); f(OP_STORE32,5,0); e(ADD_RI(0,4)); f(OP_STORE32,6,0);
  }
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  for(unsigned a=0;a<4;a++) for(unsigned b=a+1;b<4;b++) {
    register_from_memory(GPU_X,4096+a*8); register_from_memory(GPU_Y,4100+a*8);
    register_from_memory(GPU_X2,4096+b*8); register_from_memory(GPU_Y2,4100+b*8);
    pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  }
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_PRESENT); e(ENC_OP(OP_WAIT_FRAME));
  control(9,KEY_LEFT,KEY_RIGHT,0,255,true); control(10,KEY_UP,KEY_DOWN,0,255,true);
  control(11,KEY_X,KEY_Z,350,1200,false);
  e(JMP(draw));
  return (prog_t){.code=code,.codelen=pc,.data=tetra_data,.datalen=sizeof(tetra_data)};
}
