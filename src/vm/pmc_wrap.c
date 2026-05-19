#include "pmc_wrap.h"

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef DO_PMC

// uncomment counters under 1, 2 or 3 separately for different profiles. - can't be combined.
static pmc_counter_t pmcs[] = {
// 1
 // { "ex_ret_instr", 0, 0, false },
 // { "ex_ret_brn", 0, 0, false },
 // { "ex_ret_brn_misp", 0, 0, false },
 // { "all_dc_accesses", 0, 0, false },
 // { "l2_cache_accesses_from_dc_misses", 0, 0, false },
 // { "l2_cache_misses_from_dc_misses", 0, 0, false },

// 2
 // { "ic_cache_fill_l2", 0, 0, false },
 // { "ic_cache_fill_sys", 0, 0, false },
 // { "ic_fetch_stall.ic_stall_any", 0, 0, false },

// 3
  { "l1_dtlb_misses", 0, 0, false },
  { "l2_dtlb_misses", 0, 0, false },

};

static size_t pmc_count = sizeof(pmcs) / sizeof(pmcs[0]);

bool vm_pmc_init(void) {
  if (pmc_init() < 0) {
    warn("pmc_init");
    return false;
  }

  for (size_t i = 0; i < pmc_count; i++) {
    if (pmc_allocate(pmcs[i].name, PMC_MODE_TC, 0, PMC_CPU_ANY, &pmcs[i].id, 1) < 0) {
      warn("pmc_allocate %s", pmcs[i].name);
      return false;
    }
    pmcs[i].active = true;
  }

  return true;
}

void vm_pmc_start(void) {
  for (size_t i = 0; i < pmc_count; i++) {
    if (pmcs[i].active && pmc_start(pmcs[i].id) < 0) {
      warn("pmc_start %s", pmcs[i].name);
    }
  }
}

void vm_pmc_stop_read(void) {
  for (size_t i = 0; i < pmc_count; i++) {
    if (!pmcs[i].active) {
      continue;
    }

    if (pmc_stop(pmcs[i].id) < 0) {
      warn("pmc_stop %s", pmcs[i].name);
    }

    if (pmc_read(pmcs[i].id, &pmcs[i].value) < 0) {
      warn("pmc_read %s", pmcs[i].name);
    }
  }
}

void vm_pmc_print(void) {
  fprintf(stderr, "\nPMC region results:\n");
  for (size_t i = 0; i < pmc_count; i++) {
    if (pmcs[i].active) {
      fprintf(stderr, "%-40s %ju\n",
              pmcs[i].name,
              (uintmax_t)pmcs[i].value);
    }
  }
}

void vm_pmc_shutdown(void) {
  for (size_t i = 0; i < pmc_count; i++) {
    if (pmcs[i].active) {
      pmc_release(pmcs[i].id);
      pmcs[i].active = false;
    }
  }
}
#endif // DO_PMC
