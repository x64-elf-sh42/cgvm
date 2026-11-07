#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

prog_t solar_physics_probe(void);
static double get(machine_t *m,unsigned a) {
  uint64_t bits=0;double v;
  for(unsigned k=0;k<8;k++)bits|=(uint64_t)m->data[a+k]<<(k*8);
  memcpy(&v,&bits,8);return v;
}
static void put(machine_t *m,unsigned a,double v) {
  uint64_t bits;memcpy(&bits,&v,8);
  for(unsigned k=0;k<8;k++)m->data[a+k]=(uint8_t)(bits>>(k*8));
}
static unsigned u(machine_t *m,unsigned a) {uint32_t v;assert(bus_read(m,a,4,&v));return v;}
static void set(machine_t *m,unsigned a,unsigned v) {assert(bus_write(m,a,4,v));}
static void step(machine_t *m) {
  for(unsigned i=0;i<2000;i++) {
    cpu_result_t r=cpu_step(m,10000);
    if(r.status==CPU_FAULT)fprintf(stderr,"navigation fault %u at %u, address %u\n",m->fault,m->fault_pc,m->fault_address);
    assert(r.status!=CPU_FAULT);
    if(r.status==CPU_YIELD)return;
    assert(r.status==CPU_BUDGET);
  }
  assert(!"unbounded navigation work");
}
static void tap(machine_t *m,unsigned key) {machine_key(m,key,true);step(m);machine_key(m,key,false);step(m);}
static void frame(machine_t *m) {
  uint64_t target=m->frames+1;
  while(m->frames<target) {
    cpu_result_t r=machine_step(m,10000);
    if(r.status==CPU_FAULT)fprintf(stderr,"navigation render fault %u at %u\n",m->fault,m->fault_pc);
    assert(r.status!=CPU_FAULT);
    if(r.status==CPU_WAIT)machine_advance(m,FRAME_TICKS-m->ticks%FRAME_TICKS);
  }
}
static void pose(machine_t *m,double x,double y,double z) {put(m,8600,x);put(m,8608,y);put(m,8616,z);}
static double angle_error(unsigned actual,double radians) {
  double expected=radians*256/(2*acos(-1));
  double delta=fmod((double)actual-expected+384,256)-128;
  return fabs(delta);
}
static void snapshot(machine_t *m,const char *path) {
  FILE *out=fopen(path,"wb");assert(out);
  fprintf(out,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,out);fclose(out);
}
int main(void) {
  machine_t *m=malloc(sizeof(*m));assert(m);
  prog_t p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  /* T in any manual range leaves pose, heading, throttle, and range intact. */
  pose(m,0,1e10,0);
  for(unsigned mode=0;mode<3;mode++) {
    set(m,8484,mode);m->regs[9]=32;m->regs[10]=0;
    double before[3];for(unsigned k=0;k<3;k++)before[k]=get(m,8600+k*8);
    tap(m,KEY_T);assert(!u(m,8472)&&u(m,8484)==mode&&m->regs[11]==0&&m->regs[9]==32);
    for(unsigned k=0;k<3;k++)assert(get(m,8600+k*8)==before[k]);
  }
  /* Holding T while selecting CRUISE cannot accidentally engage it. */
  set(m,8484,2);machine_key(m,KEY_T,true);step(m);
  machine_key(m,KEY_F,true);step(m);assert(u(m,8484)==3&&!u(m,8472));
  machine_key(m,KEY_F,false);step(m);assert(!u(m,8472));machine_release_keys(m);step(m);
  machine_key(m,KEY_Z,true);step(m);assert(m->regs[11]==0);machine_release_keys(m);step(m);
  /* Selecting Earth at the starting position is already nearby, with no snap. */
  pose(m,0,0,149.6e6-6378*3);set(m,8464,3);m->regs[9]=64;
  double before=get(m,8616);tap(m,KEY_T);
  assert(!u(m,8472)&&u(m,8476)==4&&get(m,8616)==before&&m->regs[9]==64);
  /* A fresh press starts distant travel; holding does not toggle; X stops it. */
  set(m,8464,4);machine_key(m,KEY_T,true);step(m);assert(u(m,8472)==1);
  step(m);assert(u(m,8472)==1);machine_key(m,KEY_T,false);
  tap(m,KEY_X);assert(!u(m,8472)&&u(m,8484)==3);
  before=get(m,8616);step(m);assert(get(m,8616)==before);
  /* Check the nearby boundary without other bodies obstructing the route. */
  pose(m,0,1e10,0);
  for(unsigned k=0;k<3;k++)put(m,20000+4*64+k*8,k==1?1e10:0);
  double radius=get(m,20000+4*64+24),arrival=get(m,20000+4*64+40);
  double nearby=fmax(5*radius,2*arrival);
  pose(m,0,1e10,-nearby);tap(m,KEY_T);assert(!u(m,8472)&&u(m,8476)==4);
  pose(m,0,1e10,-nearby-10e6);machine_key(m,KEY_T,true);step(m);assert(u(m,8472)==1);
  machine_release_keys(m);tap(m,KEY_X);
  /* Each category has its own wrap and held keys advance exactly once. */
  p=solar_physics_probe();assert(machine_load(m,&p));step(m);set(m,8464,0);
  const unsigned counts[]={10,2,3,5,3,3,2,2,2};
  const unsigned bodies[][10]={{0,1,2,3,4,5,6,7,8,9},{3,10},{4,11,12},{5,13,14,15,16},
    {6,17,18},{7,19,20},{8,21},{9,22},{23,24}};
  for(unsigned group=0;group<9;group++) {
    assert(u(m,8488)==group&&u(m,8464)==bodies[group][0]);
    for(unsigned item=1;item<=counts[group];item++) {
      machine_key(m,KEY_TAB,true);step(m);assert(u(m,8464)==bodies[group][item%counts[group]]);
      step(m);assert(u(m,8464)==bodies[group][item%counts[group]]);
      machine_key(m,KEY_TAB,false);step(m);
    }
    machine_key(m,KEY_G,true);step(m);assert(u(m,8488)==(group+1)%9);
    step(m);assert(u(m,8488)==(group+1)%9);machine_key(m,KEY_G,false);step(m);
  }
  /* Distant selected moons stay hidden; the same moon appears near its parent. */
  p=solar_flight();assert(machine_load(m,&p));frame(m);
  set(m,8464,16);set(m,8488,3);
  double x=get(m,20000+5*64),y=get(m,20000+5*64+8),z=get(m,20000+5*64+16);
  double system_radius=3*1882700;
  pose(m,x,y,z-system_radius-1);frame(m);
  assert(!u(m,11900+5*4));for(unsigned body=13;body<=16;body++)assert(!u(m,9300+body*4));
  pose(m,x,y,z-system_radius+1);frame(m);
  assert(u(m,11900+5*4)==1&&u(m,9300+16*4)==1);
  snapshot(m,"/tmp/cgvm-navigation-jupiter.ppm");
  /* The scanner reports the nearest real cached rock with a usable bearing. */
  set(m,8488,8);set(m,8464,23);
  x=get(m,20000+23*64);y=get(m,20000+23*64+8);z=get(m,20000+23*64+16);
  pose(m,x,y,z-2000);frame(m);assert(u(m,12420)==1&&!u(m,12400)&&!u(m,12500));
  machine_key(m,KEY_S,true);frame(m);machine_key(m,KEY_S,false);frame(m);
  assert(u(m,12400)==1&&u(m,12500)==1);
  double nearest=1e100,rock[3]={0};
  for(unsigned slot=0;slot<27;slot++) {
    unsigned address=24000+slot*64;if(get(m,address+24)<=0)continue;
    double d=0;
    for(unsigned k=0;k<3;k++){double v=get(m,address+k*8)-get(m,8600+k*8);d+=v*v;}
    if(d<nearest){nearest=d;for(unsigned k=0;k<3;k++)rock[k]=get(m,address+k*8);}
  }
  assert(fabs(get(m,12424)-sqrt(nearest))<1e-6);
  double dx=rock[0]-x,dy=rock[1]-y,dz=rock[2]-(z-2000);
  assert(angle_error(u(m,12404),atan2(dx,dz))<.51);
  assert(angle_error(u(m,12408),atan2(-dy,hypot(dx,dz)))<.51);
  assert(angle_error(u(m,12488),atan2(z-2000,-x))<.51);
  snapshot(m,"/tmp/cgvm-navigation-belt.ppm");
  /* One fly-to command handles alignment, SYSTEM travel, braking and arrival. */
  set(m,8484,1);machine_key(m,KEY_R,true);frame(m);machine_key(m,KEY_R,false);frame(m);
  unsigned approaches=0;
  while(u(m,12700)==2&&approaches++<2000){assert(u(m,12400));frame(m);}
  assert(approaches<2000&&u(m,10804)>0&&u(m,8476)==9&&u(m,8484)==2);
  assert(fabs(get(m,12424)-get(m,12728)-.05)<1e-5);m->regs[11]=0;
  snapshot(m,"/tmp/cgvm-rock-arrival.ppm");
  pose(m,rock[0],rock[1],rock[2]-50);m->regs[9]=m->regs[10]=0;frame(m);
  assert(u(m,12400)==1&&fabs(get(m,12424)-50)<1e-5&&u(m,12404)==0&&u(m,12408)==0);
  assert(u(m,10804)>0);snapshot(m,"/tmp/cgvm-navigation-rock.ppm");
  /* Compass follows the actual forward direction, including inverted pitch. */
  m->regs[9]=64;frame(m);assert(u(m,12412)==64&&u(m,12416)==0);
  m->regs[9]=32;m->regs[10]=128;frame(m);assert(u(m,12412)==160&&u(m,12416)==0);
  pose(m,0,1e10,0);m->regs[9]=m->regs[10]=0;frame(m);
  assert(!u(m,12400)&&!u(m,12420)&&u(m,12500)==3);
  machine_key(m,KEY_C,true);frame(m);machine_key(m,KEY_C,false);frame(m);
  assert(!u(m,12500));snapshot(m,"/tmp/cgvm-navigation-idle.ppm");
  machine_key(m,KEY_S,true);frame(m);machine_key(m,KEY_S,false);frame(m);
  assert(!u(m,12400)&&u(m,12500)==2);snapshot(m,"/tmp/cgvm-navigation-no-contact.ppm");
  /* A pulse outside the loaded belt cache can still return a distant rock. */
  p=solar_flight();assert(machine_load(m,&p));frame(m);pose(m,2*149.6e6,0,0);m->regs[9]=64;frame(m);
  machine_key(m,KEY_S,true);frame(m);machine_key(m,KEY_S,false);frame(m);
  assert(u(m,12400)==1&&get(m,12424)>20e6&&get(m,12424)<=50e6);
  for(unsigned slot=0;slot<27;slot++)assert(get(m,24000+slot*64+24)==0);
  snapshot(m,"/tmp/cgvm-navigation-wide-scan.ppm");
  puts("Solar navigation: safe cruise arming/proximity, grouped destinations, contextual moons, compass and real-rock scanner passed");
  free(m);
}
