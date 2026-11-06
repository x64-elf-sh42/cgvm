#include "machine.h"
#include <string.h>
#include <float.h>
_Static_assert(sizeof(double)==8 && DBL_MANT_DIG==53, "binary64 required");
_Static_assert(sizeof(float)==4 && FLT_RADIX==2 && FLT_MANT_DIG==24, "binary32 required");
#define FL_EQ 0x1000u
#define FL_NE 0x8000u
#define FL_LT 0x2000u
#define FL_GT 0x4000u
#define FL_CMP_MASK 0xf000u

void machine_reset(machine_t *m) { memset(m, 0, sizeof(*m)); }
bool machine_load(machine_t *m, const prog_t *p) {
  if(!p || !p->code || !p->codelen || p->codelen>CODE_SIZE ||
     p->entry_pc>=p->codelen || p->datalen>DATA_SIZE ||
     (p->datalen && !p->data)) return false;
  machine_reset(m);
  memcpy(m->code,p->code,p->codelen*sizeof(insn_t));
  if(p->datalen) memcpy(m->data,p->data,p->datalen);
  m->code_len=p->codelen;
  m->regs[REG_PC]=p->entry_pc;
  return true;
}

// PROGRAM EXECUTOR
cpu_result_t cpu_step(machine_t *m, size_t budget) {
  size_t timed=0;
  cpu_result_t result = { CPU_BUDGET, 0 };
  if(m->state == CPU_HALT || m->state == CPU_FAULT || m->state == CPU_WAIT)
    return (cpu_result_t){m->state, 0};
  #define DISPATCH() do { \
    machine_advance(m,result.executed-timed); timed=result.executed; \
    if(result.executed == budget) goto ve_fin; \
    if(pc >= m->code_len) { m->fault=FAULT_FETCH; goto fault; } \
    regs[REG_PC]=pc; \
    ins=code[pc]; \
    if(OP(ins)==OP_UNDEFINED || OP(ins)>=OP_MAX_OPERATION) { m->fault=FAULT_OPCODE; goto fault; } \
    if(OP(ins)>=OP_MOV_RR && OP(ins)<=OP_DIV_RR && OP(ins)!=OP_MOV_RM && RR_DST(ins)==REG_PC) { m->fault=FAULT_PC_WRITE; goto fault; } \
    result.executed++; \
    goto *dispatch[OP(ins)]; \
  } while(0)

  static void *dispatch[OP_MAX_OPERATION] = {
    [OP_FLOAD64]=&&op_double_transfer, [OP_FSTORE64]=&&op_double_transfer,
    [OP_FSTORE32]=&&op_float_transfer,
    [OP_FLOAD32]=&&op_float_transfer, [OP_FMOV_RR]=&&op_float_transfer,
    [OP_ITOF]=&&op_float_transfer, [OP_FTOI]=&&op_float_transfer,
    [OP_LOAD8]=&&op_bus, [OP_LOAD16]=&&op_bus, [OP_LOAD32]=&&op_bus,
    [OP_STORE8]=&&op_bus, [OP_STORE16]=&&op_bus, [OP_STORE32]=&&op_bus,
    [OP_LUI]=&&op_lui, [OP_WAIT_FRAME]=&&op_wait,
    [OP_UNDEFINED] = &&op_undefined,
    [OP_EXIT] = &&op_exit,
    [OP_YIELD] = &&op_yield,
    [OP_MOV_RR] = &&op_mov_rr,
    [OP_MOV_RI] = &&op_mov_ri,
    [OP_MOV_RM] = &&op_mov_rm,
    [OP_MOV_MR] = &&op_mov_mr,
    [OP_ADD_RR] = &&op_add_rr,
    [OP_ADD_RI] = &&op_add_ri,
    [OP_SUB_RR] = &&op_sub_rr,
    [OP_SUB_RI] = &&op_sub_ri,
    [OP_MUL_RR] = &&op_mul_rr,
    [OP_DIV_RR] = &&op_div_rr,
    [OP_FADD_RR] = &&op_fadd_rr,
    [OP_FSUB_RR] = &&op_fsub_rr,
    [OP_FMUL_RR] = &&op_fmul_rr,
    [OP_FDIV_RR] = &&op_fdiv_rr,  
    [OP_CMP_RR] = &&op_cmp_rr,
    [OP_CMP_RI] = &&op_cmp_ri,
    [OP_FCMP_RR] = &&op_fcmp_rr,
    [OP_JMP] = &&op_jmp,
    [OP_JE] = &&op_je,
    [OP_JNE] = &&op_jne,
    [OP_JG] = &&op_jg,
    [OP_JL] = &&op_jl,
    [OP_JGE] = &&op_jge,
    [OP_JLE] = &&op_jle,
    [OP_SET_PIXEL] = &&op_set_pixel,
    [OP_FB_CLEAR] = &&op_fb_clear,
    [OP_FB_SWAP] = &&op_fb_swap
  };

  reg_t *regs = m->regs;
  fp_reg_t *fpregs = m->fpregs;
  insn_t *code = m->code;
  uint8_t *data = m->data;
  uint32_t pc = regs[REG_PC];
  insn_t ins = 0;
  cpu_flags_t flags = m->flags;
  uint32_t x=0,y=0,c=0;
  double dx=0,dy=0;
  machine_pixel_t *fb = m->fb;  
  machine_pixel_t *fb2 = m->fb2;
  size_t screenw = m->width;
  size_t screenh = m->height;

    
  DISPATCH(); // start execution at pc
  
  op_undefined:
    m->fault=FAULT_OPCODE;
    goto fault;
  op_exit:
    result.status=CPU_HALT;
    pc++;
    goto ve_fin;
  op_yield:
    result.status=CPU_YIELD;
    pc++;
    goto ve_fin;

  op_double_transfer: {
    unsigned d=RR_DST(ins), s=RR_SRC(ins); uint32_t a=regs[s];
    /* Binary64 transfers are RAM-only and validate the whole range first. */
    if(a%8 || a>DATA_SIZE-8) { result.executed--; m->fault=FAULT_BUS; m->fault_address=a; goto fault; }
    uint64_t bits=0;
    if(OP(ins)==OP_FLOAD64) {
      for(unsigned k=0;k<8;k++) bits|=(uint64_t)data[a+k]<<(8*k);
      memcpy(&fpregs[d],&bits,8);
    } else {
      memcpy(&bits,&fpregs[d],8);
      for(unsigned k=0;k<8;k++) data[a+k]=(uint8_t)(bits>>(8*k));
    }
    pc++; DISPATCH();
  }
  op_float_transfer: {
    unsigned d=RR_DST(ins), s=RR_SRC(ins);
    if(OP(ins)==OP_FSTORE32) {
      double source=fpregs[d];
      if(source!=source || source>FLT_MAX || source < -FLT_MAX) {
        result.executed--; m->fault=FAULT_CONVERSION; goto fault;
      }
      float v=(float)source; uint32_t bits; memcpy(&bits,&v,4);
      if(!bus_write(m,regs[s],4,bits)) { result.executed--; m->fault=FAULT_BUS; m->fault_address=regs[s]; goto fault; }
    } else if(OP(ins)==OP_FMOV_RR) fpregs[d]=fpregs[s];
    else if(OP(ins)==OP_ITOF) fpregs[d]=(double)(int64_t)(regs[s]<=INT32_MAX ? (int64_t)regs[s] : (int64_t)regs[s]-4294967296LL);
    else if(OP(ins)==OP_FLOAD32) {
      uint32_t bits;
      if(!bus_read(m,regs[s],4,&bits)) { result.executed--; m->fault=FAULT_BUS; m->fault_address=regs[s]; goto fault; }
      float v; memcpy(&v,&bits,4); fpregs[d]=v;
    } else {
      if(d==REG_PC) { result.executed--; m->fault=FAULT_PC_WRITE; goto fault; }
      double v=fpregs[s];
      if(v!=v || v < -2147483648.0 || v > 2147483647.0) {
        result.executed--; m->fault=FAULT_CONVERSION; goto fault;
      }
      regs[d]=(uint32_t)(int32_t)v;
    }
    pc++; DISPATCH();
  }
  op_lui:
    if(RR_DST(ins)==REG_PC) { result.executed--; m->fault=FAULT_PC_WRITE; goto fault; }
    regs[RR_DST(ins)]=IMM20(ins)<<12;
    pc++; DISPATCH();
  op_wait:
    if(m->frame_event) m->frame_event=false;
    else result.status=CPU_WAIT;
    pc++;
    goto ve_fin;
  op_bus: {
    unsigned op=OP(ins), width=1u << ((op-OP_LOAD8)%3);
    uint32_t address=regs[RR_SRC(ins)], value=regs[RR_DST(ins)];
    bool load=op<=OP_LOAD32;
    if(load && RR_DST(ins)==REG_PC) { result.executed--; m->fault=FAULT_PC_WRITE; goto fault; }
    bool ok=load ? bus_read(m,address,width,&value) : bus_write(m,address,width,value);
    if(!ok) { result.executed--; m->fault=FAULT_BUS; m->fault_address=address; goto fault; }
    if(load) regs[RR_DST(ins)]=value;
    pc++; DISPATCH();
  }

  // mov
  op_mov_rr:
    regs[RR_DST(ins)] = regs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  op_mov_ri:
    regs[RR_DST(ins)] = IMM20(ins);
    pc++;
    DISPATCH();

  op_mov_mr:
    regs[RR_DST(ins)] = data[IMM20(ins)];
    pc++;
    DISPATCH();

  op_mov_rm:
    data[IMM20(ins)] = (uint8_t)regs[RR_DST(ins)]; // dst is first one in struct
    pc++;
    DISPATCH();

  // maths
  op_add_rr:
    regs[RR_DST(ins)]+=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  op_add_ri:
    regs[RR_DST(ins)]+=IMM20(ins);
    pc++;
    DISPATCH();
    
  op_sub_rr:
    regs[RR_DST(ins)]-=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  op_sub_ri:
    regs[RR_DST(ins)]-=IMM20(ins);
    pc++;
    DISPATCH();
    
  op_mul_rr:
    regs[RR_DST(ins)]*=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();
    
  op_div_rr:
    if(!regs[RR_SRC(ins)]) { result.executed--; m->fault=FAULT_DIV_ZERO; goto fault; }
    regs[RR_DST(ins)]/=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  op_fadd_rr:
    fpregs[RR_DST(ins)]+=fpregs[RR_SRC(ins)];
    pc++;
    DISPATCH();
  op_fsub_rr:
    fpregs[RR_DST(ins)]-=fpregs[RR_SRC(ins)];
    pc++;
    DISPATCH();
  op_fmul_rr:
    fpregs[RR_DST(ins)]*=fpregs[RR_SRC(ins)];
    pc++;
    DISPATCH();
  op_fdiv_rr:
    fpregs[RR_DST(ins)]/=fpregs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  // comparison/branching
  op_fcmp_rr:
    flags&=~FL_CMP_MASK;
    dy = fpregs[RR_DST(ins)];
    dx = fpregs[RR_SRC(ins)];
    goto op_fcmp;
        
  op_cmp_rr:
    flags&=~FL_CMP_MASK;
    y = regs[RR_DST(ins)];
    x = regs[RR_SRC(ins)];    
    goto op_cmp;

  op_cmp_ri:
    flags&=~FL_CMP_MASK;
    y = regs[RR_DST(ins)];
    x = IMM20(ins);
    goto op_cmp;

  op_fcmp:
    if(dy==dx) {
      flags|=FL_EQ;
    }  else {
      flags|=FL_NE;
    }
    if(dy<dx) {
      flags|=FL_LT;
    } else if(dy>dx) {
      flags|=FL_GT;
    }
    pc++;
    DISPATCH();
    
  op_cmp: 
    if(y==x) {
      flags|=FL_EQ;
    } else {
      flags|=FL_NE;
    }
    if(y<x) {
      flags|=FL_LT;
    } else if(y>x) {
      flags|=FL_GT;
    }
    pc++;
    DISPATCH();
       
 op_jmp:
    pc=IMM24(ins);
    DISPATCH();
    
  op_je:
    if(flags&FL_EQ) {
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jne:
    if(flags&FL_NE) {
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jg:
    if(flags&FL_GT && flags&FL_NE) {
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();
    
  op_jl:
    if(flags&FL_LT && flags&FL_NE) {
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jge:
    if(flags&FL_GT || flags&FL_EQ) {
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jle:
    if(flags&FL_LT || flags&FL_EQ) {
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  // graphics
  op_set_pixel:
    if(!fb || !fb2) { result.executed--; m->fault=FAULT_DEVICE; goto fault; }
    x = regs[REG_R5];
    y = regs[REG_R6];
    c = IMM24(ins);
    if(y < screenh && x < screenw) {
      fb[(y*screenw)+x].red = (c >> 16) & 0xff;
      fb[(y*screenw)+x].green = (c >> 8) & 0xff;
      fb[(y*screenw)+x].blue = (uint8_t)c;
    }
    pc++;
    DISPATCH();
    
  op_fb_clear:
    if(!fb || !fb2) { result.executed--; m->fault=FAULT_DEVICE; goto fault; }
    c = IMM24(ins);
    for(uint32_t i=0;i<screenw*screenh; i++){
      fb[i].red = (c>>16)&0xff;
      fb[i].green = (c>>8)&0xff;
      fb[i].blue = (uint8_t)c;
    }
    pc++;
    DISPATCH();

  op_fb_swap:
    if(!fb || !fb2) { result.executed--; m->fault=FAULT_DEVICE; goto fault; }
    memcpy(fb2, fb, screenw*screenh*sizeof(machine_pixel_t));
    pc++;
    DISPATCH();



  fault:
  result.status=CPU_FAULT;
  m->fault_pc=pc;
ve_fin:
  machine_advance(m,result.executed-timed);
  m->flags=flags;
  regs[REG_PC]=pc;
  m->state=result.status;
  return result;
  #undef DISPATCH
}

/* Bus accesses validate before performing any side effect. */
static uint8_t *bus_memory(machine_t *m, uint32_t a, unsigned w) {
  if(a<DATA_SIZE && w<=DATA_SIZE-a) return m->data+a;
  if(a>=VIDEO_BASE && a-VIDEO_BASE<sizeof(m->video) &&
     w<=sizeof(m->video)-(a-VIDEO_BASE)) return (uint8_t*)m->video+(a-VIDEO_BASE);
  return NULL;
}
static bool valid_width(uint32_t a, unsigned w) {
  return (w==1 || w==2 || w==4) && a%w==0;
}
bool bus_read(machine_t *m, uint32_t a, unsigned w, uint32_t *v) {
  if(!valid_width(a,w)) return false;
  uint8_t *p=bus_memory(m,a,w);
  if(p) { *v=0; for(unsigned i=0;i<w;i++) *v|=(uint32_t)p[i]<<(8*i); return true; }
  if(w!=4 || a<GPU_BASE) return false;
  if(a>=INPUT_BASE+0x100 && a<INPUT_BASE+0x180) {
    unsigned offset=a-(INPUT_BASE+0x100), key=8+(offset&31)/4;
    if(key>=INPUT_KEY_COUNT) return false;
    switch(offset&~31u) {
      case INPUT_HELD: *v=m->keys[key]; return true;
      case INPUT_PRESSED: *v=m->pressed[key]; return true;
      case INPUT_RELEASED: *v=m->released[key]; return true;
      default: return false;
    }
  }
  if(a>=INPUT_BASE && a<INPUT_BASE+0x80) {
    unsigned offset=a-INPUT_BASE, key=(offset&31)/4;
    if(key>=INPUT_KEY_COUNT) return false;
    switch(offset&~31u) {
      case INPUT_HELD: *v=m->keys[key]; return true;
      case INPUT_PRESSED: *v=m->pressed[key]; return true;
      case INPUT_RELEASED: *v=m->released[key]; return true;
      default: return false;
    }
  }
  if(a==CLOCK_BASE) { *v=(uint32_t)m->ticks; return true; }
  switch(a-GPU_BASE) {
    case GPU_WIDTH: *v=VIDEO_WIDTH; break;
    case GPU_HEIGHT: *v=VIDEO_HEIGHT; break;
    case GPU_STRIDE: *v=VIDEO_WIDTH*3; break;
    case GPU_FORMAT: *v=1; break;
    case GPU_STATUS: *v=m->pending | (m->frame_event<<1) | (m->gpu_error<<2); break;
    case GPU_COLOR: *v=m->color; break;
    case GPU_X: *v=m->pixel_x; break;
    case GPU_Y: *v=m->pixel_y; break;
    case GPU_X3: *v=m->pixel_x3; break;
    case GPU_Y3: *v=m->pixel_y3; break;
    case GPU_Q1: *v=m->q1; break;
    case GPU_Q2: *v=m->q2; break;
    case GPU_Q3: *v=m->q3; break;
    case GPU_DEPTH: *v=m->depth_enabled; break;
    case GPU_X2: *v=m->pixel_x2; break;
    case GPU_Y2: *v=m->pixel_y2; break;
    case GPU_FRAME: *v=(uint32_t)m->frames; break;
    default: return false;
  }
  return true;
}
/* Liang-Barsky clipping bounds raster work even for extreme guest endpoints. */
static bool clip(double p, double q, double *lo, double *hi) {
  if(p==0) return q>=0;
  double t=q/p;
  if(p<0) { if(t>*hi) return false; if(t>*lo) *lo=t; }
  else { if(t<*lo) return false; if(t<*hi) *hi=t; }
  return true;
}
static double signed_coord(uint32_t v) { return v<=INT32_MAX ? (double)v : (double)v-4294967296.0; }
static void gpu_line(machine_t *m, machine_pixel_t color) {
  double x=signed_coord(m->pixel_x), y=signed_coord(m->pixel_y);
  double dx=signed_coord(m->pixel_x2)-x, dy=signed_coord(m->pixel_y2)-y, lo=0,hi=1;
  if(!clip(-dx,x,&lo,&hi) || !clip(dx,VIDEO_WIDTH-1-x,&lo,&hi) ||
     !clip(-dy,y,&lo,&hi) || !clip(dy,VIDEO_HEIGHT-1-y,&lo,&hi)) return;
  int x0=(int)(x+lo*dx+0.5), y0=(int)(y+lo*dy+0.5);
  int x1=(int)(x+hi*dx+0.5), y1=(int)(y+hi*dy+0.5);
  int ax=x1>x0 ? x1-x0 : x0-x1, ay=y1>y0 ? y1-y0 : y0-y1;
  int sx=x0<x1 ? 1:-1, sy=y0<y1 ? 1:-1, err=ax-ay;
  int steps=ax>ay?ax:ay, step=0;
  for(;;) {
    double t=steps ? (double)step/steps : 0;
    double q=m->q1+((double)m->q2-m->q1)*(lo+(hi-lo)*t);
    if((unsigned)x0<VIDEO_WIDTH && (unsigned)y0<VIDEO_HEIGHT) {
      size_t i=y0*VIDEO_WIDTH+x0;
      if(!m->depth_enabled || q>m->depth[i]) {
        m->video[i]=color; if(m->depth_enabled) m->depth[i]=q;
      }
    }
    step++;
    if(x0==x1 && y0==y1) break;
    int e=2*err;
    if(e>-ay) { err-=ay; x0+=sx; }
    if(e<ax) { err+=ax; y0+=sy; }
  }
}
static double orient(double ax,double ay,double bx,double by,double x,double y) {
  return (bx-ax)*(y-ay)-(by-ay)*(x-ax);
}
static void gpu_triangle(machine_t *m,machine_pixel_t color) {
  double x[3]={signed_coord(m->pixel_x),signed_coord(m->pixel_x2),signed_coord(m->pixel_x3)};
  double y[3]={signed_coord(m->pixel_y),signed_coord(m->pixel_y2),signed_coord(m->pixel_y3)};
  double area=orient(x[0],y[0],x[1],y[1],x[2],y[2]);
  if(area==0) return;
  double minx=x[0],maxx=x[0],miny=y[0],maxy=y[0];
  for(unsigned k=1;k<3;k++) { if(x[k]<minx) minx=x[k]; if(x[k]>maxx) maxx=x[k]; if(y[k]<miny) miny=y[k]; if(y[k]>maxy) maxy=y[k]; }
  if(maxx<0 || maxy<0 || minx>=VIDEO_WIDTH || miny>=VIDEO_HEIGHT) return;
  int left=minx<0?0:(int)minx, right=maxx>=VIDEO_WIDTH?VIDEO_WIDTH-1:(int)maxx;
  int top=miny<0?0:(int)miny, bottom=maxy>=VIDEO_HEIGHT?VIDEO_HEIGHT-1:(int)maxy;
  for(int py=top;py<=bottom;py++) for(int px=left;px<=right;px++) {
    double a=orient(x[1],y[1],x[2],y[2],px+.5,py+.5)/area;
    double b=orient(x[2],y[2],x[0],y[0],px+.5,py+.5)/area;
    double c=1-a-b;
    if(a<0 || b<0 || c<0) continue;
    double q=a*m->q1+b*m->q2+c*m->q3; size_t i=py*VIDEO_WIDTH+px;
    if(!m->depth_enabled || q>m->depth[i]) {
      m->video[i]=color; if(m->depth_enabled) m->depth[i]=q;
    }
  }
}
bool bus_write(machine_t *m, uint32_t a, unsigned w, uint32_t v) {
  if(!valid_width(a,w)) return false;
  uint8_t *p=bus_memory(m,a,w);
  if(p) { for(unsigned i=0;i<w;i++) p[i]=(uint8_t)(v>>(8*i)); return true; }
  if(w!=4 || a<GPU_BASE) return false;
  if(a>=INPUT_BASE+0x100 && a<INPUT_BASE+0x180) {
    unsigned offset=a-(INPUT_BASE+0x100);
    if(offset<INPUT_ACK || offset>=INPUT_ACK+4*(INPUT_KEY_COUNT-8) || (v&~3u)) return false;
    unsigned key=8+(offset-INPUT_ACK)/4;
    if(v&1) m->pressed[key]=false;
    if(v&2) m->released[key]=false;
    return true;
  }
  if(a>=INPUT_BASE && a<INPUT_BASE+0x80) {
    unsigned offset=a-INPUT_BASE;
    if(offset<INPUT_ACK || offset>=INPUT_ACK+4*8 || (v&~3u)) return false;
    unsigned key=(offset-INPUT_ACK)/4;
    if(v&1) m->pressed[key]=false;
    if(v&2) m->released[key]=false;
    return true;
  }
  switch(a-GPU_BASE) {
    case GPU_COLOR: m->color=v&0xffffff; return true;
    case GPU_X3: m->pixel_x3=v; return true;
    case GPU_Y3: m->pixel_y3=v; return true;
    case GPU_Q1: m->q1=v; return true;
    case GPU_Q2: m->q2=v; return true;
    case GPU_Q3: m->q3=v; return true;
    case GPU_DEPTH: if(v>1) return false; m->depth_enabled=v; return true;
    case GPU_X2: m->pixel_x2=v; return true;
    case GPU_Y2: m->pixel_y2=v; return true;
    case GPU_X: m->pixel_x=v; return true;
    case GPU_Y: m->pixel_y=v; return true;
    case GPU_STATUS:
      if(v&~6u) return false;
      if(v&2) m->frame_event=false;
      if(v&4) m->gpu_error=false;
      return true;
    case GPU_COMMAND: break;
    default: return false;
  }
  machine_pixel_t color={(uint8_t)(m->color>>16),(uint8_t)(m->color>>8),(uint8_t)m->color};
  switch(v) {
    case GPU_TRIANGLE: gpu_triangle(m,color); return true;
    case GPU_LINE: gpu_line(m,color); return true;
    case GPU_PRESENT:
      if(m->pending) { m->gpu_error=true; return true; }
      memcpy(m->submitted,m->video,sizeof(m->video)); m->pending=true; return true;
    case GPU_CLEAR:
      memset(m->depth,0,sizeof(m->depth));
      for(size_t i=0;i<VIDEO_PIXELS;i++) m->video[i]=color;
      return true;
    case GPU_PIXEL:
      if(m->pixel_x<VIDEO_WIDTH && m->pixel_y<VIDEO_HEIGHT)
        m->video[m->pixel_y*VIDEO_WIDTH+m->pixel_x]=color;
      return true;
    default: return false;
  }
}
void machine_advance(machine_t *m, uint64_t ticks) {
  uint64_t before=m->ticks/FRAME_TICKS;
  m->ticks+=ticks;
  if(m->ticks/FRAME_TICKS!=before && m->pending) {
    memcpy(m->published,m->submitted,sizeof(m->published));
    m->pending=false; m->frame_event=true; m->frames++;
  }
}
cpu_result_t machine_step(machine_t *m, size_t budget) {
  if(m->state==CPU_WAIT) {
    if(!budget) return (cpu_result_t){CPU_WAIT,0};
    if(!m->frame_event) machine_advance(m,FRAME_TICKS-m->ticks%FRAME_TICKS);
    if(!m->frame_event) return (cpu_result_t){CPU_WAIT,0};
    m->frame_event=false; m->state=CPU_BUDGET;
  }
  return cpu_step(m,budget);
}

void machine_key(machine_t *m, unsigned key, bool down) {
  if(key>=INPUT_KEY_COUNT || m->keys[key]==down) return;
  m->keys[key]=down;
  if(down) m->pressed[key]=true; else m->released[key]=true;
}
void machine_release_keys(machine_t *m) {
  for(unsigned key=0;key<INPUT_KEY_COUNT;key++) machine_key(m,key,false);
}
