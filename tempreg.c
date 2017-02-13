/*
 * tempreg.c
 * Ryan Mallon (2006)
 *
 * Temporary register allocator for intermediate representations
 *
 */
#include "icg.h"
#include "symtable.h"
#include "scope.h"
#include "cflags.h"
#include "cerror.h"

extern compiler_options_t cflags;
static int base_reg, temp_regs, max_regs;

/*
 * Initialise the temporary register allocator
 */
void tr_init(void) {
  base_reg = 0;

  if(cflags.flags & CFLAG_OUTPUT_IC)
    base_reg = IC_NUM_SREGS;

  temp_regs = base_reg;
  max_regs = temp_regs;
}

/*
 * Allocate a temporary register
 */
int tr_alloc(void) {
  if(temp_regs == max_regs)
    max_regs++;
  return temp_regs++;
}

/*
 * Set the next available temporary register
 */
void tr_set_lowest(int lowest) {
  while(temp_regs < lowest)
    tr_alloc();
}

/*
 * Allocate k temporary registers
 * FIXME: Shouldn't really use this
 */
void tr_alloc_k(int k) {
  int i;
  for(i = 0; i < k; i++)
    tr_alloc();
}

/*
 * Deallocate a temporary register
 */
void tr_dealloc(void) {
  temp_regs--;
}

/*
 * Deallocate all temorary registers
 */
void tr_dealloc_all(void) {
  temp_regs = base_reg;
}

/*
 * Return the maximum number of required temporaries
 */
int tr_max_temps(void) {
  return max_regs;
}
