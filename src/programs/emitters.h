#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../cgvm.h"

size_t emit_hline(insn_t *code, size_t pc, uint32_t x, uint32_t y, uint32_t len, uint32_t color);
size_t emit_vline(insn_t *code, size_t pc, uint32_t x, uint32_t y, uint32_t len, uint32_t color);
