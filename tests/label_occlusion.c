#include "programs/flight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
prog_t solar_physics_probe(void);
prog_t solar_no_labels_probe(void);
static double get(machine_t*m,unsigned a){double v;uint64_t b=0;for(unsigned k=0;k<8;k++)b|=(uint64_t)m->data[a+k]<<(k*8);memcpy(&v,&b,8);return v;}
static void put(machine_t*m,unsigned a,double v){uint64_t b;memcpy(&b,&v,8);for(unsigned k=0;k<8;k++)m->data[a+k]=(uint8_t)(b>>(k*8));}
static unsigned u(machine_t*m,unsigned a){uint32_t v;assert(bus_read(m,a,4,&v));return v;}
static void step(machine_t*m){cpu_result_t r;do{r=cpu_step(m,10000);assert(r.status!=CPU_FAULT);}while(r.status==CPU_BUDGET);assert(r.status==CPU_YIELD);}
static void frame(machine_t*m){uint64_t goal=m->frames+1;while(m->frames<goal){cpu_result_t r=machine_step(m,10000);if(r.status==CPU_FAULT)fprintf(stderr,"label render fault %u at %u\n",m->fault,m->fault_pc);assert(r.status!=CPU_FAULT);if(r.status==CPU_WAIT)machine_advance(m,FRAME_TICKS-m->ticks%FRAME_TICKS);}}
static void scene(machine_t*m,const double rock[3],double rock_depth,double label_depth){
  double cam[]={rock[0]-.195*rock_depth,rock[1]-.0175*rock_depth,rock[2]-rock_depth};
  for(unsigned k=0;k<3;k++)put(m,8600+k*8,cam[k]);
  m->regs[9]=m->regs[10]=m->regs[11]=0;assert(bus_write(m,8460,4,0));assert(bus_write(m,8464,4,4));
  for(unsigned b=0;b<25;b++){put(m,20000+b*64,cam[0]+1e12);put(m,20000+b*64+8,cam[1]);put(m,20000+b*64+16,cam[2]-1e12);}
  for(unsigned k=0;k<3;k++)put(m,20000+4*64+k*8,cam[k]+(k==2?label_depth:0));
  put(m,20000+4*64+24,.0001);put(m,20000+4*64+32,.0002);put(m,20000+4*64+40,.0003);
}
int main(void){
  machine_t*m=malloc(sizeof(*m));assert(m);prog_t p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  put(m,8600,-390e6-.1);put(m,8608,-.1);put(m,8616,-3e6);step(m);
  unsigned slot=27;double radius=1e100;
  for(unsigned i=0;i<27;i++){double r=get(m,24000+i*64+24);if(r>0&&r<radius){radius=r;slot=i;}}assert(slot<27&&radius<1);
  double rock[3];for(unsigned k=0;k<3;k++)rock[k]=get(m,24000+slot*64+k*8);
  machine_pixel_t *baseline=malloc(sizeof(m->published));double *depth=malloc(sizeof(m->depth));assert(baseline&&depth);
  const double rocks[]={20,2,20},labels[]={1e6,1e6,10};
  for(unsigned view=0;view<3;view++){
    p=solar_no_labels_probe();assert(machine_load(m,&p));frame(m);scene(m,rock,rocks[view],labels[view]);frame(m);
    memcpy(baseline,m->published,sizeof(m->published));memcpy(depth,m->depth,sizeof(m->depth));
    p=solar_flight();assert(machine_load(m,&p));frame(m);scene(m,rock,rocks[view],labels[view]);frame(m);
    assert(u(m,9300+4*4)==1);unsigned x=u(m,11000+4*16),y=u(m,11004+4*16);
    assert(x==532&&y==360);unsigned foreground=0,painted=0,changed_geometry=0;
    uint32_t bits=u(m,16384);float scale;memcpy(&scale,&bits,4);double label_q=fmax(1,floor(scale/labels[view]));
    for(unsigned py=y;py<y+34;py++)for(unsigned px=x;px<x+120;px++){
      size_t i=py*1024+px;assert(m->depth[i]==depth[i]);bool same=!memcmp(&m->published[i],&baseline[i],sizeof(*baseline));
      if(depth[i]>label_q){foreground++;assert(same);}
      if(depth[i]>0&&!same)changed_geometry++;
      if(depth[i]==0&&!same)painted++;
    }
    if(view<2)assert(foreground>10&&changed_geometry==0);else assert(changed_geometry>10);
    if(!view){assert(painted>1000);FILE*out=fopen("/tmp/cgvm-label-occlusion.ppm","wb");assert(out);fprintf(out,"P6\n1024 768\n255\n");fwrite(m->published,sizeof(m->published),1,out);fclose(out);}
  }
  puts("Label occlusion: foreground rocks remain visible, nearer labels cover farther geometry, backgrounds are opaque, scene depth is unchanged");
  free(depth);free(baseline);free(m);
}
