#include "cgvm.h"

#include "programs/manylines.h"
#include "programs/flight.h"
#include "util.h"

extern const prog_t line_draw;

int main(int argc, [[maybe_unused]]char *argv[argc+1]) {
  // return status of main. - currently unused.
  int status = 0;

  // Guest-rendered wireframe vehicle with a fixed external camera.
  prog_t bdraw = solar_flight();
  
  printf("ASTRA: Arrows steer, Z/X throttle, F flight mode, G group, Tab select, T cruise, S scan, C clear, L hold rock bearing, R fly-to rock in SYSTEM, M map.\n");

  vm_reset();
  // load program into vm
  vm_load(&bdraw);

  // run vm to execute program
  status = vm_run() ? 0 : 1;

  
  return status;
}
