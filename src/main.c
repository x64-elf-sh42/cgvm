#include "cgvm.h"

#include "programs/manylines.h"
#include "programs/boxdraw.h"

extern const prog_t line_draw;

int main(int argc, [[maybe_unused]]char *argv[argc+1]) {
  // return status of main. - currently unused.
  int status = 0;

  // test many_lines program
  prog_t bdraw = boxdraw();
  vm_reset();
  // load program into vm
  vm_load(&bdraw);

  // run vm to execute program
  vm_run();

  
  return status;
}
