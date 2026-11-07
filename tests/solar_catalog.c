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
  machine_t *m=malloc(sizeof(*m));assert(m);
  prog_t p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  const unsigned parents[]={3,4,4,5,5,5,5,6,6,7,7,8,9};
  const double separations[]={384400,9378,23459,421800,671100,1070400,1882700,1221830,238020,436300,583500,354760,19596};
  for(unsigned i=0;i<13;i++) {
    double d=0;for(unsigned k=0;k<3;k++) {double v=get(m,20000+(i+10)*64+k*8)-get(m,20000+parents[i]*64+k*8);d+=v*v;}
    assert(fabs(sqrt(d)-separations[i])<1e-5);
  }
  /* Manual flight mode is latched, bounded, and resets throttle on switching. */
  machine_key(m,KEY_F,true);step(m);assert(u(m,8484)==1&&m->regs[11]==0);
  step(m);assert(u(m,8484)==1);machine_key(m,KEY_F,false);
  m->regs[9]=128;m->regs[11]=300;double before=get(m,8616);step(m);assert(fabs(get(m,8616)-before+50000)<1e-6);
  machine_key(m,KEY_Z,true);step(m);assert(m->regs[11]==300);machine_release_keys(m);
  /* A fast manual step into Earth's safety sphere must stop outside it. */
  put(m,8616,149.6e6-6478-5);m->regs[9]=0;m->regs[11]=300;step(m);
  assert(u(m,8476)==2&&m->regs[11]==0&&distance(m,3)>6478);
  machine_key(m,KEY_F,true);step(m);assert(u(m,8484)==2&&m->regs[11]==0);machine_release_keys(m);
  machine_key(m,KEY_F,true);step(m);assert(u(m,8484)==3&&m->regs[11]==0);machine_release_keys(m);
  machine_key(m,KEY_F,true);step(m);assert(u(m,8484)==0);machine_release_keys(m);
  put(m,8616,149.6e6-6378*3);
  /* Outermost catalogue destination remains within navigation precision. */
  set(m,8464,9);set(m,8472,1);unsigned travel=0;
  while(u(m,8472)&&travel++<7000)step(m);
  assert(travel<7000&&u(m,8476)==1);
  assert(fabs(distance(m,9)-get(m,20000+9*64+40))<.01);
  /* Every new body is reachable from a nearby clear approach. */
  for(unsigned body=9;body<25;body++) {
    double r=get(m,20000+body*64+40);
    for(unsigned k=0;k<3;k++) put(m,8600+k*8,get(m,20000+body*64+k*8)-(k==2?3*r:0));
    set(m,8464,body);set(m,8472,1);unsigned n=0;
    while(u(m,8472)&&n++<400)step(m);
    assert(n<400&&u(m,8476)==1);assert(fabs(distance(m,body)-r)<.001);
  }
  set(m,8488,8);set(m,8464,24);machine_key(m,KEY_TAB,true);step(m);assert(u(m,8464)==23);machine_release_keys(m);
  p=solar_flight();assert(machine_load(m,&p));rendered_frame(m);
  for(unsigned body=9;body<25;body++) {
    unsigned group=body==9?7:body==10?1:body<=12?2:body<=16?3:body<=18?4:body<=20?5:body==21?6:body==22?7:8;
    unsigned first=group==1?10:group==2?11:group==3?13:group==4?17:group==5?19:group==6?21:22;
    unsigned row=body==9?0:body>=23?body-23:body-first+1;
    double r=get(m,20000+body*64+24);
    for(unsigned k=0;k<3;k++) put(m,8600+k*8,get(m,20000+body*64+k*8)-(k==2?3*r:0));
    set(m,8488,group);set(m,8464,body);m->regs[9]=m->regs[10]=m->regs[11]=0;
    rendered_frame(m);assert(u(m,8872+body*4)==2);
    assert(u(m,9300+body*4)==1);
    /* The selected row's text itself is green within its named system. */
    unsigned green=0,blue=0,y=100+row*23;
    for(unsigned py=y;py<y+15;py++)for(unsigned px=850;px<985;px++) {
      machine_pixel_t c=m->published[py*1024+px];
      green+=c.red==0x33&&c.green==0xff&&c.blue==0x44;
      blue+=c.red==0x9b&&c.green==0xd7&&c.blue==0xff;
    }
    assert(green>0&&blue==0);
    if(body==14) {FILE *f=fopen("/tmp/cgvm-moons.ppm","wb");assert(f);fprintf(f,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,f);fclose(f);}
  }
  puts("Catalogue: moon separations, all new arrivals, selection wrap and rendered bodies passed");free(m);
}
