/*
 * peephole.c
 *
 * Ryan Mallon (2005)
 *
 * Peephole optomiser for IC code
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "icg.h"
#include "debug.h"

extern int num_instructions;

static void remove_instruction(ic_list_t *);

/*
 * Run the peephole optomiser on the given program listing and return a new
 * list.
 *
 */
void peephole_optomise(void) {
#if 0
  ic_list_t *current, *next;

  current = ic_list_head();

  /* Free the unused instruction at the end of the list */
  free(ic_list_tail->next);
  ic_list_tail->next = NULL;

  while(current) {

    next = current->next;

    switch(current->instruction->op) {
    case IC_RET:
      /* Remove duplicate return instructions */
      if(current->next && current->next->instruction->op == IC_RET)
	remove_instruction(current);

      break;

    case IC_JMP:
      /* Remove jumps that jump to the next instruction */
      if(current->jump == current->next)
	remove_instruction(current);

      break;
    }


    current = next;
  }

  /* Renumber and label the list */
  ic_number_list();
#endif
}

static void remove_instruction(ic_list_t *node) {
#if 0
  if(node == ic_list_head())
    ic_list_head() = node->next;

  else if(node == ic_list_tail)
    ic_list_tail = node->prev;

  else {
    node->next->prev = node->prev;
    node->prev->next = node->next;
  }

  free(node->instruction);
  free(node);
#endif
}
