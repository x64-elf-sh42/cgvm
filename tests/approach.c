#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
prog_t flight_clip_probe(const float vertices[9]);
static void w(machine_t *m,unsigned r,uint32_t v) { assert(bus_write(m,GPU_BASE+r,4,v)); }
static void setf(machine_t *m,unsigned a,float v) {
  uint32_t bits; memcpy(&bits,&v,4); assert(bus_write(m,a,4,bits));
}
static float getf(machine_t *m,unsigned a) {
  uint32_t bits; float v; assert(bus_read(m,a,4,&bits)); memcpy(&v,&bits,4); return v;
}
static void frame(machine_t *m) {
  uint64_t target=m->frames+1;
  while(m->frames<target) { cpu_result_t r=machine_step(m,1000); assert(r.status!=CPU_FAULT); }
}
static void reference(machine_t *m,const float *v) {
  float out[12]; unsigned count=0;
  for(unsigned a=0;a<3;a++) {
    unsigned b=(a+1)%3; bool ai=v[a*3+2]>=.5, bi=v[b*3+2]>=.5;
    if(ai!=bi) {
      double t=(.5-v[a*3+2])/(v[b*3+2]-v[a*3+2]);
      for(unsigned k=0;k<2;k++) out[count*3+k]=(float)(v[a*3+k]+t*(v[b*3+k]-v[a*3+k]));
      out[count*3+2]=.5; count++;
    }
    if(bi) { memcpy(out+count*3,v+b*3,3*sizeof(float)); count++; }
  }
  w(m,GPU_DEPTH,1);w(m,GPU_COLOR,0x33ff44);
  for(unsigned t=1;t+1<count;t++) {
    unsigned ids[3]={0,t,t+1}, xs[3]={GPU_X,GPU_X2,GPU_X3},ys[3]={GPU_Y,GPU_Y2,GPU_Y3},qs[3]={GPU_Q1,GPU_Q2,GPU_Q3};
    for(unsigned k=0;k<3;k++) {
      float *p=out+ids[k]*3;
      w(m,xs[k],(uint32_t)(int32_t)(512+400*(double)p[0]/p[2]));
      w(m,ys[k],(uint32_t)(int32_t)(384-400*(double)p[1]/p[2]));
      w(m,qs[k],(uint32_t)(1000000.0/p[2]));
    }
    w(m,GPU_COMMAND,GPU_TRIANGLE);
  }
}
int main(void) {
  machine_t *m=malloc(sizeof(*m)),*ref=malloc(sizeof(*ref));assert(m&&ref);
  for(unsigned mask=0;mask<8;mask++) {
    float v[9]={-.25f,-.25f,1,.25f,-.25f,1,0,.25f,1};
    for(unsigned k=0;k<3;k++) if(mask&(1u<<k)) v[k*3+2]=.25;
    prog_t p=flight_clip_probe(v); assert(machine_load(m,&p));
    assert(cpu_step(m,10000).status==CPU_HALT);
    machine_reset(ref); reference(ref,v);
    assert(memcmp(m->video,ref->video,sizeof(m->video))==0);
  }
  float on_plane[9]={-.25f,-.25f,.5,.25f,-.25f,.5,0,.25f,.5};
  prog_t p=flight_clip_probe(on_plane); assert(machine_load(m,&p));
  assert(cpu_step(m,10000).status==CPU_HALT);
  machine_reset(ref);reference(ref,on_plane);assert(!memcmp(m->video,ref->video,sizeof(m->video)));
  p=flight();assert(machine_load(m,&p));
  setf(m,8192,-80);setf(m,8196,20);setf(m,8200,140);
  frame(m);m->regs[11]=300;
  for(unsigned i=0;i<100;i++) { uint64_t before=m->ticks; frame(m); assert(m->ticks-before<FRAME_TICKS+2000); }
  FILE *out=fopen("/tmp/cgvm-planet-approach.ppm","wb"); assert(out);
  fprintf(out,"P6\n1024 768\n255\n"); fwrite(m->published,sizeof(m->published),1,out); fclose(out);
  float z=getf(m,8200);assert(z<=152 && z>151.5 && m->regs[11]==300 && m->regs[14]>0);
  machine_key(m,KEY_Z,true);
  for(unsigned i=0;i<60;i++) frame(m);
  assert(getf(m,8200)<=152);
  machine_release_keys(m);m->regs[9]=32;
  float initial_x=getf(m,8192);
  for(unsigned i=0;i<90;i++) {
    frame(m);
    double x=getf(m,8192)+80,y=getf(m,8196)-20,zdist=getf(m,8200)-220;
    assert(x*x+y*y+zdist*zdist>=68.0*68.0-.01);
  }
  assert(getf(m,8192)>initial_x+5 && m->regs[11]==300);
  z=getf(m,8200);
  m->regs[9]=128;m->regs[11]=300;
  for(unsigned i=0;i<10;i++) frame(m);
  assert(getf(m,8200)<z-1 && m->regs[11]==300);
  for(unsigned i=0;i<35;i++) frame(m);
  assert(m->regs[14]==0 && getf(m,8400)>1);
  printf("Approach: 8 clipping masks, on-plane geometry, surface hold, tangential slide, warning expiry and escape passed (%llu ticks/frame near planet)\n",(unsigned long long)(m->ticks/m->frames));
  free(m);free(ref);
}
