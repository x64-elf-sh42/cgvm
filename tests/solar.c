#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
prog_t solar_physics_probe(void);
static double get(machine_t *m,unsigned a) { uint64_t b=0;double v;for(unsigned i=0;i<8;i++) b|=(uint64_t)m->data[a+i]<<(8*i);memcpy(&v,&b,8);return v; }
static void put(machine_t *m,unsigned a,double v) { uint64_t b;memcpy(&b,&v,8);for(unsigned i=0;i<8;i++) m->data[a+i]=(uint8_t)(b>>(8*i)); }
static unsigned u(machine_t *m,unsigned a) { uint32_t v;assert(bus_read(m,a,4,&v));return v; }
static void set(machine_t *m,unsigned a,unsigned v) { assert(bus_write(m,a,4,v)); }
static void step(machine_t *m) { cpu_result_t r=cpu_step(m,200000);if(r.status==CPU_FAULT) fprintf(stderr,"fault %d at %u\n",m->fault,m->fault_pc);assert(r.status==CPU_YIELD); }
static double distance(machine_t *m,unsigned body) {
  double d=0;for(unsigned k=0;k<3;k++) {double x=get(m,8600+k*8)-get(m,20000+body*64+k*8);d+=x*x;}return sqrt(d);
}
static void rendered_frame(machine_t *m) {
  uint64_t target=m->frames+1;
  while(m->frames<target) {
    cpu_result_t r=machine_step(m,10000);
    if(r.status==CPU_FAULT) {
      fprintf(stderr,"render fault %u at PC %u, yaw %u pitch %u, position %.12g %.12g %.12g\n",m->fault,m->fault_pc,m->regs[9],m->regs[10],get(m,8600),get(m,8608),get(m,8616));
      for(unsigned k=0;k<16;k++) fprintf(stderr,"F%u %.17g R%u %u\n",k,m->fpregs[k],k,m->regs[k]);
    }
    assert(r.status!=CPU_FAULT);
    /* Stop at publication without executing the next frame. Tests may then
     * change pose atomically, rather than midway through a culled mesh. */
    if(r.status==CPU_WAIT) machine_advance(m,FRAME_TICKS-m->ticks%FRAME_TICKS);
  }
}
int main(void) {
  machine_t *m=malloc(sizeof(*m));assert(m);prog_t p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  assert(get(m,20000+3*64+16)==149.6e6);assert(get(m,20000+3*64+24)==6378);
  set(m,8484,2);
  double before=get(m,8616);m->regs[11]=300;step(m);assert(fabs(get(m,8616)-before-1.0/6000)<1e-7);
  /* Destination travel and arrival. */
  set(m,8464,4);machine_key(m,KEY_T,true);step(m);machine_key(m,KEY_T,false);
  unsigned steps=0;while(u(m,8472)&&steps++<1000) step(m);
  assert(steps<1000&&u(m,8476)==1&&m->regs[11]==0);
  assert(fabs(distance(m,4)-get(m,20000+4*64+40))<.1);
  /* Long-haul travel is bounded and arrives at the outer system. */
  p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  set(m,8464,8);set(m,8472,1);steps=0;
  while(u(m,8472)&&steps++<5000) step(m);
  assert(steps<5000&&u(m,8476)==1);
  assert(fabs(distance(m,8)-get(m,20000+8*64+40))<1);
  /* Travel toggles once per press and can be cancelled. */
  set(m,8464,4);machine_key(m,KEY_T,true);step(m);assert(u(m,8472)==1);
  step(m);assert(u(m,8472)==1);machine_key(m,KEY_T,false);machine_key(m,KEY_T,true);step(m);assert(u(m,8472)==0);
  machine_release_keys(m);
  /* A full cruise step crossing Earth must still stop outside it. */
  put(m,8600,0);put(m,8608,0);put(m,8616,149.6e6-1e6);
  set(m,8464,8);set(m,8472,1);
  for(unsigned k=0;k<3;k++) put(m,20000+8*64+k*8,k==2?149.6e6+1e6:0);
  step(m);assert(u(m,8476)==2&&u(m,8472)==0);
  assert(get(m,8616)<149.6e6-6478&&distance(m,3)>6478);
  /* Precision remains adequate at Neptune-sized coordinates. */
  set(m,8472,0);set(m,8484,2);put(m,8600,4.5e9);put(m,8608,1e8);put(m,8616,0);m->regs[9]=64;m->regs[10]=0;m->regs[11]=300;
  double x=get(m,8600);step(m);assert(get(m,8600)>x&&get(m,8600)-x<.00018);
  /* Render initial full-scale scene without a window. */
  p=solar_flight();assert(machine_load(m,&p));
  rendered_frame(m);
  FILE *f=fopen("/tmp/cgvm-solar.ppm","wb");assert(f);fprintf(f,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,f);fclose(f);
  /* Sun meshes require reciprocal-depth precision at million-km distances. */
  put(m,8600,0);put(m,8608,0);put(m,8616,-5e6);m->regs[9]=0;m->regs[10]=0;
  rendered_frame(m);rendered_frame(m);
  unsigned bright=0;for(unsigned y=360;y<408;y++)for(unsigned x=470;x<512;x++)bright+=m->published[y*VIDEO_WIDTH+x].red>100;
  assert(bright>100);
  /* Regression: the off-screen Sun used to project beyond int32 at yaw 64. */
  put(m,8600,0);put(m,8608,0);put(m,8616,-8e6);m->regs[9]=64;m->regs[10]=0;
  rendered_frame(m);rendered_frame(m);
  assert(u(m,8872)==3); /* Sun is culled; labels may occupy the center pixel. */
  /* Near and distant limbs, vertical looks, and inverted views. */
  const double views[]={-.85e6,-8e6,-13e6};
  const unsigned angles[]={0,32,48,56,64,72,96,128,192};
  for(unsigned d=0;d<3;d++) for(unsigned a=0;a<9;a++) {
    put(m,8616,views[d]);m->regs[9]=angles[a];m->regs[10]=0;rendered_frame(m);
    m->regs[9]=0;m->regs[10]=angles[a];rendered_frame(m);
  }
  /* Exercise the renderer during complete trips, not just the physics probe. */
  const unsigned destinations[]={2,8};
  for(unsigned route=0;route<2;route++) {
    p=solar_flight();assert(machine_load(m,&p));rendered_frame(m);
    set(m,8464,destinations[route]);set(m,8472,1);
    unsigned frames=0;
    while(u(m,8472)&&frames++<5000) rendered_frame(m);
    assert(frames<5000&&u(m,8476)==1);
    rendered_frame(m);rendered_frame(m);
  }
  printf("Solar: scale, precision, cruise arrival, swept obstruction and render passed (%llu ticks)\n",(unsigned long long)m->ticks);
  free(m);
}
