#include "flight.h"
#include "emitters.h"
#include <assert.h>
#include <string.h>
/* Constant data is binary32 little-endian; projection scratch begins at 4096. */
#include "flight_data.h"
#include "solid_data.h"
static uint8_t scene_data[65536];
static insn_t code[CODE_SIZE];
static size_t pc;
static void e(insn_t ins) { assert(pc<sizeof(code)/sizeof(code[0])); code[pc++]=ins; }
static void f(unsigned op,unsigned d,unsigned s) { e(ENC_RR(op,d,s)); }
static void loadf(unsigned d,unsigned address) { e(MOV_RI(0,address)); f(OP_FLOAD32,d,0); }
static void copy(unsigned d,unsigned s) { f(OP_FMOV_RR,d,s); }
static void key(unsigned k,unsigned d) {
  e(ENC_RI(OP_LUI,0,INPUT_BASE>>12)); e(ADD_RI(0,k*4)); f(OP_LOAD32,d,0);
}
static void control(unsigned reg,unsigned negative,unsigned positive,unsigned low,unsigned high,bool wrap) {
  key(negative,2); key(positive,3); e(CMP_RR(2,3)); size_t idle=pc++; e(CMP_RI(2,0)); size_t plus=pc++;
  e(CMP_RI(reg,low)); size_t bottom=pc++;
  e(SUB_RI(reg,1)); size_t minusdone=pc++;
  code[bottom]=ENC_I24(OP_JLE,pc); e(MOV_RI(reg,wrap?high:low)); size_t bottomdone=pc++;
  code[plus]=ENC_I24(OP_JE,pc); e(CMP_RI(reg,high)); size_t top=pc++;
  e(ADD_RI(reg,1)); size_t plusdone=pc++;
  code[top]=ENC_I24(OP_JGE,pc); e(MOV_RI(reg,wrap?low:high));
  code[idle]=ENC_I24(OP_JE,pc); code[minusdone]=JMP(pc); code[bottomdone]=JMP(pc); code[plusdone]=JMP(pc);
}
static void trig(unsigned index,unsigned sinreg,unsigned cosreg) {
  f(OP_MOV_RR,0,index); e(MOV_RI(1,4)); f(OP_MUL_RR,0,1);
  f(OP_FLOAD32,sinreg,0); e(ADD_RI(0,1024)); f(OP_FLOAD32,cosreg,0);
}
/* RAM constants follow the trig tables. Persistent position is at 8192. */
enum { ZERO=2048, NEAR=2052, FAR=2056, FOCAL=2060, CX=2064, CY=2068,
       CHASE=2072, STEP=2076, POS=8192 };
static void storef(unsigned src,unsigned address) { e(MOV_RI(0,address)); f(OP_FSTORE32,src,0); }
static void gpu_reg(unsigned offset,unsigned reg) {
  e(ENC_RI(OP_LUI,0,GPU_BASE>>12)); e(ADD_RI(0,offset)); f(OP_STORE32,reg,0);
}
/* World to camera: inverse yaw, inverse pitch, then chase displacement. */
static void transform(unsigned vertex,unsigned dst) {
  loadf(11,vertex); loadf(12,vertex+4); loadf(13,vertex+8);
  loadf(14,POS); f(OP_FSUB_RR,11,14);
  loadf(14,POS+4); f(OP_FSUB_RR,12,14);
  loadf(14,POS+8); f(OP_FSUB_RR,13,14);
  copy(dst,11); f(OP_FMUL_RR,dst,1); copy(14,13); f(OP_FMUL_RR,14,0); f(OP_FSUB_RR,dst,14);
  f(OP_FMUL_RR,11,0); f(OP_FMUL_RR,13,1); f(OP_FADD_RR,11,13);
  copy(dst+1,11); f(OP_FMUL_RR,dst+1,2); copy(14,12); f(OP_FMUL_RR,14,3); f(OP_FADD_RR,dst+1,14);
  copy(dst+2,11); f(OP_FMUL_RR,dst+2,3); f(OP_FMUL_RR,12,2); f(OP_FSUB_RR,dst+2,12);
  loadf(14,CHASE); f(OP_FADD_RR,dst+2,14);
}
static void clip_endpoint(unsigned a,unsigned b) {
  /* t=(near-az)/(bz-az); a.xy += t*(b.xy-a.xy); a.z=near */
  loadf(11,NEAR); f(OP_FSUB_RR,11,a+2);
  copy(12,b+2); f(OP_FSUB_RR,12,a+2); f(OP_FDIV_RR,11,12);
  for(unsigned k=0;k<2;k++) { copy(12,b+k); f(OP_FSUB_RR,12,a+k); f(OP_FMUL_RR,12,11); f(OP_FADD_RR,a+k,12); }
  loadf(a+2,NEAR);
}
static void project(unsigned a,unsigned xreg,unsigned yreg) {
  loadf(15,FOCAL); f(OP_FMUL_RR,a,15); f(OP_FDIV_RR,a,a+2);
  f(OP_FMUL_RR,a+1,15); f(OP_FDIV_RR,a+1,a+2);
  loadf(15,CX); f(OP_FADD_RR,a,15);
  loadf(15,CY); f(OP_FSUB_RR,15,a+1);
  f(OP_FTOI,xreg,a); f(OP_FTOI,yreg,15);
}
static void edge(unsigned a,unsigned b,bool ship) {
  if(ship) { for(unsigned k=0;k<3;k++) { loadf(5+k,a+k*4); loadf(8+k,b+k*4); } }
  else { transform(a,5); transform(b,8); }
  loadf(15,FAR); f(OP_FCMP_RR,7,15); size_t far_first=pc++;
  f(OP_FCMP_RR,10,15); size_t far_skip=pc++;
  code[far_first]=ENC_I24(OP_JLE,pc);
  loadf(15,NEAR); f(OP_FCMP_RR,7,15); size_t first_visible=pc++;
  f(OP_FCMP_RR,10,15); size_t behind=pc++;
  clip_endpoint(5,8); size_t clipped=pc++;
  code[first_visible]=ENC_I24(OP_JGE,pc);
  f(OP_FCMP_RR,10,15); size_t second_visible=pc++;
  clip_endpoint(8,5);
  code[clipped]=JMP(pc); code[second_visible]=ENC_I24(OP_JGE,pc);
  loadf(15,16384); f(OP_FDIV_RR,15,7); f(OP_FTOI,2,15); gpu_reg(GPU_Q1,2);
  loadf(15,16384); f(OP_FDIV_RR,15,10); f(OP_FTOI,2,15); gpu_reg(GPU_Q2,2);
  project(5,5,6); project(8,7,8);
  gpu_reg(GPU_X,5); gpu_reg(GPU_Y,6); gpu_reg(GPU_X2,7); gpu_reg(GPU_Y2,8);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  code[far_skip]=JG(pc); code[behind]=JL(pc);
}
static void append_vertex(unsigned base) {
  for(unsigned k=0;k<3;k++) { f(OP_FSTORE32,base+k,12); e(ADD_RI(12,4)); }
  e(ADD_RI(13,1));
}
static void clipped_triangle(unsigned a,unsigned b,unsigned c) {
  unsigned vertices[3]={a,b,c};
  const unsigned xr[3]={GPU_X,GPU_X2,GPU_X3},yr[3]={GPU_Y,GPU_Y2,GPU_Y3},qr[3]={GPU_Q1,GPU_Q2,GPU_Q3};
  for(unsigned v=0;v<3;v++) {
    for(unsigned k=0;k<3;k++) loadf(5+k,9100+vertices[v]*12+k*4);
    loadf(15,16384); f(OP_FDIV_RR,15,7); f(OP_FTOI,2,15); gpu_reg(qr[v],2);
    project(5,5,6); gpu_reg(xr[v],5); gpu_reg(yr[v],6);
  }
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_TRIANGLE);
}
static void polygon(void) {
  e(MOV_RI(12,9100)); e(MOV_RI(13,0));
  for(unsigned edge=0;edge<3;edge++) {
    for(unsigned k=0;k<3;k++) { loadf(5+k,9000+edge*12+k*4); loadf(8+k,9000+((edge+1)%3)*12+k*4); }
    loadf(15,NEAR); f(OP_FCMP_RR,7,15); size_t a_out=pc++;
    f(OP_FCMP_RR,10,15); size_t both_in=pc++;
    clip_endpoint(8,5); /* exiting edge */
    code[both_in]=ENC_I24(OP_JGE,pc); append_vertex(8); size_t done=pc++;
    code[a_out]=JL(pc);
    f(OP_FCMP_RR,10,15); size_t both_out=pc++;
    clip_endpoint(5,8); append_vertex(5); append_vertex(8);
    code[done]=JMP(pc); code[both_out]=JL(pc);
  }
  e(CMP_RI(13,3)); size_t empty=pc++;
  clipped_triangle(0,1,2);
  e(CMP_RI(13,4)); size_t triangle=pc++;
  clipped_triangle(0,2,3);
  code[empty]=JL(pc); code[triangle]=JL(pc);
}
static void solid(const unsigned *face) {
  unsigned address=face[0];
  /* Cull outward-facing planes whose normal points away from the camera. */
  loadf(4,ZERO);
  for(unsigned k=0;k<3;k++) {
    loadf(5,address+k*4); loadf(6,POS+k*4); f(OP_FSUB_RR,5,6);
    if(k==0) { copy(6,0); f(OP_FMUL_RR,6,3); }
    else if(k==1) { loadf(6,ZERO); f(OP_FSUB_RR,6,2); }
    else { copy(6,1); f(OP_FMUL_RR,6,3); }
    loadf(7,CHASE); f(OP_FMUL_RR,6,7); f(OP_FADD_RR,5,6);
    loadf(6,address+36+k*4); f(OP_FMUL_RR,5,6); f(OP_FADD_RR,4,5);
  }
  loadf(5,ZERO); f(OP_FCMP_RR,4,5); size_t backface=pc++;
  for(unsigned v=0;v<3;v++) {
    transform(address+v*12,5);
    for(unsigned k=0;k<3;k++) storef(5+k,9000+v*12+k*4);
  }
  /* Flat Lambert shading in guest code; the sun is emissive. */
  if(face[4]) loadf(4,16400);
  else {
    loadf(4,address+36); loadf(5,address+48); f(OP_FMUL_RR,4,5);
    for(unsigned k=1;k<3;k++) { loadf(5,address+36+k*4); loadf(6,address+48+k*4); f(OP_FMUL_RR,5,6); f(OP_FADD_RR,4,5); }
    loadf(5,16396); f(OP_FCMP_RR,4,5); size_t positive=pc++;
    copy(4,5); code[positive]=ENC_I24(OP_JGE,pc);
    loadf(5,16392); f(OP_FMUL_RR,4,5); loadf(5,16388); f(OP_FADD_RR,4,5);
  }
  e(MOV_RI(3,0));
  for(unsigned k=0;k<3;k++) {
    e(MOV_RI(2,face[k+1])); f(OP_ITOF,5,2); f(OP_FMUL_RR,5,4); f(OP_FTOI,2,5);
    e(MOV_RI(1,k==0?65536:k==1?256:1)); f(OP_MUL_RR,2,1); f(OP_ADD_RR,3,2);
  }
  gpu_reg(GPU_COLOR,3); polygon();
  code[backface]=ENC_I24(OP_JGE,pc);
}
static void squared_distance(unsigned center) {
  loadf(4,ZERO);
  for(unsigned k=0;k<3;k++) {
    loadf(5,POS+k*4); loadf(6,center+k*4); f(OP_FSUB_RR,5,6);
    f(OP_FMUL_RR,5,5); f(OP_FADD_RR,4,5);
  }
}
/* Guest Newton iteration: nonnegative F4 -> distance in F4. */
static void distance_root(void) {
  copy(6,4); loadf(7,16400); f(OP_FADD_RR,6,7);
  loadf(8,NEAR); e(MOV_RI(2,32)); size_t loop=pc;
  copy(7,4); f(OP_FDIV_RR,7,6); f(OP_FADD_RR,6,7); f(OP_FMUL_RR,6,8);
  e(SUB_RI(2,1)); e(CMP_RI(2,0)); e(JG(loop)); copy(4,6);
}
static void move(void) {
  e(CMP_RI(14,0)); size_t no_contact=pc++;
  e(SUB_RI(14,1)); code[no_contact]=ENC_I24(OP_JE,pc);
  for(unsigned k=0;k<3;k++) { loadf(5,POS+k*4); storef(5,8300+k*4); }
  f(OP_ITOF,4,11); loadf(15,STEP); f(OP_FMUL_RR,4,15);
  /* forward=(sin(yaw)*cos(pitch), -sin(pitch), cos(yaw)*cos(pitch)) */
  copy(5,0); f(OP_FMUL_RR,5,3); f(OP_FMUL_RR,5,4);
  loadf(6,POS); f(OP_FADD_RR,6,5); storef(6,POS);
  copy(5,2); f(OP_FMUL_RR,5,4); loadf(6,POS+4); f(OP_FSUB_RR,6,5); storef(6,POS+4);
  copy(5,1); f(OP_FMUL_RR,5,3); f(OP_FMUL_RR,5,4);
  loadf(6,POS+8); f(OP_FADD_RR,6,5); storef(6,POS+8);
  for(unsigned body=0;body<2;body++) {
    unsigned center=16404+body*20;
    squared_distance(center); loadf(5,center+12); f(OP_FCMP_RR,4,5); size_t safe=pc++;
    distance_root();
    e(MOV_RI(2,body?108:68)); f(OP_ITOF,5,2); loadf(6,8500); f(OP_FADD_RR,5,6);
    f(OP_FDIV_RR,5,4); copy(10,5);
    for(unsigned k=0;k<3;k++) {
      loadf(5,POS+k*4); loadf(6,center+k*4); f(OP_FSUB_RR,5,6);
      f(OP_FMUL_RR,5,10); f(OP_FADD_RR,5,6); storef(5,POS+k*4);
    }
    e(MOV_RI(2,body+1)); e(MOV_RI(0,8416)); f(OP_STORE32,2,0);
    e(MOV_RI(14,30)); code[safe]=ENC_I24(OP_JGE,pc);
  }
}
static void hud_line(unsigned x,unsigned y,unsigned x2,unsigned y2) {
  e(ENC_RI(OP_LUI,0,GPU_BASE>>12));e(ADD_RI(0,GPU_X));
  e(MOV_RI(1,x));f(OP_STORE32,1,0);e(ADD_RI(0,4));
  e(MOV_RI(1,y));f(OP_STORE32,1,0);e(ADD_RI(0,8));
  e(MOV_RI(1,x2));f(OP_STORE32,1,0);e(ADD_RI(0,4));
  e(MOV_RI(1,y2));f(OP_STORE32,1,0);e(SUB_RI(0,GPU_Y2-GPU_COMMAND));
  e(MOV_RI(1,GPU_LINE));f(OP_STORE32,1,0);
}
/* Compact 3x5 uppercase guest-rendered glyphs. No host text overlay. */
static unsigned glyph(char c) {
  switch(c) {
    case 'J':return 0x7b24;
    case 'K':return 0x5aed;
    case 'W':return 0x5fed;
    case 'Q':return 0x4f6f;
    case 'Z':return 0x72a7;
    case 'V':return 0x2b6d;
    case 'B':return 0x3aeb;
    case 'G':return 0x7b4f;
    case 'H':return 0x5bed;
    case 'A':return 0x5bea;
    case 'C':return 0x724f;
    case 'D':return 0x3b6b;
    case 'E':return 0x72cf;
    case 'F':return 0x12cf;
    case 'I':return 0x7497;
    case 'L':return 0x7249;
    case 'M':return 0x5bfd;
    case 'N':return 0x5ffd;
    case 'O':return 0x7b6f;
    case 'P':return 0x12eb;
    case 'R':return 0x5aeb;
    case 'S':return 0x79cf;
    case 'T':return 0x2497;
    case 'U':return 0x7b6d;
    case 'X':return 0x5aad;
    case 'Y':return 0x24ad;
    case '0':return 0x7b6f;
    case '1':return 0x749a;
    case '2':return 0x73e7;
    case '3':return 0x79e7;
    case '4':return 0x49ed;
    case '5':return 0x79cf;
    case '6':return 0x7bcf;
    case '7':return 0x24a7;
    case '8':return 0x7bef;
    case '9':return 0x79ef;
    case '+':return 0x5d0;
    default:return 0;
  }
}
static void hud_text(const char *text,unsigned x,unsigned y) {
  for(;*text;text++,x+=12) {
    unsigned g=glyph(*text);
    for(unsigned row=0;row<5;row++) for(unsigned col=0;col<3;) {
      if(!(g&(1u<<(row*3+col)))) { col++; continue; }
      unsigned start=col;
      while(col<3 && (g&(1u<<(row*3+col)))) col++;
      hud_line(x+start*3,y+row*3,x+col*3-2,y+row*3);
      hud_line(x+start*3,y+row*3+1,x+col*3-2,y+row*3+1);
    }
  }
}
static void hud_number_unit(unsigned x,unsigned y,const char *unit) {
  /* R7 contains clamped clearance; render three decimal digits. */
  for(unsigned place=0;place<3;place++) {
    unsigned divisor=place==0?100:place==1?10:1;
    f(OP_MOV_RR,8,7); e(MOV_RI(2,divisor)); f(OP_DIV_RR,8,2);
    f(OP_MOV_RR,3,8); f(OP_MUL_RR,3,2); f(OP_SUB_RR,7,3);
    for(unsigned digit=0;digit<10;digit++) {
      e(CMP_RI(8,digit)); size_t skip=pc++;
      char text[2]={(char)('0'+digit),0}; hud_text(text,x+place*12,y);
      code[skip]=JNE(pc);
    }
  }
  hud_text(unit,x+42,y);
}
static void hud_number(unsigned x,unsigned y) { hud_number_unit(x,y,"U"); }
static void body_name(unsigned idreg,unsigned x,unsigned y) {
  e(CMP_RI(idreg,1)); size_t sun=pc++;
  hud_text("TERRA",x,y); size_t end=pc++;
  code[sun]=JNE(pc); hud_text("SOL",x,y); code[end]=JMP(pc);
}
static void ray_ahead(void) {
  e(MOV_RI(15,0)); e(MOV_RI(2,999999)); f(OP_ITOF,4,2); storef(4,8420);
  /* Store unit forward, since square root uses its own floating temporaries. */
  copy(5,0); f(OP_FMUL_RR,5,3); storef(5,8432);
  loadf(5,ZERO); f(OP_FSUB_RR,5,2); storef(5,8436);
  copy(5,1); f(OP_FMUL_RR,5,3); storef(5,8440);
  for(unsigned body=0;body<2;body++) {
    unsigned center=16404+body*20;
    loadf(9,ZERO); loadf(10,ZERO);
    for(unsigned k=0;k<3;k++) {
      loadf(5,center+k*4); loadf(6,POS+k*4); f(OP_FSUB_RR,5,6);
      copy(7,5); f(OP_FMUL_RR,7,5); f(OP_FADD_RR,10,7);
      loadf(6,8432+k*4); f(OP_FMUL_RR,5,6); f(OP_FADD_RR,9,5);
    }
    loadf(5,ZERO); f(OP_FCMP_RR,9,5); size_t behind=pc++;
    copy(4,9); f(OP_FMUL_RR,4,9); f(OP_FSUB_RR,4,10);
    loadf(5,center+12); f(OP_FADD_RR,4,5);
    loadf(5,ZERO); f(OP_FCMP_RR,4,5); size_t miss=pc++;
    storef(9,8444); distance_root(); loadf(9,8444); f(OP_FSUB_RR,9,4);
    loadf(5,ZERO); f(OP_FCMP_RR,9,5); size_t positive=pc++;
    copy(9,5); code[positive]=ENC_I24(OP_JGE,pc);
    loadf(5,8420); f(OP_FCMP_RR,9,5); size_t farther=pc++;
    storef(9,8420); e(MOV_RI(15,body+1));
    code[behind]=ENC_I24(OP_JLE,pc); code[miss]=JL(pc); code[farther]=ENC_I24(OP_JGE,pc);
  }
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);
  hud_text("AHEAD",374,700); e(CMP_RI(15,0)); size_t clear=pc++;
  body_name(15,446,700);
  loadf(4,8420); e(MOV_RI(2,999)); f(OP_ITOF,5,2); f(OP_FCMP_RR,4,5); size_t within=pc++;
  copy(4,5); code[within]=ENC_I24(OP_JLE,pc); f(OP_FTOI,7,4); hud_number(518,700);
  size_t end=pc++; code[clear]=ENC_I24(OP_JE,pc); hud_text("CLEAR",446,700); code[end]=JMP(pc);
}
static void nav_input(void) {
  const unsigned keys[2]={KEY_M,KEY_TAB}, addresses[2]={8460,8464};
  for(unsigned k=0;k<2;k++) {
    e(ENC_RI(OP_LUI,0,INPUT_BASE>>12)); e(ADD_RI(0,INPUT_PRESSED+keys[k]*4));
    f(OP_LOAD32,2,0); e(CMP_RI(2,0)); size_t skip=pc++;
    e(MOV_RI(0,addresses[k])); f(OP_LOAD32,2,0); e(MOV_RI(3,k?3:1)); f(OP_SUB_RR,3,2); f(OP_STORE32,3,0);
    e(ENC_RI(OP_LUI,0,INPUT_BASE>>12)); e(ADD_RI(0,INPUT_ACK+keys[k]*4)); e(MOV_RI(2,1)); f(OP_STORE32,2,0);
    code[skip]=ENC_I24(OP_JE,pc);
  }
}
static void panel(unsigned x,unsigned y,unsigned x2,unsigned y2) {
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x07111b);
  pc=emit_gpu_write(code,pc,GPU_X,x); pc=emit_gpu_write(code,pc,GPU_Y,y);
  pc=emit_gpu_write(code,pc,GPU_X2,x2); pc=emit_gpu_write(code,pc,GPU_Y2,y);
  pc=emit_gpu_write(code,pc,GPU_X3,x2); pc=emit_gpu_write(code,pc,GPU_Y3,y2);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_TRIANGLE);
  pc=emit_gpu_write(code,pc,GPU_X2,x); pc=emit_gpu_write(code,pc,GPU_Y2,y2);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_TRIANGLE);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x305468);
  hud_line(x,y,x2,y);hud_line(x,y2,x2,y2);hud_line(x,y,x,y2);hud_line(x2,y,x2,y2);
}
static void navigation(void) {
  /* Destination details belong to the permanent cockpit, independent of map visibility. */
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("DESTINATION",374,640);
  e(MOV_RI(0,8464));f(OP_LOAD32,8,0);body_name(8,530,640);
  e(CMP_RI(8,1));size_t sol=pc++;
  squared_distance(16404);size_t range=pc++;
  code[sol]=JNE(pc);squared_distance(16424);code[range]=JMP(pc);
  distance_root();e(MOV_RI(2,999));f(OP_ITOF,5,2);f(OP_FCMP_RR,4,5);size_t within=pc++;
  copy(4,5);code[within]=ENC_I24(OP_JLE,pc);f(OP_FTOI,7,4);hud_number(374,668);
  hud_text("CENTER",434,668);
  e(MOV_RI(0,8460));f(OP_LOAD32,2,0);e(CMP_RI(2,0));size_t hidden=pc++;
  panel(730,60,1000,350);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("SYSTEM MAP",746,74);
  hud_text("TAB SELECT",746,100);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x203948);
  for(unsigned x=770;x<=970;x+=50) hud_line(x,160,x,300);
  for(unsigned y=160;y<=300;y+=35) hud_line(770,y,970,y);
  /* Fixed local X/Z chart: x=820+X/4, y=280-Z/5. */
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x4eafdd);
  hud_line(796,236,804,236);hud_line(800,232,800,240);hud_text("TERRA",746,216);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xffb740);
  hud_line(871,170,879,170);hud_line(875,166,875,174);hud_text("SOL",894,166);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xe3f5ff);
  e(MOV_RI(0,8464));f(OP_LOAD32,8,0);e(CMP_RI(8,1));size_t select_sol=pc++;
  hud_line(792,228,808,228);hud_line(792,244,808,244);size_t selected=pc++;
  code[select_sol]=JNE(pc);hud_line(867,162,883,162);hud_line(867,178,883,178);code[selected]=JMP(pc);
  loadf(4,POS);e(MOV_RI(2,4));f(OP_ITOF,5,2);f(OP_FDIV_RR,4,5);
  e(MOV_RI(2,820));f(OP_ITOF,5,2);f(OP_FADD_RR,4,5);f(OP_FTOI,6,4);
  loadf(4,POS+8);e(MOV_RI(2,5));f(OP_ITOF,5,2);f(OP_FDIV_RR,4,5);
  e(MOV_RI(2,280));f(OP_ITOF,5,2);f(OP_FSUB_RR,5,4);f(OP_FTOI,7,5);
  /* Off-chart coordinates suppress the marker; wrapped negative values fail the upper bounds. */
  e(CMP_RI(6,770));size_t left=pc++;e(CMP_RI(6,970));size_t right=pc++;
  e(CMP_RI(7,160));size_t top=pc++;e(CMP_RI(7,300));size_t bottom=pc++;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);gpu_reg(GPU_X,6);gpu_reg(GPU_Y,7);
  e(ADD_RI(6,4));gpu_reg(GPU_X2,6);gpu_reg(GPU_Y2,7);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  e(SUB_RI(6,2));e(SUB_RI(7,2));gpu_reg(GPU_X,6);gpu_reg(GPU_Y,7);e(ADD_RI(7,4));gpu_reg(GPU_Y2,7);gpu_reg(GPU_X2,6);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  /* Heading tick in the same top-down projection as the map. */
  e(SUB_RI(6,2));e(SUB_RI(7,2));gpu_reg(GPU_X,6);gpu_reg(GPU_Y,7);
  copy(4,0);f(OP_FMUL_RR,4,3);e(MOV_RI(2,12));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);
  f(OP_ITOF,5,6);f(OP_FADD_RR,4,5);f(OP_FTOI,6,4);gpu_reg(GPU_X2,6);
  copy(4,1);f(OP_FMUL_RR,4,3);e(MOV_RI(2,12));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);
  f(OP_ITOF,5,7);f(OP_FSUB_RR,5,4);f(OP_FTOI,7,5);gpu_reg(GPU_Y2,7);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  size_t shown=pc++;code[left]=JL(pc);code[right]=JG(pc);code[top]=JL(pc);code[bottom]=JG(pc);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xffb740);hud_text("OUT OF MAP",746,304);code[shown]=JMP(pc);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("XZ MAP",746,144);
  hud_text("M CLOSE",746,328);code[hidden]=ENC_I24(OP_JE,pc);
}
static void hud(void) {
  panel(12,600,1012,752);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x305468);hud_line(350,630,350,740);hud_line(690,630,690,740);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("ASTRA",24,610);hud_text("LOCAL FLIGHT",108,610);
  hud_text("M MAP",716,712);hud_text("TAB SELECT",716,732);
  navigation();
  ray_ahead();
  /* Minimum clearance to the two exclusion spheres, in world units. */
  for(unsigned body=0;body<2;body++) {
    squared_distance(16404+body*20); distance_root();
    e(MOV_RI(2,body?108:68)); f(OP_ITOF,5,2); f(OP_FSUB_RR,4,5);
    if(body==0) {
      storef(4,8400); e(MOV_RI(2,1)); e(MOV_RI(0,8412)); f(OP_STORE32,2,0);
    }
    else { loadf(5,8400); f(OP_FCMP_RR,4,5); size_t farther=pc++;
      storef(4,8400); e(MOV_RI(2,2)); e(MOV_RI(0,8412)); f(OP_STORE32,2,0); code[farther]=ENC_I24(OP_JGE,pc); }
  }
  loadf(4,8400); loadf(5,ZERO); f(OP_FCMP_RR,4,5); size_t nonnegative=pc++;
  copy(4,5); code[nonnegative]=ENC_I24(OP_JGE,pc);
  copy(9,4); e(MOV_RI(2,999)); f(OP_ITOF,5,2); f(OP_FCMP_RR,9,5); size_t within=pc++;
  copy(9,5); code[within]=ENC_I24(OP_JLE,pc); f(OP_FTOI,7,9);
  e(MOV_RI(2,20)); f(OP_ITOF,5,2); f(OP_FCMP_RR,4,5); size_t close=pc++;
  copy(4,5); code[close]=JL(pc); f(OP_FTOI,6,4);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  e(CMP_RI(6,20)); size_t distant=pc++;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xffb740);
  code[distant]=ENC_I24(OP_JGE,pc);
  hud_text("NEAR",24,640); e(MOV_RI(0,8412)); f(OP_LOAD32,8,0); body_name(8,84,640);
  hud_text("CLEARANCE",24,670); hud_number(150,670);
  hud_line(24,692,224,692); hud_line(24,687,24,697); hud_line(224,687,224,697);
  /* Moving marker: left=surface limit, right=20 or more units clear. */
  e(MOV_RI(2,10)); f(OP_MUL_RR,6,2); e(ADD_RI(6,24));
  gpu_reg(GPU_X,6); gpu_reg(GPU_X2,6);
  pc=emit_gpu_write(code,pc,GPU_Y,686); pc=emit_gpu_write(code,pc,GPU_Y2,698);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  hud_text("0",230,686); hud_text("20+",254,686);
  hud_text("SPEED",24,708);
  e(CMP_RI(14,0)); size_t no_limit=pc++;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xff7840); hud_text("SURFACE LIMIT",716,640); e(MOV_RI(0,8416)); f(OP_LOAD32,8,0); body_name(8,716,668); size_t end=pc++;
  code[no_limit]=ENC_I24(OP_JE,pc);
  loadf(4,8400); e(MOV_RI(2,20)); f(OP_ITOF,5,2); f(OP_FCMP_RR,4,5); size_t no_warning=pc++;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xffb740); hud_text("PROXIMITY",716,640); e(MOV_RI(0,8412)); f(OP_LOAD32,8,0); body_name(8,716,668);
  size_t warned=pc++;
  code[no_warning]=ENC_I24(OP_JGE,pc);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("STATUS",716,640);hud_text("CLEAR",716,668);
  code[end]=JMP(pc); code[warned]=JMP(pc);
}
prog_t flight(void) {
  memcpy(scene_data,flight_data,sizeof(flight_data));
  memcpy(scene_data+16384,solid_data,sizeof(solid_data));
  float epsilon=.001f; uint32_t bits; memcpy(&bits,&epsilon,4);
  for(unsigned b=0;b<4;b++) scene_data[8500+b]=(uint8_t)(bits>>(b*8));
  pc=0; e(MOV_RI(14,0)); e(MOV_RI(9,0)); e(MOV_RI(10,0)); e(MOV_RI(11,0));
  e(MOV_RI(0,8460));e(MOV_RI(2,1));f(OP_STORE32,2,0);e(ADD_RI(0,4));f(OP_STORE32,2,0);
  size_t draw=pc;
  trig(9,0,1); trig(10,2,3);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0); pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_CLEAR);
  pc=emit_gpu_write(code,pc,GPU_DEPTH,1);
  for(unsigned group=0;group<3;group++) {
    size_t skip=0;
    if(group<2) { squared_distance(16404); loadf(5,16420); f(OP_FCMP_RR,4,5); skip=pc++; }
    for(unsigned face=solid_groups[group][0];face<solid_groups[group][1];face++) solid(solid_faces[face]);
    if(group<2) code[skip]=ENC_I24(group==0?OP_JL:OP_JGE,pc);
  }
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x607c92);
  for(unsigned object=0;object<24;object++)
    for(unsigned a=0;a<4;a++) for(unsigned b=a+1;b<4;b++) edge(2080+object*48+a*12,2080+object*48+b*12,false);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  for(unsigned a=0;a<4;a++) for(unsigned b=a+1;b<4;b++) edge(3232+a*12,3232+b*12,true);
  pc=emit_gpu_write(code,pc,GPU_DEPTH,0);
  hud();
  /* Speed gauge: fixed 300-pixel rail and filled-length green line. */
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x334455);
  pc=emit_gpu_write(code,pc,GPU_X,24); pc=emit_gpu_write(code,pc,GPU_Y,730);
  pc=emit_gpu_write(code,pc,GPU_X2,324); pc=emit_gpu_write(code,pc,GPU_Y2,730);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  f(OP_MOV_RR,5,11); e(ADD_RI(5,24)); gpu_reg(GPU_X2,5);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_PRESENT); e(ENC_OP(OP_WAIT_FRAME));
  nav_input();
  /* World-axis yaw changes screen direction when the camera is inverted. */
  trig(10,2,3); loadf(5,ZERO); f(OP_FCMP_RR,3,5); size_t inverted=pc++;
  control(9,KEY_LEFT,KEY_RIGHT,0,255,true); size_t yaw_done=pc++;
  code[inverted]=JL(pc); control(9,KEY_RIGHT,KEY_LEFT,0,255,true); code[yaw_done]=JMP(pc);
  control(10,KEY_UP,KEY_DOWN,0,255,true);
  control(11,KEY_X,KEY_Z,0,300,false);
  trig(9,0,1); trig(10,2,3); move(); e(JMP(draw));
  return (prog_t){.code=code,.codelen=pc,.data=scene_data,.datalen=sizeof(scene_data)};
}
#ifdef CGVM_TEST
/* Isolated guest polygon path for conformance tests, without scene or culling. */
prog_t flight_clip_probe(const float vertices[9]) {
  memset(scene_data,0,sizeof(scene_data));
  memcpy(scene_data,flight_data,sizeof(flight_data));
  memcpy(scene_data+16384,solid_data,sizeof(solid_data));
  for(unsigned i=0;i<9;i++) {
    uint32_t bits; memcpy(&bits,&vertices[i],4);
    for(unsigned b=0;b<4;b++) scene_data[9000+i*4+b]=(uint8_t)(bits>>(8*b));
  }
  pc=0;
  pc=emit_gpu_write(code,pc,GPU_DEPTH,1);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  polygon(); e(EXIT());
  return (prog_t){.code=code,.codelen=pc,.data=scene_data,.datalen=sizeof(scene_data)};
}
#endif

#include "solar.inc"
