#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void setf(machine_t *m,unsigned a,float v) { uint32_t b;memcpy(&b,&v,4);assert(bus_write(m,a,4,b)); }
static float getf(machine_t *m,unsigned a) { uint32_t b;float v;assert(bus_read(m,a,4,&b));memcpy(&v,&b,4);return v; }
static void frame(machine_t *m) {
  uint64_t next=m->frames+1;
  while(m->frames<next) { cpu_result_t r=machine_step(m,1000);assert(r.status!=CPU_FAULT); }
}
int main(void) {
  machine_t *m=malloc(sizeof(*m));assert(m);prog_t p=flight();assert(machine_load(m,&p));
  setf(m,8192,-80);setf(m,8196,20);setf(m,8200,140);frame(m);
  assert(m->regs[15]==1 && fabsf(getf(m,8420)-12)<.01f);
  m->regs[9]=128;frame(m);frame(m);assert(m->regs[15]==0);
  uint32_t near;assert(bus_read(m,8412,4,&near)&&near==1);
  m->regs[9]=64;frame(m);frame(m);assert(m->regs[15]==0);
  setf(m,8192,220);setf(m,8196,100);setf(m,8200,400);m->regs[9]=0;
  frame(m);frame(m);assert(m->regs[15]==2 && fabsf(getf(m,8420)-42)<.01f);
  /* A ray grazing just outside the exclusion sphere must miss. */
  setf(m,8192,329);frame(m);frame(m);assert(m->regs[15]==0);
  assert(machine_load(m,&p));frame(m);m->regs[10]=128;machine_key(m,KEY_RIGHT,true);
  for(unsigned i=0;i<5;i++) frame(m);
  assert(m->regs[9]>245); /* Screen-right now decrements world yaw. */
  assert(machine_load(m,&p));frame(m);machine_key(m,KEY_RIGHT,true);
  for(unsigned i=0;i<5;i++) frame(m);
  assert(m->regs[9]>0 && m->regs[9]<10);
  uint32_t selected,visible;
  assert(bus_read(m,8464,4,&selected)&&selected==1);
  machine_key(m,KEY_TAB,true);frame(m);frame(m);
  assert(bus_read(m,8464,4,&selected)&&selected==2);
  frame(m);assert(bus_read(m,8464,4,&selected)&&selected==2);
  machine_key(m,KEY_TAB,false);machine_key(m,KEY_TAB,true);frame(m);frame(m);
  assert(bus_read(m,8464,4,&selected)&&selected==1);
  machine_key(m,KEY_M,true);frame(m);frame(m);
  assert(bus_read(m,8460,4,&visible)&&visible==0);
  assert(m->published[60*VIDEO_WIDTH+730].red!=0x30);
  assert(m->published[600*VIDEO_WIDTH+12].red==0x30);
  machine_key(m,KEY_M,false);machine_key(m,KEY_M,true);frame(m);frame(m);
  assert(bus_read(m,8460,4,&visible)&&visible==1);
  FILE *out=fopen("/tmp/cgvm-navigation.ppm","wb");assert(out);
  fprintf(out,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,out);fclose(out);
  free(m);puts("Navigation: named ray hits, behind/lateral misses, nearest body and inverted yaw passed");
}
