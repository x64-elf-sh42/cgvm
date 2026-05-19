#include "vm.h"

#include <string.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include <unistd.h>
#ifdef DO_PMC
#include "pmc_wrap.h"
#endif


static vm_t vm = {0};

static void *run_sys(void *args) {
  
  vm_t *v = (vm_t*)args;
  if(!v) {return nullptr;}

  if(!SDL_Init(SDL_INIT_VIDEO)) {
      fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return nullptr;
  }
  SDL_Window *window = SDL_CreateWindow("CGVM", v->screen_width, v->screen_height, 0);
  SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
  SDL_Texture *texture = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STREAMING,
    v->screen_width,
    v->screen_height                                  
  );

  printf("starting render loop...\n");

  while(atomic_load(&v->running)) {
    
    SDL_UpdateTexture(texture, nullptr, &v->fb, v->screen_width*3);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer,texture,nullptr,nullptr);
    SDL_RenderPresent(renderer);


    SDL_Delay(1000/vm.refresh_rate); // 24fps'ish?
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
 
  return nullptr;
}


void vm_reset(void) {
  memset(&vm, 0, sizeof(vm_t));
  vm.run_system = run_sys;
  vm.screen_width = FB_WIDTH;
  vm.screen_height = FB_HEIGHT;
  vm.refresh_rate=24; // just cap it low  
  atomic_init(&vm.running, false);
  vm.program_loaded=false;
  vm.reset=true;
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
    [OP_MUL_RR] = &&op_mul_rr,
    [OP_DIV_RR] = &&op_div_rr,
    [OP_CMP_RR] = &&op_cmp_rr,
    [OP_JMP] = &&op_jmp,
    [OP_JE] = &&op_je,
    [OP_JNE] = &&op_jne,
    [OP_JG] = &&op_jg,
    [OP_JL] = &&op_jl,
    [OP_JGE] = &&op_jge,
    [OP_JLE] = &&op_jle,
    [OP_SET_PIXEL] = &&op_set_pixel,
  };

  reg_t *regs = vm.regs;
  insn_t *code = vm.code;
  uint8_t *data = vm.data;
  uint32_t pc = regs[REG_PC];
  insn_t ins = 0;
  cpu_flags_t flags = vm.flags;

  
  DISPATCH(); // start execution at pc


  // exits
  op_undefined:
    flags|=3;
    vm.flags=flags;
    regs[REG_PC]=pc;
    goto ve_fin;   
  op_exit:
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
    if(regs[RR_DST(ins)]==regs[RR_SRC(ins)]) {
      flags|=0x1000; // TODO: document flags
    } else {
      flags|=0x6000;
    }
    if(regs[RR_DST(ins)]<regs[RR_SRC(ins)]) {
      flags|=0x2000;
    }
    if(regs[RR_DST(ins)]>regs[RR_SRC(ins)]) {
      flags|=0x4000;
    }
    pc++;
    DISPATCH();
        
 op_jmp: // TODO -> set PC, need to bounds check
    pc=IMM24(ins);
    DISPATCH();
    
  op_je:
    if(flags&0x1000) {
      flags &= (uint32_t)~0x1000;
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jne:
    if(flags&0x6000) {
      flags &= (uint32_t)~0x6000;
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jg:
    if(flags&0x2000) {
      flags &= (uint32_t)~0x2000;
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();
    
  op_jl:
    if(flags&0x4000) {
      flags &= (uint32_t)~0x4000;
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  op_jge:
    if(flags&0x2000 || flags&0x1000) {
      flags &= (uint32_t)~0x3000;
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();
  op_jle:
    if(flags&0x4000 || flags&0x1000) {
      flags &= (uint32_t)~(0x5000);
      pc=IMM24(ins);
    } else {
      pc++;
    }
    DISPATCH();

  // graphics
  op_set_pixel: // set pixel from x:r5,y:r6 to color encoded in imm24
    uint32_t x = regs[REG_R5];
    uint32_t y = regs[REG_R6];
    uint32_t c = IMM24(ins);
    if(y < FB_HEIGHT && x < FB_WIDTH) {
      vm.fb[(y*FB_WIDTH)+x].red = (c >> 16) & 0xff;
      vm.fb[(y*FB_WIDTH)+x].green = (c >> 8) & 0xff;
      vm.fb[(y*FB_WIDTH)+x].blue = (uint8_t)c;
    }
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

  ve(); // we only want to run PMC on this part as it executes our program.
        // the rest of code is just setup / teardown and saving of results.

  sleep(3);
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
    bitmap_t b = { vm.fb, 1024, 768 };
    int r = gfx_save_png(&b, "test.png"); //  should pass in some path so we can do multiple runs etc. yada yada.
    if (r == 0) {
      status = true;
    } else {
      fprintf(stderr, "ERR: failed to save fb as png: %d\n", r);
    }
  }

  return status;
}
