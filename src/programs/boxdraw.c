#include "boxdraw.h"

#include "emitters.h"

static insn_t bench_code[MAX_CODE];

prog_t boxdraw(void) {
  size_t pc = 0;
  uint32_t hc = (1024/2)-50;
  uint32_t vc = (768/2)-50;

  bench_code[pc++] = MOV_RI(REG_R13, 10); // num iterations...
  bench_code[pc++] = MOV_RI(REG_R11, 0);
  bench_code[pc++] = MOV_RI(REG_R12, 0);
  bench_code[pc++] = MOV_RI(REG_R9, 100);
  bench_code[pc++] = MOV_RI(REG_R10, 300);

  size_t loop = pc;
  bench_code[pc++] = ENC_I24(OP_FB_CLEAR, 0);
  pc = emit_vline(bench_code, pc, hc, vc, 100, 0x33ff44);
  pc = emit_hline(bench_code, pc, hc, vc, 100, 0x33ff44);
  pc = emit_vline(bench_code, pc, hc+100, vc, 100, 0x33ff44);
  pc = emit_hline(bench_code, pc, hc, vc+100, 100, 0x33ff44);

  bench_code[pc++] = CMP_RR(REG_R11, REG_R12);
  size_t jmp_tgt = pc+6;
  bench_code[pc++] = JNE(jmp_tgt);
  bench_code[pc++] = ADD_RI(REG_R9, 1);
  bench_code[pc++] = CMP_RR(REG_R9, REG_R10);
  bench_code[pc++] = JL(loop);  
  bench_code[pc++] = MOV_RI(REG_R11, 1);
  bench_code[pc++] = JMP(loop);
  
  bench_code[pc++] = SUB_RI(REG_R9, 1);
  bench_code[pc++] = CMP_RR(REG_R9, REG_R12);
  bench_code[pc++] = JG(loop);
  bench_code[pc++] = MOV_RI(REG_R11, 0);

  bench_code[pc++] = SUB_RI(REG_R13, 1);
  bench_code[pc++] = CMP_RR(REG_R13, REG_R12);
  bench_code[pc++] = JG(loop);
  bench_code[pc++] = EXIT();


  return (prog_t) {
    .data = NULL,
    .datalen = 0,
    .code = bench_code,
    .codelen = pc,
  };
}
