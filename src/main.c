#include "cgvm.h"

#include "programs/manylines.h"

extern const prog_t line_draw;

int main(int argc, [[maybe_unused]]char *argv[argc+1]) {
  // return status of main. - currently unused.
  int status = 0;

  // test many_lines program
  prog_t mlines = make_line_bench();
  // load program into vm
  vm_load(&mlines);
  // run vm to execute program
  vm_run();

  
  return status;
}
