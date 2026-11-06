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
static unsigned width(machine_t*m){unsigned min=1024,max=0;for(unsigned y=260;y<440;y++)for(unsigned x=300;x<620;x++){machine_pixel_t p=m->published[y*1024+x];if(p.red>p.green && p.green>p.blue && abs((int)p.red*157-(int)p.green*201)<400 && abs((int)p.green*114-(int)p.blue*157)<300){if(x<min)min=x;if(x>max)max=x;}}return min<=max?max-min+1:0;}
int main(void){
  machine_t*m=malloc(sizeof(*m));assert(m);prog_t p=solar_flight();
  const double distances[]={800,399,200,40,26,24,10,3};unsigned previous=0;
  for(unsigned i=0;i<8;i++){
    assert(machine_load(m,&p));double radius=get(m,20000+5*64+24);
    put(m,8600,get(m,20000+5*64));put(m,8608,0);put(m,8616,get(m,20000+5*64+16)-distances[i]*radius);
    frame(m);
    assert(u(m,8464)==4); /* Jupiter must scale even though Mars is selected. */
    assert(u(m,8872+5*4)==(i==0?0:i<5?1:2));
    assert(u(m,9300+5*4)==1); /* Quarter-pixel bodies are identified, including point markers. */
    unsigned pixels=i?width(m):1;assert(pixels>=previous);assert(pixels<=2*400/distances[i]+4);previous=pixels;
    printf("Jupiter at %.0f radii: %u pixels wide\n",distances[i],pixels);
  }
  /* Right-edge leader uses left placement; off-screen/behind centers are omitted. */
  const double centers[]={900,1200,512};
  for(unsigned i=0;i<3;i++) {
    assert(machine_load(m,&p));double r=get(m,20000+5*64+24),z=(i==2?-10:10)*r;
    put(m,8600,get(m,20000+5*64)-(centers[i]-512)*z/400);
    put(m,8608,0);put(m,8616,get(m,20000+5*64+16)-z);frame(m);
    assert(u(m,9300+5*4)==(i==0));
  }
  p=solar_physics_probe();assert(machine_load(m,&p));assert(cpu_step(m,200000).status==CPU_YIELD);
  double radius=get(m,20000+5*64+24),center=get(m,20000+5*64+16);
  put(m,8600,get(m,20000+5*64));put(m,8608,0);put(m,8616,center-500*radius);
  assert(bus_write(m,8464,4,5));assert(bus_write(m,8472,4,1));unsigned steps=0;
  double old=center-get(m,8616);
  while(u(m,8472)&&steps++<500){assert(cpu_step(m,200000).status==CPU_YIELD);double now=center-get(m,8616);assert(fabs((old-now)-fmin(149.6e6/60,old-get(m,20000+5*64+40)))<1e-5);old=now;}
  assert(steps<500 && u(m,8476)==1);
  puts("Projected-size LOD, non-target growth, and constant cruise and exact arrival passed");free(m);
}
