#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

prog_t solar_physics_probe(void);
prog_t solar_region_probe(void);
static double get(machine_t *m,unsigned a) {
  uint64_t bits=0;double v;for(unsigned k=0;k<8;k++)bits|=(uint64_t)m->data[a+k]<<(k*8);
  memcpy(&v,&bits,8);return v;
}
static void put(machine_t *m,unsigned a,double v) {
  uint64_t bits;memcpy(&bits,&v,8);for(unsigned k=0;k<8;k++)m->data[a+k]=(uint8_t)(bits>>(k*8));
}
static unsigned u(machine_t *m,unsigned a) {uint32_t v;assert(bus_read(m,a,4,&v));return v;}
static void step(machine_t *m) {
  for(unsigned i=0;i<2000;i++) {
    cpu_result_t r=cpu_step(m,10000);
    if(r.status==CPU_FAULT)fprintf(stderr,"scanner fault %u at %u, address %u\n",m->fault,m->fault_pc,m->fault_address);
    assert(r.status!=CPU_FAULT);
    if(r.status==CPU_YIELD)return;
  }
  assert(!"unbounded scanner work");
}
static void tap(machine_t *m,unsigned key) {machine_key(m,key,true);step(m);machine_key(m,key,false);step(m);}
static void rock(machine_t *m,unsigned slot,double x,double y,double z) {
  unsigned a=24000+slot*64;put(m,a,x);put(m,a+8,1e10+y);put(m,a+16,z);put(m,a+24,1);put(m,a+32,2);
}
static void empty(machine_t *m) {for(unsigned slot=0;slot<27;slot++)put(m,24000+slot*64+24,0);}
int main(void) {
  machine_t *m=malloc(sizeof(*m));assert(m);prog_t p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  /* Stable cache outside the belt lets boundary fixtures isolate the scan from
   * generation. Separate rendered tests acquire and approach real guest rocks. */
  put(m,8600,0);put(m,8608,1e10);put(m,8616,0);step(m);
  assert(!u(m,12400)&&!u(m,12500));
  const double axes[][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  for(unsigned axis=0;axis<6;axis++) {
    empty(m);rock(m,0,axes[axis][0]*50e6,axes[axis][1]*50e6,axes[axis][2]*50e6);
    tap(m,KEY_S);assert(u(m,12400)==1&&u(m,12500)==1&&get(m,12424)==50e6);
    tap(m,KEY_C);assert(!u(m,12400)&&!u(m,12500));
    rock(m,0,axes[axis][0]*(50e6+1),axes[axis][1]*(50e6+1),axes[axis][2]*(50e6+1));
    tap(m,KEY_S);assert(!u(m,12400)&&u(m,12500)==2);
  }
  empty(m);rock(m,0,40e6,0,40e6);tap(m,KEY_S);
  assert(!u(m,12400)&&u(m,12500)==2); /* Spherical, not cubic, range. */
  rock(m,0,2.5e6,0,0);step(m);assert(!u(m,12400)); /* No passive acquisition. */
  /* S locks once; held S and a closer contact cannot change that lock. */
  machine_key(m,KEY_S,true);step(m);step(m);assert(u(m,12400)==1&&get(m,12504)==2.5e6);
  rock(m,1,1e6,0,0);step(m);assert(get(m,12504)==2.5e6);
  machine_key(m,KEY_S,false);step(m);tap(m,KEY_S);assert(get(m,12504)==1e6);
  /* Nearest contact wins regardless of cache order, and empty slots are ignored. */
  rock(m,26,500000,0,0);tap(m,KEY_S);assert(get(m,12504)==500000);
  tap(m,KEY_C);step(m);assert(!u(m,12400)&&!u(m,12500));
  empty(m);rock(m,0,2.5e6,0,0);tap(m,KEY_S);assert(get(m,12504)==2.5e6);
  /* A cache rebuild must not erase or retarget an in-range lock. */
  unsigned builds=u(m,10800);put(m,8600,3e6+1);step(m);
  assert(u(m,10800)>builds&&u(m,12400)==1&&get(m,12504)==2.5e6);
  assert(fabs(get(m,12424)-500001)<1e-6);
  /* Leaving range drops the contact. Returning does not reacquire silently. */
  put(m,8600,-48e6);step(m);assert(!u(m,12400)&&u(m,12500)==3);
  put(m,8600,0);step(m);assert(!u(m,12400)&&u(m,12500)==3);
  /* Clear wins over scan when both keys are pressed in the same update. */
  machine_key(m,KEY_S,true);machine_key(m,KEY_C,true);step(m);step(m);
  assert(!u(m,12400)&&!u(m,12500));machine_release_keys(m);
  /* Real distant contacts must be generated outside the rendering cache.
   * An independent exhaustive enumeration verifies the region-bound pruning. */
  machine_t *oracle=malloc(sizeof(*oracle));assert(oracle);
  const double poses[][3]={{2*149.6e6,0,0},{-3.4*149.6e6,0,0},{2.7*149.6e6,100e6,0}};
  for(unsigned view=0;view<3;view++) {
    p=solar_physics_probe();assert(machine_load(m,&p));step(m);
    for(unsigned k=0;k<3;k++)put(m,8600+k*8,poses[view][k]);step(m);
    for(unsigned slot=0;slot<27;slot++)assert(get(m,24000+slot*64+24)==0);
    uint64_t ticks=m->ticks;tap(m,KEY_S);
    assert(u(m,12400)==1&&get(m,12424)>20e6&&get(m,12424)<=50e6);
    assert(u(m,12532)>0&&u(m,12536)<=35*35*35);
    for(unsigned slot=0;slot<27;slot++)assert(get(m,24000+slot*64+24)==0);
    printf("Wide scan %u: %.3f MKM, %u generated cells, %u visited, %llu guest ticks\n",view,get(m,12424)/1e6,u(m,12532),u(m,12536),(unsigned long long)(m->ticks-ticks));
    p=solar_region_probe();assert(machine_load(oracle,&p));double nearest=1e100,expected[3]={0};
    for(int z=-17;z<=17;z++)for(int y=-17;y<=17;y++)for(int x=-17;x<=17;x++) {
      int offsets[]={x,y,z};
      for(unsigned k=0;k<3;k++)put(oracle,10536+k*8,floor(poses[view][k]/3e6)+offsets[k]);
      step(oracle);if(get(oracle,21624)<=0)continue;
      double d=0;for(unsigned k=0;k<3;k++){double v=get(oracle,21600+k*8)-poses[view][k];d+=v*v;}
      if(d<nearest){nearest=d;for(unsigned k=0;k<3;k++)expected[k]=get(oracle,21600+k*8);}
    }
    assert(fabs(get(m,12424)-sqrt(nearest))<1e-6);
    for(unsigned k=0;k<3;k++)assert(get(m,12504+k*8)==expected[k]);
  }
  free(oracle);
  puts("Scanner: explicit acquisition, spherical range boundaries, nearest choice, stable lock, cache transitions, range loss and clear passed");
  free(m);
}
