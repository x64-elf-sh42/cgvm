#include "programs/flight.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void frame(machine_t *m,size_t budget) {
  uint64_t target=m->frames+1;
  unsigned calls=0;
  while(m->frames<target) {
    cpu_result_t r=machine_step(m,budget);
    assert(r.status!=CPU_FAULT && r.status!=CPU_HALT && ++calls<200000);
  }
  assert(m->regs[11]<=300);
}
static float position(machine_t *m,unsigned axis) {
  uint32_t bits; float v;
  assert(bus_read(m,8192+axis*4,4,&bits)); memcpy(&v,&bits,4); return v;
}
static uint64_t hash(machine_t *m) {
  uint64_t h=0; for(size_t i=0;i<sizeof(m->published);i++) h=h*131+((uint8_t*)m->published)[i]; return h;
}
int main(void) {
  machine_t *m=malloc(sizeof(*m)); assert(m); prog_t p=flight();
  assert(machine_load(m,&p)); frame(m,1000); uint64_t first=hash(m);
  FILE *out=fopen("/tmp/cgvm-flight.ppm","wb"); assert(out);
  fprintf(out,"P6\n1024 768\n255\n"); fwrite(m->published,sizeof(m->published),1,out); fclose(out);
  assert(machine_load(m,&p)); frame(m,37); assert(hash(m)==first);
  for(unsigned i=0;i<3;i++) frame(m,1000);
  assert(position(m,2)==0 && hash(m)==first);
  machine_key(m,KEY_Z,true);
  for(unsigned i=0;i<400;i++) frame(m,1000);
  assert(m->regs[11]==300 && position(m,2)>35 && position(m,2)<50);
  machine_release_keys(m); float before=position(m,2);
  for(unsigned i=0;i<60;i++) frame(m,1000);
  assert(m->regs[11]==300 && position(m,2)>before+9.8 && position(m,2)<before+10.2);
  machine_key(m,KEY_Z,true); machine_key(m,KEY_X,true);
  for(unsigned i=0;i<5;i++) frame(m,1000);
  assert(m->regs[11]==300);
  machine_release_keys(m); machine_key(m,KEY_X,true);
  for(unsigned i=0;i<330;i++) frame(m,1000);
  assert(m->regs[11]==0); before=position(m,2);
  for(unsigned i=0;i<5;i++) frame(m,1000);
  assert(position(m,2)==before);
  machine_release_keys(m); machine_key(m,KEY_RIGHT,true);
  for(unsigned i=0;i<64;i++) frame(m,1000);
  machine_release_keys(m); machine_key(m,KEY_Z,true);
  for(unsigned i=0;i<60;i++) frame(m,1000);
  assert(position(m,0)>.8 && m->regs[9]==64);
  /* Put camera through the first asteroid, exercising near-plane crossings. */
  machine_release_keys(m);
  for(unsigned axis=0;axis<3;axis++) {
    uint32_t bits; assert(bus_read(m,2080+axis*4,4,&bits));
    assert(bus_write(m,8192+axis*4,4,bits));
  }
  machine_key(m,KEY_UP,true); machine_key(m,KEY_RIGHT,true);
  for(unsigned i=0;i<256;i++) frame(m,1000);
  assert(machine_load(m,&p));
  /* All geometry out of range except one edge crossing z=0.5. */
  for(unsigned object=0;object<24;object++) for(unsigned v=0;v<4;v++) {
    float xyz[3]={0,0,200};
    if(object==0) { xyz[0]=v==0 ? -1:1; xyz[2]=v==0 ? -6:-4; }
    for(unsigned axis=0;axis<3;axis++) {
      uint32_t bits; memcpy(&bits,&xyz[axis],4);
      assert(bus_write(m,2080+object*48+v*12+axis*4,4,bits));
    }
  }
  frame(m,1000);
  assert(m->published[384*VIDEO_WIDTH+112].red==0x60);
  assert(m->published[384*VIDEO_WIDTH+712].red==0x60);
  assert(m->published[384*VIDEO_WIDTH+111].red==0);
  free(m); puts("Flight: repeatable scene, speed bounds, coasting, steering, camera crossings passed");
}
