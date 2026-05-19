#include "manylines.h"

#include "emitters.h"

static insn_t bench_code[MAX_CODE];

prog_t make_line_bench(void) {
  size_t pc = 0;

  for (uint8_t y = 40; y < 220; y += 4) {
    pc = emit_line(bench_code, pc, y, 0x33ff44);
  }

  bench_code[pc++] = EXIT();

  return (prog_t) {
    .data = NULL,
    .datalen = 0,
    .code = bench_code,
    .codelen = pc,
  };
}
