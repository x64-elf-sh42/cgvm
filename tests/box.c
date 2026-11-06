#include "programs/boxdraw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static void frame(machine_t *m, size_t batch) {
  uint64_t next=m->frames+1;
  size_t calls=0;
  while(m->frames<next) {
    cpu_result_t r=machine_step(m,batch);
    assert(r.status!=CPU_FAULT && r.status!=CPU_HALT && ++calls<10000);
  }
  assert(m->regs[9]<=VIDEO_WIDTH-100 && m->regs[10]<=VIDEO_HEIGHT-100);
}
static uint64_t run(machine_t *m, size_t batch) {
  prog_t p=boxdraw(); assert(machine_load(m,&p));
  frame(m,batch); assert(m->regs[9]==462 && m->regs[10]==334);
  for(unsigned i=0;i<5;i++) frame(m,batch);
  assert(m->regs[9]==462 && m->regs[10]==334);
  machine_key(m,KEY_LEFT,true); machine_key(m,KEY_UP,true);
  for(unsigned i=0;i<170;i++) frame(m,batch);
  assert(m->regs[9]==0 && m->regs[10]==0);
  machine_release_keys(m);
  machine_key(m,KEY_RIGHT,true); machine_key(m,KEY_DOWN,true);
  for(unsigned i=0;i<320;i++) frame(m,batch);
  assert(m->regs[9]==924 && m->regs[10]==668);
  machine_key(m,KEY_LEFT,true); machine_key(m,KEY_UP,true);
  for(unsigned i=0;i<5;i++) frame(m,batch);
  assert(m->regs[9]==924 && m->regs[10]==668);
  machine_release_keys(m);
  for(unsigned i=0;i<5;i++) frame(m,batch);
  assert(m->regs[9]==924 && m->regs[10]==668);
  /* Bottom-right outline is fully visible, including the final screen pixel. */
  assert(m->published[VIDEO_PIXELS-1].green==255);
  assert(m->published[668*VIDEO_WIDTH+924].green==255);
  assert(m->published[0].green==0);
  uint64_t hash=0;
  for(size_t i=0;i<sizeof(m->published);i++) hash=hash*131+((uint8_t*)m->published)[i];
  return hash;
}
int main(void) {
  machine_t *m=malloc(sizeof(*m)); assert(m);
  uint64_t a=run(m,10000),b=run(m,37);
  assert(a==b);
  free(m); puts("Box input, edge clamping, and batch-independent output passed");
}
