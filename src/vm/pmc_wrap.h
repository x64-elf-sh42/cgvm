#pragma once

#include <pmc.h>

typedef struct pmc_counter {
  const char *name;
  pmc_id_t id;
  pmc_value_t value;
  bool active;
} pmc_counter_t;


bool vm_pmc_init(void);
void vm_pmc_start(void);
void vm_pmc_stop_read(void);
void vm_pmc_print(void);
void vm_pmc_shutdown(void);
