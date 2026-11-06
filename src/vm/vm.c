#include "vm.h"
#include "machine.h"
#include <string.h>
static machine_t machine;
static vm_t frontend;
static bool loaded;
void vm_reset(void) { machine_reset(&machine); loaded=false; }
void vm_load(const prog_t *p) { loaded=machine_load(&machine,p); }
bool vm_run(void) {
  if(!loaded) return false;
  if(!gpu_init(&frontend)) { gpu_destroy(frontend.gpu); return false; }
  gpu_t *g=frontend.gpu;
  machine.fb=g->fb; machine.fb2=g->fb2;
  machine.width=g->screen_width; machine.height=g->screen_height;
  cpu_result_t r={CPU_BUDGET,0};
  bool quit=false;
  uint64_t shown=0;
  uint64_t start=SDL_GetTicksNS();
  while(!quit) {
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
      if(e.type==SDL_EVENT_QUIT) quit=true;
      if(e.type==SDL_EVENT_WINDOW_FOCUS_LOST) machine_release_keys(&machine);
      if(e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP) {
        unsigned key=INPUT_KEY_COUNT;
        switch(e.key.scancode) {
          case SDL_SCANCODE_UP: key=KEY_UP; break;
          case SDL_SCANCODE_LEFT: key=KEY_LEFT; break;
          case SDL_SCANCODE_DOWN: key=KEY_DOWN; break;
          case SDL_SCANCODE_RIGHT: key=KEY_RIGHT; break;
          case SDL_SCANCODE_Z: key=KEY_Z; break;
          case SDL_SCANCODE_X: key=KEY_X; break;
          case SDL_SCANCODE_M: key=KEY_M; break;
          case SDL_SCANCODE_TAB: key=KEY_TAB; break;
          case SDL_SCANCODE_T: key=KEY_T; break;
          case SDL_SCANCODE_F: key=KEY_F; break;
          default: break;
        }
        machine_key(&machine,key,e.type==SDL_EVENT_KEY_DOWN);
      }
    }
    if(quit) break;
    uint64_t deadline=start+(machine.ticks/GUEST_HZ)*1000000000ull
      +(machine.ticks%GUEST_HZ)*1000000000ull/GUEST_HZ;
    uint64_t now=SDL_GetTicksNS();
    if(now<deadline) { SDL_Delay(1); continue; }
    if(machine.frames!=shown) {
      memcpy(g->fb2,machine.published,sizeof(machine.published));
      gpu_swap(g); shown=machine.frames;
    }
    if(r.status==CPU_HALT || r.status==CPU_FAULT) break;
    r=machine_step(&machine,10000);
  }
  bool ok=quit || r.status==CPU_HALT;
  if(ok) {
    bitmap_t b={machine.video,g->screen_width,g->screen_height};
    ok=gfx_save_png(&b,"test.png")==0;
  } else if(r.status==CPU_FAULT) {
    fprintf(stderr,"CPU fault %u at PC %u\n",machine.fault,machine.fault_pc);
  }
  machine.fb=machine.fb2=NULL;
  gpu_destroy(g);
  return ok;
}
