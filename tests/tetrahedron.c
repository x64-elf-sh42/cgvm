#include "programs/tetrahedron.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void frame(machine_t *m,size_t batch) {
  uint64_t target=m->frames+1;
  unsigned calls=0;
  while(m->frames<target) {
    cpu_result_t r=machine_step(m,batch);
    assert(r.status!=CPU_FAULT && r.status!=CPU_HALT && ++calls<10000);
  }
}
static uint64_t image_hash(machine_t *m) {
  uint64_t h=0;
  for(size_t i=0;i<sizeof(m->published);i++) h=h*131+((uint8_t*)m->published)[i];
  return h;
}
static void projection(machine_t *m) {
  const double vertices[4][3]={{0,1.2,0},{-1,-.8,-.8},{1,-.8,-.8},{0,-.8,1.2}};
  double yaw=m->regs[9]*2*3.141592653589793/256, pitch=m->regs[10]*2*3.141592653589793/256;
  for(unsigned i=0;i<4;i++) {
    double x=vertices[i][0],y=vertices[i][1],z=vertices[i][2];
    double xx=x*cos(yaw)+z*sin(yaw), zz=z*cos(yaw)-x*sin(yaw);
    double yy=y*cos(pitch)-zz*sin(pitch), depth=zz*cos(pitch)+y*sin(pitch)+m->regs[11]*.01;
    uint32_t sx,sy;
    assert(bus_read(m,4096+i*8,4,&sx) && bus_read(m,4100+i*8,4,&sy));
    assert(abs((int)sx-(int)(512+400*xx/depth))<=1);
    assert(abs((int)sy-(int)(384-400*yy/depth))<=1);
  }
}
int main(void) {
  machine_t *m=malloc(sizeof(*m)); assert(m);
  prog_t p=tetrahedron(); assert(machine_load(m,&p));
  frame(m,1); projection(m); uint64_t initial=image_hash(m);
  FILE *out=fopen("/tmp/cgvm-tetrahedron.ppm","wb"); assert(out);
  fprintf(out,"P6\n1024 768\n255\n"); fwrite(m->published,sizeof(m->published),1,out); fclose(out);
  assert(machine_load(m,&p)); frame(m,10000); assert(image_hash(m)==initial);
  for(unsigned i=0;i<3;i++) frame(m,10000);
  assert(image_hash(m)==initial);
  machine_key(m,KEY_RIGHT,true); machine_key(m,KEY_DOWN,true);
  for(unsigned i=0;i<64;i++) frame(m,10000);
  machine_release_keys(m); frame(m,10000); projection(m);
  assert(image_hash(m)!=initial);
  machine_key(m,KEY_X,true);
  for(unsigned i=0;i<260;i++) frame(m,10000);
  assert(m->regs[11]==350); machine_release_keys(m); frame(m,10000); projection(m);
  machine_key(m,KEY_Z,true);
  for(unsigned i=0;i<860;i++) frame(m,10000);
  assert(m->regs[11]==1200); machine_release_keys(m); frame(m,10000); projection(m);
  /* Opposing controls cancel, and the guest retains finite projected coordinates. */
  uint32_t yaw=m->regs[9]; machine_key(m,KEY_LEFT,true); machine_key(m,KEY_RIGHT,true);
  for(unsigned i=0;i<4;i++) frame(m,10000);
  assert(m->regs[9]==yaw);
  machine_reset(m);
  assert(bus_write(m,GPU_BASE+GPU_COLOR,4,0x33ff44));
  assert(bus_write(m,GPU_BASE+GPU_X,4,0x80000000));
  assert(bus_write(m,GPU_BASE+GPU_X2,4,0x7fffffff));
  assert(bus_write(m,GPU_BASE+GPU_COMMAND,4,GPU_LINE));
  assert(m->video[0].green==255 && m->video[1023].green==255);
  insn_t convert[]={ENC_RR(OP_FTOI,5,1),EXIT()};
  p=(prog_t){.code=convert,.codelen=2}; assert(machine_load(m,&p)); m->fpregs[1]=NAN;
  assert(cpu_step(m,2).status==CPU_FAULT && m->fault==FAULT_CONVERSION);
  assert(machine_load(m,&p)); m->fpregs[1]=-3.9;
  assert(cpu_step(m,2).status==CPU_HALT && m->regs[5]==(uint32_t)-3);
  free(m); puts("Tetrahedron projection, controls, distance limits, clipping, conversions passed");
}
