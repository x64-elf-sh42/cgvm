#include "boxdraw.h"

#include "emitters.h"

static insn_t bench_code[MAX_CODE];

prog_t boxdraw(void) {
  size_t pc = 0;
  uint32_t box_dim = 100;
  uint32_t hc = (1024/2)-(box_dim/2);
  uint32_t vc = (768/2)-(box_dim/2);
  uint32_t num_iterations = 2;
  

  bench_code[pc++] = MOV_RI(REG_R13, num_iterations); // num iterations...
  bench_code[pc++] = MOV_RI(REG_R11, 0);
  bench_code[pc++] = MOV_RI(REG_R9, box_dim);

  size_t loop = pc;
  bench_code[pc++] = ENC_I24(OP_FB_CLEAR, 0);
  pc = emit_vline(bench_code, pc, hc, vc, box_dim, 0x33ff44);
  pc = emit_hline(bench_code, pc, hc, vc, box_dim, 0x33ff44);
  pc = emit_vline(bench_code, pc, hc+box_dim, vc, box_dim, 0x33ff44);
  pc = emit_hline(bench_code, pc, hc, vc+box_dim, box_dim, 0x33ff44);

  bench_code[pc++] = ENC_I24(OP_FB_SWAP, 0);
  bench_code[pc++] = CMP_RI(REG_R11, 0);
  size_t jmp_tgt = pc+6;
  bench_code[pc++] = JNE(jmp_tgt);
  bench_code[pc++] = ADD_RI(REG_R9, 1);
  bench_code[pc++] = CMP_RI(REG_R9, box_dim*3);
  bench_code[pc++] = JL(loop);  
  bench_code[pc++] = MOV_RI(REG_R11, 1);
  bench_code[pc++] = JMP(loop);
  
  bench_code[pc++] = SUB_RI(REG_R9, 1);
  bench_code[pc++] = CMP_RI(REG_R9, 0);
  bench_code[pc++] = JG(loop);
  bench_code[pc++] = MOV_RI(REG_R11, 0);

  bench_code[pc++] = SUB_RI(REG_R13, 1);
  bench_code[pc++] = CMP_RI(REG_R13, 0);
  bench_code[pc++] = JG(loop);
  bench_code[pc++] = EXIT();

  return (prog_t) {
    .data = NULL,
    .datalen = 0,
    .code = bench_code,
    .codelen = pc,
  };
}
