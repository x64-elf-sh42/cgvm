#pragma once

struct prog {
  uint8_t *data;
  size_t datalen;
  insn_t *code;
  size_t codelen;
  uint32_t entry_pc;
};
typedef struct prog prog_t;
