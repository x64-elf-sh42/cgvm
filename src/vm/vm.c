#include "vm.h"

#include <string.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include <unistd.h>
#ifdef DO_PMC
#include "pmc_wrap.h"
#endif


#define FL_EQ 0x1000u
#define FL_NE 0x8000u
#define FL_LT 0x2000u
#define FL_GT 0x4000u
#define FL_CMP_MASK 0xf000u

static vm_t vm = {0};

static void *run_sys(void *args) {
  
  vm_t *v = (vm_t*)args;
  if(!v) {return nullptr;}

  gpu_init(v); // contains SDL init so needs to be on render thread.
  while(!atomic_load(&v->gpu->ready)) {}; // wait for gpu to become ready befor trying rendering..?

  printf("starting render loop...\n");

  while(atomic_load(&v->running)) {
    gpu_swap(v->gpu);
    SDL_Delay(1000/v->gpu->refresh_rate); // 24fps'ish?
  }

  gpu_destroy(v->gpu);
  return nullptr;
}

void vm_reset(void) {
  memset(&vm, 0, sizeof(vm_t));
  vm.run_system = run_sys;
  atomic_init(&vm.running, false);
  vm.program_loaded=false;
  vm.io_space = io_space_get();
  vm.reset=true;

  // reset io/gpu?

}

void vm_load(const prog_t *p) {
  if(!p->code || p->codelen==0 || (p->entry_pc >= p->codelen)) {
    fprintf(stderr, "invalid program\n");
    return;
  }
  if(p->data && p->datalen) {
    memcpy(vm.data, p->data, p->datalen);
  }
  memcpy(vm.code, p->code, p->codelen*sizeof(insn_t));
  vm.regs[REG_PC]=p->entry_pc;
  vm.program_loaded=true;
  vm.reset=false; 
}

// PROGRAM EXECUTOR
static void ve() {
  #define DISPATCH() do {   \
    ins = code[pc];         \
    goto *dispatch[OP(ins)]; \
  } while(0)

  static void *dispatch[OP_MAX_OPERATION] = {
    [OP_UNDEFINED] = &&op_undefined,
    [OP_EXIT] = &&op_exit,
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
    [OP_CMP_RR] = &&op_cmp_rr,
    [OP_CMP_RI] = &&op_cmp_ri,
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

  reg_t *regs = vm.regs;
  insn_t *code = vm.code;
  uint8_t *data = vm.data;
  uint32_t pc = regs[REG_PC];
  insn_t ins = 0;
  cpu_flags_t flags = vm.flags;
  uint32_t x=0,y=0,c=0;
  pixel_t *fb = vm.gpu->fb;  
  pixel_t *fb2 = vm.gpu->fb2;
  size_t screenw = (size_t)vm.gpu->screen_width;
  size_t screenh = (size_t)vm.gpu->screen_height;
  
  DISPATCH(); // start execution at pc


  // exits
  op_undefined:
    flags&= ~FL_CMP_MASK;
    flags|=3;
    vm.flags=flags;
    regs[REG_PC]=pc;
    goto ve_fin;   
  op_exit:
    flags&=~FL_CMP_MASK;
    flags|=1;
    vm.flags=flags;
    regs[REG_PC]=pc;
    goto ve_fin;

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
  op_add_rr:// check for overflow setf
    regs[RR_DST(ins)]+=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();
  op_add_ri:
    regs[RR_DST(ins)]+=IMM20(ins);
    pc++;
    DISPATCH();
    
  op_sub_rr: // check underflow setf
    regs[RR_DST(ins)]-=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  op_sub_ri:
    regs[RR_DST(ins)]-=IMM20(ins);
    pc++;
    DISPATCH();
    
  op_mul_rr: // check overflow setf
    regs[RR_DST(ins)]*=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();
    
  op_div_rr: // todo check for div0
    regs[RR_DST(ins)]/=regs[RR_SRC(ins)];
    pc++;
    DISPATCH();

  // comparison/branching
  op_cmp_rr:
    flags&=~FL_CMP_MASK;
    y = regs[RR_DST(ins)];
    x = regs[RR_SRC(ins)];    
    goto op_cmp;
  op_cmp_ri:
    flags&=~FL_CMP_MASK;
    y = regs[RR_DST(ins)];
    x = IMM20(ins);
    goto op_cmp; // could fallthru

 op_cmp: 
    if(y==x) {
      flags|=FL_EQ; // TODO: document flags
    } else {
      flags|=FL_NE;
    }
    if(y<x) {
      flags|=FL_LT;
    }
    if(y>x) {
      flags|=FL_GT;
    }
    pc++;
    DISPATCH();
       
 op_jmp: // TODO -> set PC, need to bounds check
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
  op_set_pixel: // set pixel from x:r5,y:r6 to color encoded in imm24
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
    c = IMM24(ins);
    for(uint32_t i=0;i<screenw*screenh; i++){
      fb[i].red = (c>>16)&0xff;
      fb[i].green = (c>>8)&0xff;
      fb[i].blue = (uint8_t)c;
    }
    pc++;
    DISPATCH();
  op_fb_swap:
    memcpy(fb2, fb, screenw*screenh*sizeof(pixel_t));
    pc++;
    DISPATCH();

    
ve_fin:

}


bool vm_run(void) {
  bool status = false;

#ifdef DO_PMC
  bool have_pmc = vm_pmc_init();
  if (have_pmc) {
    vm_pmc_start();
  }
#endif

  // start background_thread
  // using vm.run_system as thread_fn
  atomic_store(&vm.running, true);

  pthread_attr_init(&vm.sys_tattr);
  pthread_create(&vm.sys_thread, &vm.sys_tattr, vm.run_system, &vm);

  while (!vm.gpu || !atomic_load(&vm.gpu->ready)) {
    SDL_Delay(1);
  }

  printf("starting program...\n");
  
  ve(); // we only want to run PMC on this part as it executes our program.
        // the rest of code is just setup / teardown and saving of results.

  atomic_store(&vm.running, false);
  // join system background thread
  pthread_join(vm.sys_thread, NULL);
  pthread_attr_destroy(&vm.sys_tattr);
  

#ifdef DO_PMC
  if (have_pmc) {
    vm_pmc_stop_read();
    vm_pmc_print();
    vm_pmc_shutdown();
  }
#endif

  if (vm.flags & 2) {
    fprintf(stderr, "ERR: vm.flags: %08x\n", vm.flags);
  } else {
    bitmap_t b = { vm.gpu->fb, vm.gpu->screen_width, vm.gpu->screen_height };
    int r = gfx_save_png(&b, "test.png"); //  should pass in some path so we can do multiple runs etc. yada yada.
    if (r == 0) {
      status = true;
    } else {
      fprintf(stderr, "ERR: failed to save fb as png: %d\n", r);
    }
  }

  return status;
}
