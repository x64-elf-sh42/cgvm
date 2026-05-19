#include "manylines.h"

#include "emitters.h"

static insn_t bench_code[MAX_CODE];

prog_t make_line_bench(void) {
  size_t pc = 0;

  for (uint32_t y = 40; y < 440; y += 4) {
    pc = emit_vline(bench_code, pc, 40, y, 100, 0x33ff44);
  }

  bench_code[pc++] = EXIT();

  return (prog_t) {
    .data = NULL,
    .datalen = 0,
    .code = bench_code,
    .codelen = pc,
  };
}
