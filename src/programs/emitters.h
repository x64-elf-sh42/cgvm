#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../cgvm.h"

size_t emit_line(insn_t *code, size_t pc, uint8_t y, uint32_t color);
