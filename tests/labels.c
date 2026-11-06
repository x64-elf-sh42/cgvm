#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
prog_t solar_physics_probe(void);
static double get(machine_t*m,unsigned a){double v;uint64_t b=0;for(unsigned k=0;k<8;k++)b|=(uint64_t)m->data[a+k]<<(k*8);memcpy(&v,&b,8);return v;}
static void put(machine_t*m,unsigned a,double v){uint64_t b;memcpy(&b,&v,8);for(unsigned k=0;k<8;k++)m->data[a+k]=(uint8_t)(b>>(k*8));}
static unsigned u(machine_t*m,unsigned a){uint32_t v;assert(bus_read(m,a,4,&v));return v;}
static void frame(machine_t*m){uint64_t goal=m->frames+1;while(m->frames<goal){cpu_result_t r=machine_step(m,10000);assert(r.status!=CPU_FAULT);if(r.status==CPU_WAIT)machine_advance(m,FRAME_TICKS-m->ticks%FRAME_TICKS);}}
int main(void) {
  machine_t *m=malloc(sizeof(*m));assert(m);prog_t p=solar_flight();
  for(unsigned map=0;map<2;map++) {
    assert(machine_load(m,&p));frame(m);
    for(unsigned b=0;b<25;b++) {put(m,20000+b*64,0);put(m,20000+b*64+8,0);put(m,20000+b*64+16,1e6);put(m,20000+b*64+24,1);}
    put(m,8600,0);put(m,8608,0);put(m,8616,0);
    assert(bus_write(m,8460,4,map));assert(bus_write(m,8464,4,12));frame(m);
    assert(u(m,9300+12*4)==1);unsigned count=0;
    for(unsigned b=0;b<25;b++)if(u(m,9300+b*4)) {
      count++;unsigned x=u(m,11000+b*16),y=u(m,11004+b*16),w=u(m,11008+b*16),h=u(m,11012+b*16);
      assert(x+w<=1024&&y+h<600);
      if(map)assert(x+w<=620||y>=390);
      for(unsigned c=0;c<b;c++)if(u(m,9300+c*4)) {
        unsigned cx=u(m,11000+c*16),cy=u(m,11004+c*16);
        assert(x+w<=cx||cx+120<=x||y+h<=cy||cy+34<=y);
      }
    }
    assert(count>1&&count<25);
    unsigned dx=u(m,11000+12*16),dy=u(m,11004+12*16),green=0;
    for(unsigned y=dy;y<dy+16;y++)for(unsigned x=dx;x<dx+80;x++) {
      machine_pixel_t c=m->published[y*1024+x];green+=c.red==0x33&&c.green==0xff&&c.blue==0x44;
    }
    assert(green>200);
    if(!map){FILE *out=fopen("/tmp/cgvm-labels.ppm","wb");assert(out);fprintf(out,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,out);fclose(out);}
  }
  p=solar_physics_probe();assert(machine_load(m,&p));assert(cpu_step(m,200000).status==CPU_YIELD);
  put(m,8600,0);put(m,8608,1e10);put(m,8616,0);m->regs[11]=300;
  const double steps[]={150,15000,1.0/6000};
  for(unsigned mode=0;mode<3;mode++) {
    assert(bus_write(m,8484,4,mode));put(m,10400,1);double old=get(m,8616);
    assert(cpu_step(m,200000).status==CPU_YIELD);assert(fabs(get(m,8616)-old-steps[mode])<1e-6);
    put(m,10400,2);old=get(m,8616);assert(cpu_step(m,200000).status==CPU_YIELD);
    assert(fabs(get(m,8616)-old-2*steps[mode])<1e-6);
  }
  puts("Labels: selected priority, non-overlap, readable Deimos, map avoidance; three manual speeds and elapsed-time movement passed");free(m);
}
