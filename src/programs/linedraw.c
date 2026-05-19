#include "../cgvm.h"
/*
 > Title: Line Draw
 > descr: draws line from 100,100 -> 200,100.
------------
data:
 0:    100
datalen: 1
code:
 0:    mov r7, 1           ; amount to increment x by per loop iteration
 1:    mov r8, 200         ; target amount for x
 2:    mov r5, byte [0]    ; (x=100)
 3:    mov r6, byte [0]    ; (y=100)
 4:    set_pixel 0x33ff44  ; set pixel at (r5,r6) to provided colour.
 5:    add r5, r7          ; (x+=1)
 6:    cmp r5, r8          ; if x<200
 7:    jl 4                ; jmp to code[4] (set_pixel)
 8:    exit                ; exit vm      
codelen: 9
------------
*/

const prog_t line_draw = {
  .data=(uint8_t [1]){100},
  .datalen=1,
  .code=(insn_t [9]) {
     MOV_RI(REG_R7, 1),        // used to increment. might want inc/dec insns
    MOV_RI(REG_R8, 200),      // target value (100 -> 200)
    MOV_MR(REG_R5, 0),        // read data[0] (100) into R5 as x coordinate
    MOV_MR(REG_R6, 0),        // read data[0] (100) into R6 as y coordinate 
    PIXEL(0x33ff44),          // draw pixel at x,y with colour 0x33ff44
    ADD_RR(REG_R5, REG_R7),   // increment r5
    CMP_RR(REG_R5, REG_R8),   // check if r5 is at our target value
    JL(4),                    // loop back to set_pixel while r5 < r8
    EXIT(),  
  },
  .codelen=9
};
