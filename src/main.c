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
  
  printf("ASTRA: Arrow keys steer, Z/X throttle, F flight range, Tab destination, T travel, M map.\n");

  vm_reset();
  // load program into vm
  vm_load(&bdraw);

  // run vm to execute program
  status = vm_run() ? 0 : 1;

  
  return status;
}
