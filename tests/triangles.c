#include "vm/machine.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static void w(machine_t *m,unsigned reg,uint32_t v) { assert(bus_write(m,GPU_BASE+reg,4,v)); }
int main(void) {
  machine_t *m=calloc(1,sizeof(*m)); assert(m);
  w(m,GPU_DEPTH,1);
  w(m,GPU_X,10); w(m,GPU_Y,10); w(m,GPU_X2,30); w(m,GPU_Y2,10); w(m,GPU_X3,10); w(m,GPU_Y3,30);
  w(m,GPU_Q1,100); w(m,GPU_Q2,100); w(m,GPU_Q3,100);
  w(m,GPU_COLOR,0xff0000); w(m,GPU_COMMAND,GPU_TRIANGLE);
  size_t inside=15*VIDEO_WIDTH+15;
  assert(m->video[inside].red==255 && m->video[0].red==0);
  w(m,GPU_Q1,50);w(m,GPU_Q2,50);w(m,GPU_Q3,50);
  w(m,GPU_COLOR,0x0000ff);w(m,GPU_COMMAND,GPU_TRIANGLE);
  assert(m->video[inside].red==255);
  w(m,GPU_Q1,200);w(m,GPU_Q2,200);w(m,GPU_Q3,200);
  w(m,GPU_COMMAND,GPU_TRIANGLE); assert(m->video[inside].blue==255);
  w(m,GPU_X,10);w(m,GPU_Y,15);w(m,GPU_X2,20);w(m,GPU_Y2,15);
  w(m,GPU_Q1,50);w(m,GPU_Q2,50);w(m,GPU_COLOR,0x00ff00);
  w(m,GPU_COMMAND,GPU_LINE);assert(m->video[inside].blue==255);
  w(m,GPU_Q1,300);w(m,GPU_Q2,300);w(m,GPU_COMMAND,GPU_LINE);
  assert(m->video[inside].green==255);
  /* Overlay backgrounds and text at equal Q must both pass without hiding
   * later text or replacing the geometry's original depth. */
  w(m,GPU_DEPTH,GPU_DEPTH_TEST_ONLY);uint32_t mode;assert(bus_read(m,GPU_BASE+GPU_DEPTH,4,&mode)&&mode==GPU_DEPTH_TEST_ONLY);
  w(m,GPU_Q1,400);w(m,GPU_Q2,400);w(m,GPU_COLOR,0xff0000);w(m,GPU_COMMAND,GPU_LINE);
  assert(m->video[inside].red==255&&m->depth[inside]==300);
  w(m,GPU_COLOR,0xffffff);w(m,GPU_COMMAND,GPU_LINE);assert(m->video[inside].blue==255&&m->depth[inside]==300);
  w(m,GPU_X,10);w(m,GPU_Y,10);w(m,GPU_X2,30);w(m,GPU_Y2,10);w(m,GPU_X3,10);w(m,GPU_Y3,30);
  w(m,GPU_Q1,500);w(m,GPU_Q2,500);w(m,GPU_Q3,500);w(m,GPU_COLOR,0xff0000);w(m,GPU_COMMAND,GPU_TRIANGLE);
  assert(m->video[inside].red==255&&m->video[inside].green==0&&m->depth[inside]==300);
  w(m,GPU_Q1,250);w(m,GPU_Q2,250);w(m,GPU_Q3,250);w(m,GPU_COLOR,0x0000ff);w(m,GPU_COMMAND,GPU_TRIANGLE);
  assert(m->video[inside].red==255&&m->depth[inside]==300);
  w(m,GPU_DEPTH,GPU_DEPTH_WRITE);
  w(m,GPU_COLOR,0);w(m,GPU_COMMAND,GPU_CLEAR);assert(m->depth[inside]==0);
  w(m,GPU_X,0x80000000);w(m,GPU_Y,0x80000000);
  w(m,GPU_X2,0x7fffffff);w(m,GPU_Y2,0);w(m,GPU_X3,0);w(m,GPU_Y3,0x7fffffff);
  w(m,GPU_COLOR,0xffffff);w(m,GPU_COMMAND,GPU_TRIANGLE);
  assert(m->video[inside].red==255);
  assert(!bus_write(m,GPU_BASE+GPU_DEPTH,4,3));
  free(m); puts("Triangles: fill, overlap, line occlusion, read-only depth overlays, clear, extreme clipping passed");
}
