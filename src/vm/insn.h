#pragma once

#include <stdint.h>

typedef uint32_t insn_t;

// decode
#define OP(i)       ((uint8_t)((i) >> 24))
#define RR_DST(i)   (((i) >> 20) & 0xF)
#define RR_SRC(i)   (((i) >> 16) & 0xF)
#define IMM16(i)    ((i) & 0xFFFF)
#define RI_DST(i)   (((i) >> 20) & 0xF)
#define IMM20(i)    ((i) & 0xFFFFF)
#define IMM24(i)    ((i) & 0xFFFFFF)

// encode
#define ENC_RR(op, dst, src) \
    (((uint32_t)(op)  << 24) | \
     ((uint32_t)(dst) << 20) | \
     ((uint32_t)(src) << 16))

#define ENC_RI(op, dst, imm20) \
    (((uint32_t)(op)  << 24) | \
     ((uint32_t)(dst) << 20) | \
     ((uint32_t)(imm20) & 0xFFFFF))

#define ENC_I24(op, imm24) \
    (((uint32_t)(op) << 24) | \
     ((uint32_t)(imm24) & 0xFFFFFF))

#define ENC_OP(op) \
    ((uint32_t)(op) << 24)

// emit macros
//
#define MOV_RI(r, v)   ENC_RI(OP_MOV_RI, r, v)
#define MOV_MR(r, a)   ENC_RI(OP_MOV_MR, r, a)
#define ADD_RR(a, b)   ENC_RR(OP_ADD_RR, a, b)
#define ADD_RI(a, v)   ENC_RI(OP_ADD_RI, a, v)
#define SUB_RI(a, v)   ENC_RI(OP_SUB_RI, a, v)
#define CMP_RR(a, b)   ENC_RR(OP_CMP_RR, a, b)
#define CMP_RI(a, v)   ENC_RI(OP_CMP_RI, a, v)
#define JL(addr)       ENC_I24(OP_JL, addr)
#define JG(addr)       ENC_I24(OP_JG, addr)
#define JNE(addr)      ENC_I24(OP_JNE, addr)
#define JMP(addr)      ENC_I24(OP_JMP, addr)
#define PIXEL(rgb)     ENC_I24(OP_SET_PIXEL, rgb)
#define EXIT()         ENC_OP(OP_EXIT)
