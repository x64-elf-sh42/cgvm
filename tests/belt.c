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
static void step(machine_t *m) {
  for(unsigned n=0;n<2000;n++) {
    cpu_result_t r=cpu_step(m,10000);
    assert(r.status!=CPU_FAULT);
    if(r.status==CPU_YIELD)return;
    assert(r.status==CPU_BUDGET);
  }
  assert(!"unbounded streaming work");
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
prog_t solar_region_probe(void);
#define PROXY 21600u
#define CACHE 24000u
#define CELL 10536u
static void pose(machine_t *m,double x,double y,double z) {put(m,8600,x);put(m,8608,y);put(m,8616,z);}
static bool contains(machine_t *m,const unsigned char *record) {
  for(unsigned i=0;i<27;i++)if(!memcmp(m->data+CACHE+i*64,record,64))return true;
  return false;
}
int main(void) {
  machine_t *m=malloc(sizeof(*m));assert(m);
  prog_t p=solar_region_probe();assert(machine_load(m,&p));
  unsigned occupied=0;double radius_sum=0;unsigned char saved[64];
  /* Independently check the distribution and physical bounds in interior cells. */
  for(unsigned i=0;i<1200;i++) {
    double c[]={i%2?130:-131,(int)(i%40)-20,(int)((i/40)%30)-15};
    for(unsigned k=0;k<3;k++)put(m,CELL+k*8,c[k]);step(m);
    double r=get(m,PROXY+24);if(!r)continue;occupied++;radius_sum+=r;
    assert(r>=.5&&r<=8&&get(m,PROXY+32)==r+1);
    for(unsigned k=0;k<3;k++) {
      double offset=get(m,PROXY+k*8)-c[k]*3e6;
      assert(offset>=20&&offset<=3e6-20);
      assert(get(m,PROXY+40+k*8)>=.7&&get(m,PROXY+40+k*8)<1);
    }
  }
  assert(occupied>780&&occupied<1020);assert(radius_sum/occupied<2);
  printf("Interior occupancy %u/1200, mean radius %.3f km\n",occupied,radius_sum/occupied);
  const double cells[][3]={{130,0,0},{-131,-1,-1},{0,0,0},{180,0,0},{130,30,0}};
  for(unsigned i=0;i<5;i++) {
    for(unsigned k=0;k<3;k++)put(m,CELL+k*8,cells[i][k]);step(m);memcpy(saved,m->data+PROXY,64);
    for(unsigned k=0;k<3;k++)put(m,CELL+k*8,999);step(m);
    for(unsigned k=0;k<3;k++)put(m,CELL+k*8,cells[i][k]);step(m);assert(!memcmp(saved,m->data+PROXY,64));
    if(i>=2)assert(get(m,PROXY+24)==0);
  }
  /* Procedural objects cannot overlap a catalogue body's safety shell. */
  unsigned found=0;
  for(unsigned i=0;i<20;i++) {
    put(m,CELL,130);put(m,CELL+8,0);put(m,CELL+16,i);step(m);
    if(get(m,PROXY+24)>0){found=1;break;}
  }
  assert(found);
  for(unsigned k=0;k<3;k++)put(m,20000+23*64+k*8,get(m,PROXY+k*8));
  step(m);assert(get(m,PROXY+24)==0);
  /* Cache refreshes only on region changes, including negative boundaries. */
  p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  assert(u(m,10800)==1);step(m);assert(u(m,10800)==1);
  pose(m,-390e6,0,0);step(m);assert(get(m,10560)==-130);
  pose(m,-390e6-.1,-.1,-3e6);step(m);
  assert(get(m,10560)==-131&&get(m,10568)==-1&&get(m,10576)==-1);
  unsigned char cache[27*64];memcpy(cache,m->data+CACHE,sizeof(cache));
  pose(m,4.5e9,0,0);step(m);for(unsigned i=0;i<27;i++)assert(get(m,CACHE+i*64+24)==0);
  pose(m,-390e6-.1,-.1,-3e6);step(m);assert(!memcmp(cache,m->data+CACHE,sizeof(cache)));
  /* Obtain a real procedural object; there are no fixed encounter fixtures. */
  unsigned slot=0;while(slot<27&&!get(m,CACHE+slot*64+24))slot++;assert(slot<27);
  memcpy(saved,m->data+CACHE+slot*64,64);
  double xyz[3],r=get(m,CACHE+slot*64+24);
  for(unsigned k=0;k<3;k++)xyz[k]=get(m,CACHE+slot*64+k*8);
  pose(m,xyz[0],xyz[1],xyz[2]-r-6);step(m);assert(contains(m,saved));
  set(m,8484,1);m->regs[9]=m->regs[10]=0;m->regs[11]=300;step(m);
  assert(u(m,8476)==2&&m->regs[11]==0);
  assert(fabs(get(m,8616)-(xyz[2]-r-2))<1e-6);
  /* A cruise segment crosses many unloaded regions; axis-aligned and diagonal. */
  const double directions[][3]={{0,0,1},{0,0,-1},{1,0,0},{0,1,0},{.6,0,.8}};
  for(unsigned d=0;d<5;d++) {
    p=solar_physics_probe();assert(machine_load(m,&p));step(m);
    for(unsigned k=0;k<3;k++) {
      put(m,8600+k*8,xyz[k]-directions[d][k]*30e6);
      put(m,20000+4*64+k*8,xyz[k]+directions[d][k]*30e6);
    }
    step(m);assert(!contains(m,saved));
    put(m,10400,100);set(m,8464,4);set(m,8472,1);step(m);
    assert(u(m,8476)==2&&u(m,8472)==0&&u(m,10808)>54);
    double distance=0;for(unsigned k=0;k<3;k++){double v=get(m,8600+k*8)-xyz[k];distance+=v*v;}
    assert(fabs(sqrt(distance)-r-2)<1e-5);
  }
  /* Anonymous geometry up close; no markers/labels or extra destinations. */
  p=solar_flight();printf("Streamed solar program: %zu words, %zu data bytes\n",p.codelen,p.datalen);
  assert(p.codelen<3500000&&p.datalen<200000);assert(machine_load(m,&p));
  pose(m,xyz[0],xyz[1],xyz[2]-fmax(4*r,4));rendered_frame(m);
  assert(u(m,10804)>0);machine_pixel_t pixel=m->published[384*1024+512];assert(pixel.red||pixel.green||pixel.blue);
  FILE *out=fopen("/tmp/cgvm-streamed-rock.ppm","wb");assert(out);fprintf(out,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,out);fclose(out);
  pose(m,xyz[0],xyz[1],xyz[2]-1000*r);rendered_frame(m);assert(u(m,10804)==0);
  for(unsigned i=0;i<48;i++)assert(u(m,10000+i*4)==0);
  /* A cell boundary must not replace rocks visible on both sides. */
  p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  double edge=floor(xyz[0]/3e6)*3e6;
  pose(m,edge-.1,xyz[1],xyz[2]);step(m);assert(contains(m,saved));
  pose(m,edge+.1,xyz[1],xyz[2]);step(m);assert(contains(m,saved));
  set(m,8464,0);
  for(unsigned b=0;b<50;b++) {machine_key(m,KEY_TAB,true);step(m);assert(u(m,8464)==(b+1)%25);machine_key(m,KEY_TAB,false);step(m);}
  /* Earlier named-body lookahead and selected-target range remain intact. */
  p=solar_flight();assert(machine_load(m,&p));rendered_frame(m);
  for(unsigned k=0;k<3;k++)put(m,8600+k*8,get(m,20000+11*64+k*8)-(k==2?1e8:0));
  set(m,8464,4);rendered_frame(m);assert(u(m,9300+11*4)==1);
  put(m,8600,get(m,20000+11*64)-.8e8);rendered_frame(m);assert(u(m,9300+11*4)==0);
  put(m,8600,get(m,20000+11*64));put(m,8616,get(m,20000+11*64+16)-2e9);
  rendered_frame(m);assert(u(m,9300+11*4)==0);
  set(m,8464,11);rendered_frame(m);assert(u(m,9300+11*4)==1);
  /* Rendered cruise still uses guest time correctly across clock wrap. */
  p=solar_flight();assert(machine_load(m,&p));rendered_frame(m);
  pose(m,0,1e10,0);
  for(unsigned k=0;k<3;k++)put(m,20000+8*64+k*8,k==1?1e10:k==2?1e9:0);
  set(m,8464,8);set(m,8472,1);m->ticks=UINT32_MAX-50000ull;
  set(m,10408,(uint32_t)m->ticks);machine_advance(m,200000);
  double old=get(m,8616);rendered_frame(m);
  assert(u(m,8472)==1&&get(m,10400)>=2);
  assert(fabs((get(m,8616)-old)-get(m,10400)*149.6e6/60)<1e-5);
  puts("Regional belt: population, bounds, repeat visits, cache reuse, negative boundaries, long swept collisions, dark geometry and named-only navigation passed");free(m);
}
