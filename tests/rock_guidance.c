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
static void set(machine_t*m,unsigned a,unsigned v){assert(bus_write(m,a,4,v));}
static void step(machine_t*m){for(unsigned i=0;i<2000;i++){cpu_result_t r=cpu_step(m,10000);if(r.status==CPU_FAULT)fprintf(stderr,"guidance fault %u at %u, address %u\n",m->fault,m->fault_pc,m->fault_address);assert(r.status!=CPU_FAULT);if(r.status==CPU_YIELD)return;}assert(!"unbounded guidance");}
static void tap(machine_t*m,unsigned key){machine_key(m,key,true);step(m);machine_key(m,key,false);step(m);}
static void pose(machine_t*m,const double xyz[3],const double dir[3],double distance){for(unsigned k=0;k<3;k++)put(m,8600+k*8,xyz[k]-dir[k]*distance);}
static double distance(machine_t*m,const double xyz[3]){double d=0;for(unsigned k=0;k<3;k++){double v=get(m,8600+k*8)-xyz[k];d+=v*v;}return sqrt(d);}
static void locked(machine_t*m,const double xyz[3],double radius){
  machine_release_keys(m);set(m,12528,0);set(m,12400,1);set(m,12500,1);set(m,12700,0);set(m,8472,0);set(m,8484,1);set(m,8476,0);
  for(unsigned k=0;k<3;k++)put(m,12504+k*8,xyz[k]);put(m,12728,radius);put(m,10400,1);m->regs[11]=0;
}
int main(void){
  machine_t*m=malloc(sizeof(*m));assert(m);prog_t p=solar_physics_probe();assert(machine_load(m,&p));step(m);
  tap(m,KEY_L);assert(u(m,8476)==5&&!u(m,12700));tap(m,KEY_R);assert(u(m,8476)==6&&!u(m,12700));
  put(m,8600,-390e6-.1);put(m,8608,-.1);put(m,8616,-3e6);step(m);tap(m,KEY_S);assert(u(m,12400));
  double xyz[3],r=get(m,12728);for(unsigned k=0;k<3;k++)xyz[k]=get(m,12504+k*8);assert(r>=.5&&r<=8);
  const double forward[]={0,0,1},side[]={1,0,0};
  locked(m,xyz,r);pose(m,xyz,side,1e6);set(m,8484,0);step(m);tap(m,KEY_L);
  assert(u(m,12700)==1&&m->regs[9]==64&&m->regs[11]==0);
  machine_key(m,KEY_Z,true);step(m);assert(u(m,12700)==1&&m->regs[11]==1);machine_release_keys(m);
  tap(m,KEY_L);assert(!u(m,12700)&&m->regs[11]==1);m->regs[11]=0;
  /* R does not launch from LOCAL or FLIGHT, and held input cannot toggle it. */
  set(m,8484,2);tap(m,KEY_R);assert(!u(m,12700)&&u(m,8476)==6&&m->regs[11]==0);
  const double directions[][3]={{0,0,1},{1,0,0},{.6,0,.8},{.36,.48,.8}};
  for(unsigned d=0;d<4;d++){
    locked(m,xyz,r);pose(m,xyz,directions[d],5e6);m->regs[9]=m->regs[10]=0;step(m);
    machine_key(m,KEY_R,true);step(m);assert(u(m,12700)==2);
    double old=distance(m,xyz),speed=3e6;unsigned n=0;
    while(u(m,12700)==2&&n++<3000){step(m);double now=distance(m,xyz);assert(now<=old+1e-6&&now>=r+.0499);assert(get(m,12736)<=speed+1e-5&&get(m,12736)<=3e6);speed=get(m,12736);old=now;}
    assert(n<3000&&u(m,8476)==9&&u(m,8484)==2&&m->regs[11]==0);machine_release_keys(m);step(m);
    assert(fabs(distance(m,xyz)-r-.05)<1e-4&&u(m,12400));
    double stop[3];for(unsigned k=0;k<3;k++)stop[k]=get(m,8600+k*8);step(m);for(unsigned k=0;k<3;k++)assert(get(m,8600+k*8)==stop[k]);
  }
  /* R takes over even when throttle/steering were held. Arrival cannot resume
   * those old controls or require a manual braking response. */
  locked(m,xyz,r);pose(m,xyz,forward,4e6);step(m);
  machine_key(m,KEY_Z,true);machine_key(m,KEY_RIGHT,true);step(m);
  machine_key(m,KEY_R,true);step(m);assert(u(m,12700)==2);machine_key(m,KEY_R,false);
  unsigned held_steps=0;while(u(m,12700)==2&&held_steps++<300)step(m);
  assert(held_steps<300&&u(m,8476)==9&&u(m,8484)==2&&m->regs[11]==0);
  double stopped[3];for(unsigned k=0;k<3;k++)stopped[k]=get(m,8600+k*8);
  for(unsigned i=0;i<20;i++){step(m);assert(m->regs[11]==0);for(unsigned k=0;k<3;k++)assert(get(m,8600+k*8)==stopped[k]);}
  machine_release_keys(m);machine_key(m,KEY_Z,true);step(m);assert(m->regs[11]==1);machine_release_keys(m);
  locked(m,xyz,r);pose(m,xyz,forward,4e6);step(m);machine_key(m,KEY_X,true);step(m);
  machine_key(m,KEY_R,true);step(m);assert(u(m,12700)==2);machine_key(m,KEY_R,false);
  held_steps=0;while(u(m,12700)==2&&held_steps++<300)step(m);
  for(unsigned i=0;i<20;i++){step(m);assert(u(m,8476)==9&&m->regs[11]==0);}machine_release_keys(m);
  locked(m,xyz,r);pose(m,xyz,forward,100000);step(m);tap(m,KEY_R);assert(u(m,12704)!=3);step(m);assert(u(m,8476)==9);
  /* Every override releases fly-to and leaves throttle at rest. */
  const unsigned cancels[]={KEY_R,KEY_X,KEY_UP,KEY_LEFT,KEY_Z,KEY_F,KEY_C,KEY_S,KEY_TAB,KEY_G};
  for(unsigned i=0;i<sizeof(cancels)/sizeof(cancels[0]);i++){
    locked(m,xyz,r);pose(m,xyz,forward,5e6);step(m);tap(m,KEY_R);assert(u(m,12700)==2);
    tap(m,cancels[i]);assert(!u(m,12700));if(cancels[i]!=KEY_Z)assert(m->regs[11]==0);
  }
  /* Switching from fly-to to alignment stops movement, retaining heading lock. */
  locked(m,xyz,r);pose(m,xyz,forward,5e6);step(m);tap(m,KEY_R);tap(m,KEY_L);assert(u(m,12700)==1&&m->regs[11]==0);
  /* Clearing or losing the scanner target cannot leave autopilot running. */
  locked(m,xyz,r);pose(m,xyz,forward,5e6);step(m);tap(m,KEY_R);pose(m,xyz,forward,60e6);step(m);
  assert(!u(m,12700)&&!u(m,12400)&&u(m,8476)==8&&m->regs[11]==0);
  /* Long updates clip arrival exactly; an already-close target never backs out. */
  locked(m,xyz,r);pose(m,xyz,forward,r+1);step(m);put(m,10400,100);tap(m,KEY_R);
  assert(!u(m,12700)&&u(m,8476)==9&&fabs(distance(m,xyz)-r-.05)<1e-4);
  locked(m,xyz,r);pose(m,xyz,forward,r+.03);step(m);double old=distance(m,xyz);tap(m,KEY_R);
  assert(!u(m,12700)&&fabs(distance(m,xyz)-old)<1e-6);
  /* Nearby and imminent warnings are distinct, including before a fast hit. */
  locked(m,xyz,r);pose(m,xyz,forward,r+5);m->regs[9]=m->regs[10]=0;step(m);assert(u(m,12704)==1);
  pose(m,xyz,forward,r+.05);step(m);assert(u(m,12704)==2&&fabs(get(m,12712)-.05)<1e-6);
  pose(m,xyz,forward,100000);m->regs[11]=300;put(m,8736,50000);step(m);assert(u(m,12704)==3);
  m->regs[9]=128;put(m,8736,50000);step(m);assert(!u(m,12704));
  /* Manual high-speed exclusion stops about 11 metres beyond bounding radius. */
  pose(m,xyz,forward,r+.5);m->regs[9]=0;m->regs[11]=300;step(m);
  assert(u(m,8476)==10&&m->regs[11]==0&&fabs(distance(m,xyz)-r-.011)<1e-4);step(m);assert(u(m,12704)==2);
  /* Planetary obstacles still cancel rock guidance before the target. */
  locked(m,xyz,r);pose(m,xyz,forward,5e6);
  for(unsigned k=0;k<3;k++)put(m,20000+64+k*8,xyz[k]-(k==2?2e6:0));put(m,20000+64+32,1000);
  step(m);tap(m,KEY_R);unsigned n=0;while(u(m,12700)==2&&n++<1000)step(m);
  assert(n<1000&&u(m,8476)==2&&!u(m,12700)&&m->regs[11]==0&&distance(m,xyz)>2e6);
  puts("Rock guidance: bearing hold, automatic cruise/arrival, held-input takeover, overrides, target loss, warnings, close exclusion and obstacles passed");free(m);
}
