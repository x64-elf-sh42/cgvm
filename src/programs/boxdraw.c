#include "boxdraw.h"

#include "emitters.h"

static insn_t bench_code[MAX_CODE];

prog_t boxdraw(void) {
  size_t pc = 0;
  uint32_t hc = (1024/2)-50;
  uint32_t vc = (768/2)-50;
  
  pc = emit_vline(bench_code, pc, hc, vc, 100, 0x33ff44);
  pc = emit_hline(bench_code, pc, hc, vc, 100, 0x33ff44);
  pc = emit_vline(bench_code, pc, hc+100, vc, 100, 0x33ff44);
  pc = emit_hline(bench_code, pc, hc, vc+100, 100, 0x33ff44);



  bench_code[pc++] = EXIT();

  return (prog_t) {
    .data = NULL,
    .datalen = 0,
    .code = bench_code,
    .codelen = pc,
  };
}
